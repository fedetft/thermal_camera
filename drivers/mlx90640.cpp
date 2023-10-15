/***************************************************************************
 *   Copyright (C) 2022 by Terraneo Federico                               *
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

#include "mlx90640.h"
#include <cstdio>
#include <thread>
#include <stdexcept>
#include <chrono>
#include <interfaces/endianness.h>

using namespace std;
using namespace miosix;

MLX90640Refresh refreshFromInt(int rate)
{
    if(rate>=32) return MLX90640Refresh::R32;
    if(rate>=16) return MLX90640Refresh::R16;
    if(rate>=8) return MLX90640Refresh::R8;
    if(rate>=4) return MLX90640Refresh::R4;
    if(rate>=2) return MLX90640Refresh::R2;
    return MLX90640Refresh::R1;
    //NOTE: discard lower refresh rates
}

MLX90640::MLX90640(I2C1Master *i2c, unsigned char devAddr)
    : i2c(i2c), devAddr(devAddr<<1) //Make room for r/w bit
{
    // Wait 80ms as recommended by the datasheet.
    // If we don't do this, on some sensors the EEPROM readout might be glitched
    std::this_thread::sleep_for(80ms);
    if(read(0x2400,MLX90640EEPROM::eepromSize,eeprom.eeprom)==false || MLX90640_ExtractParameters(eeprom.eeprom,&params))
        throw runtime_error("EEPROM failure");
    if(setRefresh(MLX90640Refresh::R1)==false)
        throw runtime_error("I2C failure");
    lastFrameReady=chrono::system_clock::now();
}

const MLX90640EEPROM& MLX90640::getEEPROM()
{
    return eeprom;
}

bool MLX90640::setRefresh(MLX90640Refresh rr)
{
    unsigned short cr1;
    if(read(0x800d,1,&cr1)==false) return false;
    cr1 &= ~(0b111<<7);
    cr1 |= static_cast<unsigned short>(rr)<<7;
    if(write(0x800d,cr1)==false) return false;
    this->rr=rr; //Write succeeded, commit refresh rate
    return true;
}

bool MLX90640::readFrame(MLX90640RawFrame *rawFrame)
{
    for(int i=0;i<2;i++)
        if(readSpecificSubFrame(i,rawFrame->subframe[i])==false) return false;
    return true;
}

void MLX90640::processFrame(const MLX90640RawFrame *rawFrame, MLX90640Frame *frame, float emissivity)
{
    const float taShift=8.f; //Default shift for MLX90640 in open air
    for(int i=0;i<2;i++)
    {
        float vdd=MLX90640_GetVdd(rawFrame->subframe[i],&params);
        float Ta=MLX90640_GetTa(rawFrame->subframe[i],&params,vdd);
        float Tr=Ta-taShift; //Reflected temperature based on the sensor ambient temperature
        MLX90640_CalculateToShort(rawFrame->subframe[i],&params,emissivity,vdd,Ta,Tr,frame->temperature);
    }
}

bool MLX90640::readSpecificSubFrame(int index, unsigned short rawFrame[834])
{
    const int maxRetry=3;
    for(int i=0;i<maxRetry;i++)
    {
        if(readSubFrame(rawFrame)==false) return false;
        if(rawFrame[833]==index)
        {
            if(i>0) iprintf("readSpecificSubFrame tried %d times\n", i+1);
            return true;
        }
    }
    return false;
}

bool MLX90640::readSubFrame(unsigned short rawFrame[834])
{
    //Optimized sensor reading algorithm, follows the recommended measurement
    //flow from the datasheet, which consists in waiting 80% of the nominal
    //frame time and then start polling. To minimize polling overhead we add
    //explicit sleeps to enforse a poll period dependent on the framerate
    this_thread::sleep_until(lastFrameReady+waitTime());
    unsigned short statusReg;
    bool pollingError=false;
    for(;;)
    {
        if(read(0x8000,1,&statusReg)==false) { pollingError=true; break; }
        if(statusReg & (1<<3)) break;
        this_thread::sleep_for(pollTime());
    }
    // Notice we are setting lastFrameReady even if reading from the sensor
    // failed. This guarantees the next attempt will sleep anyway before
    // polling. If that sleep is not performed, and the reads continue to fail,
    // the read attempts will enter an infinite loop.
    lastFrameReady=chrono::system_clock::now();
    if(pollingError) return false;
    const int maxRetry=3;
    for(int i=0;i<maxRetry;i++)
    {
        if(write(0x8000,statusReg & ~(1<<3))==false) return false;
        if(read(0x0400,832,rawFrame)==false) return false;
        if(read(0x8000,1,&statusReg)==false) return false;
        if((statusReg & (1<<3))==0)
        {
            if(i>0) iprintf("readSubFrame tried %d times\n", i+1);
            break; //Frame read and no new frame flag set
        }
        if(i==maxRetry-1) return false;
    }
    if(read(0x800D,1,&rawFrame[832])==false) return false;
    rawFrame[833]=statusReg & (1<<0);
    return true;
}

std::chrono::microseconds MLX90640::waitTime()
{
    int halfRefreshTime=2000000/(1<<static_cast<unsigned short>(rr));
    return chrono::microseconds(halfRefreshTime*8/10); //Return 80% of that
}

std::chrono::microseconds MLX90640::pollTime()
{
    int halfRefreshTime=2000000/(1<<static_cast<unsigned short>(rr));
    //We are going to wait for the 20% of that time, and split that in 6
    return chrono::microseconds(halfRefreshTime/5/6);
}

bool MLX90640::read(unsigned int addr, unsigned int len, unsigned short *data)
{
    unsigned short tx=toBigEndian16(addr);
    if(i2c->sendRecv(devAddr,&tx,2,data,2*len)==false) return false;
    for(unsigned int i=0;i<len;i++) data[i]=fromBigEndian16(data[i]);
    return true;
}

bool MLX90640::write(unsigned int addr, unsigned short data)
{
    unsigned short tx[2];
    tx[0]=toBigEndian16(addr);
    tx[1]=toBigEndian16(data);
    return i2c->send(devAddr,&tx,4);
}
