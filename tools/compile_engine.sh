#!/bin/bash

# Compile Engine Script for id Tech 3 using CMake

set -e

# Set build type, default to Release, or use BUILD_TYPE=Debug for debug builds
BUILD_TYPE=${BUILD_TYPE:-Release}

# Determine project root directory (parent of /tools)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "Building id Tech 3 engine (${BUILD_TYPE})..."

# Remove and recreate build directory in project root to ensure a clean build
BUILD_DIR="$PROJECT_ROOT/build"
RELEASE_DIR="$PROJECT_ROOT/release"

if [ -d "$BUILD_DIR" ]; then
    echo "Clearing build directory..."
    rm -rf "$BUILD_DIR"
fi
mkdir "$BUILD_DIR"

cd "$BUILD_DIR"

# Run CMake configuration from build dir, pointing to root as source dir
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
echo "Build completed. Binaries are in the build directory ($BUILD_DIR)."
echo "  - Client:   $BUILD_DIR/idtech3.x86_64"
echo "  - Server:   $BUILD_DIR/idtech3.ded.x86_64"
echo "  - Renderers: $BUILD_DIR/idtech3_*.so"

# Move .so files to /release directory, overwrite if they exist
echo ""
echo "Copying engine binaries and renderer .so files to $RELEASE_DIR..."
if [ ! -d "$RELEASE_DIR" ]; then
    echo "Creating $RELEASE_DIR..."
    mkdir -p "$RELEASE_DIR"
fi

# Copy main client executable to release (overwrite if it exists)
if [ -f "idtech3.x86_64" ]; then
    cp -f "idtech3.x86_64" "$RELEASE_DIR/"
    echo "Copied idtech3.x86_64 to $RELEASE_DIR/"
fi

shopt -s nullglob
for sofile in idtech3_*.so; do
    cp -f "$sofile" "$RELEASE_DIR/"
    echo "Moved $sofile to $RELEASE_DIR/"
done
shopt -u nullglob

# Copy shared ImGui runtime if present (required when USE_CIMGUI=ON)
if [ -f "libimgui_shared.so" ]; then
	cp -f "libimgui_shared.so" "$RELEASE_DIR/"
	echo "Copied libimgui_shared.so to $RELEASE_DIR/"
fi

echo "Engine binary and renderer .so files updated in $RELEASE_DIR"
