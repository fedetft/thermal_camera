/***************************************************************************
 *   Copyright (C) 2022 by Daniele Cattaneo and Federico Terraneo          *
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

#include "drivers/misc.h"
#include "renderer.h"
#include "images/batt100icon.h"
#include "images/batt75icon.h"
#include "images/batt50icon.h"
#include "images/batt25icon.h"
#include "images/batt0icon.h"
#include "images/miosixlogoicon.h"
#include "images/emissivityicon.h"
#include "images/smallcelsiusicon.h"
#include "images/largecelsiusicon.h"
#include <memory>
#include <mxgui/misc_inst.h>
#include <mxgui/display.h>

#ifndef _MIOSIX
#define sniprintf snprintf
#define iprintf printf
#endif

enum class ButtonPressed
{
    None,
    Up,
    On
};

struct ApplicationOptions
{
    int frameRate=8; //NOTE: to get beyond 8fps the I2C bus needs to be overclocked too!
    float emissivity=0.95f;
};

class IOHandlerBase
{
public:
    ButtonPressed checkButtons();

    void saveOptions(ApplicationOptions& options);
};

/**
 * The thermal camera application logic lives here, except all code which
 * interacts with the hardware
 */
template<class IOHandler>
class ApplicationUI
{
public:
    enum Lifecycle
    {
        Boot,
        Ready,
        Quit
    };
    ApplicationOptions options;
    Lifecycle lifecycle;

    ApplicationUI(IOHandler& ioHandler, mxgui::Display& display)
        : display(display), renderer(std::make_unique<ThermalImageRenderer>()),
        ioHandler(ioHandler)
    {
        enterBootMessage();
    }

    void update()
    {
        switch (state) {
            case BootMsg: updateBootMessage(); break;
            case Main: updateMain(); break;
            case Menu: updateMenu(); break;
            case Shutdown:
            default: break;
        }
    }

    void drawFrame(MLX90640Frame *processedFrame)
    {
        if(processedFrame==nullptr) return; //Happens on shutdown
        #ifdef _MIOSIX
        auto t1 = miosix::getTime();
        #endif
        bool smallCached=(state == Menu); //Cache now if the main thread changes it
        if(smallCached==false) renderer->render(processedFrame);
        else renderer->renderSmall(processedFrame);
        #ifdef _MIOSIX
        auto t2 = miosix::getTime();
        #endif
        {
            mxgui::DrawingContext dc(display);
            dc.setTextColor(std::make_pair(mxgui::white,mxgui::black));
            if(smallCached==false)
            {
                //For mxgui::point coordinates see ui-mockup-main-screen.png
                renderer->draw(dc,mxgui::Point(1,13));
                drawTemperature(dc,mxgui::Point(0,114),mxgui::Point(16,122),smallFont,
                                renderer->minTemperature());
                drawTemperature(dc,mxgui::Point(99,114),mxgui::Point(115,122),smallFont,
                                renderer->maxTemperature());
                drawTemperature(dc,mxgui::Point(38,108),mxgui::Point(70,122),largeFont,
                                renderer->crosshairTemperature());
                mxgui::Color *buffer=dc.getScanLineBuffer();
                renderer->legend(buffer,dc.getWidth());
                for(int y=124;y<=127;y++)
                    dc.scanLineBuffer(mxgui::Point(0,y),dc.getWidth());
            } else {
                //For mxgui::point coordinates see ui-mockup-menu-screen.png
                renderer->drawSmall(dc,mxgui::Point(1,1));
                drawTemperature(dc,mxgui::Point(96,12),mxgui::Point(112,20),smallFont,
                                renderer->maxTemperature());
                drawTemperature(dc,mxgui::Point(96,25),mxgui::Point(112,33),smallFont,
                                renderer->minTemperature());
            }
        }
        #ifdef _MIOSIX
        auto t3 = miosix::getTime();
        iprintf("render = %lld draw = %lld\n",t2-t1,t3-t2);
        #endif
        //process = 78ms render = 1.9ms draw = 15ms 8Hz scaled short DMA UI
    }

    void drawBatteryIcon(BatteryLevel level)
    {
        mxgui::DrawingContext dc(display);
        mxgui::Point batteryIconPoint(104,0);
        switch(level)
        {
            case BatteryLevel::B100: dc.drawImage(batteryIconPoint,batt100icon); break;
            case BatteryLevel::B75:  dc.drawImage(batteryIconPoint,batt75icon); break;
            case BatteryLevel::B50:  dc.drawImage(batteryIconPoint,batt50icon); break;
            case BatteryLevel::B25:  dc.drawImage(batteryIconPoint,batt25icon); break;
            case BatteryLevel::B0:   dc.drawImage(batteryIconPoint,batt0icon); break;
        }
    }

private:
    ApplicationUI(const ApplicationUI&)=delete;
    ApplicationUI& operator=(const ApplicationUI&)=delete;

    const mxgui::Font& smallFont = mxgui::tahoma;
    const mxgui::Font& largeFont = mxgui::droid21;

    enum State
    {
        BootMsg,
        Main,
        Menu,
        Shutdown
    };
    State state = State::BootMsg;
    mxgui::Display& display;
    std::unique_ptr<ThermalImageRenderer> renderer;
    IOHandler& ioHandler;

    void enterBootMessage()
    {
        const char s0[]="Miosix";
        const char s1[]="Thermal camera";
        const int s0pix=miosixlogoicon.getWidth()+1+largeFont.calculateLength(s0);
        const int s1pix=smallFont.calculateLength(s1);
        mxgui::DrawingContext dc(display);
        dc.setFont(largeFont);
        int width=dc.getWidth();
        int y=10;
        dc.drawImage(mxgui::Point((width-s0pix)/2,y),miosixlogoicon);
        dc.write(mxgui::Point((width-s0pix)/2+miosixlogoicon.getWidth()+1,y),s0);
        y+=dc.getFont().getHeight();
        dc.line(mxgui::Point((width-s1pix)/2,y),mxgui::Point((width-s1pix)/2+s1pix,y),mxgui::white);
        y+=4;
        dc.setFont(smallFont);
        dc.write(mxgui::Point((width-s1pix)/2,y),s1);
    }

    void updateBootMessage()
    {
        if (lifecycle == Ready) {
            enterMain();
        }
    }

    void enterMain()
    {
        state = Main;
        drawStaticPartOfMainScreen();
    }

    void drawStaticPartOfMainScreen()
    {
        mxgui::DrawingContext dc(display);
        dc.clear(mxgui::black);
        //For mxgui::point coordinates see ui-mockup-main-screen.png
        dc.drawImage(mxgui::Point(0,0),emissivityicon);
        char line[16];
        snprintf(line,sizeof(line),"%.2f  %2dfps ",options.emissivity,options.frameRate);
        dc.setFont(smallFont);
        dc.write(mxgui::Point(11,0),line);
        const mxgui::Color darkGrey=to565(128,128,128), lightGrey=to565(192,192,192);
        dc.line(mxgui::Point(0,12),mxgui::Point(0,107),darkGrey);
        dc.line(mxgui::Point(1,12),mxgui::Point(127,12),darkGrey);
        dc.line(mxgui::Point(127,13),mxgui::Point(127,107),lightGrey);
        dc.line(mxgui::Point(1,107),mxgui::Point(126,107),lightGrey);
        dc.drawImage(mxgui::Point(18,115),smallcelsiusicon);
        dc.drawImage(mxgui::Point(117,115),smallcelsiusicon);
        dc.drawImage(mxgui::Point(72,109),largecelsiusicon);
    }

    void updateMain()
    {
        switch(ioHandler.checkButtons())
        {
            case ButtonPressed::On:
                enterShutdown();
                break;
            case ButtonPressed::Up:
                enterMenu();
                break;
            default: break;
        }
    }

    void drawStaticPartOfMenuScreen(mxgui::DrawingContext& dc)
    {
        dc.clear(mxgui::black);
        //For mxgui::point coordinates see ui-mockup-menu-screen.png
        dc.setFont(smallFont);
        dc.write(mxgui::Point(66,12),"Tmax");
        dc.write(mxgui::Point(66,25),"Tmin");
        dc.drawImage(mxgui::Point(114,13),smallcelsiusicon);
        dc.drawImage(mxgui::Point(114,26),smallcelsiusicon);
        mxgui::Color darkGrey=to565(128,128,128), lightGrey=to565(192,192,192);
        dc.line(mxgui::Point(0,0),mxgui::Point(0,48),darkGrey);
        dc.line(mxgui::Point(1,0),mxgui::Point(64,0),darkGrey);
        dc.line(mxgui::Point(1,48),mxgui::Point(64,48),lightGrey);
        dc.line(mxgui::Point(64,1),mxgui::Point(64,47),lightGrey);
    }

    enum MenuEntry
    {
        Back = 0,
        Emissivity,
        FrameRate,
        SaveChanges,
        NumEntries
    };
    int menuEntry;

    void enterMenu()
    {
        state = Menu;
        menuEntry = Back;
        mxgui::DrawingContext dc(display);
        drawStaticPartOfMenuScreen(dc);
        for (int i=0; i<NumEntries; i++) drawMenuEntry(dc, i);
    }

    void _drawMenuEntry(mxgui::DrawingContext& dc, int i, const char *label, const char *value="")
    {
        const mxgui::Color selectedBGColor = mxgui::Color(to565(255,128,0));
        const mxgui::Color selectedFGColor = mxgui::black;
        const mxgui::Color unselectedBGColor = mxgui::black;
        const mxgui::Color unselectedFGColor = mxgui::white;
        const auto fontHeight = dc.getFont().getHeight();
        short top = 50+i*fontHeight;
        if (i==menuEntry) 
        {
            dc.clear(mxgui::Point(0,top),mxgui::Point(127,top+fontHeight),selectedBGColor);
            dc.setTextColor(std::make_pair(selectedFGColor,selectedBGColor));
        }
        else 
        {
            dc.clear(mxgui::Point(0,top),mxgui::Point(127,top+fontHeight),unselectedBGColor);
            dc.setTextColor(std::make_pair(unselectedFGColor,unselectedBGColor));
        }
        dc.write(mxgui::Point(3,top),label);
        dc.write(mxgui::Point(75,top),value);
    }

    void drawMenuEntry(mxgui::DrawingContext& dc, int id)
    {
        dc.setFont(smallFont);
        char buffer[8];
        switch (id) {
            case Back: _drawMenuEntry(dc, Back, "Back"); break;
            case Emissivity:
                snprintf(buffer, 8, "%.2f", options.emissivity);
                _drawMenuEntry(dc, Emissivity, "Emissivity", buffer);
                break;
            case FrameRate:
                sniprintf(buffer, 8, "%d", options.frameRate);
                _drawMenuEntry(dc, FrameRate, "Frame rate", buffer);
                break;
            case SaveChanges: _drawMenuEntry(dc, SaveChanges, "Save changes"); break;
        }
    }

    void updateMenu()
    {
        mxgui::DrawingContext dc(display);
        switch(ioHandler.checkButtons())
        {
            case ButtonPressed::On:
                switch(menuEntry)
                {
                    case Emissivity:
                        if(options.emissivity>0.925) options.emissivity=0.05;
                        else options.emissivity+=0.05;
                        drawMenuEntry(dc, Emissivity);
                        break;
                    case FrameRate: 
                        if(options.frameRate>=8) options.frameRate=1;
                        else options.frameRate*=2;
                        drawMenuEntry(dc, FrameRate);
                        break;
                    case SaveChanges:
                        ioHandler.saveOptions(options);
                        break;
                    case Back:
                        enterMain();
                        return;
                }
                break;
            case ButtonPressed::Up:
                {
                    int oldEntry = menuEntry;
                    menuEntry=(menuEntry+1)%NumEntries;
                    drawMenuEntry(dc, oldEntry);
                    drawMenuEntry(dc, menuEntry);
                    break;
                }
            default: break;
        }
    }

    void enterShutdown()
    {
        state = Shutdown;
        lifecycle = Quit;
        #ifdef _MIOSIX
        miosix::MemoryProfiling::print();
        #endif
    }

    void drawTemperature(mxgui::DrawingContext& dc, mxgui::Point a, mxgui::Point b,
                         mxgui::Font f, short temperature)
    {
        char line[8];
        sniprintf(line,sizeof(line),"%3d",temperature);
        int len=f.calculateLength(line);
        int toBlank=b.x()-a.x()-len;
        if(toBlank>0)
        {
            dc.clear(a,mxgui::Point(a.x()+toBlank,b.y()),mxgui::black);
            a=mxgui::Point(a.x()+toBlank+1,a.y());
        }
        dc.setFont(f);
        dc.clippedWrite(a,a,b,line);
    }

    static inline unsigned short to565(unsigned short r, unsigned short g, unsigned short b)
    {
        return ((r & 0b11111000) << 8) | ((g & 0b11111100) << 3) | ((b & 0b11111000) >> 3);
    }
};

