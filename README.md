
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

How to flash
============

Install the dfu-util program on your pc.

Connect the USB cable between the PC an thermal camera, while the camera is off.

Use a jumper wire to connect BOOT0 (on the 4 pin header) to +3.3V (a convenient
spot is a capacitor next to the voltage regulator). You can just hold the wire
with your hands.

While the jumper is in place, press and hold the power on button of the camera.

You can disconnect the jumper but keep holding the power button. If you release
the power button, you need to reconnect the jumper and start again.

On the PC, run the command while holding the power button.
```
dfu-util -a 0 -s 0x08000000:leave -D main.bin
```

You should see a progress bar on the PC, keep holding the power button till the
progress bar reaches 100%.

Release the power button, and disconnect the USB cable.

Power on the camera and enjoy the new firmware. You cannot brick the camera,
if something goes wrong you can repeat the operation.
