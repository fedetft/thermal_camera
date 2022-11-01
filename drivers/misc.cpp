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

#include "misc.h"
#include "hwmapping.h"
#include <interfaces/arch_registers.h>

using namespace miosix;

void initializeBoard()
{
    {
        FastInterruptDisableLock dLock;
        RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
        RCC_SYNC();
        up_btn::mode(Mode::INPUT_PULL_UP);
        on_btn::mode(Mode::INPUT_PULL_DOWN);
        keep_on::mode(Mode::OUTPUT);
        keep_on::high();
        vbatt_sense::mode(Mode::INPUT_ANALOG);
        flash_cs::mode(Mode::OUTPUT);
        flash_cs::high();
    }
    ADC1->CR1=0;
    ADC1->CR2=0; //Keep the ADC OFF to save power
    ADC1->SMPR2=ADC_SMPR2_SMP1_2; //Sample for 85 cycles channel 1 (battery)
    ADC1->SQR1=0; //Do only one conversion
    ADC1->SQR2=0;
    ADC1->SQR3=1; //Convert channel 1 (battery voltage)
}

void shutdownBoard()
{
    Thread::sleep(50);
    keep_on::low();
    for(;;) Thread::sleep(1);
}

int getBatteryVoltage()
{
    ADC1->CR2=ADC_CR2_ADON; //Turn ADC ON
    delayUs(3); //Power up time
    ADC1->CR2 |= ADC_CR2_SWSTART; //Start conversion
    while((ADC1->SR & ADC_SR_EOC)==0) ; //Wait for conversion
    int result=ADC1->DR; //Read result
    ADC1->CR2=0; //Turn ADC OFF
    //return the voltage iv V multiplied by 10, so 40=4V
    return result/62; //4096/2/Vcc/10=62, Vcc=3.3V
}

BatteryLevel batteryLevel(int voltage)
{
    if(voltage>38) return BatteryLevel::B100;
    if(voltage>37) return BatteryLevel::B75;
    if(voltage>36) return BatteryLevel::B50;
    if(voltage>35) return BatteryLevel::B25;
    return BatteryLevel::B0;
}
