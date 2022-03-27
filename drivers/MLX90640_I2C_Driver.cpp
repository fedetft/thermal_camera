
// API reimplementation for Miosix

#include <cstdio>
#include <miosix.h>
#include <interfaces/endianness.h>
#include <util/software_i2c.h>
#include <drivers/stm32f2_f4_i2c.h>
#include "MLX90640_I2C_Driver.h"
#include "hwmapping.h"

using namespace std;
using namespace miosix;

//#define I2C_SOFTWARE

#ifdef I2C_SOFTWARE
using i2c = SoftwareI2C<sen_sda, sen_scl, 50, true>;
#define TRY_SEND(x) { if(i2c::send(x) == false) { i2c::sendStop(); return -1; } }
#else
I2C1Master *i2c=nullptr;
#endif

void MLX90640_I2CInit()
{
    #ifdef I2C_SOFTWARE
    i2c::init();
    #else
    i2c=new I2C1Master(sen_sda::getPin(), sen_scl::getPin(), 400);
    #endif
}

//Read a number of words from startAddress. Store into Data array.
//Returns 0 if successful, -1 if error
int MLX90640_I2CRead(uint8_t slaveAddr, unsigned int startAddress, unsigned int nWordsRead, uint16_t *data)
{
    if(nWordsRead == 0) return 0;
    #ifdef I2C_SOFTWARE
    i2c::sendStart();
    TRY_SEND((slaveAddr << 1) | 0);
    TRY_SEND(startAddress >> 8);
    TRY_SEND(startAddress & 0xff);
    i2c::sendRepeatedStart();
    TRY_SEND((slaveAddr << 1) | 1);
    for(unsigned int i = 0; i < nWordsRead - 1; i++)
    {
        unsigned short result = i2c::recvWithAck();
        result <<= 8;
        result |= i2c::recvWithAck();
        *data++ = result;
    }
    unsigned short result = i2c::recvWithAck();
    result <<= 8;
    result |= i2c::recvWithNack(); //Last byte NAK
    *data++ = result;
    i2c::sendStop();
    #else
    unsigned char buffer[2];
    buffer[0] = startAddress >> 8;
    buffer[1] = startAddress & 0xff;
    if(i2c->send(slaveAddr << 1, &buffer, 2, false)  == false) return -1;
    if(i2c->recv(slaveAddr << 1, data, 2*nWordsRead) == false) return -1;
    for(unsigned int i=0;i<nWordsRead;i++) data[i]=fromBigEndian16(data[i]);
    #endif
    return 0;
}

//Write two bytes to a two byte address
//Returns 0 if successful, -1 if I2C error, -2 if readback error
int MLX90640_I2CWrite(uint8_t slaveAddr, unsigned int writeAddress, uint16_t data)
{
    #ifdef I2C_SOFTWARE
    i2c::sendStart();
    TRY_SEND((slaveAddr << 1) | 0);
    TRY_SEND(writeAddress >> 8);
    TRY_SEND(writeAddress & 0xff);
    TRY_SEND(data >> 8);
    TRY_SEND(data & 0xff);
    i2c::sendStop();
    #else
    unsigned char buffer[4];
    buffer[0] = writeAddress >> 8;
    buffer[1] = writeAddress & 0xff;
    buffer[2] = data >> 8;
    buffer[3] = data & 0xff;
    if(i2c->send(slaveAddr << 1, &buffer, 4) == false) return -1;
    #endif
    uint16_t dataCheck;
    MLX90640_I2CRead(slaveAddr, writeAddress, 1, &dataCheck);
    if(dataCheck != data)
    {
        iprintf("reg[0x%x] = 0x%x ", writeAddress, data);
        iprintf("Error readback is 0x%x\n", dataCheck);
        return -2;
    }
    return 0;
}

//Set I2C Freq, in kHz
//MLX90640_I2CFreqSet(1000) sets frequency to 1MHz
void MLX90640_I2CFreqSet(int freq)
{
    
}
