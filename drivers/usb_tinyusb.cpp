/***************************************************************************
 *   Copyright (C) 2023 by Daniele Cattaneo                                *
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

#include "usb_tinyusb.h"
#include "tusb.h"
#include "hwmapping.h"
#include "interfaces/arch_registers.h"

using namespace miosix;

FastMutex txMutex;
ConditionVariable txComplete;

FastMutex rxMutex;
ConditionVariable rxAvail;

USBCDC::USBCDC(miosix::Priority intSvcThreadPrio)
{
    {
        FastInterruptDisableLock dLock;
        RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;
        RCC_SYNC();
        usb_dm::alternateFunction(10);
        usb_dp::alternateFunction(10);
        usb_dm::mode(Mode::ALTERNATE);
        usb_dp::mode(Mode::ALTERNATE_OD);
        //Disable VBus sensing
        USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_NOVBUSSENS;
        USB_OTG_FS->GCCFG &= ~USB_OTG_GCCFG_VBUSBSEN;
        USB_OTG_FS->GCCFG &= ~USB_OTG_GCCFG_VBUSASEN;
    }
    bool r=tusb_init();
    if (!r)
    {
        iprintf("USB initialization error\n");
        return;
    }
    tinyusbThread=Thread::create(USBCDC::thread,2048U,intSvcThreadPrio,nullptr,0);
    iprintf("USB initialization OK, thread %p\n",reinterpret_cast<void*>(tinyusbThread));
}

USBCDC::~USBCDC()
{
    if (tinyusbThread)
        tinyusbThread->terminate();
}

void *USBCDC::thread(void *unused)
{
    while (!Thread::testTerminate()) tud_task();
    iprintf("tinyUSB thread min free stack %d\n",
            MemoryProfiling::getAbsoluteFreeStack());
    return nullptr;
}

bool USBCDC::connected()
{
    return tud_ready();
}

void USBCDC::putChar(char c)
{
    write((uint8_t *)&c, 1);
}

void USBCDC::write(const uint8_t *buf, int size)
{
    Lock<FastMutex> lock(txMutex);
    if (forceEOF)
        return;

    int sent = tud_cdc_n_write(0, buf, size);
    size -= sent;
    buf += sent;
    while (size > 0) {
        txComplete.wait(lock);
        if (forceEOF) return;
        sent = tud_cdc_n_write(0, buf, size);
        size -= sent;
        buf += sent;
    } while (size > 0);

    if (tud_cdc_n_write_flush(0) > 0) {
        txComplete.wait(lock);
    }
}

bool USBCDC::write(const uint8_t *buf, int size, long long maxTime)
{
    Lock<FastMutex> lock(txMutex);
    if (forceEOF)
        return false;
    long long timeout = getTime() + maxTime;

    int sent = tud_cdc_n_write(0, buf, size);
    size -= sent;
    buf += sent;
    while (size > 0) {
        if (txComplete.timedWait(lock, timeout) == TimedWaitResult::Timeout || forceEOF)
            return false;
        sent = tud_cdc_n_write(0, buf, size);
        size -= sent;
        buf += sent;
    }

    if (tud_cdc_n_write_flush(0) > 0) {
        if (txComplete.timedWait(lock, timeout) == TimedWaitResult::Timeout || forceEOF)
            return false;
    }
    return true;
}

void USBCDC::print(const char *str)
{
    int len = strlen(str);
    write(reinterpret_cast<const uint8_t *>(str), len);
}

bool USBCDC::print(const char *str, long long maxTime)
{
    int len = strlen(str);
    return write(reinterpret_cast<const uint8_t *>(str), len, maxTime);
}

bool USBCDC::waitForInputUnlocked(miosix::Lock<miosix::FastMutex>& lock, long long deadline)
{
    while (tud_cdc_n_available(0) == 0 && !forceEOF) {
        TimedWaitResult status = rxAvail.timedWait(lock, deadline);
        if (status == TimedWaitResult::Timeout)
            break;
    }
    return forceEOF ? false : tud_cdc_n_available(0) != 0;
}

bool USBCDC::waitForInput(long long maxWait)
{
    long long deadline = getTime() + maxWait;
    Lock<FastMutex> lock(rxMutex);
    return waitForInputUnlocked(lock, deadline);
}

int USBCDC::getCharUnlocked(miosix::Lock<miosix::FastMutex>& lock)
{
    while (!tud_cdc_n_available(0) && !forceEOF)
        rxAvail.wait(lock);
    return forceEOF ? -1 : tud_cdc_n_read_char(0);
}

int USBCDC::getChar()
{
    Lock<FastMutex> lock(rxMutex);
    return getCharUnlocked(lock);
}

bool USBCDC::readLine(char *buf, int size)
{
    if (size == 0 || forceEOF)
        return false;
    Lock<FastMutex> lock(rxMutex);
    int c = getCharUnlocked(lock);
    while (c != -1 && c != '\r' && c != '\n') {
        *buf++ = static_cast<char>(c);
        size--;
        if (size == 1) {
            *buf++ = '\0';
            return false;
        }
        c = getCharUnlocked(lock);
    }
    *buf++ = '\0';
    return c != -1;
}

void USBCDC::prepareShutdown()
{
    tud_disconnect();
    forceEOF = true;
    __DMB();
    {
        Lock<FastMutex> lock(rxMutex);
        rxAvail.signal();
    }
    {
        Lock<FastMutex> lock(txMutex);
        txComplete.signal();
    }
    tinyusbThread->terminate();
}

//
// Interrupt handlers to be forwarded to TinyUSB
//

__attribute__((naked)) void OTG_FS_IRQHandler(void)
{
    saveContext();
    tud_int_handler(0);
    restoreContext();
}

__attribute__((naked)) void OTG_HS_IRQHandler(void)
{
    saveContext();
    tud_int_handler(1);
    restoreContext();
}

//
// TinyUSB callbacks
//

extern "C" void tud_mount_cb(void)
{
    iprintf("USB mounted\n");
}

extern "C" void tud_umount_cb(void)
{
    iprintf("USB unmounted\n");
}

extern "C" void tud_suspend_cb(bool remote_wakeup_en)
{
    iprintf("USB suspended\n");
}

extern "C" void tud_resume_cb(void)
{
    iprintf("USB resumed\n");
}

extern "C" void tud_cdc_tx_complete_cb(uint8_t itf)
{
    Lock<FastMutex> lock(txMutex);
    txComplete.signal();
}

extern "C" void tud_cdc_rx_cb(uint8_t itf)
{
    Lock<FastMutex> lock(rxMutex);
    rxAvail.signal();
}

enum USBStringDescID {
    LocaleIDs = 0,
    None = 0,
    Manufacturer,
    Product,
    Serial
};

// Called when the device descriptor is requested from the host.
uint8_t const *tud_descriptor_device_cb(void)
{
    static const uint16_t vid = 0xcafe;
    // Some OSes (Windows...) remember device configuration based on vid/pid,
    // so if the set of supported interfaces changes, the pid should also
    // change.
    static const uint16_t pid = 0x4001;
    static const uint16_t version = 0x0100;
    static const tusb_desc_device_t desc_device = {
        .bLength = sizeof(tusb_desc_device_t),
        .bDescriptorType = TUSB_DESC_DEVICE,
        .bcdUSB = 0x0200, // Descriptor version number

        // Use Interface Association Descriptor (IAD) for CDC
        //   As required by USB Specs IAD's subclass must be common class (2)
        // and protocol must be IAD (1)
        .bDeviceClass = TUSB_CLASS_MISC,
        .bDeviceSubClass = MISC_SUBCLASS_COMMON,
        .bDeviceProtocol = MISC_PROTOCOL_IAD,

        .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,

        .idVendor = vid,
        .idProduct = pid,
        .bcdDevice = version, // Device version number

        .iManufacturer = USBStringDescID::Manufacturer,
        .iProduct = USBStringDescID::Product,
        .iSerialNumber = USBStringDescID::Serial,

        .bNumConfigurations = 0x01
    };
    return reinterpret_cast<uint8_t const *>(&desc_device);
}

// Called when the configuration descriptor is requested from the host.
//   TinyUSB will automatically configure and instantiate the appropriate USB
// drivers depending on the descriptor, therefore this is the main source of
// truth for things like endpoint IDs and so on.
//   Descriptor contents must exist long enough for transfer to complete.
uint8_t const *tud_descriptor_configuration_cb(uint8_t index)
{
    enum USBInterfaceID {
        CDC = 0,
        CDCData, // Implicitly added by the TUD_CDC_DESCRIPTOR macro
        Total
    };
    enum USBEndpointID {
        CDCOut = 0x02,
        CDCNotif = 0x81,
        CDCIn = 0x82
    };
    static const size_t length = TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN;
    static uint8_t const desc_fs_configuration[length] = {
        // Configuration descriptor
        TUD_CONFIG_DESCRIPTOR(
            1,                          // config number
            USBInterfaceID::Total,      // interface count
            USBStringDescID::None,      // string index
            length,                     // total length
            0x00,                       // attribute
            100                         // power in mA
        ),
        // Interface descriptor (macro sets class/subclass automatically)
        TUD_CDC_DESCRIPTOR(
            USBInterfaceID::CDC,        // Interface number
            USBStringDescID::None,      // string index
            USBEndpointID::CDCNotif,    // notification endpoint address
            8,                          // notification endpoint size
            USBEndpointID::CDCOut,      // data out endpoint address
            USBEndpointID::CDCIn,       // data in endpoint address
            64                          // data endpoint size
        ),
    };
    return desc_fs_configuration;
}

struct __attribute__((packed)) STM32DeviceIDFields
{
    uint16_t waferX;
    uint16_t waferY;
    uint8_t wafer;
    char lot[7];
};
#define STM32_DEVICE_ID (reinterpret_cast<STM32DeviceIDFields*>(UID_BASE))

template <size_t L>
struct USBStringDesc
{
    uint8_t size;
    uint8_t type = TUSB_DESC_STRING;
    char16_t str[L];
};

// Invoked when host requests each string descriptor.
// Application returns a pointer to descriptor, whose contents must exist long
// enough for transfer to complete.
uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
    static const size_t descMaxStrLen=31;
    static USBStringDesc<descMaxStrLen> desc;
    uint8_t length;

    if(index==USBStringDescID::LocaleIDs) // Locale ID
    {
        desc.str[0]=0x0409; // US English
        length=1;
    } else {
        const char *str;
        const int chipid_length = 7+2+4+4;
        char chipid_buf[chipid_length+1];

        if(index == USBStringDescID::Manufacturer) str = "TinyUSB";
        else if(index == USBStringDescID::Product) str = "Thermal Camera";
        else if(index == USBStringDescID::Serial) { // Serial
            for(int i=0; i<7; i++) chipid_buf[i] = STM32_DEVICE_ID->lot[i];
            siprintf(chipid_buf+7, "%02X%04X%04X",
                STM32_DEVICE_ID->wafer,
                STM32_DEVICE_ID->waferX,
                STM32_DEVICE_ID->waferY);
            str = chipid_buf;
        } else {
            // Do not return anything for other indexes (for example 0xEE which
            // is Windows-proprietary)
            return NULL;
        }
        // copy string into descriptor buffer, lazily converting it to UTF-16
        length = (uint8_t)strlen(str);
        if(length > descMaxStrLen) length = descMaxStrLen;
        for(int i=0; i<length; i++)
            desc.str[i] = str[i];
    }
    desc.size = 2 * length + 2;
    return reinterpret_cast<uint16_t *>(&desc);
}
