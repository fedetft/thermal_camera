
Thermal camera
===============

An open source thermal camera, including firmware, schematic and PCB design.

More documentation coming soon.


How to build
============

This guide assumes you already have a Miosix build environment installed, if not see
[here](https://miosix.org/wiki/index.php?title=Linux_Quick_Start).

```
# Submodules
git submodule init
git submodule update

# Fix Makefile version
sed -i 's/MAKEFILE_VERSION := 1.09/MAKEFILE_VERSION := 1.10/' mxgui/Makefile

# Code generators
cd mxgui/_tools/code_generators
chmod +x compile_freetype.sh
mkdir build
cd build
cmake ..
make

# Build
cd ../../../..
make -j
```
