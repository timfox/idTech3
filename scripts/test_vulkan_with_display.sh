#!/usr/bin/env bash
set -euo pipefail

# Test Vulkan renderer with virtual display
echo "Setting up virtual display for Vulkan testing..."

# Use the real display (:0) for Vulkan surface creation
export DISPLAY=:0

echo "Using real display for Vulkan testing..."

# Test the engine with Vulkan renderer
cd /home/tim/Desktop/idtech3/release
timeout 15s ./idtech3.x86_64 \
  +set fs_game mymod \
  +set cl_renderer vulkan \
  +set r_vkValidation 0 \
  +set r_vkRayTracing 0 \
  +set r_vkMeshShaders 0 \
  +set com_developer 1 \
  +set r_developer 1 \
  +set r_fullscreen 0 \
  +set r_windowed 1 \
  +set r_width 800 \
  +set r_height 600 \
  +set r_showFPS 1 \
  +set com_maxfps 10 \
  +map q3dm1 \
  2>&1

echo "Test completed."