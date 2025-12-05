#!/bin/bash

# Compile Engine Script for id Tech 3 using CMake
set -e

# Usage: ./compile_engine.sh [game_name] [Debug|Release] [clean]
# Notes:
# - game_name only affects how we copy/rename into release (engine CMake target is fixed to idtech3)
# - build type defaults to Release

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
RELEASE_DIR="$PROJECT_ROOT/release"

GAME_NAME="idtech3"
BUILD_TYPE="Release"
CLEAN=0

normalize_build_type() {
    local arg=$(echo "$1" | tr '[:upper:]' '[:lower:]')
    case "$arg" in
        debug|dbg|d) echo "Debug" ;;
        release|rel|r) echo "Release" ;;
        *) echo "" ;;
    esac
}

# Argument parsing
for arg in "$@"; do
    norm_bt="$(normalize_build_type "$arg")"
    if [ -n "$norm_bt" ]; then
        BUILD_TYPE="$norm_bt"
        continue
    fi
    if [ "$arg" = "clean" ]; then
        CLEAN=1
        continue
    fi
    # Anything else is treated as desired game name (for copied filenames)
    GAME_NAME="$arg"
done

echo "Building id Tech 3 engine (${BUILD_TYPE})..."
echo "Project root: $PROJECT_ROOT"
echo "Build dir:    $BUILD_DIR"
echo "Release dir:  $RELEASE_DIR"

if [ $CLEAN -eq 1 ] && [ -d "$BUILD_DIR" ]; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"

cd "$BUILD_DIR"

echo "Running CMake configuration..."
cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} "$PROJECT_ROOT"

# Determine number of CPU cores for parallel build
if command -v nproc &>/dev/null; then
    CORES=$(nproc)
elif [[ "$OSTYPE" == "darwin"* ]]; then
    CORES=$(sysctl -n hw.ncpu)
else
    CORES=4
fi

echo "Building with ${CORES} parallel jobs..."
cmake --build . -- -j${CORES}

echo ""
echo "Build completed. Binaries are in $BUILD_DIR"
echo "  - Client:   $BUILD_DIR/idtech3.x86_64"
echo "  - Server:   $BUILD_DIR/idtech3.server.x86_64 (if built)"
echo "  - Renderers: $BUILD_DIR/idtech3_*_*.so"

echo ""
echo "Copying engine binaries and renderer .so files to $RELEASE_DIR..."
mkdir -p "$RELEASE_DIR"

# Copy main client executable (ship with .so suffix)
if [ -f "idtech3.x86_64" ]; then
    cp -f "idtech3.x86_64" "$RELEASE_DIR/${GAME_NAME}.x86_64.so"
    echo "Copied client -> $RELEASE_DIR/${GAME_NAME}.x86_64.so"
fi

# Copy dedicated server executable (if present) with both native and legacy name
if [ -f "idtech3.server.x86_64" ]; then
    cp -f "idtech3.server.x86_64" "$RELEASE_DIR/${GAME_NAME}.server.x86_64"
    cp -f "idtech3.server.x86_64" "$RELEASE_DIR/${GAME_NAME}.ded.x86_64"
    echo "Copied server -> ${RELEASE_DIR}/${GAME_NAME}.server.x86_64 (alias *.ded.x86_64)"
fi

# Copy renderers; keep only canonical CMake names (no extra aliases)
shopt -s nullglob
for sofile in idtech3_*_*.so; do
    base=$(basename "$sofile")
    cp -f "$sofile" "$RELEASE_DIR/$base"
    echo "Copied renderer -> $RELEASE_DIR/$base"
done
shopt -u nullglob

# Copy shared ImGui runtime if present (required when USE_CIMGUI=ON)
if [ -f "libimgui_shared.so" ]; then
    cp -f "libimgui_shared.so" "$RELEASE_DIR/"
    echo "Copied libimgui_shared.so to $RELEASE_DIR/"
fi

# Quick validation summary
MISSING=0
if [ ! -f "$RELEASE_DIR/${GAME_NAME}.x86_64" ]; then
    echo "Warning: Client binary missing at $RELEASE_DIR/${GAME_NAME}.x86_64"
    MISSING=1
fi
shopt -s nullglob
RENDERER_FILES=("$RELEASE_DIR"/idtech3_*_*.so)
shopt -u nullglob
if [ ${#RENDERER_FILES[@]} -eq 0 ]; then
    echo "Warning: Renderer .so files not found in $RELEASE_DIR"
    MISSING=1
fi
if [ $MISSING -eq 0 ]; then
    echo "✓ Engine artifacts ready in $RELEASE_DIR"
fi

echo "Engine binary and renderer .so files updated in $RELEASE_DIR"
