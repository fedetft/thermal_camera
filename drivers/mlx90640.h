/***************************************************************************
 *   Copyright (C) 2022 by Terraneo Federico                               *
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
#include <drivers/stm32f2_f4_i2c.h>
#include <drivers/MLX90640_API.h>

/**
 * Raw MLX90640 frame as read from the sensor by MLX90640::readFrame()
 * Needs to be passed to MLX90640::processFrame() to compute temperatures
 */
class MLX90640RawFrame
{
public:
    unsigned short subframe[2][834]; // Heavy object! ~3.4 KByte
};

/**
 * Processed MLX90640 frame with temperature data. Temperature is stored
 * as an array of short, one per pixel, which contain the temperature in
 * degrees celsius multiplied by a scale factor so as to preserve a resolution
 * of less than 1°C. Note that the sensor is quite noisy, so the fractional
 * data is more useful for showing as an image, rather than to display as
 * a number.
 */
class MLX90640Frame
{
public:
    static const int nx=32, ny=24; ///< Image resolution
    static const int scaleFactor=::scaleFactor; ///< Temperature scale factor
    short temperature[nx*ny]; // Heavy object! 1.5 KByte
    
    /**
     * \param x x coordinate
     * \param y y coordinate
     * \return the temperature at the given coordinate, with 0,0 the top left
     * point, compensating for the sensor orientation on the board
     */
    short getTempAt(int x, int y) { return temperature[(nx-1-x)+y*nx]; }
};

/**
 * Possible refresh rates of the MLX90640. Note that the framerate is
 * expressed in full frames per second, while the datasheet expresses it in
 * nummber of half-frames per second.
 */
enum class MLX90640Refresh : unsigned short
{
    R025 = 0b000, ///<  1 Half frame every 2 seconds, so 0.25 frames/sec
    R05  = 0b001, ///<  1 Half frame every second,    so 0.5 frames/sec
    R1   = 0b010, ///<  2 Half frames every second,   so  1 frames/sec
    R2   = 0b011, ///<  4 Half frames every second,   so  2 frames/sec
    R4   = 0b100, ///<  8 Half frames every second,   so  4 frames/sec
    R8   = 0b101, ///< 16 Half frames every second,   so  8 frames/sec
    R16  = 0b110, ///< 32 Half frames every second,   so 16 frames/sec
    R32  = 0b111  ///< 64 Half frames every second,   so 32 frames/sec
};

/**
 * Convert an integer number of fps into the closest MLX90640 framerate value.
 * NOTE: this function always returns at least 1fps, discarding the lower
 * settings supported by the sensor
 * \param rate number of frames per second as int
 * \return number of fps as MLX90640Refresh enum
 */
MLX90640Refresh refreshFromInt(int rate);

/**
 * Optimized MLX90640 driver.
 * NOTE: all member functions of this class need to be called from the same
 * thread EXCEPT for processFrame() that can be called from a separate thread
 * to speed up computation
 */
class MLX90640
{
public:
    /**
     * Constructor
     * \param i2c pointer to I2C driver
     * \param devAddr MLX90640 device address
     */
    MLX90640(miosix::I2C1Master *i2c, unsigned char devAddr=0x33);
    
    /**
     * Set the sensor refresh rate
     * \param rr refres rate
     * \return true if success
     */
    bool setRefresh(MLX90640Refresh rr);
    
    /**
     * \return the currently set refresh rate
     */
    MLX90640Refresh getRefresh() const { return rr; }
    
    /**
     * Read a frame from the sensor.
     * Blocking call, waits until subframe 0 and subframe 1 have been received
     * from the sensor
     * \param rawFrame pointer to a caller-allocated MLX90640RawFrame object
     * where the frame will be stored
     * \return true on success, false on failure
     */
    bool readFrame(MLX90640RawFrame *rawFrame);
    
    /**
     * Process a raw frame computing the themperature of each pixel.
     * This is a compute-intensive task that requires no interaction with the
     * hardware
     * NOTE: this member function can be called from a separate thread with
     * respect to all the other member functions of this class to speed up
     * computation
     * \param rawFrame pointer to a caller-allocated MLX90640RawFrame object
     * contaning a valid frame from the sensor
     * \param frame pointer to a caller-allocated MLX90640Frame object where
     * the pixel temperatures will be stored
     * \param emissivity the user-selected emissivity value, that is necessary
     * to compute the temperatures
     */
    void processFrame(const MLX90640RawFrame *rawFrame, MLX90640Frame *frame, float emissivity);
    
private:
    /**
     * Blocking call tthat waits until a specific subframe number has been
     * received
     * \param index subframe number
     * \param rawFrame pointer to a caller-allocated buffer to store the
     * subframe data
     * \return true on success, false on failure
     */
    bool readSpecificSubFrame(int index, unsigned short rawFrame[834]);
    
    /**
     * Blocking call that waits until a subframe has been received from the
     * sensor
     * \param rawFrame pointer to a caller-allocated buffer to store the
     * subframe data
     * \return true on success, false on failure
     */
    bool readSubFrame(unsigned short rawFrame[834]);
    
    /**
     * \return 80% of the half refresh time based on framerate, used to decide
     * when to start polling
     */
    std::chrono::microseconds waitTime();
    
    /**
     * \return optimized poll period based on framerate to minimize CPU usage
     * when polling the sensor
     */
    std::chrono::microseconds pollTime();
    
    /**
     * Read data from the sensor
     * \param addr register address
     * \param len number of 16bit words to read
     * \param data pointer to a caller-allocated buffer of len 16bit words
     * \return true on success, false on failure
     */
    bool read(unsigned int addr, unsigned int len, unsigned short *data);
    
    /**
     * Write one 16bit register to the sensor
     * \param addr register address
     * \param data 16bit word to write to the specified register
     */
    bool write(unsigned int addr, unsigned short data);
    
    miosix::I2C1Master *i2c;
    const unsigned char devAddr;
    MLX90640Refresh rr;
    std::chrono::time_point<std::chrono::system_clock> lastFrameReady;
    paramsMLX90640 params; // Heavy object! ~11 KByte
};
