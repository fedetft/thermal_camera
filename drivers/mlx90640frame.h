/***************************************************************************
 *   Copyright (C) 2022 by Daniele Cattaneo and Terraneo Federico          *
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

#include "MLX90640_API.h"

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
 * Raw MLX90640 EEPROM contents
 */
class MLX90640EEPROM
{
public:
    static const unsigned int eepromSize=832;
    unsigned short eeprom[eepromSize]; // Heavy object! ~1.7 KByte
};

/**
 * Raw MLX90640 frame as read from the sensor by MLX90640::readFrame()
 */
class MLX90640RawFrame
{
public:
    unsigned short subframe[2][834]; // Heavy object! ~3.4 KByte

    /**
     * Processes this raw frame, computing the themperature of each pixel.
     * This is a compute-intensive task that requires no interaction with the
     * hardware.
     * \param output pointer to a caller-allocated MLX90640Frame object where
     * the pixel temperatures will be stored
     * \param params reference to the calibration parameters of the MLX90640
     * sensor, as parsed from the internal EEPROM
     * \param emissivity the user-selected emissivity value, that is necessary
     * to compute the temperatures
     */
    void process(MLX90640Frame *output, paramsMLX90640& params, float emissivity) const
    {
        const float taShift=8.f; //Default shift for MLX90640 in open air
        for(int i=0;i<2;i++)
        {
            float vdd=MLX90640_GetVdd(this->subframe[i],&params);
            float Ta=MLX90640_GetTa(this->subframe[i],&params,vdd);
            float Tr=Ta-taShift; //Reflected temperature based on the sensor ambient temperature
            MLX90640_CalculateToShort(this->subframe[i],&params,emissivity,vdd,Ta,Tr,output->temperature);
        }
    }
};
