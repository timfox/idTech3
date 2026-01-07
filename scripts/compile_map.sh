#!/bin/bash

# Simple map compilation script for idTech3
# This script compiles .map files to .bsp files using q3map2

set -e

# Check arguments
if [ $# -lt 1 ]; then
    echo "Usage: $0 <map_name> [mod_name]"
    echo "Example: $0 testmap mymod"
    exit 1
fi

MAP_NAME="$1"
MOD_NAME="${2:-mymod}"

# Set paths
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MOD_DIR="$PROJECT_ROOT/mods/$MOD_NAME"
MAP_FILE="$MOD_DIR/maps/${MAP_NAME}.map"
BSP_FILE="$MOD_DIR/maps/${MAP_NAME}.bsp"

echo "Map Compilation Script"
echo "======================"
echo "Map: $MAP_NAME"
echo "Mod: $MOD_NAME"
echo "Source: $MAP_FILE"
echo "Target: $BSP_FILE"
echo

# Check if map file exists
if [ ! -f "$MAP_FILE" ]; then
    echo "Error: Map file not found: $MAP_FILE"
    exit 1
fi

# Try to find q3map2
Q3MAP2_PATHS=(
    "$PROJECT_ROOT/build/radiant/install/q3map2"
    "$PROJECT_ROOT/tools/radiant/install/q3map2"
    "/usr/local/bin/q3map2"
    "/usr/bin/q3map2"
    "$(which q3map2 2>/dev/null)"
)

Q3MAP2=""
for path in "${Q3MAP2_PATHS[@]}"; do
    if [ -x "$path" ]; then
        Q3MAP2="$path"
        break
    fi
done

if [ -z "$Q3MAP2" ]; then
    echo "Warning: q3map2 not found. Trying to build it..."
    echo

    # Try to build q3map2
    if [ -d "$PROJECT_ROOT/tools/radiant" ]; then
        cd "$PROJECT_ROOT/tools/radiant"
        echo "Building q3map2 with SCons..."

        # Try to build just q3map2 without GTK dependencies
        if command -v scons >/dev/null 2>&1; then
            scons q3map2 BUILD_GAMEPACK=0 2>&1 | head -20
            if [ -x "q3map2" ]; then
                Q3MAP2="./q3map2"
                echo "Built q3map2 successfully"
            fi
        else
            echo "SCons not found. Please install SCons to build q3map2"
        fi
    fi

    if [ -z "$Q3MAP2" ]; then
        echo "Could not build q3map2. Please install it manually:"
        echo "  sudo apt-get install q3map2  # Ubuntu/Debian"
        echo "  or download from: https://github.com/TTimo/GtkRadiant"
        exit 1
    fi
fi

echo "Using q3map2: $Q3MAP2"
echo

# Change to mod directory
cd "$MOD_DIR"

# Compile the map
echo "Compiling map..."
echo "Step 1: BSP compilation..."
"$Q3MAP2" -fs_basepath "$PROJECT_ROOT" -fs_game "$MOD_NAME" -bsp "$MAP_FILE"

echo "Step 2: VIS compilation..."
"$Q3MAP2" -fs_basepath "$PROJECT_ROOT" -fs_game "$MOD_NAME" -vis "$BSP_FILE"

echo "Step 3: Light compilation..."
"$Q3MAP2" -fs_basepath "$PROJECT_ROOT" -fs_game "$MOD_NAME" -light "$BSP_FILE"

echo
echo "Map compilation completed!"
echo "Output: $BSP_FILE"
echo
echo "To test the map, run:"
echo "  ./idtech3.x86_64 +set fs_game $MOD_NAME +map $MAP_NAME"