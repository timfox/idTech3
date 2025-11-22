#!/bin/bash

# Compile Game Script for id Tech 3 mod

set -e

echo "Building game modules..."

# Navigate to the game mod build scripts directory
cd mymod/scripts/build

# Run the build process
make

# Copy libraries to mod directory where engine expects them
echo "Copying libraries to mod directory..."
MOD_DIR="../../build/mymod"
mkdir -p "$MOD_DIR"

# Copy from vm/ directory (where CMake outputs them)
# From mymod/scripts/build, vm is at ../../vm
VM_DIR="../../vm"
if [ -d "$VM_DIR" ]; then
    echo "Found libraries in $VM_DIR"
    cp -v "$VM_DIR"/*.so "$MOD_DIR/" 2>&1
else
    echo "Warning: $VM_DIR not found (current dir: $(pwd))"
fi

# Verify files were copied
echo ""
if [ -d "$MOD_DIR" ]; then
    SO_COUNT=$(ls -1 "$MOD_DIR"/*.so 2>/dev/null | wc -l)
    if [ "$SO_COUNT" -gt 0 ]; then
        echo "Libraries copied to build/mymod/:"
        ls -lh "$MOD_DIR"/*.so
    else
        echo "Warning: No .so files copied to build/mymod/"
        echo "  Check if libraries were built in: $VM_DIR"
    fi
fi

echo ""
echo "Game mod build completed!"
echo "  Libraries: build/mymod/*.so"
echo "  Source: mymod/scripts/"
