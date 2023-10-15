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
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/file.h>

std::unique_ptr<MLX90640Frame> DummyFrameSource::getLastFrame()
{
    static const MLX90640Frame testFrame = { .temperature = {
        71, 70, 69, 71, 70, 72, 72, 72, 84, 96,101, 90, 74, 74, 87,102,100, 91, 71, 73, 88, 98, 99, 87, 74, 73, 67, 70, 67, 69, 64, 67,
        72, 72, 71, 70, 71, 73, 73, 75, 86, 98,103, 91, 73, 73, 92,103,100, 90, 71, 72, 96,100, 99, 87, 73, 72, 68, 68, 69, 68, 67, 66,
        69, 71, 71, 72, 72, 73, 73, 75, 88,104,105, 90, 73, 76, 99,107,100, 84, 73, 76,101,105, 95, 81, 71, 71, 69, 69, 70, 69, 66, 67,
        71, 72, 71, 70, 73, 73, 74, 75, 88,104,104, 91, 75, 78,105,107, 96, 84, 73, 78,105,105, 88, 78, 71, 71, 69, 69, 69, 69, 65, 68,
        72, 72, 73, 73, 74, 73, 72, 75, 88,105,106, 95, 75, 82,106,107, 94, 80, 79, 93,105,104, 82, 74, 72, 71, 69, 70, 70, 70, 66, 68,
        73, 73, 75, 74, 73, 72, 74, 75, 91,103,107, 97, 77, 85,108,107, 93, 80, 85, 96,106,105, 78, 73, 71, 70, 69, 70, 69, 70, 67, 66,
        74, 75, 79, 88, 92, 80, 73, 74, 89,105,110,103, 81, 94,106,107, 92, 80, 93,103,103,100, 75, 72, 71, 70, 68, 69, 69, 69, 67, 69,
        76, 76, 81, 92, 97, 85, 73, 74, 89,105,112,106, 84, 94,108,107, 89, 79,100,104,103, 98, 74, 71, 71, 70, 69, 68, 69, 69, 67, 68,
        74, 76, 79, 88,105,100, 73, 75, 88,104,110,107, 88,100,107,107, 87, 85,104,105,100, 87, 70, 70, 70, 71, 67, 69, 70, 70, 67, 69,
        74, 74, 80, 84,106,105, 75, 76, 89,102,109,106, 93,103,108,107, 86, 87,105,105, 96, 84, 69, 69, 69, 69, 69, 67, 70, 69, 68, 68,
        73, 75, 75, 81,107,108, 89, 79, 86,102,109,109,105,108,109,106, 90, 97,104,105, 90, 79, 70, 69, 71, 70, 70, 70, 69, 70, 68, 70,
        74, 75, 77, 78,101,109, 98, 84, 87,102,110,109,108,109,108,108, 97, 99,105,104, 84, 76, 70, 70, 71, 70, 71, 70, 71, 70, 68, 69,
        72, 74, 74, 77, 91,106,108,101, 98,109,109,110,107,109,107,108,106,104,103,102, 80, 74, 71, 71, 72, 71, 69, 70, 72, 73, 70, 71,
        74, 73, 75, 75, 87,101,110,110,110,109,110,110,108,109,107,107,107,105,104,100, 78, 74, 72, 72, 72, 74, 73, 73, 79, 81, 74, 73,
        74, 75, 72, 75, 80, 88,110,112,109,110,113,113,112,111,107,108,107,105,102, 99, 76, 75, 71, 74, 73, 76, 90, 98,103,103, 87, 79,
        75, 74, 73, 73, 81, 83,110,110,110,111,114,114,112,111,110,108,107,106,103, 97, 75, 72, 73, 72, 76, 84,101,100,104,104, 83, 79,
        74, 74, 72, 74, 78, 80,108,111,113,113,117,116,114,114,110,110,108,106,103, 98, 76, 75, 73, 75, 95,102,103,104, 92, 84, 76, 73,
        73, 74, 73, 75, 77, 81,108,110,113,114,116,116,116,114,113,111,108,107,105, 99, 78, 75, 74, 80,102,104,106,104, 83, 80, 73, 73,
        77, 74, 73, 75, 77, 79,106,112,114,116,117,119,117,116,113,111,110,107,105,106, 82, 76, 90,100,105,107, 98, 87, 78, 76, 71, 70,
        76, 74, 72, 73, 77, 79,109,111,115,116,118,118,117,116,113,113,110,110,109,108, 91, 88,102,103,108,107, 87, 82, 76, 73, 71, 70,
        73, 73, 74, 74, 77, 80,107,111,115,116,119,120,120,118,116,116,110,109,110,111,108,107,105,108,105, 97, 80, 77, 74, 72, 69, 70,
        74, 73, 75, 74, 77, 80,109,111,115,117,120,120,121,119,116,114,111,109,109,110,110,108,105,106, 98, 88, 78, 74, 74, 70, 70, 70,
        74, 74, 72, 75, 78, 81,106,111,115,116,119,121,118,119,113,113,110,109,109,110,109,106,104,106, 84, 79, 73, 73, 71, 71, 69, 69,
        76, 74, 76, 72, 79, 85,108,108,113,115,119,119,117,115,111,110,110,110,110,108,108,107,105,100, 79, 77, 73, 73, 70, 71, 70, 68}};
    return std::unique_ptr<MLX90640Frame>(new MLX90640Frame(testFrame));
}

DeviceFrameSource::DeviceFrameSource(std::string devicePath)
{
    stopped = false;
    emiss = 0.95f;
    ioThread = std::thread(&DeviceFrameSource::ioThreadMain, this, devicePath);
}

std::unique_ptr<MLX90640Frame> DeviceFrameSource::getLastFrame()
{
    std::lock_guard<std::mutex> lock(lastFrameMutex);
    return std::unique_ptr<MLX90640Frame>(new MLX90640Frame(lastFrame));
}

void DeviceFrameSource::ioThreadMain(std::string devicePath)
{
    const char *cstr = devicePath.c_str();
    connect(cstr);
    while (!stopped) {
        sleep(1);
        connect(cstr);
    }
    fprintf(stderr, "DeviceFrameSource: stopped\n");
}

size_t DeviceFrameSource::parseHex(const char *hex, size_t bufSz, void *buf)
{
    uint8_t tmp = 0;
    uint8_t *outp = reinterpret_cast<uint8_t *>(buf);
    if (bufSz == 0) return 0;
    for (int i = 0;; i++)
    {
        char c = hex[i];
        if ('0' <= c && c <= '9') tmp = tmp >> 4 | ((c - '0') << 4);
        else if ('A' <= c && c <= 'F') tmp = tmp >> 4 | ((c - 'A' + 10) << 4);
        else break;
        if (i % 2)
        {
            *outp++ = tmp;
            if (--bufSz == 0) break;
        }
    }
    return outp - reinterpret_cast<uint8_t *>(buf);
}

void DeviceFrameSource::connect(const char *cstr)
{
    fprintf(stderr, "new connection attempt to %s\n", cstr);

    // non blocking IO otherwise the incorrect default TTY setup will make open
    // hang waiting for a DTS signal which will never arrive
    int fd = open(cstr, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd == -1) {
        fprintf(stderr, "open error\n");
        return;
    }

    flock(fd, LOCK_EX | LOCK_NB);
    tcflush(fd, TCIOFLUSH);

    struct termios tio = {0};
    cfsetispeed(&tio, 300); // baud rates are fake and do not matter
    cfsetospeed(&tio, 300);
    tio.c_iflag = IGNCR;
    tio.c_oflag = 0;
    tio.c_cflag = CS8 | CLOCAL | CREAD;
    tio.c_lflag = 0;
    tio.c_cc[VTIME] = 0; // Inter-character timer unused
    tio.c_cc[VMIN] = 1;  // Blocking read until 1 character received
    tcsetattr(fd, TCSANOW, &tio);

    // Stop output from previous commands
    // Do this before C file buffering comes into play
    write(fd, "\nstop_stream\n", 13);
    tcdrain(fd);
    usleep(100 * 1000);
    tcflush(fd, TCIOFLUSH);

    // Close and reopen otherwise... we stop receiving data sometimes?????
    close(fd);
    fd = open(cstr, O_RDWR | O_NOCTTY | O_NONBLOCK);

    flock(fd, LOCK_EX | LOCK_NB);
    tcflush(fd, TCIOFLUSH);

    // struct termios tio = {0};
    cfsetispeed(&tio, 300);
    cfsetospeed(&tio, 300);
    tio.c_iflag = IGNCR;
    tio.c_oflag = 0;
    tio.c_cflag = CS8 | CLOCAL | CREAD;
    tio.c_lflag = 0;
    tio.c_cc[VTIME] = 0; // Inter-character timer unused
    tio.c_cc[VMIN] = 1;  // Blocking read until 1 character received
    tcsetattr(fd, TCSANOW, &tio);

    // Set file configuration back to blocking mode
    fcntl(fd, F_SETFL, 0);

    FILE *fp = fdopen(fd, "r+");
    if (fp == NULL) {
        fprintf(stderr, "fdopen error\n");
        close(fd);
        return;
    }

    fprintf(stderr, "connection to %s established\n", cstr);

    const size_t charBufSz = 10000;
    char buf[charBufSz];

    fprintf(fp, "get_eeprom\n");
    fgets(buf, charBufSz, fp);
    fprintf(stderr, "eeprom=%s", buf);
    size_t sz = parseHex(buf, sizeof(eeprom.eeprom), eeprom.eeprom);
    paramsMLX90640 mlx90640;
    MLX90640_ExtractParameters(eeprom.eeprom, &mlx90640);

    fprintf(fp, "start_stream\n");

    while (!stopped && !feof(fp))
    {
        MLX90640RawFrame rawFrame;
        fgets(buf, charBufSz, fp);
        if (feof(fp))
            break;
        sz = parseHex(buf+2, sizeof(rawFrame.subframe[0]), rawFrame.subframe[0]);
        fgets(buf, charBufSz, fp);
        if (feof(fp))
            break;
        sz = parseHex(buf+2, sizeof(rawFrame.subframe[1]), rawFrame.subframe[1]);

        MLX90640Frame newFrame;
        rawFrame.process(&newFrame, mlx90640, emiss.load());
        {
            std::lock_guard<std::mutex> lock(lastFrameMutex);
            lastFrame = newFrame;
        }
    }

    fprintf(fp, "stop_stream\n");

    flockfile(fp);
    fclose(fp);

    fprintf(stderr, "connection to %s closed\n", cstr);
}

