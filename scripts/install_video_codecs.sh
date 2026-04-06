#!/usr/bin/env bash
# Install development packages for video codec support (FFmpeg, dav1d, libvpx, Theora).
# Run this before building the engine to enable all video codecs (H.264, H.265, VP8, VP9, AV1, Theora).
#
# Usage: ./scripts/install_video_codecs.sh
#
# Supported: Debian, Ubuntu, Raspberry Pi OS

set -euo pipefail

if ! command -v apt-get &>/dev/null; then
  echo "Error: apt-get not found. This script supports Debian/Ubuntu/Raspberry Pi OS."
  exit 1
fi

echo "Installing video codec development packages..."

sudo apt-get update
sudo apt-get install -y \
  libavcodec-dev \
  libavformat-dev \
  libavutil-dev \
  libswscale-dev \
  libswresample-dev \
  libdav1d-dev \
  libvpx-dev \
  libtheora-dev \
  pkg-config

echo ""
echo "Video codec packages installed. Rebuild the engine:"
echo "  ./scripts/compile_engine.sh vulkan"
echo ""
echo "Note: Cross-compiling for aarch64 from x86 disables codecs by default."
echo "      Build natively on the target for full codec support."
