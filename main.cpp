/*
 * STM32F205RC
 * SPI1 speed (display) 15MHz
 * I2C1 speed (sensor)  227kHz TODO
 * 
 * read time = 0.397s draw time = 0.088s
 */

#include <cstdio>
#include <algorithm>
#include <miosix.h>
#include <mxgui/display.h>
#include <mxgui/misc_inst.h>
#include <drivers/display_er_oledm015.h>
#include <drivers/hwmapping.h>
#include <drivers/MLX90640_API.h>
#include <drivers/MLX90640_I2C_Driver.h>
#include <colormap.h>

using namespace std;
using namespace miosix;
using namespace mxgui;

namespace mxgui {
void registerDisplayHook(DisplayManager& dm)
{
    dm.registerDisplay(new DisplayErOledm015);
}
} //namespace mxgui

const unsigned char MLX90640_address = 0x33;
const float ta_shift = 8; //Default shift for MLX90640 in open air

const int nx = 32, ny = 24;
const int pixSize = 2; // image becomes 2*63 x 2*47 or 126 x 94 pixels
const float minRange = 15.f;

paramsMLX90640 mlx90640;
float temperature[nx*ny];

float getTempAt(int x, int y)
{
    //return temperature[x + (ny - 1 - y) * nx];
    return temperature[(nx - 1 - x) + y * nx];
}

Color pixMap(float t, float m, float r)
{
    int pixel = 255.5f * ((t - m) / r);
    return colormap[max(0, min(255, pixel))];
}

Color interpolate2d(int x, int y, float m, float r)
{
    if((x & 1) == 0 && (y & 1) == 0)
        return pixMap(getTempAt(x / 2, y / 2), m, r);

    if((x & 1) == 0) //1d interp along y axis
    {
        float t = getTempAt(x / 2, y / 2) + getTempAt(x / 2, (y / 2) + 1);
        return pixMap(t / 2.f, m, r);
    }
    
    if((y & 1) == 0) //1d interp along x axis
    {
        float t = getTempAt(x / 2, y / 2) + getTempAt((x / 2) + 1, y / 2);
        return pixMap(t / 2.f, m, r);
    }
    
    //2d interpolation
    float t = getTempAt(x / 2, y / 2)       + getTempAt((x / 2) + 1, y / 2)
            + getTempAt(x / 2, (y / 2) + 1) + getTempAt((x / 2) + 1, (y / 2) + 1);
    return pixMap(t / 4.f, m, r);
}

void run(Display& display)
{
    MLX90640_I2CInit();
    
    //Disabled as it's not an improvement
    //By TFT: required by modified version of MLX90640_GetFrameData
//     unsigned short controlReg;
//     if(MLX90640_I2CRead(MLX90640_address, 0x800D, 1, &controlReg) == 0)
//     {
//         controlReg |= 1<<2; //Set bit 2, transfer data only when enable overwrite
//         if(MLX90640_I2CWrite(MLX90640_address, 0x800D, controlReg) != 0)
//             puts("Error writing control reg");
//     } else puts("Error reading control reg");
    
    {
        uint16_t eeMLX90640[832]; //Temporary buffer
        if(MLX90640_DumpEE(MLX90640_address, eeMLX90640)) puts("Err 1");
        if(MLX90640_ExtractParameters(eeMLX90640, &mlx90640)) puts("Err 2");
    }
    //Note: true rate is half because that's the rate for one half frame
    //MLX90640_SetRefreshRate(MLX90640_address, 0b010); //Set rate to 2Hz
    MLX90640_SetRefreshRate(MLX90640_address, 0b011); //Set rate to 4Hz
    //MLX90640_SetRefreshRate(MLX90640_address, 0b100); //Set rate to 8Hz
    //MLX90640_SetRefreshRate(MLX90640_address, 0b101); //Set rate to 16Hz
    //MLX90640_SetRefreshRate(MLX90640_address, 0b111); //Set rate to 64Hz
    
    for(;;)
    {
        auto t1 = getTime();
        // Get temperature map
        for(int i = 0; i < 2; i++) //Read both subpages
        {
            uint16_t mlx90640Frame[834]; //Temporary buffer
            int frameNo = MLX90640_GetFrameData(MLX90640_address, mlx90640Frame);
            iprintf("%d ", frameNo);

            float vdd = MLX90640_GetVdd(mlx90640Frame, &mlx90640); //Unused?
            float Ta = MLX90640_GetTa(mlx90640Frame, &mlx90640);

            float tr = Ta - ta_shift; //Reflected temperature based on the sensor ambient temperature
            float emissivity = 0.95;

            MLX90640_CalculateTo(mlx90640Frame, &mlx90640, emissivity, tr, temperature);
        }
        puts("");
        auto t2 = getTime();
        
        //Draw temperature map
        float minVal = temperature[0], maxVal = temperature[0];
        for(int i = 1; i < nx * ny; i++)
        {
            minVal = min(minVal, temperature[i]);
            maxVal = max(maxVal, temperature[i]);
        }
        float range = max(minRange, maxVal - minVal);
        
        {
            DrawingContext dc(display);
            for(int y = 0; y < (2 * ny) - 1; y++)
            {
                for(int x = 0; x < (2 * nx) - 1; x++)
                {
                    Color color = interpolate2d(x, y, minVal, range);
                    dc.clear(Point(x * pixSize, y * pixSize),
                             Point(((x + 1) * pixSize) - 1, ((y + 1) * pixSize) - 1),
                             color);
                }
            }
            dc.setFont(droid21);
            dc.write(Point(0, 2 * ny * pixSize),
                       to_string(static_cast<int>(minVal + .5f)) + " "
                     + to_string(static_cast<int>(maxVal + .5f)) + "      ");
        }
        
        auto t3 = getTime();
        
        //With 2Hz speed, reading takes ~1000ms, drawing takes ~4ms
        //With a logic analyzer, some more breakdown:
        //Reading half a frame takes:
        //- 358ms long I2C read that's polling waiting for the frame to start
        //- 21ms computation?
        //- 68ms short I2C read full frame at ~230kHz
        //- 46ms computation
        //4Hz is good, 8Hz is a bit too fast
        //8Hz the slack is 6ms for one half frame, and actually negative for the
        //second half
        iprintf("read time = %lld draw time = %lld\n", t2 - t1, t3 - t2);
        
        //Print temperature map
//         for(int i = 0; i < 768; i++) printf("%.0f ", temperature[i]);
//         puts("");
        
        if(on_btn::value()==1) break;
    }
}

int main()
{
    keep_on::mode(Mode::OUTPUT);
    keep_on::high();
    on_btn::mode(Mode::INPUT_PULL_DOWN);
    up_btn::mode(Mode::INPUT_PULL_UP);
    
    auto& display=DisplayManager::instance().getDisplay();
    {
        DrawingContext dc(display);
        dc.setFont(tahoma);
        dc.write(Point(0,0),"Miosix thermal camera");
        dc.write(Point(0,15),"experimental firmware");
    }
    
    while(on_btn::value()==1) Thread::sleep(100);
    Thread::sleep(200);
    
    run(display);
    
//     for(;;)
//     {
//         Thread::sleep(100);
//         if(up_btn::value()==0) puts("UP button");
//         if(on_btn::value()==1)
//         {
//             puts("ON button, bye");
//             break;
//         }
//     }
    
    display.turnOff();
    keep_on::low();
    for(;;) Thread::sleep(1);
}
