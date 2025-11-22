#!/bin/bash

# Compile Game Script for id Tech 3 mod

set -e

# Get absolute paths BEFORE changing directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
BUILD_DIR="$PROJECT_ROOT/build"
MOD_DIR="$BUILD_DIR/mymod"

echo "Building game modules..."
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"
echo "Mod directory: $MOD_DIR"

# Navigate to the game mod build scripts directory
cd "$PROJECT_ROOT/mymod/gamesrc/build"

# Run the build process
make

# Copy libraries to mod directory where engine expects them
# The engine looks for files at: fs_basepath/mymod/uix86_64.so
# fs_basepath defaults to the build/ directory
echo "Copying libraries to mod directory..."

mkdir -p "$MOD_DIR"

# Copy from vm/ directory (where Makefile outputs them)
# We're currently in mymod/gamesrc/build, so vm is at ../../vm
VM_DIR="../../vm"
if [ -d "$VM_DIR" ]; then
    echo "Found libraries in $VM_DIR"
    echo "Copying to $MOD_DIR/"
    cp -v "$VM_DIR"/*.so "$MOD_DIR/" 2>&1 || true
else
    echo "Warning: $VM_DIR not found (current dir: $(pwd))"
fi

# Verify files were copied
echo ""
if [ -d "$MOD_DIR" ]; then
    SO_COUNT=$(ls -1 "$MOD_DIR"/*.so 2>/dev/null | wc -l)
    if [ "$SO_COUNT" -gt 0 ]; then
        echo "Libraries copied to $MOD_DIR/:"
        ls -lh "$MOD_DIR"/*.so
    else
        echo "Warning: No .so files copied to $MOD_DIR/"
        echo "  Check if libraries were built in: $VM_DIR"
    fi
fi

echo ""
echo "Game mod build completed!"
echo "  Libraries: build/mymod/*.so"
echo "  Source: mymod/gamesrc/"
