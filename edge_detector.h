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
    ButtonEdgeDetector(bool initialState)
        : state(initialState), longPressState(AlreadyTriggered) {}
    
    using clock = std::chrono::steady_clock;

    void update(bool newState)
    {
        newState=newState^(!activeLevel);
        down=(newState^state)&newState;
        up=(newState^state)&state;
        if (down) downTime=clock().now();
        if (!newState) longPressState=Waiting;
        else if (longPressState==Trigger) longPressState=AlreadyTriggered;
        else if (longPressState==Waiting && (clock().now()-downTime)>=longPressWaitTime) longPressState=Trigger;
        state=newState;
        //if (down) printf("%p down\n", this);
        //if (up) printf("%p up\n", this);
        //if (longPressState==Trigger) printf("%p long press\n", this);
    }

    bool getDownEvent()
    {
        return down;
    }

    bool getState()
    {
        return state;
    }

    bool getUpEvent()
    {
        return up;
    }

    bool getLongPressEvent()
    {
        return longPressState == Trigger;
    }

private:
    const clock::duration longPressWaitTime = std::chrono::milliseconds(600);

    bool state;
    bool down, up;
    clock::time_point downTime;
    enum LongPressState { Waiting, Trigger, AlreadyTriggered };
    LongPressState longPressState;
};
