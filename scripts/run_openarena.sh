#!/bin/bash
# Script to run OpenArena with the idTech3 engine

echo "Starting OpenArena..."
cd /home/tim/Desktop/idtech3/release

# Run OpenArena in windowed mode
./idtech3.x86_64 +set fs_game openarena +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 0

echo ""
echo "OpenArena exited."