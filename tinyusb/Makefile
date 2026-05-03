##
## Makefile for tinyusb Miosix integration
## This makefile builds libtinyusb.a
##

## KPATH and CONFPATH are forwarded by the parent Makefile
MAKEFILE_VERSION := 3.01
include $(KPATH)/Makefile.kcommon

## List of all tinyusb source files (both .c and .cpp)
## These files will end up in libtinyusb.a
SRC := \
    tinyusb/src/portable/synopsys/dwc2/dcd_dwc2.c \
    tinyusb/src/common/tusb_fifo.c \
    tinyusb/src/class/cdc/cdc_rndis_host.c \
    tinyusb/src/class/cdc/cdc_host.c \
    tinyusb/src/class/cdc/cdc_device.c \
    tinyusb/src/class/video/video_device.c \
    tinyusb/src/class/net/ncm_device.c \
    tinyusb/src/class/net/ecm_rndis_device.c \
    tinyusb/src/class/dfu/dfu_rt_device.c \
    tinyusb/src/class/dfu/dfu_device.c \
    tinyusb/src/class/midi/midi_device.c \
    tinyusb/src/class/usbtmc/usbtmc_device.c \
    tinyusb/src/class/bth/bth_device.c \
    tinyusb/src/class/audio/audio_device.c \
    tinyusb/src/class/msc/msc_host.c \
    tinyusb/src/class/msc/msc_device.c \
    tinyusb/src/class/hid/hid_device.c \
    tinyusb/src/class/hid/hid_host.c \
    tinyusb/src/class/vendor/vendor_host.c \
    tinyusb/src/class/vendor/vendor_device.c \
    tinyusb/src/device/usbd_control.c \
    tinyusb/src/device/usbd.c \
    tinyusb/src/tusb.c

INCLUDE_DIRS := -I. -Icmsis_stubs -Itinyusb/src -I$(CONFPATH)/config

all: $(OBJ)
	$(ECHO) "[AR  ] libtinyusb.a"
	$(Q)$(AR) rcs libtinyusb.a $(OBJ)

clean:
	-rm -f $(OBJ) $(OBJ:.o=.d) libtinyusb.a

# We need to compile all .c files with the C++ compiler for integrating TinyUSB
# without becoming crazy
%.o : %.c
	$(ECHO) "[CXX ] $<"
	$(Q)$(CXX) $(CXXFLAGS) $< -o $@

-include $(OBJ:.o=.d)
