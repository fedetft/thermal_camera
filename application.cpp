
#include <application.h>
#include <drivers/hwmapping.h>
#include <colormap.h>
#include <thread>

using namespace std;
using namespace miosix;
using namespace mxgui;

/*
 * STM32F205RC
 * SPI1 speed (display) 15MHz
 * I2C1 speed (sensor)  400kHz
 */

Color pixMap(float t, float m, float r)
{
    int pixel = 255.5f * ((t - m) / r);
    return colormap[max(0, min(255, pixel))];
}

//
// class DisplayFrame
//

DisplayFrame::~DisplayFrame() {}

//
// class MainDisplayFrame
//

MainDisplayFrame::MainDisplayFrame(MLX90640Frame *processedFrame)
{
    int nx=MLX90640Frame::nx, ny=MLX90640Frame::ny;
    const float minRange=15.f;
    float minVal=processedFrame->temperature[0], maxVal=processedFrame->temperature[0];
    for(int i=0;i<nx*ny;i++)
    {
        minVal=min(minVal,processedFrame->temperature[i]);
        maxVal=max(maxVal,processedFrame->temperature[i]);
    }
    float range=max(minRange,maxVal-minVal);
    
    for(int y=0;y<(2*ny)-1;y++)
        for(int x=0;x<(2*nx)-1;x++)
            irImage[x][y]=interpolate2d(processedFrame,x,y,minVal,range);
    
    caption=to_string(static_cast<int>(minVal + .5f)) + " "
           +to_string(static_cast<int>(maxVal + .5f)) + "      ";
}

Color MainDisplayFrame::interpolate2d(MLX90640Frame *processedFrame, int x, int y, float m, float r)
{
    if((x & 1) == 0 && (y & 1) == 0)
        return pixMap(processedFrame->getTempAt(x / 2, y / 2), m, r);

    if((x & 1) == 0) //1d interp along y axis
    {
        float t = processedFrame->getTempAt(x / 2, y / 2) + processedFrame->getTempAt(x / 2, (y / 2) + 1);
        return pixMap(t / 2.f, m, r);
    }
    
    if((y & 1) == 0) //1d interp along x axis
    {
        float t = processedFrame->getTempAt(x / 2, y / 2) + processedFrame->getTempAt((x / 2) + 1, y / 2);
        return pixMap(t / 2.f, m, r);
    }
    
    //2d interpolation
    float t = processedFrame->getTempAt(x / 2, y / 2)       + processedFrame->getTempAt((x / 2) + 1, y / 2)
            + processedFrame->getTempAt(x / 2, (y / 2) + 1) + processedFrame->getTempAt((x / 2) + 1, (y / 2) + 1);
    return pixMap(t / 4.f, m, r);
}

void MainDisplayFrame::draw(Display& display)
{
    auto t1 = getTime();
    int nx=MLX90640Frame::nx, ny=MLX90640Frame::ny;
    const int pixSize=2; // image becomes 2*63 x 2*47 or 126 x 94 pixels
    DrawingContext dc(display);
    for(int y=0;y<(2*ny)-1;y++)
        for(int x=0;x<(2*nx)-1;x++)
            dc.clear(Point(x*pixSize,y*pixSize),
                     Point(((x+1)*pixSize)-1,((y+1)*pixSize)-1),
                     irImage[x][y]);
    dc.setFont(droid21);
    dc.write(Point(0,2*ny*pixSize),caption.c_str());
    auto t2 = getTime();
    iprintf("draw = %lld\n",t2-t1);
}


//
// class Application
//

Application::Application(Display& display)
    : display(display),
      i2c(make_unique<I2C1Master>(sen_sda::getPin(),sen_scl::getPin(),400)),
      sensor(make_unique<MLX90640>(i2c.get()))
{
    sensor->setRefresh(MLX90640Refresh::R4);
}

void Application::run()
{
    thread mt(&Application::measureThread,this);
    thread dt(&Application::displayThread,this);
    auto processedFrame=make_unique<MLX90640Frame>();
    for(;;)
    {
        auto t1 = getTime();
        
        MLX90640RawFrame *rawFrame=nullptr;
        rawFrameQueue.get(rawFrame);
        
        auto t2 = getTime();
        sensor->processFrame(*rawFrame,*processedFrame.get());
        delete rawFrame;
        
        auto t3 = getTime();
        
        displayQueue.put(new MainDisplayFrame(processedFrame.get()));
        
        auto t4 = getTime();
        
        //read = 785745170 process = 114363196 draw = 87720263 1Hz with double
        //read = 826804835 process =  75025464 draw = 87711063 1Hz with float
        //read =  64109598 process =  75087397 draw = 87643263 4Hz with float
        //read = 137824495 process =  75290863 render = 11667433 draw = 75779830 4Hz with float
        iprintf("read = %lld process = %lld render = %lld\n",t2-t1,t3-t2,t4-t3);
        
        if(on_btn::value()==1) break;
    }
    
    quit=true;
    if(displayQueue.isEmpty()) displayQueue.put(nullptr);
    mt.join();
    dt.join();
}

void Application::measureThread()
{
    Thread::getCurrentThread()->setPriority(2);
    while(!quit)
    {
        MLX90640RawFrame *rawFrame=new MLX90640RawFrame;
        sensor->readFrame(*rawFrame);
        bool success;
        {
            FastInterruptDisableLock dLock;
            success=rawFrameQueue.IRQput(rawFrame); //Nonblocking put
        }
        if(success==false) delete rawFrame;
    }
}

void Application::displayThread()
{
    while(!quit)
    {
        DisplayFrame *frame=nullptr;
        displayQueue.get(frame);
        if(frame==nullptr) continue;
        frame->draw(display);
        delete frame;
    }
}
