#!/bin/bash

# Script to compile Radiant

# Exit if any command fails
set -e

# Set the source and build directories
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$SRC_DIR/build"

echo "Radiant compilation script"
echo "Source directory: $SRC_DIR"
echo "Build directory: $BUILD_DIR"
echo

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Run CMake to configure the project
echo "Configuring project with CMake..."
cmake ..

# Build the project
echo "Building Radiant..."
cmake --build . -- -j$(nproc)

echo
echo "Radiant compilation finished!"
