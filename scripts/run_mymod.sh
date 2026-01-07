#!/bin/bash
# Script to run the custom mymod game

echo "Starting MyMod..."
cd /home/tim/Desktop/idtech3/release

# Run MyMod in windowed mode
./idtech3.x86_64 +set fs_game mymod +set r_mode 6 +set r_fullscreen 0 +set r_vulkan 0

echo ""
echo "MyMod exited."