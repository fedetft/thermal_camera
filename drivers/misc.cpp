
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

void waitPowerButtonReleased()
{
    while(on_btn::value()==1) Thread::sleep(100);
    Thread::sleep(50);
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
