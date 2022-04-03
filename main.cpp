
#include <miosix.h>
#include <drivers/hwmapping.h>
#include <application.h>

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
    keep_on::mode(Mode::OUTPUT);
    keep_on::high();
    on_btn::mode(Mode::INPUT_PULL_DOWN);
    up_btn::mode(Mode::INPUT_PULL_UP);
    
    auto& display=DisplayManager::instance().getDisplay();
    {
        DrawingContext dc(display);
        dc.setFont(tahoma);
        dc.write(Point(0, 0),"Miosix thermal camera");
        dc.write(Point(0,15),"experimental firmware");
    }
    
    while(on_btn::value()==1) Thread::sleep(100);
    Thread::sleep(200);
    
    try {
        Application app(display);
        app.run();
    } catch(exception& e) {
        iprintf("exception: %s\n",e.what());
    }
    
    display.turnOff();
    keep_on::low();
    for(;;) Thread::sleep(1);
}
