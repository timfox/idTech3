#!/bin/bash
# Run the engine with Vulkan, using custom SDL if installed in /usr/local or $HOME/sdl2-vulkan-install.
# Use this when you've built SDL with Vulkan (scripts/build_sdl_vulkan_rpi.sh) and the system
# SDL lacks Vulkan support.
#
# Usage: ./run_vulkan.sh [engine args...]
# Example: ./run_vulkan.sh +set fs_game unwaking +set cl_renderer vulkan

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Pick engine binary by architecture
case "$(uname -m)" in
  aarch64|arm64) ENGINE="${SCRIPT_DIR}/idtech3.aarch64" ;;
  x86_64)       ENGINE="${SCRIPT_DIR}/idtech3" ;;
  *)            ENGINE="${SCRIPT_DIR}/idtech3" ;;
esac
[ ! -x "$ENGINE" ] && ENGINE="${SCRIPT_DIR}/idtech3"

# Prefer custom SDL: /usr/local (system install) or $HOME/sdl2-vulkan-install (user install)
if [ -f "/usr/local/lib/libSDL2.so" ] || [ -f "/usr/local/lib/aarch64-linux-gnu/libSDL2.so" ]; then
  export LD_LIBRARY_PATH="/usr/local/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
elif [ -n "$HOME" ] && [ -f "$HOME/sdl2-vulkan-install/lib/libSDL2.so" ]; then
  export LD_LIBRARY_PATH="$HOME/sdl2-vulkan-install/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

exec "$ENGINE" "$@"
