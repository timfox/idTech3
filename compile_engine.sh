#!/bin/bash

# Compile Engine Script for id Tech 3 using CMake

set -e

# Set build type, default to Release, or use BUILD_TYPE=Debug for debug builds
BUILD_TYPE=${BUILD_TYPE:-Release}

echo "Building id Tech 3 engine (${BUILD_TYPE})..."

# Create build directory if it doesn't exist
if [ ! -d build ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build

# Run CMake configuration step if needed
if [ ! -f "CMakeCache.txt" ]; then
    echo "Running CMake configuration..."
    cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} ..
else
    echo "CMake cache found, skipping configuration..."
fi

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
echo "Build completed. Binaries are in the build directory."
echo "  - Client: build/idtech3.x86_64"
echo "  - Server: build/idtech3.ded.x86_64"
echo "  - Renderers: build/idtech3_*.so"

