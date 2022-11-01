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

#include <renderer.h>
#include <colormap.h>
#include <mxgui/misc_inst.h>

using namespace std;
using namespace mxgui;

//
// class ThermalImageRenderer
//

void ThermalImageRenderer::draw(DrawingContext& dc, Point p)
{
    Image img(94,126,irImage);
    dc.drawImage(p,img);
}

void ThermalImageRenderer::drawSmall(DrawingContext& dc, Point p)
{
    Image img(47,63,irImageSmall);
    dc.drawImage(p,img);
}

void ThermalImageRenderer::legend(mxgui::Color *legend, int legendSize)
{
    int colormapRange=max(0,min<int>(maxTemp-minTemp,minRange))*255/minRange;
    for(int i=0;i<legendSize;i++) legend[i]=colormap[colormapRange*i/(legendSize-1)];
}

void ThermalImageRenderer::doRender(MLX90640Frame *processedFrame, bool small)
{
    const int nx=MLX90640Frame::nx, ny=MLX90640Frame::ny;
    minTemp=processedFrame->temperature[0];
    maxTemp=processedFrame->temperature[0];
    crosshairTemp=processedFrame->getTempAt(nx/2,ny/2);
    for(int i=0;i<nx*ny;i++)
    {
        minTemp=min(minTemp,processedFrame->temperature[i]);
        maxTemp=max(maxTemp,processedFrame->temperature[i]);
    }
    short range=max<short>(minRange*processedFrame->scaleFactor,maxTemp-minTemp);
    if(small)
    {
        for(int y=0;y<(2*ny)-1;y++)
        {
            for(int x=0;x<(2*nx)-1;x++)
            {
                Color c=interpolate2d(processedFrame,x,y,minTemp,range);
                irImageSmall[y][x]=c; //Image layout in memory is reversed
            }
        }
    } else {
        for(int y=0;y<(2*ny)-1;y++)
        {
            for(int x=0;x<(2*nx)-1;x++)
            {
                Color c=interpolate2d(processedFrame,x,y,minTemp,range);
                irImage[2*y  ][2*x  ]=c; //Image layout in memory is reversed
                irImage[2*y+1][2*x  ]=c;
                irImage[2*y  ][2*x+1]=c;
                irImage[2*y+1][2*x+1]=c;
            }
        }
         //Draw crosshair
        static const unsigned char xrange[]={58,59,60,65,66,67};
        for(unsigned int xdex=0;xdex<sizeof(xrange);xdex++)
            for(int y=46;y<=47;y++)
                crosshairPixel(xrange[xdex],y);
        static const unsigned char yrange[]={42,43,44,49,50,51};
        for(int x=62;x<=63;x++)
            for(unsigned int ydex=0;ydex<sizeof(yrange);ydex++)
                crosshairPixel(x,yrange[ydex]);
    }
    //Scale temperatures to express them in °C
    minTemp=roundedDiv(minTemp,processedFrame->scaleFactor);
    maxTemp=roundedDiv(maxTemp,processedFrame->scaleFactor);
    crosshairTemp=roundedDiv(crosshairTemp,processedFrame->scaleFactor);
}

Color ThermalImageRenderer::interpolate2d(MLX90640Frame *processedFrame, int x, int y, short m, short r)
{
    if((x & 1)==0 && (y & 1)==0)
        return pixMap(processedFrame->getTempAt(x/2,y/2),m,r);

    if((x & 1)==0) //1d interp along y axis
    {
        short t=processedFrame->getTempAt(x/2,y/2)+processedFrame->getTempAt(x/2,(y/2)+1);
        return pixMap(roundedDiv(t,2),m,r);
    }

    if((y & 1)==0) //1d interp along x axis
    {
        short t=processedFrame->getTempAt(x/2,y/2)+processedFrame->getTempAt((x/2)+1,y/2);
        return pixMap(roundedDiv(t,2),m,r);
    }

    //2d interpolation
    short t=processedFrame->getTempAt(x/2,y/2)    +processedFrame->getTempAt((x/2)+1,y/2)
           +processedFrame->getTempAt(x/2,(y/2)+1)+processedFrame->getTempAt((x/2)+1,(y/2)+1);
    return pixMap(roundedDiv(t,4),m,r);
}

Color ThermalImageRenderer::pixMap(short t, short m, short r)
{
    int pixel=(255*(t-m))/r;
    return colormap[max(0,min(255,pixel))];
}

void ThermalImageRenderer::crosshairPixel(int x, int y)
{
    irImage[y][x]=colorBrightness(irImage[y][x])>16 ? black : white;
}

int ThermalImageRenderer::colorBrightness(mxgui::Color c)
{
    return ((c & 31) + (c>>6) + (c>>11))/3;
}
