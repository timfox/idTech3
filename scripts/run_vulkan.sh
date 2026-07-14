#!/bin/bash
# Run the engine with Vulkan, using custom SDL if installed in /usr/local or $HOME/sdl3-vulkan-install.
# Use this when you've built SDL with Vulkan (scripts/build_sdl_vulkan_rpi.sh) and the system
# SDL lacks Vulkan support.
#
# Usage: ./run_vulkan.sh [engine args...]
# Example: ./run_vulkan.sh +set fs_game /path/to/your-mod +set cl_renderer vulkan

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Engine lives in release/ (sibling of scripts/ when run from repo, or same dir when copied to release/)
RELEASE_DIR="$(cd "$SCRIPT_DIR/../release" 2>/dev/null && pwd)"
[ -z "$RELEASE_DIR" ] && RELEASE_DIR="$SCRIPT_DIR"

# Pick engine binary by architecture
case "$(uname -m)" in
  aarch64|arm64) ENGINE="${RELEASE_DIR}/idtech3.aarch64" ;;
  x86_64)       ENGINE="${RELEASE_DIR}/idtech3" ;;
  *)            ENGINE="${RELEASE_DIR}/idtech3" ;;
esac
[ ! -x "$ENGINE" ] && ENGINE="${RELEASE_DIR}/idtech3"

# Prefer custom SDL: /usr/local (system install) or $HOME/sdl3-vulkan-install (user install)
if [ -f "/usr/local/lib/libSDL3.so" ] || [ -f "/usr/local/lib/aarch64-linux-gnu/libSDL3.so" ]; then
  export LD_LIBRARY_PATH="/usr/local/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
elif [ -n "$HOME" ] && [ -f "$HOME/sdl3-vulkan-install/lib/libSDL3.so" ]; then
  export LD_LIBRARY_PATH="$HOME/sdl3-vulkan-install/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi

exec "$ENGINE" "$@"
