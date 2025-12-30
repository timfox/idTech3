#!/bin/bash

# Test script for Vulkan renderer startup
# This script tests if the Vulkan renderer can initialize properly

echo "Testing Vulkan renderer startup..."

# Set environment variables for Vulkan debugging
export VK_LOADER_DEBUG=all
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation

# Set basic Quake 3 parameters
export SDL_VIDEODRIVER=x11

# Run with minimal parameters to test Vulkan initialization
timeout 30s ./idtech3.x86_64 \
    +set fs_game mymod \
    +set cl_renderer vulkan \
    +set r_mode 6 \
    +set r_vkDevice 0 \
    +set r_fullscreen 0 \
    +set r_windowed 1 \
    +set com_error 1 \
    +set com_developer 1 \
    +set r_developer 1 \
    +set developer 1 \
    +set r_showFPS 0 \
    +set cl_showTime 0 \
    +set timedemo 0 \
    +set demo_play 0 \
    +quit

echo "Vulkan startup test completed."