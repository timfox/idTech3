#!/bin/bash

# Script to compile Radiant
#
# Default: build the Qt editor only (modern UI). Pass --gtk to also build the
# legacy GtkRadiant plus q3map2/q3data.

# Exit if any command fails
set -e

if [[ "$1" == "--help" ]]; then
  echo "Usage: $0 [--gtk]"
  echo "  --gtk    Also build legacy GtkRadiant (and q3map2/q3data)."
  exit 0
fi

BUILD_GTK=0
if [[ "$1" == "--gtk" ]]; then
  BUILD_GTK=1
fi

# Set the source and build directories
SRC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$SRC_DIR/build/radiant"

echo "Radiant compilation script"
echo "Source directory: $SRC_DIR"
echo "Build directory: $BUILD_DIR"
echo "GTK legacy build: $([[ $BUILD_GTK -eq 1 ]] && echo ON || echo OFF)"
echo

# Create build directory if it doesn't exist
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Run CMake to configure the project
echo "Configuring project with CMake..."
cmake -S "$SRC_DIR" -B "$BUILD_DIR" \
  -DBUILD_RADIANT=ON \
  -DRADIANT_BUILD_QT=ON \
  -DRADIANT_USE_ENGINE_RENDERER_VK=OFF \
  -DRADIANT_BUILD_EDITOR=$([[ $BUILD_GTK -eq 1 ]] && echo ON || echo OFF) \
  -DRADIANT_BUILD_PLUGINS=$([[ $BUILD_GTK -eq 1 ]] && echo ON || echo OFF) \
  -DRADIANT_BUILD_CONTRIB=$([[ $BUILD_GTK -eq 1 ]] && echo ON || echo OFF)

# Build the project
if [[ $BUILD_GTK -eq 1 ]]; then
  echo "Building GtkRadiant + tools (radiant, q3map2, q3data) and Qt editor..."
  cmake --build . --target radiant radiant_qt q3map2 q3data -- -j"$(nproc)"
else
  echo "Building Qt editor only (radiant_qt)..."
  cmake --build . --target radiant_qt -- -j"$(nproc)"
fi

echo
echo "Radiant compilation finished!"
