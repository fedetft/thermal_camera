
#include <application.h>
#include <drivers/hwmapping.h>
#include <drivers/misc.h>
#include <colormap.h>
#include <thread>
#include <images/batt100icon.h>
#include <images/batt75icon.h>
#include <images/batt50icon.h>
#include <images/batt25icon.h>
#include <images/batt0icon.h>
#include <images/miosixlogoicon.h>
#include <images/emissivityicon.h>
#include <images/smallcelsiusicon.h>
#include <images/largecelsiusicon.h>

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
//process = 75880497 render = 12312733 draw = 16298632 8Hz float DMA UI
//process = 77364697 render =  2053867 draw = 16339532 8Hz short DMA UI
//process = 77418064 render =  2051966 draw = 16335033 8Hz scaled short DMA UI

//
// class ThermalImageRenderer
//

void ThermalImageRenderer::render(MLX90640Frame *processedFrame)
{
    const int nx=MLX90640Frame::nx, ny=MLX90640Frame::ny;
    minTemp=processedFrame->temperature[0];
    maxTemp=processedFrame->temperature[0];
    crosshairTemp=processedFrame->getTempAt(nx/2,ny/2);
    for(int i=0;i<nx*ny;i++)
    {
        minTemp=min(minTemp,processedFrame->temperature[i]);
        maxTemp=max(maxTemp,processedFrame->temperature[i]);
    }
    short range=max<short>(minRange*processedFrame->scaleFactor,maxTemp-minTemp);
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
    //Draw crosshair
    static const unsigned char xrange[]={58,59,60,65,66,67};
    for(unsigned int xdex=0;xdex<sizeof(xrange);xdex++)
        for(int y=46;y<=47;y++)
            crosshairPixel(xrange[xdex],y);
    static const unsigned char yrange[]={42,43,44,49,50,51};
    for(int x=62;x<=63;x++)
        for(unsigned int ydex=0;ydex<sizeof(yrange);ydex++)
            crosshairPixel(x,yrange[ydex]);
    //Scale temperatures to express them in °C
    minTemp=roundedDiv(minTemp,processedFrame->scaleFactor);
    maxTemp=roundedDiv(maxTemp,processedFrame->scaleFactor);
    crosshairTemp=roundedDiv(crosshairTemp,processedFrame->scaleFactor);
}

void ThermalImageRenderer::draw(DrawingContext& dc, Point p)
{
    Image img(94,126,irImage);
    dc.drawImage(p,img);
}

void ThermalImageRenderer::legend(mxgui::Color *legend, int legendSize)
{
    int colormapRange=max(0,min<int>(maxTemp-minTemp,minRange))*255/minRange;
    for(int i=0;i<legendSize;i++) legend[i]=colormap[colormapRange*i/(legendSize-1)];
}

Color ThermalImageRenderer::interpolate2d(MLX90640Frame *processedFrame, int x, int y, short m, short r)
{
    if((x & 1)==0 && (y & 1)==0)
        return pixMap(processedFrame->getTempAt(x/2,y/2),m,r);

    if((x & 1)==0) //1d interp along y axis
    {
        short t=processedFrame->getTempAt(x/2,y/2)+processedFrame->getTempAt(x/2,(y/2)+1);
        return pixMap(roundedDiv(t,2),m,r);
    }
    
    if((y & 1)==0) //1d interp along x axis
    {
        short t=processedFrame->getTempAt(x/2,y/2)+processedFrame->getTempAt((x/2)+1,y/2);
        return pixMap(roundedDiv(t,2),m,r);
    }
    
    //2d interpolation
    short t=processedFrame->getTempAt(x/2,y/2)    +processedFrame->getTempAt((x/2)+1,y/2)
           +processedFrame->getTempAt(x/2,(y/2)+1)+processedFrame->getTempAt((x/2)+1,(y/2)+1);
    return pixMap(roundedDiv(t,4),m,r);
}

Color ThermalImageRenderer::pixMap(short t, short m, short r)
{
    int pixel=(255*(t-m))/r;
    return colormap[max(0,min(255,pixel))];
}

void ThermalImageRenderer::crosshairPixel(int x, int y)
{
    irImage[y][x]=colorBrightness(irImage[y][x])>16 ? black : white;
}

int ThermalImageRenderer::colorBrightness(mxgui::Color c)
{
    return ((c & 31) + (c>>6) + (c>>11))/3;
}

//
// class Application
//

Application::Application(Display& display)
    : display(display),
      i2c(make_unique<I2C1Master>(sen_sda::getPin(),sen_scl::getPin(),400)),
      sensor(make_unique<MLX90640>(i2c.get()))
{
    refresh=8; //NOTE: to get beyond 8fps the I2C bus needs to be overclocked too!
    sensor->setRefresh(refreshFromInt(refresh));
    emissivity=sensor->getEmissivity();
}

void Application::run()
{
    bootMessage();
    waitPowerButtonReleased();
    
    thread st(&Application::sensorThread,this);
    thread pt(&Application::processThread,this);
    
    auto renderer=make_unique<ThermalImageRenderer>();
    MLX90640Frame *processedFrame=nullptr;
    processedFrameQueue.get(processedFrame);
    delete processedFrame; //Drop first frame
    processedFrameQueue.get(processedFrame);
    drawStaticPartOfMainFrame();
    
    for(;;)
    {
        auto t1 = getTime();
        renderer->render(processedFrame);
        auto t2 = getTime();
        delete processedFrame;
        {
            DrawingContext dc(display);
            //For point coordinates see ui-mockup-mainframe.png
            char line[16];
            snprintf(line,sizeof(line),"%.2f  %2dfps ",emissivity,refresh);
            dc.setFont(tahoma);
            dc.write(Point(11,0),line);
            Point batteryIconPoint(104,0);
            switch(batteryLevel(getBatteryVoltage()))
            {
                case BatteryLevel::B100: dc.drawImage(batteryIconPoint,batt100icon); break;
                case BatteryLevel::B75:  dc.drawImage(batteryIconPoint,batt75icon); break;
                case BatteryLevel::B50:  dc.drawImage(batteryIconPoint,batt50icon); break;
                case BatteryLevel::B25:  dc.drawImage(batteryIconPoint,batt25icon); break;
                case BatteryLevel::B0:   dc.drawImage(batteryIconPoint,batt0icon); break;
            }
            renderer->draw(dc,Point(1,13));
            drawTemperature(dc,Point(0,114),Point(16,122),tahoma,renderer->minTemperature());
            drawTemperature(dc,Point(99,114),Point(115,122),tahoma,renderer->maxTemperature());
            drawTemperature(dc,Point(38,108),Point(70,122),droid21,renderer->crosshairTemperature());
            Color *buffer=dc.getScanLineBuffer();
            renderer->legend(buffer,dc.getWidth());
            for(int y=124;y<=127;y++) dc.scanLineBuffer(Point(0,y),dc.getWidth());
        }
        auto t3 = getTime();
        
        iprintf("render = %lld draw = %lld\n",t2-t1,t3-t2);
        if(on_btn::value()==1) break;
        processedFrameQueue.get(processedFrame);
    }
    
    quit=true;
    if(rawFrameQueue.isEmpty()) rawFrameQueue.put(nullptr); //Prevent deadlock
    st.join();
    pt.join();
}

void Application::bootMessage()
{
    const char s0[]="Miosix";
    const char s1[]="Thermal camera";
    const int s0pix=miosixlogoicon.getWidth()+1+droid21.calculateLength(s0);
    const int s1pix=tahoma.calculateLength(s1);
    DrawingContext dc(display);
    dc.setFont(droid21);
    int width=dc.getWidth();
    int y=10;
    dc.drawImage(Point((width-s0pix)/2,y),miosixlogoicon);
    dc.write(Point((width-s0pix)/2+miosixlogoicon.getWidth()+1,y),s0);
    y+=dc.getFont().getHeight();
    dc.line(Point((width-s1pix)/2,y),Point((width-s1pix)/2+s1pix,y),white);
    y+=4;
    dc.setFont(tahoma);
    dc.write(Point((width-s1pix)/2,y),s1);
}

void Application::drawStaticPartOfMainFrame()
{
    DrawingContext dc(display);
    dc.clear(black);
    //For point coordinates see ui-mockup-mainframe.png
    dc.drawImage(Point(0,0),emissivityicon);
    Color darkGrey=to565(128,128,128), lightGrey=to565(192,192,192);
    dc.line(Point(0,12),Point(0,107),darkGrey);
    dc.line(Point(1,12),Point(127,12),darkGrey);
    dc.line(Point(127,13),Point(127,107),lightGrey);
    dc.line(Point(1,107),Point(126,107),lightGrey);
    dc.drawImage(Point(18,115),smallcelsiusicon);
    dc.drawImage(Point(117,115),smallcelsiusicon);
    dc.drawImage(Point(72,109),largecelsiusicon);
}

void Application::drawTemperature(DrawingContext& dc, Point a, Point b,
                             Font f, short temperature)
{
    char line[8];
    sniprintf(line,sizeof(line),"%3d",temperature);
    int len=f.calculateLength(line);
    int toBlank=b.x()-a.x()-len;
    if(toBlank>0)
    {
        dc.clear(a,Point(a.x()+toBlank,b.y()),black);
        a=Point(a.x()+toBlank+1,a.y());
    }
    dc.setFont(f);
    dc.clippedWrite(a,a,b,line);
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
