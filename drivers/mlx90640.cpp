
#include "mlx90640.h"
#include <cstdio>
#include <thread>
#include <stdexcept>
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
    const unsigned int eepromSize=832;
    unsigned short eeprom[eepromSize]; // Heavy object! ~1.7 KByte
    if(read(0x2400,eepromSize,eeprom) || MLX90640_ExtractParameters(eeprom,&params))
        throw runtime_error("EEPROM failure");
    if(setRefresh(MLX90640Refresh::R1))
        throw runtime_error("I2C failure");
    setEmissivity(0.95f);
    lastFrameReady=chrono::system_clock::now();
}

int MLX90640::setRefresh(MLX90640Refresh rr)
{
    unsigned short cr1;
    if(read(0x800d,1,&cr1)) return -1;
    cr1 &= ~(0b111<<7);
    cr1 |= static_cast<unsigned short>(rr)<<7;
    if(write(0x800d,cr1)) return -1;
    this->rr=rr; //Write succeeded, commit refresh aret
    return 0;
}

int MLX90640::readFrame(MLX90640RawFrame& rawFrame)
{
    for(int i=0;i<2;i++) if(readSpecificSubFrame(i,rawFrame.subframe[i])) return -1;
    return 0;
}

void MLX90640::processFrame(const MLX90640RawFrame& rawFrame, MLX90640Frame& frame)
{
    const float taShift = 8.f; //Default shift for MLX90640 in open air
    for(int i=0;i<2;i++)
    {
        float vdd=MLX90640_GetVdd(rawFrame.subframe[i],&params);
        float Ta=MLX90640_GetTa(rawFrame.subframe[i],&params,vdd);
        float Tr=Ta-taShift; //Reflected temperature based on the sensor ambient temperature
        MLX90640_CalculateToShort(rawFrame.subframe[i],&params,emissivity,vdd,Ta,Tr,frame.temperature);
    }
}

int MLX90640::readSpecificSubFrame(int index, unsigned short rawFrame[834])
{
    const int maxRetry=3;
    for(int i=0;i<maxRetry;i++)
    {
        if(readSubFrame(rawFrame)) return -1;
        if(rawFrame[833]==index)
        {
            if(i>0) iprintf("readSpecificSubFrame tried %d times\n", i+1);
            return 0;
        }
    }
    return -1;
}

int MLX90640::readSubFrame(unsigned short rawFrame[834])
{
    this_thread::sleep_until(lastFrameReady+waitTime());
    unsigned short statusReg;
    do {
        if(read(0x8000,1,&statusReg)) return -1;
        this_thread::sleep_for(pollTime());
    } while((statusReg & (1<<3))==0);
    lastFrameReady=chrono::system_clock::now();
    const int maxRetry=3;
    for(int i=0;i<maxRetry;i++)
    {
        if(write(0x8000,statusReg & ~(1<<3))) return -1;
        if(read(0x0400,832,rawFrame)) return -1;
        if(read(0x8000,1,&statusReg)) return -1;
        if((statusReg & (1<<3))==0)
        {
            if(i>0) iprintf("readSubFrame tried %d times\n", i+1);
            break; //Frame read and no new frame flag set
        }
        if(i==maxRetry-1) return -1;
    }
    if(read(0x800D,1,&rawFrame[832])) return -1;
    rawFrame[833]=statusReg & (1<<0);
    return 0;
}

std::chrono::microseconds MLX90640::waitTime()
{
    //Compute half refresh time based on framerate
    int halfRefreshTime=2000000/(1<<static_cast<unsigned short>(rr));
    //Return 80% of that
    return chrono::microseconds(halfRefreshTime*8/10);
}

std::chrono::microseconds MLX90640::pollTime()
{
    //Compute half refresh time based on framerate
    int halfRefreshTime=2000000/(1<<static_cast<unsigned short>(rr));
    //We are going to wait for the 20% of that time, and split that in 6
    return chrono::microseconds(halfRefreshTime/5/6);
}

int MLX90640::read(unsigned int addr, unsigned int len, unsigned short *data)
{
    unsigned short tx=toBigEndian16(addr);
    if(i2c->sendRecv(devAddr,&tx,2,data,2*len)==false) return -1;
    for(unsigned int i=0;i<len;i++) data[i]=fromBigEndian16(data[i]);
    return 0;
}

int MLX90640::write(unsigned int addr, unsigned short data)
{
    unsigned short tx[2];
    tx[0]=toBigEndian16(addr);
    tx[1]=toBigEndian16(data);
    if(i2c->send(devAddr,&tx,4)==false) return -1;
    return 0;
}
