
#include <application.h>
#include <drivers/hwmapping.h>
#include <drivers/misc.h>
#include <colormap.h>
#include <thread>
#include <images/batt100.h>
#include <images/batt75.h>
#include <images/batt50.h>
#include <images/batt25.h>
#include <images/batt0.h>

using namespace std;
using namespace miosix;
using namespace mxgui;

/*
 * STM32F205RC
 * SPI1 speed (display) 15MHz
 * I2C1 speed (sensor)  400kHz
 */

//read = 785745170 process = 114363196 draw = 87720263 1Hz double
//read = 826804835 process =  75025464 draw = 87711063 1Hz float
//read =  64109598 process =  75087397 draw = 87643263 4Hz float
//read = 137824495 process =  75290863 render = 11667433 draw = 75779830 4Hz float
//read =  13095166 process =  75856930 render = 12045033 draw = 14686200 8Hz float
//read =  16454166 process =  76031631 render = 12072232 draw = 15198566 8Hz float DMA .14A
//process = 76482397 render = 12227133 draw = 13807066 8Hz float DMA
//NOTE: to get beyond 8fps the I2C bus needs to be overclocked too!

//
// class ThermalImageRenderer
//

void ThermalImageRenderer::render(MLX90640Frame *processedFrame)
{
    const int nx=MLX90640Frame::nx, ny=MLX90640Frame::ny;
    minTemp=processedFrame->temperature[0];
    maxTemp=processedFrame->temperature[0];
    for(int i=0;i<nx*ny;i++)
    {
        minTemp=min(minTemp,processedFrame->temperature[i]);
        maxTemp=max(maxTemp,processedFrame->temperature[i]);
    }
    float range=max(minRange,maxTemp-minTemp);
    for(int y=0;y<(2*ny)-1;y++)
    {
        for(int x=0;x<(2*nx)-1;x++)
        {
            Color c=interpolate2d(processedFrame,x,y,minTemp,range);
            irImage[2*y  ][2*x  ]=c; //Image layout in memory is reversed
            irImage[2*y+1][2*x  ]=c;
            irImage[2*y  ][2*x+1]=c;
            irImage[2*y+1][2*x+1]=c;
        }
    }
}

void ThermalImageRenderer::draw(DrawingContext& dc, Point p)
{
    Image img(94,126,irImage);
    dc.drawImage(p,img);
}

Color ThermalImageRenderer::interpolate2d(MLX90640Frame *processedFrame, int x, int y, float m, float r)
{
    if((x & 1)==0 && (y & 1)==0)
        return pixMap(processedFrame->getTempAt(x/2,y/2),m,r);

    if((x & 1)==0) //1d interp along y axis
    {
        float t=processedFrame->getTempAt(x/2,y/2)+processedFrame->getTempAt(x/2,(y/2)+1);
        return pixMap(t/2.f,m,r);
    }
    
    if((y & 1)==0) //1d interp along x axis
    {
        float t=processedFrame->getTempAt(x/2,y/2)+processedFrame->getTempAt((x/2)+1,y/2);
        return pixMap(t/2.f,m,r);
    }
    
    //2d interpolation
    float t=processedFrame->getTempAt(x/2,y/2)    +processedFrame->getTempAt((x/2)+1,y/2)
           +processedFrame->getTempAt(x/2,(y/2)+1)+processedFrame->getTempAt((x/2)+1,(y/2)+1);
    return pixMap(t/4.f,m,r);
}

Color ThermalImageRenderer::pixMap(float t, float m, float r)
{
    int pixel=255.5f*((t-m)/r);
    return colormap[max(0,min(255,pixel))];
}

//
// class Application
//

Application::Application(Display& display)
    : display(display),
      i2c(make_unique<I2C1Master>(sen_sda::getPin(),sen_scl::getPin(),400)),
      sensor(make_unique<MLX90640>(i2c.get()))
{
    sensor->setRefresh(MLX90640Refresh::R8);
}

void Application::run()
{
    thread st(&Application::sensorThread,this);
    thread pt(&Application::processThread,this);
    
    auto renderer=make_unique<ThermalImageRenderer>();
    for(;;)
    {
        MLX90640Frame *processedFrame=nullptr;
        processedFrameQueue.get(processedFrame);
        auto t1 = getTime();
        renderer->render(processedFrame);
        auto t2 = getTime();
        delete processedFrame;
        {
            DrawingContext dc(display);
            const int infoBarY=0, thermalImageY=12, tempDataY=108;
            
            Point batteryIconPoint(display.getWidth()-batt0.getWidth(),infoBarY);
            switch(batteryLevel(getBatteryVoltage()))
            {
                case BatteryLevel::B100: dc.drawImage(batteryIconPoint,batt100); break;
                case BatteryLevel::B75:  dc.drawImage(batteryIconPoint,batt75); break;
                case BatteryLevel::B50:  dc.drawImage(batteryIconPoint,batt50); break;
                case BatteryLevel::B25:  dc.drawImage(batteryIconPoint,batt25); break;
                case BatteryLevel::B0:   dc.drawImage(batteryIconPoint,batt0); break;
            }
            
            renderer->draw(dc,Point(1,thermalImageY));
            
            string caption=to_string(static_cast<int>(renderer->minTemperature()+.5f)) + " "
                          +to_string(static_cast<int>(renderer->maxTemperature()+.5f)) + "      ";
            dc.setFont(droid21);
            Point tempDataTop(0,tempDataY);
            Point tallTextTempDataBottom(127,tempDataY+15);
            dc.clippedWrite(tempDataTop,tempDataTop,tallTextTempDataBottom,caption.c_str());
            
        }
        auto t3 = getTime();
        
        iprintf("render = %lld draw = %lld\n",t2-t1,t3-t2);
        if(on_btn::value()==1) break;
    }
    
    quit=true;
    if(rawFrameQueue.isEmpty()) rawFrameQueue.put(nullptr); //Prevent deadlock
    st.join();
    pt.join();
}

void Application::sensorThread()
{
    //High priority for sensor read, prevents I2C reads from starving
    Thread::getCurrentThread()->setPriority(MAIN_PRIORITY+1);
    while(!quit)
    {
        auto *rawFrame=new MLX90640RawFrame;
        sensor->readFrame(*rawFrame);
        bool success;
        {
            FastInterruptDisableLock dLock;
            success=rawFrameQueue.IRQput(rawFrame); //Nonblocking put
        }
        if(success==false) delete rawFrame; //Drop frame without leaking memory
    }
}

void Application::processThread()
{
    //Low priority for processing, prevents display writes from starving
    Thread::getCurrentThread()->setPriority(MAIN_PRIORITY-1);
    while(!quit)
    {
        MLX90640RawFrame *rawFrame=nullptr;
        rawFrameQueue.get(rawFrame);
        if(rawFrame==nullptr) continue; //Happens on shutdown
        auto t1=getTime();
        auto *processedFrame=new MLX90640Frame;
        sensor->processFrame(*rawFrame,*processedFrame);
        delete rawFrame;
        processedFrameQueue.put(processedFrame);
        auto t2=getTime();
        iprintf("process = %lld\n",t2-t1);
    }
}
