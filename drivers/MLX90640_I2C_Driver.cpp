
// API reimplementation for Miosix

#include <cstdio>
#include <miosix.h>
#include <util/software_i2c.h>
#include "MLX90640_I2C_Driver.h"
#include "hwmapping.h"

using namespace std;
using namespace miosix;

using sda = sen_sda;
using scl = sen_scl;
using i2c = SoftwareI2C<sda, scl, 50, true>;

#define TRY_SEND(x) { if(i2c::send(x) == false) { i2c::sendStop(); return -1; } }

void MLX90640_I2CInit()
{
    i2c::init();
}

//Read a number of words from startAddress. Store into Data array.
//Returns 0 if successful, -1 if error
int MLX90640_I2CRead(uint8_t slaveAddr, unsigned int startAddress, unsigned int nWordsRead, uint16_t *data)
{
    if(nWordsRead == 0) return 0;

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
    return 0;
}

//Write two bytes to a two byte address
//Returns 0 if successful, -1 if I2C error, -2 if readback error
int MLX90640_I2CWrite(uint8_t slaveAddr, unsigned int writeAddress, uint16_t data)
{
    i2c::sendStart();
    TRY_SEND((slaveAddr << 1) | 0);
    TRY_SEND(writeAddress >> 8);
    TRY_SEND(writeAddress & 0xff);
    TRY_SEND(data >> 8);
    TRY_SEND(data & 0xff);
    i2c::sendStop();
    
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
