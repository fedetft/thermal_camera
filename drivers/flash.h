/***************************************************************************
 *   Copyright (C) 2012-2022 by Terraneo Federico                          *
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

#include <mutex>

/**
 * Class to access a 25Q64 FLASH memory
 */
class Flash
{
public:
    /**
     * Singleton
     * \return an instance of the Flash class
     */
    static Flash& instance();
    
    /**
     * \return the FLASH's size in bytes
     */
    unsigned int size() const { return 8*1024*1024; }

    /**
     * \return the FLASH's page size (maximum programmable unit) in bytes
     */
    unsigned int pageSize() const { return 256; }

    /**
     * \return the FLASH's sector size (minimum erasable unit) in bytes
     */
    unsigned int sectorSize() const { return 4*1024; }

    /**
     * \return the FLASH's block size (maximum erasable unit) in bytes
     */
    unsigned int blockSize() const { return 64*1024; }

    /**
     * Erase a memory sector
     * \para addr address of sector to erase
     */
    void eraseSector(unsigned int addr);

    /**
     * Erase a memory block
     * \param add address of block to erase
     */
    void eraseBlock(unsigned int addr);
    
    /**
     * Write a block of data into the flash.
     * NOTE: data to be written needs to fit within a page boundary.
     * \param addr start address into the flash where the data block will be
     * written
     * \param data data block
     * \param size data block size
     * \return true on success, false on failure
     */
    bool write(unsigned int addr, const void *data, int size);
    
    /**
     * Read a block of data from the flash
     * \param addr start address into the flash where the data block will be read
     * \param data data block
     * \param size data block size
     * \return true on success, false on failure
     */
    bool read(unsigned int addr, void *data, int size);
    
private:
    Flash();
    
    Flash(const Flash&)=delete;
    Flash& operator= (const Flash&)=delete;

    /**
     * Send the write enable command, required before writing/erasing.
     * Requires the mutex to be locked.
     */
    void writeEnable();

    /**
     * Read one of the status registers.
     * Requires the mutex to be locked.
     * \param reg 1 for status register 1, 2 for status register 2
     */
    unsigned short readStatus(int reg=1);
    
    std::mutex m; ///Mutex to protect concurrent access to the hardware
};
