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

#pragma once

#include <memory>
#include <miosix.h>
#include <mxgui/display.h>
#include <drivers/stm32f2_f4_i2c.h>
#include <drivers/mlx90640.h>
#include <drivers/hwmapping.h>
#include <renderer.h>
#include <edge_detector.h>

enum class ButtonPressed
{
    None,
    Up,
    On
};

/**
 * The thermal camera application logic lives here
 */
class Application
{
public:
    
    Application(mxgui::Display& display);

    void run();
    
private:
    Application(const Application&)=delete;
    Application& operator=(const Application&)=delete;
    
    void bootMessage();
    
    void drawStaticPartOfMainScreen();

    void drawStaticPartOfMenuScreen();

    void menuScreen();

    void drawBatteryIcon(mxgui::DrawingContext& dc);

    void drawTemperature(mxgui::DrawingContext& dc, mxgui::Point a, mxgui::Point b,
                         mxgui::Font f, short temperature);

    ButtonPressed checkButtons();
    
    void sensorThread();
    
    void processThread();

    void renderThread();
    
    static inline unsigned short to565(unsigned short r, unsigned short g, unsigned short b)
    {
        return ((r & 0b11111000) << 8) | ((g & 0b11111100) << 3) | ((b & 0b11111000) >> 3);
    }

    mxgui::Display& display;
    ButtonEdgeDetector upButton;
    ButtonEdgeDetector onButton;
    int prevBatteryVoltage=42; //4.2V
    std::unique_ptr<miosix::I2C1Master> i2c;
    std::unique_ptr<MLX90640> sensor;
    std::unique_ptr<ThermalImageRenderer> renderer;
    struct Options
    {
        int frameRate=8; //NOTE: to get beyond 8fps the I2C bus needs to be overclocked too!
        float emissivity=0.95f;
    };
    Options options;
    miosix::Queue<MLX90640RawFrame*, 1> rawFrameQueue;
    miosix::Queue<MLX90640Frame*, 1> processedFrameQueue;
    bool small=false;
    bool quit=false;
};
