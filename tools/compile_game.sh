#!/bin/bash

# Compile Game Script for id Tech 3 mods

set -e

# Determine absolute paths relative to /tools directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

MOD_NAME="${1:-mymod}"
MOD_ROOT="$PROJECT_ROOT/$MOD_NAME"
MOD_SOURCE_DIR="$MOD_ROOT/gamesrc"
MOD_BUILD_DIR="$MOD_SOURCE_DIR/build"
MOD_VM_DIR="$MOD_ROOT/vm"
RELEASE_MOD_DIR="$PROJECT_ROOT/release/$MOD_NAME"
RELEASE_VM_DIR="$RELEASE_MOD_DIR/vm"

if [ ! -d "$MOD_SOURCE_DIR" ]; then
    echo "Error: ${MOD_SOURCE_DIR} not found."
    echo "Usage: $0 [mod_name]"
    exit 1
fi

echo "Building game modules..."
echo "Project root: $PROJECT_ROOT"
echo "Mod name: $MOD_NAME"
echo "Module sources: $MOD_SOURCE_DIR"
echo "Release destination: $RELEASE_MOD_DIR"

# Navigate to the game mod source directory
cd "$MOD_SOURCE_DIR"

# Clean old build directory if it exists (to remove stale CMake cache)
if [ -d "$MOD_BUILD_DIR" ]; then
    echo "Cleaning old build directory..."
    rm -rf "$MOD_BUILD_DIR"
fi

# Remove old VM files from the mod's own release directory
if [ -d "$RELEASE_VM_DIR" ]; then
    echo "Removing old VM files from $RELEASE_VM_DIR ..."
    rm -f "$RELEASE_VM_DIR"/*.so "$RELEASE_VM_DIR"/*.dll 2>/dev/null || true
fi

# Create build directory and configure CMake
mkdir -p "$MOD_BUILD_DIR"
cd "$MOD_BUILD_DIR"
cmake ..

# Run the build process
make

# VM files should be in mod/vm/ directory
echo "Checking for compiled VM files in $MOD_VM_DIR"
mkdir -p "$MOD_VM_DIR"

# Move compiled files from mod/vm to release/mod/vm for THIS mod only
shopt -s nullglob
ARTIFACTS=("$MOD_VM_DIR"/*.so "$MOD_VM_DIR"/*.dll)
shopt -u nullglob

if [ ${#ARTIFACTS[@]} -eq 0 ]; then
    echo "Warning: No shared libraries found in $MOD_VM_DIR/"
else
    mkdir -p "$RELEASE_VM_DIR"
    echo "Moving files to mod's release directory: $RELEASE_VM_DIR"
    for lib in "${ARTIFACTS[@]}"; do
        libname=$(basename "$lib")
        mv -v "$lib" "$RELEASE_VM_DIR/$libname"
    done
    echo "Libraries moved to $RELEASE_VM_DIR/"
fi

# Verify files were moved
echo ""
if [ -d "$RELEASE_VM_DIR" ]; then
    SO_COUNT=$(ls -1 "$RELEASE_VM_DIR"/*.so "$RELEASE_VM_DIR"/*.dll 2>/dev/null | wc -l)
    if [ "$SO_COUNT" -gt 0 ]; then
        echo "Libraries available in $RELEASE_VM_DIR/:"
        ls -lh "$RELEASE_VM_DIR"/*.so "$RELEASE_VM_DIR"/*.dll 2>/dev/null || true
    else
        echo "Warning: No shared libraries found in $RELEASE_VM_DIR/"
    fi
fi

echo ""
echo "Game mod build completed!"
echo "  Libraries: $RELEASE_VM_DIR/*.so"
echo "  Source: $MOD_SOURCE_DIR/"
