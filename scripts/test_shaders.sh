#!/bin/bash

cd /home/tim/Desktop/idtech3

echo "Testing Vulkan renderer with SPIR-V shaders..."

# Check if shaders exist
echo "Checking for SPIR-V shader files:"
ls -la mods/mymod/shaders/*.spv 2>/dev/null || echo "No SPIR-V files found in mymod/shaders/"

# Run for a short time to test shader loading
echo "Running Vulkan renderer test..."
timeout 6s ./release/idtech3.x86_64 +set fs_game mymod +set cl_renderer vulkan +set r_fullscreen 0 +set r_mode 4 2>&1 | grep -E "fallback|shader|SPIR-V|Successfully|loaded|Created fallback" | head -10

echo ""
echo "Shader test completed."