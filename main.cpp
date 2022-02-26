
#include <cstdio>
#include <miosix.h>
#include <mxgui/display.h>
#include <mxgui/misc_inst.h>
#include <display_er_oledm015.h>
#include <hwmapping.h>

using namespace std;
using namespace miosix;
using namespace mxgui;

namespace mxgui {
void registerDisplayHook(DisplayManager& dm)
{
    dm.registerDisplay(new DisplayErOledm015);
}
} //namespace mxgui

int main()
{
    puts("Working");
    
    keep_on::mode(Mode::OUTPUT);
    keep_on::high();
    on_btn::mode(Mode::INPUT_PULL_DOWN);
    up_btn::mode(Mode::INPUT_PULL_UP);
    
    auto& display=DisplayManager::instance().getDisplay();
    {
        DrawingContext dc(display);
        dc.setFont(tahoma);
        dc.write(Point(0,0),"Testing display...");
    }
    
    while(on_btn::value()==1)
    {
       Thread::sleep(100);
       puts("Waiting for ON release");
    }
    
    for(;;)
    {
        Thread::sleep(100);
        if(up_btn::value()==0) puts("UP button");
        if(on_btn::value()==1)
        {
            puts("ON button, bye");
            break;
        }
    }
    
    display.turnOff();
    keep_on::low();
    for(;;) Thread::sleep(1);
}
