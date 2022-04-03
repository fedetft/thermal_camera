
#pragma once

#include <memory>
#include <string>
#include <miosix.h>
#include <mxgui/display.h>
#include <mxgui/misc_inst.h>
#include <drivers/stm32f2_f4_i2c.h>
#include <drivers/display_er_oledm015.h>
#include <drivers/mlx90640.h>

class DisplayFrame
{
public:
    virtual void draw(mxgui::Display& display)=0;
    virtual ~DisplayFrame();
};

class MainDisplayFrame : public DisplayFrame
{
public:
    MainDisplayFrame(MLX90640Frame *processedFrame);
    
    mxgui::Color interpolate2d(MLX90640Frame *processedFrame, int x, int y, float m, float r);
    
    void draw(mxgui::Display & display) override;
    
private:
    mxgui::Color irImage[126][94];
    std::string caption; //TODO
};

class Application
{
public:
    
    Application(mxgui::Display& display);

    void run();
    
private:
    void measureThread();
    
    void displayThread();
    
    mxgui::Display& display;
    std::unique_ptr<miosix::I2C1Master> i2c;
    std::unique_ptr<MLX90640> sensor;
    miosix::Queue<MLX90640RawFrame*, 2> rawFrameQueue;
    miosix::Queue<DisplayFrame*, 1> displayQueue;
    bool quit=false;
};
