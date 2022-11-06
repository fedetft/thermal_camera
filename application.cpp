/***************************************************************************
 *   Copyright (C) 2022 by Terraneo Federico                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   As a special exception, if other files instantiate templates or use   *
 *   macros or inline functions from this file, or you compile this file   *
 *   and link it with other works to produce a work based on this file,    *
 *   this file does not by itself cause the resulting work to be covered   *
 *   by the GNU General Public License. However the source code for this   *
 *   file must still be made available in accordance with the GNU General  *
 *   Public License. This exception does not invalidate any other reasons  *
 *   why a work based on this file might be covered by the GNU General     *
 *   Public License.                                                       *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <http://www.gnu.org/licenses/>   *
 ***************************************************************************/

#include <application.h>
#include <thread>
#include <mxgui/misc_inst.h>
#include <drivers/misc.h>
#include <drivers/options_save.h>
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

//
// class Application
//

Application::Application(Display& display)
    : display(display),
      upButton(up_btn::getPin(),0), onButton(on_btn::getPin(),1),
      i2c(make_unique<I2C1Master>(sen_sda::getPin(),sen_scl::getPin(),400)),
      sensor(make_unique<MLX90640>(i2c.get())),
      renderer(make_unique<ThermalImageRenderer>())
{
    loadOptions(&options,sizeof(options));
    if(sensor->setRefresh(refreshFromInt(options.frameRate))==false)
        puts("Error setting framerate");
}

void Application::run()
{
    bootMessage();
    onButton.pressed(); upButton.pressed(); //Discard buttons already pressed at this point
    thread st(&Application::sensorThread,this);
    thread pt(&Application::processThread,this);

    //Drop first frame before starting the render thread
    MLX90640Frame *processedFrame=nullptr;
    processedFrameQueue.get(processedFrame);
    delete processedFrame;

    thread rt(&Application::renderThread,this);
    
    drawStaticPartOfMainScreen();
    while(quit==false)
    {
        switch(checkButtons())
        {
            case ButtonPressed::On:
                MemoryProfiling::print();
                quit=true;
                break;
            case ButtonPressed::Up:
                menuScreen();
                drawStaticPartOfMainScreen();
                break;
            default: break;
        }
    }
    
    st.join();
    if(rawFrameQueue.isEmpty()) rawFrameQueue.put(nullptr); //Prevents deadlock
    pt.join();
    if(processedFrameQueue.isEmpty()) processedFrameQueue.put(nullptr); //Prevents deadlock
    rt.join();
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

void Application::drawStaticPartOfMainScreen()
{
    DrawingContext dc(display);
    dc.clear(black);
    //For point coordinates see ui-mockup-main-screen.png
    dc.drawImage(Point(0,0),emissivityicon);
    char line[16];
    snprintf(line,sizeof(line),"%.2f  %2dfps ",options.emissivity,options.frameRate);
    dc.setFont(tahoma);
    dc.write(Point(11,0),line);
    const Color darkGrey=to565(128,128,128), lightGrey=to565(192,192,192);
    dc.line(Point(0,12),Point(0,107),darkGrey);
    dc.line(Point(1,12),Point(127,12),darkGrey);
    dc.line(Point(127,13),Point(127,107),lightGrey);
    dc.line(Point(1,107),Point(126,107),lightGrey);
    dc.drawImage(Point(18,115),smallcelsiusicon);
    dc.drawImage(Point(117,115),smallcelsiusicon);
    dc.drawImage(Point(72,109),largecelsiusicon);
}

void Application::drawStaticPartOfMenuScreen()
{
    DrawingContext dc(display);
    dc.clear(black);
    //For point coordinates see ui-mockup-menu-screen.png
    dc.setFont(tahoma);
    dc.write(Point(66,12),"Tmax");
    dc.write(Point(66,25),"Tmin");
    dc.drawImage(Point(114,13),smallcelsiusicon);
    dc.drawImage(Point(114,26),smallcelsiusicon);
    Color darkGrey=to565(128,128,128), lightGrey=to565(192,192,192);
    dc.line(Point(0,0),Point(0,48),darkGrey);
    dc.line(Point(1,0),Point(64,0),darkGrey);
    dc.line(Point(1,48),Point(64,48),lightGrey);
    dc.line(Point(64,1),Point(64,47),lightGrey);
}

void Application::menuScreen()
{
    small=true;
    drawStaticPartOfMenuScreen();
    enum MenuEntry
    {
        Emissivity         = 0,
        EmissivitySelected = 0 | 16,
        FrameRate          = 1,
        FrameRateSelected  = 1 | 16,
        SaveChanges        = 2,
        Back               = 3
    };
    const int numEntries=4;
    int entry=Emissivity;
    const char labels[numEntries][16]=
    {
        " Emissivity ",
        " Frame rate ",
        " Save changes ",
        " Back "
    };
    for(;;)
    {
        {
            DrawingContext dc(display);
            auto menuEntryColor=[&](bool active)
            {
                if(active) dc.setTextColor(make_pair(black,to565(255,128,0)));
                else dc.setTextColor(make_pair(white,black));
            };
            dc.setFont(tahoma);
            for(int i=0;i<numEntries;i++)
            {
                menuEntryColor(entry==i);
                dc.write(Point(0,50+i*tahoma.getHeight()),labels[i]);
            }
            menuEntryColor(entry==EmissivitySelected);
            dc.write(Point(75,50+0*tahoma.getHeight()),
                     string(" ")+to_string(options.emissivity).substr(0,4));
            menuEntryColor(entry==FrameRateSelected);
            dc.write(Point(75,50+1*tahoma.getHeight()),
                     string(" ")+to_string(options.frameRate)+"  ");
            dc.setTextColor(make_pair(white,black));
        }
        switch(checkButtons())
        {
            case ButtonPressed::On:
                switch(entry)
                {
                    case Emissivity: entry=EmissivitySelected; break;
                    case EmissivitySelected: entry=Emissivity; break;
                    case FrameRate: entry=FrameRateSelected; break;
                    case FrameRateSelected: entry=FrameRate; break;
                    case SaveChanges: saveOptions(&options,sizeof(options)); break;
                    case Back:
                        small=false;
                        return;
                }
                break;
            case ButtonPressed::Up:
                switch(entry)
                {
                    case EmissivitySelected:
                        if(options.emissivity>0.925) options.emissivity=0.05;
                        else options.emissivity+=0.05;
                        break;
                    case FrameRateSelected:
                        if(options.frameRate>=8) options.frameRate=1;
                        else options.frameRate*=2;
                        break;
                    case Back:
                        entry=Emissivity;
                        break;
                    default:
                        entry++;
                        break;
                }
                break;
            default: break;
        }
    }
}

void Application::drawBatteryIcon(mxgui::DrawingContext& dc)
{
    Point batteryIconPoint(104,0);
    prevBatteryVoltage=min(prevBatteryVoltage,getBatteryVoltage());
    switch(batteryLevel(prevBatteryVoltage))
    {
        case BatteryLevel::B100: dc.drawImage(batteryIconPoint,batt100icon); break;
        case BatteryLevel::B75:  dc.drawImage(batteryIconPoint,batt75icon); break;
        case BatteryLevel::B50:  dc.drawImage(batteryIconPoint,batt50icon); break;
        case BatteryLevel::B25:  dc.drawImage(batteryIconPoint,batt25icon); break;
        case BatteryLevel::B0:   dc.drawImage(batteryIconPoint,batt0icon); break;
    }
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

ButtonPressed Application::checkButtons()
{
    for(;;)
    {
        {
            DrawingContext dc(display);
            drawBatteryIcon(dc); //Here for convenience
        }
        Thread::sleep(80);
        bool on=onButton.pressed();
        bool up=upButton.pressed();
        if(on) return ButtonPressed::On;
        if(up) return ButtonPressed::Up;
    }
}

void Application::sensorThread()
{
    //High priority for sensor read, prevents I2C reads from starving
    Thread::getCurrentThread()->setPriority(MAIN_PRIORITY+1);
    auto previousRefreshRate=sensor->getRefresh();
    while(!quit)
    {
        auto *rawFrame=new MLX90640RawFrame;
        bool success;
        do {
            auto currentRefreshRate=refreshFromInt(options.frameRate);
            if(previousRefreshRate!=currentRefreshRate)
            {
                if(sensor->setRefresh(currentRefreshRate))
                    previousRefreshRate=currentRefreshRate;
                else puts("Error setting framerate");
            }
            success=sensor->readFrame(rawFrame);
            if(success==false) puts("Error reading frame");
        } while(success==false);
        {
            FastInterruptDisableLock dLock;
            success=rawFrameQueue.IRQput(rawFrame); //Nonblocking put
        }
        if(success==false)
        {
            puts("Dropped frame");
            delete rawFrame; //Drop frame without leaking memory
        }
    }
    iprintf("sensorThread min free stack %d\n",
            MemoryProfiling::getAbsoluteFreeStack());
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
        sensor->processFrame(rawFrame,processedFrame,options.emissivity);
        delete rawFrame;
        processedFrameQueue.put(processedFrame);
        auto t2=getTime();
        iprintf("process = %lld\n",t2-t1);
    }
    iprintf("processThread min free stack %d\n",
            MemoryProfiling::getAbsoluteFreeStack());
}

void Application::renderThread()
{
    while(!quit)
    {
        MLX90640Frame *processedFrame=nullptr;
        processedFrameQueue.get(processedFrame);
        if(processedFrame==nullptr) continue; //Happens on shutdown
        auto t1 = getTime();
        bool smallCached=small; //Cache now if the main thread changes it
        if(smallCached==false) renderer->render(processedFrame);
        else renderer->renderSmall(processedFrame);
        delete processedFrame;
        auto t2 = getTime();
        {
            DrawingContext dc(display);
            if(smallCached==false)
            {
                //For point coordinates see ui-mockup-main-screen.png
                renderer->draw(dc,Point(1,13));
                drawTemperature(dc,Point(0,114),Point(16,122),tahoma,
                                renderer->minTemperature());
                drawTemperature(dc,Point(99,114),Point(115,122),tahoma,
                                renderer->maxTemperature());
                drawTemperature(dc,Point(38,108),Point(70,122),droid21,
                                renderer->crosshairTemperature());
                Color *buffer=dc.getScanLineBuffer();
                renderer->legend(buffer,dc.getWidth());
                for(int y=124;y<=127;y++)
                    dc.scanLineBuffer(Point(0,y),dc.getWidth());
            } else {
                //For point coordinates see ui-mockup-menu-screen.png
                renderer->drawSmall(dc,Point(1,1));
                drawTemperature(dc,Point(96,12),Point(112,20),tahoma,
                                renderer->maxTemperature());
                drawTemperature(dc,Point(96,25),Point(112,33),tahoma,
                                renderer->minTemperature());
            }
        }
        auto t3 = getTime();
        iprintf("render = %lld draw = %lld\n",t2-t1,t3-t2);
        //process = 78ms render = 1.9ms draw = 15ms 8Hz scaled short DMA UI
    }
    iprintf("renderThread min free stack %d\n",
            MemoryProfiling::getAbsoluteFreeStack());
}
