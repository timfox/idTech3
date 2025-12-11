#!/bin/bash

# Script to compile Radiant

# Exit if any command fails
set -e

# Set the source and build directories
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$SRC_DIR/build/radiant"

echo "Radiant compilation script"
echo "Source directory: $SRC_DIR"
echo "Build directory: $BUILD_DIR"
echo

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Run CMake to configure the project
echo "Configuring project with CMake (Radiant only)..."
cmake -S "$SRC_DIR" -B "$BUILD_DIR" \
  -DBUILD_RADIANT=ON \
  -DRADIANT_BUILD_PLUGINS=ON \
  -DRADIANT_BUILD_CONTRIB=ON

# Build the project
echo "Building Radiant (editor + q3map2 + q3data)..."
cmake --build . --target radiant q3map2 q3data -- -j"$(nproc)"

echo
echo "Radiant compilation finished!"
