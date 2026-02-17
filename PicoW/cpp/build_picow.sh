#!/bin/bash
# Build script for Raspberry Pi Pico W (RP2040)

echo "Building for Pico W..."

mkdir -p build_picow
cd build_picow

# Configure for Pico W
cmake -DPICO_BOARD=pico_w ..

# Build
make -j4

echo "Build complete. Check build_picow/ZwiftPowerLighting.uf2"
