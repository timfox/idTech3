#!/bin/bash
# Script to run Quake 3 Arena (base game)

echo "Starting Quake 3 Arena..."
cd /home/tim/Desktop/idtech3/release

# Run base game in windowed mode
./idtech3.x86_64 +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 0

echo ""
echo "Quake 3 Arena exited."