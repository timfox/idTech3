#!/bin/bash

cd /home/tim/Desktop/idtech3

echo "Testing Vulkan renderer initialization..."

# Run for a short time and capture output
timeout 8s ./release/idtech3.x86_64 +set fs_game mymod +set cl_renderer vulkan +set r_fullscreen 0 +set r_mode 4 2>&1 | head -30

echo ""
echo "Test completed. Check above output for Vulkan initialization status."