#!/bin/bash
# Build script for Raspberry Pi Pico 2 W (RP2350)

# Ensure SDK is capable (User must have Pico SDK v2.0.0+)
echo "Building for Pico 2 W..."

mkdir -p build_pico2w
cd build_pico2w

# Configure for Pico 2 W
cmake -DPICO_BOARD=pico2_w ..

# Build
make -j4

echo "Build complete. Check build_pico2w/ZwiftPowerLighting.uf2"
