/***************************************************************************
 *   Copyright (C) 2012-2022 by Terraneo Federico                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#include <drivers/flash.h>
#include <drivers/hwmapping.h>
#include <interfaces/delays.h>

using namespace std;
using namespace miosix;

/**
 * Transfer a byte through SPI2 where the flash is connected
 * \param data byte to send
 * \return byte received
 */
static unsigned char spi2sendRecv(unsigned char data=0)
{
    SPI2->DR=data;
    while((SPI2->SR & SPI_SR_RXNE)==0) ;
    return SPI2->DR;
}

//
// class Flash
//

Flash& Flash::instance()
{
    static Flash singleton;
    return singleton;
}

void Flash::eraseSector(unsigned int addr)
{
    lock_guard<mutex> l(m);
    writeEnable();
    flash_cs::low();
    spi2sendRecv(0x20); //Sector erase
    spi2sendRecv((addr>>16) & 0xff);
    spi2sendRecv((addr>>8) & 0xff);
    spi2sendRecv(addr & 0xff);
    flash_cs::high();

    do Thread::sleep(20); while(readStatus() & 0x1); //Sector erase time ~60ms
}

void Flash::eraseBlock(unsigned int addr)
{
    lock_guard<mutex> l(m);
    writeEnable();
    flash_cs::low();
    spi2sendRecv(0xd8); //Block erase (64K)
    spi2sendRecv((addr>>16) & 0xff);
    spi2sendRecv((addr>>8) & 0xff);
    spi2sendRecv(addr & 0xff);
    flash_cs::high();

    do Thread::sleep(20); while(readStatus() & 0x1); //Sector erase time ~150ms
}

bool Flash::write(unsigned int addr, const void *data, int size)
{
    if(addr>=this->size() || addr+size>this->size()) return false;
    lock_guard<mutex> l(m);
    writeEnable();
    flash_cs::low();
    spi2sendRecv(0x02); //Page program
    spi2sendRecv((addr>>16) & 0xff);
    spi2sendRecv((addr>>8) & 0xff);
    spi2sendRecv(addr & 0xff);
    
    //DMA1 stream 4 channel 0 = SPI2_TX
    
    error=false;

    //Wait until the SPI is busy, required otherwise the last byte is not
    //fully sent
    while((SPI2->SR & SPI_SR_TXE)==0) ;
    while(SPI2->SR & SPI_SR_BSY) ;
    SPI2->CR1=0;
    SPI2->CR2=SPI_CR2_TXDMAEN;
    SPI2->CR1=SPI_CR1_SSM
            | SPI_CR1_SSI
            | SPI_CR1_MSTR
            | SPI_CR1_SPE;

    waiting=Thread::getCurrentThread();

    DMA1_Stream4->CR=0;
    DMA1_Stream4->PAR=reinterpret_cast<unsigned int>(&SPI2->DR);
    DMA1_Stream4->M0AR=reinterpret_cast<unsigned int>(data);
    DMA1_Stream4->NDTR=size;
    DMA1_Stream4->CR=DMA_SxCR_PL_1 //High priority because fifo disabled
                | DMA_SxCR_MINC    //Increment memory pointer
                | DMA_SxCR_DIR_0   //Memory to peripheral
                | DMA_SxCR_TCIE    //Interrupt on transfer complete
                | DMA_SxCR_TEIE    //Interrupt on transfer error
                | DMA_SxCR_DMEIE   //Interrupt on direct mode error
                | DMA_SxCR_EN;     //Start DMA
    
    {
        FastGlobalIrqLock dLock;
        while(waiting) waiting->IRQglobalIrqUnlockAndWait(dLock);
    }

    //Wait for last byte to be sent
    while((SPI2->SR & SPI_SR_TXE)==0) ;
    while(SPI2->SR & SPI_SR_BSY) ;
    SPI2->CR1=0;
    SPI2->CR2=0;
    SPI2->CR1=SPI_CR1_SSM
            | SPI_CR1_SSI
            | SPI_CR1_MSTR
            | SPI_CR1_SPE;
    
    //Quirk: reset RXNE by reading DR, or a byte remains in the input buffer
    volatile short temp=SPI1->DR;
    (void)temp;
    
    flash_cs::high();

    do Thread::sleep(1); while(readStatus() & 0x1); //Page program time < 1ms
    return !error;
}

bool Flash::read(unsigned int addr, void *data, int size)
{
    if(addr>=this->size() || addr+size>this->size()) return false;
    lock_guard<mutex> l(m);
    flash_cs::low();
    spi2sendRecv(0x03); //Read command is acceptable because F < 33MHz
    spi2sendRecv((addr>>16) & 0xff);
    spi2sendRecv((addr>>8) & 0xff);
    spi2sendRecv(addr & 0xff);
    
    //DMA1 stream 3 channel 0 = SPI2_RX

    error=false;

    //Wait until the SPI is busy, required otherwise the last byte is not
    //fully sent
    while((SPI2->SR & SPI_SR_TXE)==0) ;
    while(SPI2->SR & SPI_SR_BSY) ;
    //Quirk: reset RXNE by reading DR before starting the DMA, or the first
    //byte in the DMA buffer is garbage
    volatile short temp=SPI2->DR;
    (void)temp;
    SPI2->CR1=0;
    SPI2->CR2=SPI_CR2_RXDMAEN;

    waiting=Thread::getCurrentThread();

    DMA1_Stream3->CR=0;
    DMA1_Stream3->PAR=reinterpret_cast<unsigned int>(&SPI2->DR);
    DMA1_Stream3->M0AR=reinterpret_cast<unsigned int>(data);
    DMA1_Stream3->NDTR=size;
    DMA1_Stream3->CR=DMA_SxCR_PL_1 //High priority because fifo disabled
                | DMA_SxCR_MINC    //Increment memory pointer
                | DMA_SxCR_TCIE    //Interrupt on transfer complete
                | DMA_SxCR_TEIE    //Interrupt on transfer error
                | DMA_SxCR_DMEIE   //Interrupt on direct mode error
                | DMA_SxCR_EN;     //Start DMA

    //Quirk: start the SPI in RXONLY mode only *after* the DMA has been
    //setup or the SPI doesn't wait for the DMA and the first bytes are lost
    SPI2->CR1=SPI_CR1_RXONLY
            | SPI_CR1_SSM
            | SPI_CR1_SSI
            | SPI_CR1_MSTR
            | SPI_CR1_SPE;
        
    {
        FastGlobalIrqLock dLock;
        while(waiting) waiting->IRQglobalIrqUnlockAndWait(dLock);
    }

    SPI2->CR1=0;
    
    //Quirk, disabling the SPI in RXONLY mode is difficult
    while(SPI2->SR & SPI_SR_RXNE) temp=SPI2->DR;
    delayUs(1); //The last transfer may still be in progress
    while(SPI2->SR & SPI_SR_RXNE) temp=SPI2->DR;
    
    SPI2->CR2=0;
    SPI2->CR1=SPI_CR1_SSM
            | SPI_CR1_SSI
            | SPI_CR1_MSTR
            | SPI_CR1_SPE;

    flash_cs::high();
    return !error;
}

Flash::Flash()
{
    {
        GlobalIrqLock dLock;
        flash_mosi::mode(Mode::ALTERNATE);
        flash_mosi::alternateFunction(5);
        flash_miso::mode(Mode::ALTERNATE);
        flash_miso::alternateFunction(5);
        flash_sck::mode(Mode::ALTERNATE);
        flash_sck::alternateFunction(5);
        flash_cs::mode(Mode::OUTPUT);
        flash_cs::high();
        RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
        RCC->APB1ENR |= RCC_APB1ENR_SPI2EN;
        IRQregisterIrq(dLock,DMA1_Stream3_IRQn,&Flash::SPI2rxDmaHandler,this);
        IRQregisterIrq(dLock,DMA1_Stream4_IRQn,&Flash::SPI2txDmaHandler,this);
        SPI2->CR2=0;
        SPI2->CR1=SPI_CR1_SSM  //Software cs
                | SPI_CR1_SSI  //Hardware cs internally tied high
                | SPI_CR1_MSTR //Master mode
                | SPI_CR1_SPE; //SPI enabled
    }
    iprintf("FLASH\nstatus1=0x%x\nstatus2=0x%x\n",readStatus(1),readStatus(2));
}

void Flash::writeEnable()
{
    flash_cs::low();
    spi2sendRecv(0x06); //Write enable
    flash_cs::high();
    delayUs(1);
}

unsigned short Flash::readStatus(int reg)
{
    flash_cs::low();
    spi2sendRecv(reg==2 ? 0x35 : 0x05); // Read status register 2 / 1
    unsigned short result=spi2sendRecv();
    flash_cs::high();
    return result;
}

void Flash::SPI2rxDmaHandler()
{
    if(DMA1->LISR & (DMA_LISR_TEIF3 | DMA_LISR_DMEIF3)) error=true;
    DMA1->LIFCR=DMA_LIFCR_CTCIF3
              | DMA_LIFCR_CTEIF3
              | DMA_LIFCR_CDMEIF3;
    if(waiting) waiting->IRQwakeup();
    waiting=nullptr;
}

void Flash::SPI2txDmaHandler()
{
    if(DMA1->HISR & (DMA_HISR_TEIF4 | DMA_HISR_DMEIF4)) error=true;
    DMA1->HIFCR=DMA_HIFCR_CTCIF4
              | DMA_HIFCR_CTEIF4
              | DMA_HIFCR_CDMEIF4;
    if(waiting) waiting->IRQwakeup();
    waiting=nullptr;
}
