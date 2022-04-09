
#include <cstdio>
#include <drivers/misc.h>
#include <application.h>
#include <mxgui/display.h>
#include <drivers/display_er_oledm015.h>

using namespace std;
using namespace mxgui;

namespace mxgui {
void registerDisplayHook(DisplayManager& dm)
{
    dm.registerDisplay(new DisplayErOledm015);
}
} //namespace mxgui

int main()
{
    initializeBoard();
    auto& display=DisplayManager::instance().getDisplay();
    
    try {
        Application app(display);
        app.run();
    } catch(exception& e) {
        iprintf("exception: %s\n",e.what());
    }
    
    display.turnOff();
    shutdownBoard();
}
