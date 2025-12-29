#!/bin/bash

# Package Mod Script (from /tools) - Creates pak0.pk3 for a mod in /release/mymod

set -e

# Determine absolute paths relative to /tools
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Use mod name as argument, or default to mymod
MOD_NAME="${1:-mymod}"
MOD_SOURCE_DIR="$PROJECT_ROOT/$MOD_NAME"
RELEASE_MOD_DIR="$PROJECT_ROOT/release/$MOD_NAME"
PAK_FILE="$RELEASE_MOD_DIR/pak0.pk3"

echo "Packaging mod into pak0.pk3..."
echo " Mod source: $MOD_SOURCE_DIR"
echo " Output pak: $PAK_FILE"

# Ensure release mod directory exists
mkdir -p "$RELEASE_MOD_DIR"

# Verify mod source exists
if [ ! -d "$MOD_SOURCE_DIR" ]; then
    echo "Error: Mod source directory '$MOD_SOURCE_DIR' does not exist!"
    exit 1
fi

# Change to mod source directory
cd "$MOD_SOURCE_DIR"

# Create pak0.pk3, excluding build artifacts and source files
zip -r "$PAK_FILE" . \
    -x "build/*" \
    -x "vm/*" \
    -x "gamesrc/build/*" \
    -x "gamesrc/*.c" \
    -x "gamesrc/*.h" \
    -x "gamesrc/CMakeLists.txt" \
    -x "gamesrc/Makefile" \
    -x "gamesrc/cgame/*" \
    -x "gamesrc/game/*" \
    -x "gamesrc/ui/*" \
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
echo "  pak0.pk3 is ready in: $RELEASE_MOD_DIR"