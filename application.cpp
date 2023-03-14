/***************************************************************************
 *   Copyright (C) 2022 by Terraneo Federico and Daniele Cattaneo          *
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
    : display(display), ui(*this, display, ButtonState(1^up_btn::value(),on_btn::value())),
      i2c(make_unique<I2C1Master>(sen_sda::getPin(),sen_scl::getPin(),1000)),
      sensor(make_unique<MLX90640>(i2c.get()))
{
    loadOptions(&ui.options,sizeof(ui.options));
    if(sensor->setRefresh(refreshFromInt(ui.options.frameRate))==false)
        puts("Error setting framerate");
}

void Application::run()
{
    //High priority for sensor read, prevents I2C reads from starving
    sensorThread = Thread::create(Application::sensorThreadMainTramp, 2048U, Priority(MAIN_PRIORITY+1), static_cast<void*>(this), Thread::JOINABLE);
    //Low priority for processing, prevents display writes from starving
    Thread *processThread = Thread::create(Application::processThreadMainTramp, 2048U, Priority(MAIN_PRIORITY-1), static_cast<void*>(this), Thread::JOINABLE);

    //Drop first frame before starting the render thread
    MLX90640Frame *processedFrame=nullptr;
    processedFrameQueue.get(processedFrame);
    delete processedFrame;

    Thread *renderThread = Thread::create(Application::renderThreadMainTramp, 2048U, Priority(), static_cast<void*>(this), Thread::JOINABLE);
    
    ui.lifecycle = UI::Ready;
    while (ui.lifecycle != UI::Quit) {
        //auto t1 = miosix::getTime();
        ui.update();
        //auto t2 = miosix::getTime();
        //iprintf("ui update = %lld\n",t2-t1);
        Thread::sleep(80);
    }
    
    sensorThread->wakeup(); //Prevents deadlock if acquisition is paused
    sensorThread->join();
    if(rawFrameQueue.isEmpty()) rawFrameQueue.put(nullptr); //Prevents deadlock
    processThread->join();
    if(processedFrameQueue.isEmpty()) processedFrameQueue.put(nullptr); //Prevents deadlock
    renderThread->join();
}

ButtonState Application::checkButtons()
{
    // up button is inverted
    return ButtonState(1^up_btn::value(),on_btn::value());
}

BatteryLevel Application::checkBatteryLevel()
{
    prevBatteryVoltage=min(prevBatteryVoltage,getBatteryVoltage());
    return batteryLevel(prevBatteryVoltage);
}

void Application::setPause(bool pause)
{
    sensorThread->wakeup();
}

void Application::saveOptions(ApplicationOptions& options)
{
    ::saveOptions(&options,sizeof(options));
}

void *Application::sensorThreadMainTramp(void *p)
{
    static_cast<Application *>(p)->sensorThreadMain();
    return nullptr;
}

void Application::sensorThreadMain()
{
    auto previousRefreshRate=sensor->getRefresh();
    while(ui.lifecycle!=UI::Quit)
    {
        auto *rawFrame=new MLX90640RawFrame;
        bool success;
        do {
            auto currentRefreshRate=refreshFromInt(ui.options.frameRate);
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
        while (ui.paused && ui.lifecycle!=UI::Quit) Thread::wait();
    }
    iprintf("sensorThread min free stack %d\n",
            MemoryProfiling::getAbsoluteFreeStack());
}

void *Application::processThreadMainTramp(void *p)
{
    static_cast<Application *>(p)->processThreadMain();
    return nullptr;
}

void Application::processThreadMain()
{
    while(ui.lifecycle != UI::Quit)
    {
        MLX90640RawFrame *rawFrame=nullptr;
        rawFrameQueue.get(rawFrame);
        if(rawFrame==nullptr) continue; //Happens on shutdown
        //auto t1=getTime();
        auto *processedFrame=new MLX90640Frame;
        sensor->processFrame(rawFrame,processedFrame,ui.options.emissivity);
        delete rawFrame;
        processedFrameQueue.put(processedFrame);
        //auto t2=getTime();
        //iprintf("process = %lld\n",t2-t1);
    }
    iprintf("processThread min free stack %d\n",
            MemoryProfiling::getAbsoluteFreeStack());
}

void *Application::renderThreadMainTramp(void *p)
{
    static_cast<Application *>(p)->renderThreadMain();
    return nullptr;
}

void Application::renderThreadMain()
{
    while(ui.lifecycle != UI::Quit)
    {
        MLX90640Frame *processedFrame=nullptr;
        processedFrameQueue.get(processedFrame);
        ui.updateFrame(processedFrame);
    }
    iprintf("renderThread min free stack %d\n",
            MemoryProfiling::getAbsoluteFreeStack());
}
