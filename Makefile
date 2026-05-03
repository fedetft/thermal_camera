##
## Makefile template for Miosix embedded OS kernel-side applications
##

## Path to the Miosix kernel root (miosix-kernel/miosix)
KPATH := miosix-kernel/miosix

## Path to the Miosix config root, containing:
##  - config/Makefile.inc
##  - config/miosix_config.h (optional)
##  - config/board/<board name>/board_config.h (optional)
## Leave it as $(KPATH) to use the default settings built in to miosix. Any
## missing file is replaced by the defaults as well.
##   To change the config, copy and paste the miosix/config directory to
## your project root and change this path to the project root (typically ".")
CONFPATH := .

## When using CONFPATH := $(KPATH) you can choose the board here. You can also
## override other variables defined by Makefile.inc.
# OPT_BOARD := [options in miosix-kernel/miosix/config/Makefile.inc]

## Include the Miosix common makefile for kernel-side applications
MAKEFILE_VERSION := 3.01
include $(KPATH)/Makefile.kcommon

##
## List here your source files (both .s, .c and .cpp)
##
SRC :=                                             \
main.cpp application.cpp renderer.cpp colormap.cpp \
version.cpp                                        \
drivers/display_er_oledm015.cpp drivers/misc.cpp   \
drivers/mlx90640.cpp drivers/MLX90640_API.cpp      \
drivers/flash.cpp drivers/options_save.cpp         \
drivers/usb_tinyusb.cpp

IMG :=  \
images/batt0icon.png \
images/batt25icon.png \
images/batt50icon.png \
images/batt75icon.png \
images/batt100icon.png \
images/miosixlogoicon.png \
images/emissivityicon.png \
images/smallcelsiusicon.png \
images/largecelsiusicon.png \
images/pauseicon.png \
images/usbicon.png

SRC2 := $(IMG:.png=.cpp)
# Images should be compiled first to prevent missing includes
SRC := $(SRC2) $(SRC)
%.cpp : %.png
	./mxgui/_tools/code_generators/build/pngconverter --in $< --depth 16

# This prevents make from deleting the intermediate .cpp files
.PRECIOUS: $(SRC2)

# Consider the version number file to be always outdated
.PHONY: version.cpp
# Dynamically get the version from git
VERSION := $(shell if ! git describe --always 2> /dev/null; then echo 0000000; fi)

##
## List here additional include directories (in the form -Iinclude_dir)
##
INCLUDE_DIRS := -I. -I./mxgui -I./tinyusb/tinyusb/src -I./tinyusb

##
## List here additional static libraries with relative path
##
LIBS := mxgui/libmxgui.a tinyusb/libtinyusb.a

##
## List here subdirectories which contains makefiles
##
SUBDIRS += mxgui tinyusb

##
## Attach a romfs filesystem image after the kernel
##
ROMFS_DIR :=

all: $(if $(ROMFS_DIR), image, main)

main: $(OBJ) all-recursive
	$(ECHO) "[LD  ] main.elf"
	$(Q)$(CXX) $(LFLAGS) -o main.elf $(OBJ) $(LINK_LIBS)
	$(ECHO) "[CP  ] main.hex"
	$(Q)$(CP) -O ihex   main.elf main.hex
	$(ECHO) "[CP  ] main.bin"
	$(Q)$(CP) -O binary main.elf main.bin
	$(Q)$(SZ) main.elf

clean: clean-recursive
	$(Q)rm -f $(OBJ) $(OBJ:.o=.d) main.elf main.hex main.bin main.map

-include $(OBJ:.o=.d)
