
#pragma once

#include <miosix.h>

namespace miosix {

//Display
using oled_sck    = Gpio<GPIOA_BASE,5>;  //SPI1
using oled_mosi   = Gpio<GPIOA_BASE,7>;  //SPI1
using oled_cs     = Gpio<GPIOB_BASE,0>;
using oled_dc     = Gpio<GPIOC_BASE,5>;
using oled_res    = Gpio<GPIOC_BASE,4>;

//Sensor
using sen_scl     = Gpio<GPIOB_BASE,6>;  //I2C1
using sen_sda     = Gpio<GPIOB_BASE,7>;  //I2C1

//Bottons and power management
using up_btn      = Gpio<GPIOC_BASE,10>;
using on_btn      = Gpio<GPIOC_BASE,11>;
using keep_on     = Gpio<GPIOA_BASE,2>;
using vbatt_sense = Gpio<GPIOA_BASE,1>;  //ADC123_IN1

//USB
using usb_dm      = Gpio<GPIOA_BASE,11>; //OTG_FS_DM
using usb_dp      = Gpio<GPIOA_BASE,12>; //OTG_FS_DP
using usb_dm_2    = Gpio<GPIOC_BASE,8>;
using usb_dp_2    = Gpio<GPIOC_BASE,7>;

//FLASH
using flash_cs    = Gpio<GPIOB_BASE,12>; //SPI2
using flash_sck   = Gpio<GPIOB_BASE,13>; //SPI2
using flash_miso  = Gpio<GPIOB_BASE,14>; //SPI2
using flash_mosi  = Gpio<GPIOB_BASE,15>; //SPI2

//Serial
using boot_tx     = Gpio<GPIOA_BASE,9>;  //USART1
using boot_rx     = Gpio<GPIOA_BASE,10>; //USART1

} //namespace miosix
