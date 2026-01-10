#!/bin/bash

# Simple map compilation script for idTech3
# This script compiles .map files to .bsp files using q3map2

# Use set -e carefully - we want to handle q3map2 building gracefully
set -euo pipefail

# Check arguments
if [ $# -lt 1 ] || [[ "$*" == *"--help"* ]] || [[ "$*" == *"-h"* ]]; then
    echo "Usage: $0 <map_name> [mod_name]"
    echo ""
    echo "Arguments:"
    echo "  map_name    Name of the map file (without .map extension)"
    echo "  mod_name    Mod directory name (defaults to 'mymod')"
    echo ""
    echo "Examples:"
    echo "  $0 testmap              # Compile testmap.map from mods/mymod/maps/"
    echo "  $0 testmap mymod        # Compile testmap.map from mods/mymod/maps/"
    echo "  $0 demo demo            # Compile demo.map from mods/demo/maps/"
    echo ""
    echo "The script will:"
    echo "  1. Find or build q3map2 compiler"
    echo "  2. Run BSP compilation (-bsp)"
    echo "  3. Run VIS compilation (-vis)"
    echo "  4. Run LIGHT compilation (-light)"
    echo ""
    exit 0
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
    "$PROJECT_ROOT/release/q3map2"
    "$PROJECT_ROOT/build/radiant/install/q3map2"
    "$PROJECT_ROOT/tools/radiant/install/q3map2"
    "$PROJECT_ROOT/tools/radiant/q3map2"
    "$PROJECT_ROOT/tools/radiant/build/*/q3map2/q3map2"
    "/usr/local/bin/q3map2"
    "/usr/bin/q3map2"
    "$(which q3map2 2>/dev/null || true)"
)

Q3MAP2=""
for path in "${Q3MAP2_PATHS[@]}"; do
    # Handle glob patterns
    if [[ "$path" == *"*"* ]]; then
        for expanded_path in $path; do
            if [ -x "$expanded_path" ] 2>/dev/null; then
                Q3MAP2="$expanded_path"
                break 2
            fi
        done
    elif [ -x "$path" ] 2>/dev/null && [ -n "$path" ]; then
        Q3MAP2="$path"
        break
    fi
done

if [ -z "$Q3MAP2" ]; then
    echo "Warning: q3map2 not found. Trying to build it..."
    echo

    # Try to build q3map2 using the editor build script
    if [ -f "$PROJECT_ROOT/scripts/compile_editor.sh" ]; then
        echo "Building q3map2 using compile_editor.sh (tools-only)..."
        # Create symlink if needed for CMake
        if [ ! -d "$PROJECT_ROOT/radiant" ] && [ -d "$PROJECT_ROOT/tools/radiant" ]; then
            ln -sf tools/radiant "$PROJECT_ROOT/radiant" 2>/dev/null || true
        fi
        
        # Try to build, but don't fail if it doesn't work
        if "$PROJECT_ROOT/scripts/compile_editor.sh" --tools-only 2>&1 | tail -30; then
            if [ -x "$PROJECT_ROOT/release/q3map2" ]; then
                Q3MAP2="$PROJECT_ROOT/release/q3map2"
                echo "✓ Built q3map2 successfully via compile_editor.sh"
            fi
        fi
    fi

    # Fallback: Try to build with SCons if editor script didn't work
    if [ -z "$Q3MAP2" ] && [ -d "$PROJECT_ROOT/tools/radiant" ]; then
        cd "$PROJECT_ROOT/tools/radiant"
        echo "Trying to build q3map2 with SCons (standalone, no GTK dependencies)..."
        
        if command -v scons >/dev/null 2>&1; then
            # Try to build just q3map2 without radiant dependencies
            # SCons config needs to specify only q3map2 target
            echo "Building q3map2 standalone with SCons..."
            # Build only q3map2 target (not radiant, q3data, or setup)
            # Redirect stderr to avoid GTK dependency errors from radiant build
            if scons config=release target=q3map2 BUILD_GAMEPACK=0 2>&1 | tee /tmp/q3map2_build.log | grep -E "(q3map2|Program|built|Building)" | tail -10; then
                # Check for q3map2 in various possible locations
                for possible_path in \
                    "$PROJECT_ROOT/tools/radiant/q3map2" \
                    "$PROJECT_ROOT/tools/radiant/build/*/q3map2/q3map2" \
                    "$PROJECT_ROOT/tools/radiant/install/q3map2"; do
                    if [[ "$possible_path" == *"*"* ]]; then
                        for expanded in $possible_path; do
                            if [ -x "$expanded" ] 2>/dev/null; then
                                Q3MAP2="$expanded"
                                break 2
                            fi
                        done
                    elif [ -x "$possible_path" ] 2>/dev/null; then
                        Q3MAP2="$possible_path"
                        break
                    fi
                done
                
                if [ -n "$Q3MAP2" ]; then
                    echo "Built q3map2 successfully with SCons at: $Q3MAP2"
                fi
            fi
        else
            echo "SCons not found. Install with: pip install scons"
        fi
    fi

    if [ -z "$Q3MAP2" ]; then
        echo ""
        echo "Error: Could not find or build q3map2."
        echo ""
        echo "Options to get q3map2:"
        echo ""
        echo "  1. Build using editor script (recommended):"
        echo "     scripts/compile_editor.sh --tools-only"
        echo ""
        echo "  2. Install system package:"
        echo "     sudo apt-get install q3map2  # Ubuntu/Debian"
        echo ""
        echo "  3. Build manually with SCons:"
        echo "     cd tools/radiant"
        echo "     scons BUILD_GAMEPACK=0 q3map2"
        echo ""
        echo "  4. Download pre-built binary from:"
        echo "     https://github.com/TTimo/GtkRadiant/releases"
        echo ""
        echo "Note: If building fails, you may need to install dependencies:"
        echo "  - For SCons: pip install scons"
        echo "  - For CMake build: Ensure tools/radiant directory exists"
        echo ""
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