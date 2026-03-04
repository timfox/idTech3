# Development Setup

## Prerequisites

### Linux (Primary Platform)

```bash
sudo apt-get install cmake ninja-build pkg-config \
  libcurl4-openssl-dev mesa-common-dev libxxf86dga-dev libxrandr-dev \
  libxxf86vm-dev libasound-dev libsdl2-dev libopenal-dev \
  libfreetype6-dev lua5.4 liblua5.4-dev glslang-tools \
  libstdc++-14-dev
```

### Compiler Requirements
- **Clang 18+** (recommended) or **GCC 15+**
- C23 support required (falls back to C17 if unavailable)
- C++23 for external libraries (opus, flac, cflux2)

### Optional Video Codec Dependencies
```bash
# FFmpeg (H.264, H.265, VP9, AV1, and all FFmpeg-supported formats)
sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev

# dav1d (high-performance AV1 decoder)
sudo apt-get install libdav1d-dev

# libvpx (VP8/VP9)
sudo apt-get install libvpx-dev

# Theora
sudo apt-get install libtheora-dev
```

### macOS
```bash
brew install coreutils sdl2 openal-soft cmake ninja freetype lua
```

### Windows (MSYS2)
```bash
pacman -S base-devel mingw-w64-x86_64-{gcc,cmake,ninja,pkgconf,SDL2,openal,freetype,lua}
```

## Building

The primary build script is `scripts/compile_engine.sh`.

### Quick Start
```bash
# Vulkan renderer (recommended)
./scripts/compile_engine.sh vulkan

# OpenGL renderer
./scripts/compile_engine.sh opengl

# Debug build
./scripts/compile_engine.sh vulkan debug

# Clean build
./scripts/compile_engine.sh clean vulkan

# Quiet build (suppress compiler output)
./scripts/compile_engine.sh vulkan quiet
```

### CMake Direct
```bash
mkdir -p build && cd build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DUSE_VULKAN=ON \
  -DBUILD_FREETYPE=ON \
  -DUSE_LUA=ON \
  -DUSE_DUKTAPE=ON \
  -DSKIP_IDPAK_CHECK=ON \
  -Wno-dev
cmake --build . -j$(nproc)
```

### Enabling Video Codecs
```bash
cmake .. \
  -DUSE_FFMPEG=ON \
  -DUSE_DAV1D=ON \
  -DUSE_VPX=ON \
  -DUSE_THEORA=ON \
  ...
```

Each codec is optional and detected via pkg-config. Missing libraries are reported as warnings and the codec is disabled gracefully.

## Build Outputs

| Binary | Description |
|--------|-------------|
| `release/idtech3` | Client executable |
| `release/idtech3_server` | Dedicated server |
| `release/idtech3_vulkan.so` | Vulkan renderer plugin |
| `release/idtech3_opengl.so` | OpenGL renderer plugin |
| `release/flux_cli` | FLUX.2 image generation tool |

## Shader Compilation

Vulkan GLSL shaders are compiled to SPIR-V automatically during the CMake build:
```bash
# Manual shader compilation
./scripts/compile_shaders.sh

# Apply generated shaders
./scripts/compile_shaders.sh --apply
```

Requires `glslangValidator` from `glslang-tools`.

## Game Modules

Compile game mods (cgame, game, ui shared libraries):
```bash
./scripts/compile_game.sh mymod Release
```

## Running

```bash
# Client (requires display + GPU)
cd release && ./idtech3

# Dedicated server
cd release && ./idtech3_server +set dedicated 1 +set com_hunkMegs 128

# FFmpeg console commands
# (in-game console)
ffmpeg codecs           # List available video codecs
ffmpeg info <file>      # Show media file information
ffmpeg play <file>      # Play a video file
```

## Validation

```bash
# Smoke test (verify binaries in release/ work)
./scripts/smoke_test.sh release

# Local CI validation (shader compile + Vulkan build + smoke test)
./scripts/validate_ci_build.sh

# CTest (requires build first; runs smoke test)
cd build-vk-Release && ctest -V
# or: make test
```

## IDE Setup

### VS Code / Cursor
The CMake build generates `compile_commands.json` in the build directory. Point your editor to it for IntelliSense:
```json
{
  "cmake.buildDirectory": "${workspaceFolder}/build-vk-Release"
}
```

### CLion
Open the root `CMakeLists.txt` directly. Configure the CMake profile with the desired flags.
