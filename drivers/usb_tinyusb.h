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

#include "miosix.h"

class USBCDC
{
public:
    USBCDC(miosix::Priority intSvcThreadPrio);
    ~USBCDC();

    bool connected();

    void putChar(char c);
    void write(const uint8_t *buf, int size);
    bool write(const uint8_t *buf, int size, long long maxWait);
    void print(const char *str);
    bool print(const char *str, long long maxWait);

    bool waitForInput(long long maxWait);
    int getChar();
    bool readLine(char *buf, int size);

    // Forces all I/O functions in execution in other threads to terminate
    // as soon as possible. Any subsequent I/O operation will fail.
    void prepareShutdown();

private:
    static void *thread(void *);

    bool waitForInputUnlocked(miosix::Lock<miosix::FastMutex>& lock, long long deadline);
    int getCharUnlocked(miosix::Lock<miosix::FastMutex>& lock);

    miosix::Thread *tinyusbThread;
    volatile bool forceEOF = false;
};
