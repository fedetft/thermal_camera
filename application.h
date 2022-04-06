
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
    
    float minTemperature() const { return minTemp; }
    
    float maxTemperature() const { return maxTemp; }
    
private:
    static mxgui::Color interpolate2d(MLX90640Frame *processedFrame, int x, int y, float m, float r);
    
    static mxgui::Color pixMap(float t, float m, float r);
    
    mxgui::Color irImage[94][126];
    float minTemp, maxTemp;
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
    
    void sensorThread();
    
    void processThread();
    
    mxgui::Display& display;
    std::unique_ptr<miosix::I2C1Master> i2c;
    std::unique_ptr<MLX90640> sensor;
    miosix::Queue<MLX90640RawFrame*, 1> rawFrameQueue;
    miosix::Queue<MLX90640Frame*, 1> processedFrameQueue;
    bool quit=false;
};
