##
## Makefile for tinyusb Miosix integration
## This makefile builds libtinyusb.a
##
MAKEFILE_VERSION := 1.10
GCCMAJOR := $(shell arm-miosix-eabi-gcc --version | \
                    perl -e '$$_=<>;/\(GCC\) (\d+)/;print "$$1"')
## KPATH and CONFPATH are forwarded by the parent Makefile
include $(CONFPATH)/config/Makefile.inc

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

ifeq ("$(VERBOSE)","1")
Q := 
ECHO := @true
else
Q := @
ECHO := @echo
endif

INCLUDE_DIRS := -I. -I./cmsis_stubs -I./tinyusb/src -I$(CONFPATH)/config

## The name of the library generated by this makefile
TARGET:=libtinyusb.a

## Replaces both "foo.cpp"-->"foo.o" and "foo.c"-->"foo.o"
OBJ := $(addsuffix .o, $(basename $(SRC)))

## Includes the miosix base directory for C/C++
CXXFLAGS := $(CXXFLAGS_BASE) -I$(CONFPATH) -I$(CONFPATH)/config/$(BOARD_INC) \
            -I$(KPATH) -I$(KPATH)/arch/common -I$(KPATH)/$(ARCH_INC)     \
            -I$(KPATH)/$(BOARD_INC) $(INCLUDE_DIRS)
CFLAGS   := $(CFLAGS_BASE)   -I$(CONFPATH) -I$(CONFPATH)/config/$(BOARD_INC) \
            -I$(KPATH) -I$(KPATH)/arch/common -I$(KPATH)/$(ARCH_INC)     \
            -I$(KPATH)/$(BOARD_INC) $(INCLUDE_DIRS)
AFLAGS   := $(AFLAGS_BASE)
DFLAGS   := -MMD -MP

.PHONY: all
all: $(TARGET)

.PHONY: clean
clean:
	rm -f $(OBJ) $(TARGET) $(OBJ:.o=.d)

## Build libtinyusb.a
$(TARGET): $(OBJ)
	$(ECHO) "[AR  ] $(TARGET)"
	$(Q)$(AR) rcs $(TARGET) $(OBJ)

%.o: %.s
	$(ECHO) "[AS  ] $<"
	$(Q)$(AS) $(AFLAGS) $< -o $@

# We need to compile all .c files with the C++ compiler for integrating TinyUSB
# without becoming crazy 
%.o : %.c
	$(ECHO) "[CXX ] $<"
	$(Q)$(CXX) $(DFLAGS) $(CXXFLAGS) $< -o $@

%.o : %.cpp
	$(ECHO) "[CXX ] $<"
	$(Q)$(CXX) $(DFLAGS) $(CXXFLAGS) $< -o $@

#pull in dependecy info for existing .o files
-include $(OBJ:.o=.d)
