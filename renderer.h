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

#include <mxgui/display.h>
#include "drivers/mlx90640frame.h"

/**
 * This class contains code to convert an array of temperatures into a
 * thermal image to be displayed on screen
 */
class ThermalImageRenderer
{
public:
    /**
     * Perform the computational stage of transforming an array of temperatures
     * into an image to be displayed on screen, and compute statistics on the
     * image data
     * \param processedFrame class with the array of temperatures to display
     */
    void render(MLX90640Frame *processedFrame) { doRender(processedFrame,false); }

    /**
     * Perform the computational stage of transforming an array of temperatures
     * into an image to be displayed on screen, and compute statistics on the
     * image data (small)
     * \param processedFrame class with the array of temperatures to display
     */
    void renderSmall(MLX90640Frame *processedFrame) { doRender(processedFrame,true); }

    /**
     * Draw the already rendered image on screen
     * \param dc DrawingContext used to access the screen
     * \param p upper left point where to start drawing the image
     */
    void draw(mxgui::DrawingContext& dc, mxgui::Point p);

    /**
     * Draw the already rendered image on screen (small)
     * \param dc DrawingContext used to access the screen
     * \param p upper left point where to start drawing the image
     */
    void drawSmall(mxgui::DrawingContext& dc, mxgui::Point p);

    /**
     * Compute a legend in the form of an array of colors corresponding to the
     * coldest (legend[0]) and hottest(legend[legendSize-1]) temperaures in the
     * last rendered image
     * \param legend pointer to caller-allocated array of pixel colors, of size
     * legendSize
     * \param legendSize nuber of pixels
     */
    void legend(mxgui::Color *legend, int legendSize);

    /**
     * \return the minimum temperature of the last rendered frame in °C
     */
    short minTemperature() const { return minTemp; }

    /**
     * \return the maximum temperature of the last rendered frame in °C
     */
    short maxTemperature() const { return maxTemp; }

    /**
     * \return the center point temperature of the last rendered frame in °C
     */
    short crosshairTemperature() const { return crosshairTemp; }

private:
    void doRender(MLX90640Frame *processedFrame, bool small);

    template<void (ThermalImageRenderer::*putPix)(int x, int y, mxgui::Color c)>
    inline void renderLoop(MLX90640Frame *processedFrame, short range);

    static inline mxgui::Color interpolate2d(MLX90640Frame *processedFrame, int x, int y, short m, short r);

    static inline mxgui::Color pixMap(short t, short m, short r);

    void crosshairPixel(int x, int y);

    static int colorBrightness(mxgui::Color c);

    static inline short roundedDiv(short a, short b)
    {
        if(a>0) return (a+b/2)/b;
        return (a-b/2)/b;
    }

    inline void putPixelLarge(int y, int x, mxgui::Color c)
    {
        irImage[2*y  ][2*x  ]=c; //Image layout in memory is reversed
        irImage[2*y+1][2*x  ]=c;
        irImage[2*y  ][2*x+1]=c;
        irImage[2*y+1][2*x+1]=c;
    }

    inline void putPixelSmall(int y, int x, mxgui::Color c)
    {
        irImageSmall[y][x]=c; //Image layout in memory is reversed
    }

    union {
        mxgui::Color irImage[94][126];     // Heavy object! ~23KByte
        mxgui::Color irImageSmall[47][63];
    };
    short minTemp, maxTemp, crosshairTemp;
    const short minRange=15;
};
