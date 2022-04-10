/**
   @copyright (C) 2017 Melexis N.V.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.

*/

/*
 * Modified by TFT: replaced by stubs as this API does not lend itself to
 * a DMA implementation. Reading from the sensor has been replaced by an
 * optimized driver in mlx90640.h
 */

#ifndef _MLX90640_I2C_Driver_H_
#define _MLX90640_I2C_Driver_H_

#include <stdint.h>

inline void MLX90640_I2CInit(void) {}
inline int MLX90640_I2CRead(uint8_t slaveAddr, unsigned int startAddress, unsigned int nWordsRead, uint16_t *data) {return -1; }
inline int MLX90640_I2CWrite(uint8_t slaveAddr, unsigned int writeAddress, uint16_t data) { return -1; }
inline void MLX90640_I2CFreqSet(int freq) {}

#endif
