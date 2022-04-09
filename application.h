
#pragma once

#include <memory>
#include <miosix.h>
#include <mxgui/display.h>
#include <mxgui/misc_inst.h>
#include <drivers/stm32f2_f4_i2c.h>
#include <drivers/mlx90640.h>

class ThermalImageRenderer
{
public:
    
    void render(MLX90640Frame *processedFrame);
    
    void draw(mxgui::DrawingContext& dc, mxgui::Point p);
    
    void legend(mxgui::Color *legend, int legendSize);
    
    float minTemperature() const { return minTemp; }
    
    float maxTemperature() const { return maxTemp; }
    
    float crosshairTemperature() const { return crosshairTemp; }
    
private:
    static mxgui::Color interpolate2d(MLX90640Frame *processedFrame, int x, int y, float m, float r);
    
    static mxgui::Color pixMap(float t, float m, float r);
    
    void crosshairPixel(int x, int y);
    
    static int colorBrightness(mxgui::Color c);
    
    mxgui::Color irImage[94][126];
    float minTemp, maxTemp, crosshairTemp;
    const float minRange=15.f;
};

class Application
{
public:
    
    Application(mxgui::Display& display);

    void run();
    
private:
    Application(const Application&)=delete;
    Application& operator=(const Application&)=delete;
    
    void bootMessage();
    
    void drawStaticPartOfMainFrame();
    
    void drawTemperature(mxgui::DrawingContext& dc, mxgui::Point a, mxgui::Point b,
                         mxgui::Font f, float temperature);
    
    void sensorThread();
    
    void processThread();
    
    static inline unsigned short to565(unsigned short r, unsigned short g, unsigned short b)
    {
        return ((r & 0b11111000) << 8) | ((g & 0b11111100) << 3) | ((b & 0b11111000) >> 3);
    }
    
    mxgui::Display& display;
    std::unique_ptr<miosix::I2C1Master> i2c;
    std::unique_ptr<MLX90640> sensor;
    int refresh;
    float emissivity;
    miosix::Queue<MLX90640RawFrame*, 1> rawFrameQueue;
    miosix::Queue<MLX90640Frame*, 1> processedFrameQueue;
    bool quit=false;
};
