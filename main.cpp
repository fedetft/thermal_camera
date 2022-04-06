
#include <cstdio>
#include <drivers/misc.h>
#include <application.h>
#include <mxgui/display.h>
#include <images/miosixlogo.h>
#include <drivers/display_er_oledm015.h>

using namespace std;
using namespace mxgui;

namespace mxgui {
void registerDisplayHook(DisplayManager& dm)
{
    dm.registerDisplay(new DisplayErOledm015);
}
} //namespace mxgui


void bootMessage(Display& display)
{
    const char s0[]="Miosix";
    const char s1[]="Thermal camera";
    const int s0pix=miosixlogo.getWidth()+1+droid21.calculateLength(s0);
    const int s1pix=tahoma.calculateLength(s1);
    DrawingContext dc(display);
    dc.setFont(droid21);
    int width=dc.getWidth();
    int y=10;
    dc.drawImage(Point((width-s0pix)/2,y),miosixlogo);
    dc.write(Point((width-s0pix)/2+miosixlogo.getWidth()+1,y),s0);
    y+=dc.getFont().getHeight();
    dc.line(Point((width-s1pix)/2,y),Point((width-s1pix)/2+s1pix,y),white);
    y+=4;
    dc.setFont(tahoma);
    dc.write(Point((width-s1pix)/2,y),s1);
}

int main()
{
    initializeBoard();
    auto& display=DisplayManager::instance().getDisplay();
    bootMessage(display);
    waitPowerButtonReleased();
    {
        DrawingContext dc(display);
        dc.clear(black);
    }
    
    try {
        Application app(display);
        app.run();
    } catch(exception& e) {
        iprintf("exception: %s\n",e.what());
    }
    
    display.turnOff();
    shutdownBoard();
}
