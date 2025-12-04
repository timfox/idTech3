#!/bin/bash

# Compile Engine Script for id Tech 3 using CMake
set -e

# --- Argument parsing for game name ---
# Usage: ./compile_engine.sh [game_name]
# If no game name is given, default to "idtech3"
GAME_NAME="${1:-idtech3}"

# Set build type, default to Release, or use BUILD_TYPE=Debug for debug builds
BUILD_TYPE=${BUILD_TYPE:-Release}

# Determine project root directory (parent of /tools)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

if [!"$GAME_NAME"=="idtech3" && "$GAME_NAME" != "" ]; then
echo "Building id Tech 3 engine (${BUILD_TYPE}) as ${GAME_NAME}..."
else
echo "Building id Tech 3 engine (${BUILD_TYPE})..."
fi

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
echo "  - Client:   $BUILD_DIR/${GAME_NAME}.x86_64"
echo "  - Server:   $BUILD_DIR/${GAME_NAME}.ded.x86_64"
echo "  - Renderers: $BUILD_DIR/idtech3_*.so -> ${GAME_NAME}_*_.so"

# Move .so files and main executable to /release directory, overwrite if they exist
echo ""
echo "Copying engine binaries and renderer .so files to $RELEASE_DIR..."
if [ ! -d "$RELEASE_DIR" ]; then
    echo "Creating $RELEASE_DIR..."
    mkdir -p "$RELEASE_DIR"
fi

# Copy/rename main client executable to release (overwrite if it exists)
if [ -f "${GAME_NAME}.x86_64" ]; then
    EXT="x86_64"
    ENGINE_CLIENT_EXEC="${GAME_NAME}.${EXT}"
    cp -f "${GAME_NAME}.x86_64" "$RELEASE_DIR/$ENGINE_CLIENT_EXEC"
    echo "Copied ${GAME_NAME}.x86_64 to $RELEASE_DIR/$ENGINE_CLIENT_EXEC"
fi

# Copy/rename dedicated server executable if present
if [ -f "idtech3.ded.x86_64" ]; then
    DED_EXT="ded.x86_64"
    ENGINE_DED_EXEC="${GAME_NAME}.${DED_EXT}"
    cp -f "idtech3.ded.x86_64" "$RELEASE_DIR/$ENGINE_DED_EXEC"
    echo "Copied idtech3.ded.x86_64 to $RELEASE_DIR/$ENGINE_DED_EXEC"
fi

# Copy/rename .so (renderer) plugins: idtech3_*.so => ${GAME_NAME}_*.so
shopt -s nullglob
for sofile in ${GAME_NAME}_*_.so; do
    # for example: idtech3_opengl1.so -> mymod_opengl1.so
    base_suffix="${sofile#${GAME_NAME}}"
    newfile="${GAME_NAME}${base_suffix}"
    cp -f "${GAME_NAME}${base_suffix}" "$RELEASE_DIR/$newfile"
    echo "Moved ${GAME_NAME}${base_suffix} to $RELEASE_DIR/$newfile"
done
shopt -u nullglob

# Copy shared ImGui runtime if present (required when USE_CIMGUI=ON)
if [ -f "libimgui_shared.so" ]; then
	cp -f "libimgui_shared.so" "$RELEASE_DIR/"
	echo "Copied libimgui_shared.so to $RELEASE_DIR/"
fi

echo "Engine binary and renderer .so files updated in $RELEASE_DIR"
