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

#include <cstring>
#include <cassert>
#include <memory>
#include <drivers/options_save.h>
#include <drivers/flash.h>
#include <util/crc16.h>
#include <util/util.h>

using namespace std;
using namespace miosix;

struct Header
{
    unsigned char written;     //0x00 if written,     0xff if not written
    unsigned char invalidated; //0x00 if invalidated, 0xff if not invalidated
    unsigned short crc;
};

void loadOptions(void *options, int optionsSize)
{
    puts("loadOptions");
    auto& flash=Flash::instance();

    unsigned int size=optionsSize+sizeof(Header);
    assert(size<flash.pageSize());
    auto buffer=make_unique<unsigned char[]>(size);
    auto *header=reinterpret_cast<Header*>(buffer.get());

    for(unsigned int i=0;i<flash.sectorSize();i+=flash.pageSize())
    {
        if(flash.read(i,buffer.get(),size)==false)
        {
            iprintf("Failed to read address 0x%x\n",i);
            break; //Read error, abort
        }
        //memDump(buffer.get(),size);
        if(header->written!=0)
        {
            iprintf("Unwritten option @ address 0x%x\n",i);
            break; //Reached unwritten, abort
        }
        if(header->invalidated==0)
        {
            iprintf("Invalidated option @ address 0x%x\n",i);
            continue; //Skip invalidated entry
        }
        if(header->crc!=crc16(buffer.get()+sizeof(Header),optionsSize))
        {
            iprintf("Corrupted option @ address 0x%x\n",i);
            continue; //Corrupted
        }
        memcpy(options,buffer.get()+sizeof(Header),optionsSize);
        iprintf("Loaded options from address 0x%x\n",i);
        break;
    }
}

void saveOptions(void *options, int optionsSize)
{
    puts("saveOptions");
    auto& flash=Flash::instance();

    unsigned int size=optionsSize+sizeof(Header);
    assert(size<flash.pageSize());
    auto buffer=make_unique<unsigned char[]>(size);
    auto *header=reinterpret_cast<Header*>(buffer.get());

    int found=-1;
    for(unsigned int i=0;i<flash.sectorSize();i+=flash.pageSize())
    {
        if(flash.read(i,buffer.get(),size)==false)
        {
            iprintf("Failed to read address 0x%x\n",i);
            return;
        }
        if(header->invalidated==0) continue;
        if(header->written==0)
        {
            //Found valid written data
            if(memcmp(buffer.get()+sizeof(Header),options,optionsSize)==0)
            {
                puts("Options did not change, not saving");
                return;
            }
            //And it differs, invalidate
            iprintf("Invalidating entry @ address 0x%x\n",i);
            header->invalidated=0;
            if(flash.write(i,buffer.get(),sizeof(Header))==false)
            {
                iprintf("Failed to invalidate address 0x%x\n",i);
                return;
            }
            continue;
        }
        //Not invalidated and not written, we found one sector
        found=i;
        break;
    }
    if(found==-1)
    {
        puts("All entries full, erasing sector 0");
        flash.eraseSector(0);
        found=0;
    }
    header->written=0;
    header->invalidated=0xff;
    header->crc=crc16(options,optionsSize);
    memcpy(buffer.get()+sizeof(Header),options,optionsSize);
    iprintf("Writing options @ address 0x%x\n",found);
    if(flash.write(found,buffer.get(),size)==false) puts("Failed writing options");
}
