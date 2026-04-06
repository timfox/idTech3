# Cross-compilation toolchain: x86_64 host -> Linux aarch64 target
# Requires: gcc-aarch64-linux-gnu, g++-aarch64-linux-gnu
#   sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
#
# Cross-compiled builds may fail if SDL2, OpenAL, etc. are not available
# for the target. Use a sysroot or rely on GitHub Actions for ARM binaries.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Optional: sysroot for target libraries (uncomment and set if needed)
# set(CMAKE_SYSROOT /path/to/arm64-sysroot)
# set(CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})
# set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
# set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
