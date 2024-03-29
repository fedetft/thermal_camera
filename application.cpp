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
#include <string.h>

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
      sensor(make_unique<MLX90640>(i2c.get())), usb(make_unique<USBCDC>(Priority()))
{
    loadOptions(&ui.options,sizeof(ui.options));
    if(sensor->setRefresh(refreshFromInt(ui.options.frameRate))==false)
        puts("Error setting framerate");
    display.setBrightness(ui.options.brightness * 6);
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

    Thread *usbInteractiveThread = Thread::create(Application::usbThreadMainTramp, 2048U, Priority(), static_cast<void*>(this), Thread::JOINABLE);
    Thread *usbOutputThread = Thread::create(Application::usbFrameOutputThreadMainTramp, 2048U, Priority(), static_cast<void*>(this), Thread::JOINABLE);
    
    ui.lifecycle = UI::Ready;
    while (ui.lifecycle != UI::Quit) {
        //auto t1 = miosix::getTime();
        ui.update();
        //auto t2 = miosix::getTime();
        //iprintf("ui update = %lld\n",t2-t1);
        Thread::sleep(80);
    }

    usb->prepareShutdown();
    
    sensorThread->wakeup(); //Prevents deadlock if acquisition is paused
    sensorThread->join();
    iprintf("sensorThread joined\n");
    if(rawFrameQueue.isEmpty()) rawFrameQueue.put(nullptr); //Prevents deadlock
    processThread->join();
    iprintf("processThread joined\n");
    if(processedFrameQueue.isEmpty()) processedFrameQueue.put(nullptr); //Prevents deadlock
    renderThread->join();
    iprintf("renderThread joined\n");
    if(usbOutputQueue.isEmpty()) usbOutputQueue.put(nullptr);
    usbOutputThread->join();
    iprintf("usbOutputThread joined\n");
    usbInteractiveThread->join();
    iprintf("usbInteractiveThread joined\n");
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

bool Application::checkUSBConnected()
{
    return usb->connected();
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
        processedFrameQueue.put(processedFrame);
        usbOutputQueue.put(rawFrame);
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

void *Application::usbThreadMainTramp(void *p)
{
    static_cast<Application *>(p)->usbThreadMain();
    return nullptr;
}

char *hexDump(const uint8_t *bytes, int size, char *output)
{
    static const char *hex = "0123456789ABCDEF";
    for (int i=0; i<size; i++)
    {
        *output++ = hex[bytes[i] & 0xF];
        *output++ = hex[bytes[i] >> 4];
    }
    return output;
}

void Application::usbThreadMain()
{
    while (ui.lifecycle != UI::Quit) {
        char buf[80];
        bool success = usb->readLine(buf, 80);
        if (!success)
            continue;
        
        if (strcmp(buf, "get_eeprom") == 0) {
            const MLX90640EEPROM& eeprom = sensor->getEEPROM();
            const int hexSize = MLX90640EEPROM::eepromSize*4+2;
            char *hex = new char[hexSize];
            char *p = hexDump(reinterpret_cast<const uint8_t *>(eeprom.eeprom), MLX90640EEPROM::eepromSize*2, hex);
            *p++ = '\r'; *p++ = '\n';
            usb->write(reinterpret_cast<uint8_t *>(hex), hexSize, usbWriteTimeout);
            delete hex;
        } else if (strcmp(buf, "start_stream") == 0) {
            usbDumpRawFrames = true;
        } else if (strcmp(buf, "stop_stream") == 0) {
            usbDumpRawFrames = false;
        } else {
            usb->print("Unrecognized command\r\n", usbWriteTimeout);
        }
    }
    iprintf("usbInteractiveThread min free stack %d\n",
            MemoryProfiling::getAbsoluteFreeStack());
}

void *Application::usbFrameOutputThreadMainTramp(void *p)
{
    static_cast<Application *>(p)->usbFrameOutputThreadMain();
    return nullptr;
}

void Application::usbFrameOutputThreadMain()
{
    const int hexSize = (2+834*sizeof(uint16_t)*2+2)*2;
    char *hex = new char[hexSize];

    while(ui.lifecycle != UI::Quit)
    {
        std::unique_ptr<MLX90640RawFrame> rawFrame;
        {
            MLX90640RawFrame *pointer=nullptr;
            usbOutputQueue.get(pointer);
            rawFrame.reset(pointer);
        }
        if (!rawFrame) continue;
        if (!usb->connected())
        {
            usbDumpRawFrames = false;
        } else if (usbDumpRawFrames && !ui.paused) {
            char *p = hex;
            *p++ = '1'; *p++ = '=';
            p = hexDump(reinterpret_cast<const uint8_t *>(rawFrame->subframe[0]), 834*2, p);
            *p++ = '\r'; *p++ = '\n';
            *p++ = '2'; *p++ = '=';
            p = hexDump(reinterpret_cast<const uint8_t *>(rawFrame->subframe[1]), 834*2, p);
            *p++ = '\r'; *p++ = '\n';
            rawFrame.reset(nullptr);
            usb->write(reinterpret_cast<uint8_t *>(hex), hexSize, usbWriteTimeout);
        }
    }
    delete hex;

    iprintf("usbOutputThread min free stack %d\n",
            MemoryProfiling::getAbsoluteFreeStack());
}
