
#pragma once

#include <chrono>
#include <drivers/stm32f2_f4_i2c.h>
#include <drivers/MLX90640_API.h>

class MLX90640RawFrame
{
public:
    unsigned short subframe[2][834]; // Heavy object! ~3.4 KByte
};

class MLX90640Frame
{
public:
    static const int nx=32, ny=24;
    float temperature[nx*ny]; // Heavy object! 3 KByte
    
    float getTempAt(int x, int y) { return temperature[(nx-1-x)+y*nx]; }
};

enum class MLX90640Refresh : unsigned short
{
    R025 = 0b000, ///<  1 Half frame every 2 seconds, so 0.25 frames/sec
    R05  = 0b001, ///<  1 Half frame every second,    so 0.5 frames/sec
    R1   = 0b010, ///<  2 Half frames every second,   so  1 frames/sec
    R2   = 0b011, ///<  4 Half frames every second,   so  2 frames/sec
    R4   = 0b100, ///<  8 Half frames every second,   so  4 frames/sec
    R8   = 0b101, ///< 16 Half frames every second,   so  8 frames/sec
    R16  = 0b110, ///< 32 Half frames every second,   so 16 frames/sec
    R32  = 0b111  ///< 64 Half frames every second,   so 32 frames/sec
};

MLX90640Refresh refreshFromInt(int rate);

class MLX90640
{
public:
    
    MLX90640(miosix::I2C1Master *i2c, unsigned char devAddr=0x33);
    
    int setRefresh(MLX90640Refresh rr);
    
    MLX90640Refresh getRefresh() const { return rr; }
    
    void setEmissivity(float emissivity) { this->emissivity=emissivity; }
    
    float getEmissivity() const { return emissivity; }
    
    int readFrame(MLX90640RawFrame& rawFrame);
    
    void processFrame(const MLX90640RawFrame& rawFrame, MLX90640Frame& frame);
    
private:
    
    int readSpecificSubFrame(int index, unsigned short rawFrame[834]);
    
    int readSubFrame(unsigned short rawFrame[834]);
    
    std::chrono::microseconds waitTime();
    
    std::chrono::microseconds pollTime();
    
    int read(unsigned int addr, unsigned int len, unsigned short *data);
    int write(unsigned int addr, unsigned short data);
    
    miosix::I2C1Master *i2c;
    const unsigned char devAddr;
    MLX90640Refresh rr;
    float emissivity=0.95f;
    std::chrono::time_point<std::chrono::system_clock> lastFrameReady;
    paramsMLX90640 params; // Heavy object! ~11 KByte
};
