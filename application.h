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

#pragma once

#include <memory>
#include <miosix.h>
#include <mxgui/display.h>
#include <drivers/stm32f2_f4_i2c.h>
#include <drivers/mlx90640.h>
#include <drivers/hwmapping.h>
#include <drivers/usb_tinyusb.h>
#include "renderer.h"
#include "applicationui.h"

/**
 * Main application class. Decorates ApplicationUI with hardware I/O code.
 */
class Application: IOHandlerBase
{
public:
    
    Application(mxgui::Display& display);

    void run();

    ButtonState checkButtons();

    BatteryLevel checkBatteryLevel();
    
    bool checkUSBConnected();

    void setPause(bool pause);

    void saveOptions(ApplicationOptions& options);
    
private:
    Application(const Application&)=delete;
    Application& operator=(const Application&)=delete;

    using UI = ApplicationUI<Application>;
    
    static void *sensorThreadMainTramp(void *p);
    inline void sensorThreadMain();
    
    static void *processThreadMainTramp(void *p);
    inline void processThreadMain();

    static void *renderThreadMainTramp(void *p);
    inline void renderThreadMain();

    static void *usbThreadMainTramp(void *p);
    inline void usbThreadMain();

    static void *usbFrameOutputThreadMainTramp(void *p);
    inline void usbFrameOutputThreadMain();

    miosix::Thread *sensorThread;
    mxgui::Display& display;
    UI ui;
    int prevBatteryVoltage=42; //4.2V
    std::unique_ptr<miosix::I2C1Master> i2c;
    std::unique_ptr<MLX90640> sensor;
    std::unique_ptr<USBCDC> usb;
    miosix::Queue<MLX90640RawFrame*, 1> rawFrameQueue;
    miosix::Queue<MLX90640Frame*, 1> processedFrameQueue;
    volatile bool usbDumpRawFrames=false;
    miosix::Queue<MLX90640RawFrame*, 1> usbOutputQueue;

    const unsigned long long usbWriteTimeout = 50ULL * 1000000ULL; // 50ms
};
