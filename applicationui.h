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

#include "renderer.h"
#include "edge_detector.h"
#include "textbox.h"
#include "version.h"
#include "drivers/misc.h"
#include "images/batt100icon.h"
#include "images/batt75icon.h"
#include "images/batt50icon.h"
#include "images/batt25icon.h"
#include "images/batt0icon.h"
#include "images/miosixlogoicon.h"
#include "images/emissivityicon.h"
#include "images/smallcelsiusicon.h"
#include "images/largecelsiusicon.h"
#include "images/pauseicon.h"
#include <mxgui/misc_inst.h>
#include <mxgui/display.h>
#include <memory>
#include <mutex>

#ifndef _MIOSIX
#define sniprintf snprintf
#define iprintf printf
#endif

struct ButtonState
{
    bool up:1;
    bool on:1;
    ButtonState(int up, int on): up(up!=0), on(on!=0) {}
};

struct ApplicationOptions
{
    int frameRate=8; //NOTE: to get beyond 8fps the I2C bus needs to be overclocked too!
    float emissivity=0.95f;
};

class IOHandlerBase
{
public:
    ButtonState checkButtons();

    BatteryLevel checkBatteryLevel();

    void setPause(bool pause);

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

    ApplicationUI(IOHandler& ioHandler, mxgui::Display& display, ButtonState initialBtnState)
        : display(display), renderer(std::make_unique<ThermalImageRenderer>()),
        ioHandler(ioHandler), upBtn(initialBtnState.up), onBtn(initialBtnState.on)
    {
        enterBootMessage();
    }

    void update();

    void updateFrame(MLX90640Frame *processedFrame);

    enum Lifecycle
    {
        Boot,
        Ready,
        Quit
    };
    ApplicationOptions options;
    Lifecycle lifecycle;
    bool paused = false;

private:
    ApplicationUI(const ApplicationUI&)=delete;
    ApplicationUI& operator=(const ApplicationUI&)=delete;

    void enterBootMessage();

    void drawBatteryIcon();

    void updateBootMessage();

    void drawStaticPartOfMainScreen(mxgui::DrawingContext& dc);

    void drawPauseIndicator(mxgui::DrawingContext& dc);

    void enterMain();

    void updateMain();

    void drawStaticPartOfMenuScreen(mxgui::DrawingContext& dc);

    void enterMenu();

    void _drawMenuEntry(mxgui::DrawingContext& dc, int i, const char *label, const char *value=NULL);

    void drawMenuEntry(mxgui::DrawingContext& dc, int id);

    void updateMenu();

    void enterShutdown();

    void drawFrame(mxgui::DrawingContext& dc);

    void drawTemperature(mxgui::DrawingContext& dc, mxgui::Point a, mxgui::Point b,
                         mxgui::Font f, short temperature);

    static inline unsigned short to565(unsigned short r, unsigned short g, unsigned short b)
    {
        return ((r & 0b11111000) << 8) | ((g & 0b11111100) << 3) | ((b & 0b11111000) >> 3);
    }

    const mxgui::Font& smallFont = mxgui::tahoma;
    const mxgui::Font& largeFont = mxgui::droid21;

    mxgui::Display& display;
    std::unique_ptr<ThermalImageRenderer> renderer;
    std::unique_ptr<MLX90640Frame> lastFrame;
    std::recursive_mutex lastFrameMutex;
    IOHandler& ioHandler;
    ButtonEdgeDetector<true> upBtn;
    ButtonEdgeDetector<true> onBtn;
    enum State
    {
        BootMsg,
        Main,
        Menu,
        Shutdown
    };
    State state = State::BootMsg;
    enum MenuEntry
    {
        Back = 0,
        Emissivity,
        FrameRate,
        SaveChanges,
        NumEntries
    };
    int menuEntry;
};

template<class IOHandler>
void ApplicationUI<IOHandler>::update()
{
    ButtonState btns = ioHandler.checkButtons();
    upBtn.update(btns.up);
    onBtn.update(btns.on);
    switch (state) {
        case BootMsg: updateBootMessage(); break;
        case Main: updateMain(); break;
        case Menu: updateMenu(); break;
        case Shutdown:
        default: break;
    }
    if (state == Main || state == Menu) drawBatteryIcon();
}

template<class IOHandler>
void ApplicationUI<IOHandler>::updateFrame(MLX90640Frame *processedFrame)
{
    if (processedFrame==nullptr) return; //Happens on shutdown
    if (paused) return;
    lastFrameMutex.lock();
    lastFrame.reset(processedFrame);
    if (state == Main || state == Menu)
    {
        mxgui::DrawingContext dc(display);
        drawFrame(dc);
    }
    lastFrameMutex.unlock();
}

template<class IOHandler>
void ApplicationUI<IOHandler>::enterBootMessage()
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
    if (upBtn.getValue()) {
        #ifdef _MIOSIX
        std::string ver = std::string(miosix::getMiosixVersion()) + "\n" + thermal_camera_version;
        #else
        std::string ver = std::string("simulator\n") + thermal_camera_version;
        #endif
        TextBox::draw(dc, mxgui::Point(0,dc.getHeight()-smallFont.getHeight()*6), 
            mxgui::Point(dc.getWidth()-1,dc.getHeight()-1),
            ver.c_str(),
            TextBox::TextOnlyBackground|TextBox::CharWrap|TextBox::LeftAlignment);
    }
}

template<class IOHandler>
void ApplicationUI<IOHandler>::drawBatteryIcon()
{
    mxgui::DrawingContext dc(display);
    mxgui::Point batteryIconPoint(104,0);
    switch(ioHandler.checkBatteryLevel())
    {
        case BatteryLevel::B100: dc.drawImage(batteryIconPoint,batt100icon); break;
        case BatteryLevel::B75:  dc.drawImage(batteryIconPoint,batt75icon); break;
        case BatteryLevel::B50:  dc.drawImage(batteryIconPoint,batt50icon); break;
        case BatteryLevel::B25:  dc.drawImage(batteryIconPoint,batt25icon); break;
        case BatteryLevel::B0:   dc.drawImage(batteryIconPoint,batt0icon); break;
    }
}

template<class IOHandler>
void ApplicationUI<IOHandler>::updateBootMessage()
{
    if (upBtn.getValue()) return;
    if (lifecycle == Ready) enterMain();
}

template<class IOHandler>
void ApplicationUI<IOHandler>::drawStaticPartOfMainScreen(mxgui::DrawingContext& dc)
{
    dc.clear(mxgui::black);
    //For mxgui::point coordinates see ui-mockup-main-screen.png
    dc.drawImage(mxgui::Point(0,0),emissivityicon);
    char line[16];
    snprintf(line,sizeof(line),"%.2f  %2dfps ",options.emissivity,options.frameRate);
    dc.setFont(smallFont);
    dc.setTextColor(std::make_pair(mxgui::white,mxgui::black));
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

template<class IOHandler>
void ApplicationUI<IOHandler>::drawPauseIndicator(mxgui::DrawingContext& dc)
{
    const mxgui::Point p0(85,1);
    const mxgui::Point p1(85+8,1+8);
    if (paused) dc.drawImage(p0,pauseicon);
    else dc.clear(p0,p1,mxgui::black);
}

template<class IOHandler>
void ApplicationUI<IOHandler>::enterMain()
{
    state = Main;
    mxgui::DrawingContext dc(display);
    drawStaticPartOfMainScreen(dc);
    drawPauseIndicator(dc);
    drawFrame(dc);
    onBtn.ignoreUntilNextPress();
    upBtn.ignoreUntilNextPress();
}

template<class IOHandler>
void ApplicationUI<IOHandler>::updateMain()
{
    if(onBtn.getLongPressEvent()) enterShutdown();
    else if(onBtn.getUpEvent())
    {
        paused=!paused;
        ioHandler.setPause(paused);
        mxgui::DrawingContext dc(display);
        drawPauseIndicator(dc);
    }
    else if(upBtn.getDownEvent()) enterMenu();
}

template<class IOHandler>
void ApplicationUI<IOHandler>::drawStaticPartOfMenuScreen(mxgui::DrawingContext& dc)
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

template<class IOHandler>
void ApplicationUI<IOHandler>::enterMenu()
{
    state = Menu;
    menuEntry = Back;
    mxgui::DrawingContext dc(display);
    drawStaticPartOfMenuScreen(dc);
    drawPauseIndicator(dc);
    drawFrame(dc);
    for (int i=0; i<NumEntries; i++) drawMenuEntry(dc, i);
    upBtn.ignoreUntilNextPress();
    onBtn.ignoreUntilNextPress();
}

template<class IOHandler>
void ApplicationUI<IOHandler>::_drawMenuEntry(mxgui::DrawingContext& dc, int i,
    const char *label, const char *value)
{
    const mxgui::Color selectedBGColor = mxgui::Color(to565(255,128,0));
    const mxgui::Color selectedFGColor = mxgui::black;
    const mxgui::Color unselectedBGColor = mxgui::black;
    const mxgui::Color unselectedFGColor = mxgui::white;
    const auto fontHeight = dc.getFont().getHeight();
    short top = 50+i*fontHeight;
    if (i==menuEntry) dc.setTextColor(std::make_pair(selectedFGColor,selectedBGColor));
    else dc.setTextColor(std::make_pair(unselectedFGColor,unselectedBGColor));
    if (value)
    {
        TextBox::draw(dc, mxgui::Point(0,top), mxgui::Point(74,top+fontHeight-1), label, 0, 0, 3, 0);
        TextBox::draw(dc, mxgui::Point(75,top), mxgui::Point(dc.getWidth()-1,top+fontHeight-1), value, 0, 0, 0, 3);
    } else {
        TextBox::draw(dc, mxgui::Point(0,top), mxgui::Point(dc.getWidth()-1,top+fontHeight-1), label, 0, 0, 3, 3);
    }
}

template<class IOHandler>
void ApplicationUI<IOHandler>::drawMenuEntry(mxgui::DrawingContext& dc, int id)
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
        case SaveChanges:
            _drawMenuEntry(dc, SaveChanges, "Save changes");
            break;
    }
}

template<class IOHandler>
void ApplicationUI<IOHandler>::updateMenu()
{
    mxgui::DrawingContext dc(display);
    if (onBtn.getAutorepeatEvent())
    {
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
    } 
    else if(upBtn.getAutorepeatEvent())
    {
        int oldEntry = menuEntry;
        menuEntry=(menuEntry+1)%NumEntries;
        drawMenuEntry(dc, oldEntry);
        drawMenuEntry(dc, menuEntry);
    }
}

template<class IOHandler>
void ApplicationUI<IOHandler>::enterShutdown()
{
    state = Shutdown;
    lifecycle = Quit;
    #ifdef _MIOSIX
    miosix::MemoryProfiling::print();
    #endif
}

template<class IOHandler>
void ApplicationUI<IOHandler>::drawFrame(mxgui::DrawingContext& dc)
{
    lastFrameMutex.lock();
    if (lastFrame.get()!=nullptr)
    {
        #ifdef _MIOSIX
        auto t1 = miosix::getTime();
        #endif
        bool smallCached=(state == Menu); //Cache now if the main thread changes it
        if(smallCached==false) renderer->render(lastFrame.get());
        else renderer->renderSmall(lastFrame.get());
        #ifdef _MIOSIX
        auto t2 = miosix::getTime();
        #endif
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
        #ifdef _MIOSIX
        auto t3 = miosix::getTime();
        iprintf("render = %lld draw = %lld\n",t2-t1,t3-t2);
        #endif
        //process = 78ms render = 1.9ms draw = 15ms 8Hz scaled short DMA UI
    }
    lastFrameMutex.unlock();
}

template<class IOHandler>
void ApplicationUI<IOHandler>::drawTemperature(mxgui::DrawingContext& dc, 
    mxgui::Point a, mxgui::Point b, mxgui::Font f, short temperature)
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
