#!/bin/bash

# Compile Game Script for id Tech 3 mods

set -e

# Get absolute paths BEFORE changing directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
BUILD_DIR="$PROJECT_ROOT/build"
MOD_NAME="${1:-mymod}"
MOD_ROOT="$PROJECT_ROOT/$MOD_NAME"
MOD_SOURCE_DIR="$MOD_ROOT/gamesrc"
MOD_BUILD_DIR="$MOD_SOURCE_DIR/build"
MOD_DEST_DIR="$BUILD_DIR/$MOD_NAME"

if [ "$MOD_NAME" = "blacksun" ]; then
    MOD_DEST_DIR="$MOD_ROOT"
fi

if [ ! -d "$MOD_SOURCE_DIR" ]; then
    echo "Error: ${MOD_SOURCE_DIR} not found."
    echo "Usage: $0 [mod_name]"
    exit 1
fi

echo "Building game modules..."
echo "Project root: $PROJECT_ROOT"
echo "Build directory: $BUILD_DIR"
echo "Mod name: $MOD_NAME"
echo "Module sources: $MOD_SOURCE_DIR"
echo "Artifacts destination: $MOD_DEST_DIR"

# Navigate to the game mod source directory
cd "$MOD_SOURCE_DIR"

# Clean old build directory if it exists (to remove stale CMake cache)
if [ -d "$MOD_BUILD_DIR" ]; then
    echo "Cleaning old build directory..."
    rm -rf "$MOD_BUILD_DIR"
fi

# Clear old mod files (shared libraries) from the destination before copy
if [ -d "$MOD_DEST_DIR" ]; then
    echo "Removing old mod files from $MOD_DEST_DIR ..."
    rm -f "$MOD_DEST_DIR"/*.so "$MOD_DEST_DIR"/*.dll 2>/dev/null || true
fi

# Create build directory and configure CMake
mkdir -p "$MOD_BUILD_DIR"
cd "$MOD_BUILD_DIR"
cmake ..

# Run the build process
make

# VM files should be in mod/vm/ directory for runtime
MOD_VM_DIR="$MOD_ROOT/vm"
echo "Ensuring VM files are in mod directory: $MOD_VM_DIR"
mkdir -p "$MOD_VM_DIR"

# Determine where CMake placed the artifacts
SOURCE_LIB_DIR=""
for candidate in "$MOD_SOURCE_DIR/../vm" "$MOD_ROOT/vm" "$MOD_BUILD_DIR" "$MOD_ROOT"; do
    if compgen -G "$candidate"/*.so > /dev/null 2>&1; then
        SOURCE_LIB_DIR="$candidate"
        break
    fi
done

if [ -z "$SOURCE_LIB_DIR" ]; then
    echo "Warning: Unable to locate .so outputs."
else
    echo "Found libraries in $SOURCE_LIB_DIR"
    shopt -s nullglob
    artifacts=("$SOURCE_LIB_DIR"/*.so "$SOURCE_LIB_DIR"/*.dll)
    shopt -u nullglob
    if [ ${#artifacts[@]} -eq 0 ]; then
        echo "Warning: No shared libraries detected."
    else
        # Copy to mod/vm/ directory for runtime (only if source is different)
        if [ "$SOURCE_LIB_DIR" != "$MOD_VM_DIR" ]; then
            for lib in "${artifacts[@]}"; do
                libname=$(basename "$lib")
                cp -v "$lib" "$MOD_VM_DIR/$libname"
            done
            echo "Libraries copied to $MOD_VM_DIR/"
        else
            echo "Libraries already in $MOD_VM_DIR/ (no copy needed)"
        fi
    fi
fi

# Also copy to build directory for packaging if different
if [ "$MOD_VM_DIR" != "$MOD_DEST_DIR" ] && [ -d "$MOD_DEST_DIR" ]; then
    mkdir -p "$MOD_DEST_DIR/vm"
    if [ -d "$MOD_VM_DIR" ]; then
        cp -v "$MOD_VM_DIR"/*.so "$MOD_VM_DIR"/*.dll "$MOD_DEST_DIR/vm/" 2>/dev/null || true
    fi
fi

# Verify files were copied
echo ""
if [ -d "$MOD_VM_DIR" ]; then
    SO_COUNT=$(ls -1 "$MOD_VM_DIR"/*.so "$MOD_VM_DIR"/*.dll 2>/dev/null | wc -l)
    if [ "$SO_COUNT" -gt 0 ]; then
        echo "Libraries available in $MOD_VM_DIR/:"
        ls -lh "$MOD_VM_DIR"/*.so "$MOD_VM_DIR"/*.dll 2>/dev/null || true
    else
        echo "Warning: No shared libraries found in $MOD_VM_DIR/"
    fi
fi

echo ""
echo "Game mod build completed!"
echo "  Libraries: $MOD_DEST_DIR/*.so"
echo "  Source: $MOD_SOURCE_DIR/"
