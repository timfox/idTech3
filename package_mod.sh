#!/bin/bash

# Package Mod Script - Creates pak0.pk3 from mymod directory

set -e

# Get absolute paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$SCRIPT_DIR"
MOD_SOURCE_DIR="$PROJECT_ROOT/mymod"
BUILD_DIR="$PROJECT_ROOT/build"
MOD_DIR="$BUILD_DIR/mymod"
PAK_FILE="$MOD_DIR/pak0.pk3"

echo "Packaging mod into pak0.pk3..."
echo "Source: $MOD_SOURCE_DIR"
echo "Output: $PAK_FILE"

# Ensure mod directory exists
mkdir -p "$MOD_DIR"

# Change to mod source directory
cd "$MOD_SOURCE_DIR"

# Create pak0.pk3, excluding build artifacts and source files
# Include: all game assets, configs, shaders
# Exclude: build/, vm/, scripts/build/, source files, docs
zip -r "$PAK_FILE" . \
    -x "build/*" \
    -x "vm/*" \
    -x "scripts/build/*" \
    -x "scripts/*.c" \
    -x "scripts/*.h" \
    -x "scripts/CMakeLists.txt" \
    -x "scripts/Makefile" \
    -x "scripts/cgame/*" \
    -x "scripts/game/*" \
    -x "scripts/ui/*" \
    -x "*.md" \
    -x "*.txt" \
    -x "CHANGES" \
    -x "COPYING" \
    -x "CREDITS" \
    -x "LINUXNOTES" \
    -x "SDL.README.txt" \
    -x "OGGVORBIS.README.txt" \
    -x "NATIVE_COMPILATION.md" \
    -x "PBR_GUIDE.md" \
    -x "README" \
    -x "README.md" \
    -x "*.pspimage" \
    || {
    echo "Error: Failed to create pak0.pk3"
    exit 1
}

# Get file size
if [ -f "$PAK_FILE" ]; then
    FILE_SIZE=$(du -h "$PAK_FILE" | cut -f1)
    echo ""
    echo "✓ Successfully created pak0.pk3"
    echo "  Location: $PAK_FILE"
    echo "  Size: $FILE_SIZE"
    
    # List contents count
    FILE_COUNT=$(unzip -l "$PAK_FILE" 2>/dev/null | tail -1 | awk '{print $2}')
    echo "  Files: $FILE_COUNT"
else
    echo "Error: pak0.pk3 was not created"
    exit 1
fi

echo ""
echo "Mod packaging completed!"
echo "  pak0.pk3 is ready in: $MOD_DIR"