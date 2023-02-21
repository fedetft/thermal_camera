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

#include <chrono>

template <bool activeLevel>
class ButtonEdgeDetector
{
public:
    ButtonEdgeDetector(bool initialValue) : value(initialValue) {}
    
    using clock = std::chrono::steady_clock;

    void update(bool newValue)
    {
        newValue=newValue^(!activeLevel);
        oldState=state;
        switch(state)
        {
            case Up:
                if(!value && newValue)
                {
                    state=Down;
                    downTime=clock().now();
                }
                break;
            case Down:
                if (newValue && (clock().now()-downTime)>=longPressWaitTime) state=LongPress;
                else if (value && !newValue) state=Up;
                break;
            case LongPress:
                if (value && !newValue) state=Up;
                break;
            default: break;
        }
        autorepeatTrig=false;
        if (state==Down || state==LongPress)
        {
            clock::time_point now = clock().now();
            if (!autorepeatArmed && (now-downTime)>=autorepeatWaitTime)
            {
                autorepeatArmed = autorepeatTrig = true;
                lastARTrigTime = now;
            }
            if (autorepeatArmed && (now-lastARTrigTime)>=autorepeatPeriod)
            {
                autorepeatTrig = true;
                lastARTrigTime = now;
            }
        } else autorepeatArmed = false;
        value=newValue;
        //if (state!=oldState && state==Down) printf("%p down\n", this);
        //if (state!=oldState && state==Up) printf("%p up\n", this);
        //if (state!=oldState && state==LongPress) printf("%p long press\n", this);
        //if (autorepeatTrig) printf("%p autorepeat\n", this);
    }

    bool getDownEvent()
    {
        return state!=oldState && state==Down;
    }

    bool getUpEvent()
    {
        return state!=oldState && state==Up;
    }

    bool getLongPressEvent()
    {
        return state!=oldState && state==LongPress;
    }

    bool getAutorepeatEvent()
    {
        return getDownEvent() || autorepeatTrig;
    }

    void ignoreUntilNextPress()
    {
        state=oldState=Up;
        autorepeatArmed=false;
    }

    bool getValue()
    {
        return value;
    }

private:
    const clock::duration longPressWaitTime = std::chrono::milliseconds(600);
    const clock::duration autorepeatWaitTime = std::chrono::milliseconds(400);
    const clock::duration autorepeatPeriod = std::chrono::milliseconds(80);

    bool value;
    enum State { Up, Down, LongPress };
    State oldState=Up, state=Up;
    bool autorepeatArmed=false, autorepeatTrig=false;
    clock::time_point downTime;
    clock::time_point lastARTrigTime;
};
