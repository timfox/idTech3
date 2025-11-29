#!/bin/bash
# Fix UI library dependency name from ../vm/game.x86_64.so to game.x86_64.so
# This requires patchelf: sudo apt install patchelf

ARCH_SUFFIX="${1:-x86_64}"
UI_LIB="../vm/ui.${ARCH_SUFFIX}.so"

if [ ! -f "$UI_LIB" ]; then
    echo "Error: $UI_LIB not found"
    exit 1
fi

if ! command -v patchelf &> /dev/null; then
    echo "Error: patchelf not found. Install with: sudo apt install patchelf"
    exit 1
fi

echo "Fixing dependency in $UI_LIB..."
patchelf --replace-needed "../vm/game.${ARCH_SUFFIX}.so" "game.${ARCH_SUFFIX}.so" "$UI_LIB"

if [ $? -eq 0 ]; then
    echo "✓ Dependency fixed successfully"
    echo "Verifying:"
    readelf -d "$UI_LIB" | grep NEEDED
else
    echo "✗ Failed to fix dependency"
    exit 1
fi

