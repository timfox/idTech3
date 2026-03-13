#!/usr/bin/env bash
# One-shot setup for Raspberry Pi 5: SDL with Vulkan + video codecs.
# Run this before building the engine for full compatibility.
#
# Usage: ./scripts/setup_rpi_full.sh [--prefix DIR]
#
# Options:
#   --prefix DIR   Install SDL to DIR (default: /usr/local). Use $HOME/sdl2-vulkan for user install.

set -euo pipefail

PREFIX="/usr/local"
while [[ $# -gt 0 ]]; do
  case "$1" in
    --prefix) PREFIX="$2"; shift 2 ;;
    *) echo "Unknown option: $1"; exit 1 ;;
  esac
done

echo "=== Raspberry Pi 5 Full Setup ==="
echo "  SDL prefix: $PREFIX"
echo ""

# 1. Video codecs (FFmpeg, dav1d, libvpx, Theora)
if [ -f "$(dirname "$0")/install_video_codecs.sh" ]; then
  echo "--- Installing video codecs ---"
  "$(dirname "$0")/install_video_codecs.sh"
  echo ""
fi

# 2. SDL with Vulkan
if [ -f "$(dirname "$0")/build_sdl_vulkan_rpi.sh" ]; then
  echo "--- Building SDL with Vulkan ---"
  "$(dirname "$0")/build_sdl_vulkan_rpi.sh" --prefix "$PREFIX"
  echo ""
fi

echo "=== Setup complete ==="
echo "Rebuild the engine and run:"
echo "  ./scripts/compile_engine.sh vulkan"
echo "  ./release/run_vulkan.sh +set cl_renderer vulkan"
if [[ "$PREFIX" != "/usr/local" ]]; then
  echo ""
  echo "Or: LD_LIBRARY_PATH=$PREFIX/lib:\$LD_LIBRARY_PATH ./release/idtech3.aarch64 +set cl_renderer vulkan"
fi
