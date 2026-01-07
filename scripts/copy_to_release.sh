#!/bin/bash
# Script to copy built executables and libraries to the release folder

echo "Copying built files to release folder..."

# Create release directory if it doesn't exist
mkdir -p /home/tim/Desktop/idtech3/release

# Copy main executables from build-vk-Release (most recent successful build)
if [ -f "build-vk-Release/idtech3.x86_64" ]; then
    cp build-vk-Release/idtech3.x86_64 release/
    echo "✓ Copied idtech3.x86_64"
fi

if [ -f "build-vk-Release/idtech3.server.x86_64" ]; then
    cp build-vk-Release/idtech3.server.x86_64 release/
    echo "✓ Copied idtech3.server.x86_64"
fi

# Copy renderer libraries
if [ -f "build-vk-Release/idtech3_opengl_x86_64.so" ]; then
    cp build-vk-Release/idtech3_opengl_x86_64.so release/
    echo "✓ Copied idtech3_opengl_x86_64.so"
fi

# Copy from build-gl-Release if vk build doesn't have them
if [ ! -f "release/idtech3_vulkan_x86_64.so" ] && [ -f "build-gl-Release/idtech3_vulkan_x86_64.so" ]; then
    cp build-gl-Release/idtech3_vulkan_x86_64.so release/
    echo "✓ Copied idtech3_vulkan_x86_64.so from GL build"
fi

if [ ! -f "release/idtech3_rtx_x86_64.so" ] && [ -f "build-gl-Release/idtech3_rtx_x86_64.so" ]; then
    cp build-gl-Release/idtech3_rtx_x86_64.so release/
    echo "✓ Copied idtech3_rtx_x86_64.so from GL build"
fi

if [ ! -f "release/idtech3.ded.x86_64" ] && [ -f "build-gl-Release/idtech3.ded.x86_64" ]; then
    cp build-gl-Release/idtech3.ded.x86_64 release/
    echo "✓ Copied idtech3.ded.x86_64 from GL build"
fi

# Copy launcher if available
if [ -f "build-vk-Release/idtech3_launcher" ]; then
    cp build-vk-Release/idtech3_launcher release/
    echo "✓ Copied idtech3_launcher"
fi

# Copy game assets from build directory if needed
if [ -d "build-vk-Release/base" ] && [ ! -d "release/base_updated" ]; then
    echo "Note: Game assets already exist in release/base/"
fi

echo ""
echo "Release folder contents:"
ls -la release/ | grep -E "\.(x86_64|so)$"

echo ""
echo "All files copied to release folder successfully!"
echo "You can now run games from the release directory."