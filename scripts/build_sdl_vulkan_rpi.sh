#!/usr/bin/env bash
# Build SDL2 with Vulkan support for Raspberry Pi 4/5
# Raspberry Pi OS ships SDL without Vulkan; this script builds a Vulkan-enabled SDL.
# Run on the Raspberry Pi (native) or with appropriate cross-compile setup.
#
# Usage:
#   ./scripts/build_sdl_vulkan_rpi.sh [--prefix DIR] [--no-install]
#
# Options:
#   --prefix DIR    Install to DIR (default: /usr/local). Use $HOME/sdl2-vulkan for user install.
#   --no-install   Build only, do not install (useful for testing)
#
# After install, run the engine with:
#   ./release/idtech3.aarch64 +set cl_renderer vulkan
# If using a custom prefix: LD_LIBRARY_PATH=$PREFIX/lib:$LD_LIBRARY_PATH ./release/idtech3.aarch64 +set cl_renderer vulkan

set -euo pipefail

PREFIX="/usr/local"
DO_INSTALL=1
SDL_TAG="release-2.30.0"
BUILD_DIR="${BUILD_DIR:-$HOME/sdl2-vulkan-build}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --prefix)
      PREFIX="$2"
      shift 2
      ;;
    --no-install)
      DO_INSTALL=0
      shift
      ;;
    *)
      echo "Unknown option: $1"
      echo "Usage: $0 [--prefix DIR] [--no-install]"
      exit 1
      ;;
  esac
done

echo "=== SDL2 Vulkan build for Raspberry Pi ==="
echo "  Prefix: $PREFIX"
echo "  Install: $([ $DO_INSTALL -eq 1 ] && echo yes || echo no)"
echo ""

# Check for build deps (Raspberry Pi OS / Debian / Ubuntu)
# libvulkan-dev provides Vulkan headers (vulkan-headers is not in all Debian repos)
if command -v apt-get &>/dev/null; then
  echo "Installing build dependencies..."
  sudo apt-get update
  sudo apt-get install -y build-essential cmake ninja-build pkg-config \
    libasound2-dev libdbus-1-dev libdrm-dev libgbm-dev libibus-1.0-dev \
    libpulse-dev libudev-dev libx11-dev libxcb1-dev libxext-dev libxfixes-dev \
    libxinerama-dev libxrandr-dev libxss-dev libxxf86vm-dev libvulkan-dev
else
  echo "Note: apt-get not found. Install: cmake ninja-build pkg-config, libx11-dev, libvulkan-dev, etc."
fi

# Clone or update SDL
SDL_SRC="$BUILD_DIR/SDL"
if [[ ! -d "$SDL_SRC" ]]; then
  echo "Cloning SDL $SDL_TAG..."
  mkdir -p "$BUILD_DIR"
  git clone --depth 1 --branch "$SDL_TAG" https://github.com/libsdl-org/SDL.git "$SDL_SRC"
else
  echo "Using existing SDL at $SDL_SRC"
  (cd "$SDL_SRC" && git fetch --depth 1 origin tag "$SDL_TAG" 2>/dev/null || true)
fi

# Build
SDL_BUILD="$BUILD_DIR/build"
mkdir -p "$SDL_BUILD"
cd "$SDL_BUILD"

echo "Configuring SDL with Vulkan..."
cmake "$SDL_SRC" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$PREFIX" \
  -DSDL_VULKAN=ON \
  -DSDL_X11=ON \
  -DSDL_WAYLAND=ON \
  -DSDL_KMSDRM=ON

echo "Building..."
ninja

if [[ $DO_INSTALL -eq 1 ]]; then
  echo "Installing to $PREFIX..."
  sudo ninja install
  if [[ "$PREFIX" == "/usr/local" ]]; then
    sudo ldconfig
  fi
  echo ""
  echo "=== SDL with Vulkan installed ==="
  echo "Rebuild the engine and run:"
  echo "  ./scripts/compile_engine.sh vulkan"
  echo "  ./release/idtech3.aarch64 +set cl_renderer vulkan"
else
  echo ""
  echo "=== Build complete (not installed) ==="
  echo "To install: cd $SDL_BUILD && sudo ninja install && sudo ldconfig"
fi

if [[ "$PREFIX" != "/usr/local" ]]; then
  echo ""
  echo "Using custom prefix - set LD_LIBRARY_PATH when running:"
  echo "  LD_LIBRARY_PATH=$PREFIX/lib:\$LD_LIBRARY_PATH ./release/idtech3.aarch64 +set cl_renderer vulkan"
fi
