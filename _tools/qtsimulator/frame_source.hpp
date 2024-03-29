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

#pragma once

#include "../../drivers/mlx90640frame.h"
#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

class FrameSource
{
public:
    virtual std::unique_ptr<MLX90640Frame> getLastFrame() = 0;
    virtual void setEmissivity(float emiss) {};
    virtual void stop() {};
    virtual ~FrameSource() = default;
};

class DummyFrameSource : public FrameSource
{
public:
    DummyFrameSource() {}
    std::unique_ptr<MLX90640Frame> getLastFrame() override;
};

class DeviceFrameSource : public FrameSource
{
    MLX90640Frame lastFrame = { 0 };
    MLX90640EEPROM eeprom;
    std::mutex lastFrameMutex;
    std::thread ioThread;
    std::atomic<bool> stopped;
    std::atomic<float> emiss;

    void ioThreadMain(std::string devicePath);
    void connect(const char *cstr);
public:
    DeviceFrameSource(std::string devicePath);
    ~DeviceFrameSource()
    {
        stop();
    }
    std::unique_ptr<MLX90640Frame> getLastFrame() override;
    void setEmissivity(float _emiss) override
    {
        emiss = _emiss;
    }
    void stop() override
    {
        if (!stopped) {
            stopped = true;
            ioThread.join();
        }
    }
};
