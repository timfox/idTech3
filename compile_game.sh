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

# Create build directory and configure CMake
mkdir -p "$MOD_BUILD_DIR"
cd "$MOD_BUILD_DIR"
cmake ..

# Run the build process
make

# Copy libraries to the destination directory
echo "Copying libraries to destination..."
mkdir -p "$MOD_DEST_DIR"

# Determine where CMake placed the artifacts
SOURCE_LIB_DIR=""
for candidate in "$MOD_SOURCE_DIR/../vm" "$MOD_ROOT" "$MOD_BUILD_DIR"; do
    if compgen -G "$candidate"/*.so > /dev/null; then
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
        echo "Warning: No shared libraries detected to copy."
    elif [ "$SOURCE_LIB_DIR" != "$MOD_DEST_DIR" ]; then
        for lib in "${artifacts[@]}"; do
            cp -v "$lib" "$MOD_DEST_DIR/"
        done
    else
        echo "Source and destination directories are the same; skipping copy."
    fi
fi

# Verify files were copied
echo ""
if [ -d "$MOD_DEST_DIR" ]; then
    SO_COUNT=$(ls -1 "$MOD_DEST_DIR"/*.so "$MOD_DEST_DIR"/*.dll 2>/dev/null | wc -l)
    if [ "$SO_COUNT" -gt 0 ]; then
        echo "Libraries available in $MOD_DEST_DIR/:"
        ls -lh "$MOD_DEST_DIR"/*.so "$MOD_DEST_DIR"/*.dll 2>/dev/null || true
    else
        echo "Warning: No shared libraries found in $MOD_DEST_DIR/"
    fi
fi

echo ""
echo "Game mod build completed!"
echo "  Libraries: $MOD_DEST_DIR/*.so"
echo "  Source: $MOD_SOURCE_DIR/"
