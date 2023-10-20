/***************************************************************************
 *   Copyright (C) 2023 by Daniele Cattaneo                                *
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

#include "frame_source.hpp"
#include "../../applicationui.h"
#include "mxgui/entry.h"
#include "mxgui/display.h"
#include "mxgui/level2/input.h"
#include <thread>
#include <chrono>
#include <QApplication>

using namespace mxgui;

class ApplicationSimulator: IOHandlerBase
{
public:
    ApplicationSimulator(FrameSource *frameSrc) :
        ui(*this, DisplayManager::instance().getDisplay(), {false, false}), 
        frameSrc(frameSrc) {}

    void run()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(700));
        ui.lifecycle = ApplicationUI<ApplicationSimulator>::Ready;
        frameSrc->setEmissivity(ui.options.emissivity);
        ui.updateFrame(frameSrc->getLastFrame().release());
        while (ui.lifecycle != ApplicationUI<ApplicationSimulator>::Quit) {
            ui.update();
            frameSrc->setEmissivity(ui.options.emissivity);
            ui.updateFrame(frameSrc->getLastFrame().release());
            std::this_thread::sleep_for(std::chrono::microseconds(16666));
        }
    }

    ButtonState checkButtons()
    {
        Event e;
        for (;;)
        {
            e=InputHandler::instance().popEvent();
            if(e.getEvent() == EventType::Default) break;
            switch(e.getEvent())
            {
                case EventType::ButtonA:
                    if (e.getDirection() == EventDirection::DOWN) buttons.up=true;
                    else buttons.up=false;
                    break;
                case EventType::ButtonB:
                    if (e.getDirection() == EventDirection::DOWN) buttons.on=true;
                    else buttons.on=false;
                    break;
                default: continue;
            }
        }
        return buttons;
    }

    BatteryLevel checkBatteryLevel()
    {
        return BatteryLevel::B50;
    }

    bool checkUSBConnected()
    {
        return true;
    }

    void setPause(bool paused)
    {
        printf("pause = %d\n", paused);
    }

    void saveOptions(ApplicationOptions& options)
    {
        printf("saved options\n");
    }

private:
    ButtonState buttons = ButtonState(0, 0);
    ApplicationUI<ApplicationSimulator> ui;
    FrameSource *frameSrc;
};

ENTRY()
{
    std::unique_ptr<FrameSource> source;
    if (qApp->arguments().size() == 2) {
        std::string path = qApp->arguments().at(1).toStdString();
        source.reset(reinterpret_cast<FrameSource *>(new DeviceFrameSource(path)));
    } else {
        source.reset(reinterpret_cast<FrameSource *>(new DummyFrameSource()));
    }
    ApplicationSimulator application(source.get());
    application.run();
    return 0;
}
