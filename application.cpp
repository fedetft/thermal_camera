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
// class Application
//

Application::Application(Display& display)
    : display(display),
      upButton(miosix::up_btn::getPin(),0), onButton(miosix::on_btn::getPin(),1),
      i2c(make_unique<I2C1Master>(sen_sda::getPin(),sen_scl::getPin(),400)),
      sensor(make_unique<MLX90640>(i2c.get())),
      renderer(make_unique<ThermalImageRenderer>())
{
    refresh=8; //NOTE: to get beyond 8fps the I2C bus needs to be overclocked too!
    if(sensor->setRefresh(refreshFromInt(refresh))==false)
        puts("Error setting framerate");
}

void Application::run()
{
    bootMessage();
    checkButtons();
    thread st(&Application::sensorThread,this);
    thread pt(&Application::processThread,this);

    //Drop first frame
    MLX90640Frame *processedFrame=nullptr;
    processedFrameQueue.get(processedFrame);
    delete processedFrame;
    
    while(quit==false)
    {
        switch(mainScreen())
        {
            case ButtonPressed::On: quit=true;    break;
            case ButtonPressed::Up: menuScreen(); break;
            default: break;
        }
    }
    
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

ButtonPressed Application::checkButtons()
{
    bool on=onButton.pressed();
    bool up=upButton.pressed();
    if(on && up) return ButtonPressed::Both;
    if(on) return ButtonPressed::On;
    if(up) return ButtonPressed::Up;
    return ButtonPressed::None;
}

ButtonPressed Application::mainScreen()
{
    drawStaticPartOfMainScreen();
    for(;;)
    {
        MLX90640Frame *processedFrame=nullptr;
        processedFrameQueue.get(processedFrame);
        auto t1 = getTime();
        renderer->render(processedFrame);
        delete processedFrame;
        auto t2 = getTime();
        {
            DrawingContext dc(display);
            //For point coordinates see ui-mockup-main-screen.png
            drawBatteryIcon(dc);
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
        auto btn=checkButtons();
        if(btn!=ButtonPressed::None) return btn;
    }
}

void Application::drawStaticPartOfMainScreen()
{
    DrawingContext dc(display);
    dc.clear(black);
    //For point coordinates see ui-mockup-main-screen.png
    dc.drawImage(Point(0,0),emissivityicon);
    char line[16];
    snprintf(line,sizeof(line),"%.2f  %2dfps ",emissivity,refresh);
    dc.setFont(tahoma);
    dc.write(Point(11,0),line);
    Color darkGrey=to565(128,128,128), lightGrey=to565(192,192,192);
    dc.line(Point(0,12),Point(0,107),darkGrey);
    dc.line(Point(1,12),Point(127,12),darkGrey);
    dc.line(Point(127,13),Point(127,107),lightGrey);
    dc.line(Point(1,107),Point(126,107),lightGrey);
    dc.drawImage(Point(18,115),smallcelsiusicon);
    dc.drawImage(Point(117,115),smallcelsiusicon);
    dc.drawImage(Point(72,109),largecelsiusicon);
}

void Application::drawBatteryIcon(mxgui::DrawingContext& dc)
{
    Point batteryIconPoint(104,0);
    switch(batteryLevel(getBatteryVoltage()))
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

void Application::menuScreen()
{
    //TODO
    drawStaticPartOfMenuScreen();
    for(;;)
    {
        MLX90640Frame *processedFrame=nullptr;
        processedFrameQueue.get(processedFrame);
        renderer->renderSmall(processedFrame);
        delete processedFrame;
        {
            DrawingContext dc(display);
            //For point coordinates see ui-mockup-menu-screen.png
            drawBatteryIcon(dc);
            renderer->drawSmall(dc,Point(1,1));
        }
        upButton.pressed();
        if(onButton.pressed()) break;
    }
}

void Application::drawStaticPartOfMenuScreen()
{
    DrawingContext dc(display);
    dc.clear(black);
    //For point coordinates see ui-mockup-menu-screen.png
    //TODO
}

void Application::sensorThread()
{
    //High priority for sensor read, prevents I2C reads from starving
    Thread::getCurrentThread()->setPriority(MAIN_PRIORITY+1);
    while(!quit)
    {
        auto *rawFrame=new MLX90640RawFrame;
        bool success;
        do {
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
        sensor->processFrame(rawFrame,processedFrame,emissivity);
        delete rawFrame;
        processedFrameQueue.put(processedFrame);
        auto t2=getTime();
        iprintf("process = %lld\n",t2-t1);
    }
}
