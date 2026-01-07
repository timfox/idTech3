#!/bin/bash
# Script to run OpenArena with Vulkan renderer

echo "Starting OpenArena with Vulkan..."
cd /home/tim/Desktop/idtech3/release

# Run OpenArena with Vulkan renderer in windowed mode
./idtech3.x86_64 +set fs_game openarena +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 1

echo ""
echo "OpenArena (Vulkan) exited."