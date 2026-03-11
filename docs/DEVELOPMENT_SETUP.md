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

### Building for Multiple Platforms

**Native builds** (run on each target):
- **x86_64**: Run the script on an x86_64 Linux machine.
- **aarch64**: Run the script on an aarch64 Linux machine (e.g. Raspberry Pi 5, ARM server).

```bash
./scripts/compile_engine.sh vulkan
```

**Cross-compilation** (x86_64 host → Linux aarch64 target):
```bash
# Install cross-compiler
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# Cross-compile for aarch64 (experimental; may fail without ARM sysroot for SDL2/OpenAL)
./scripts/compile_engine.sh vulkan aarch64
```

Outputs go to `release/` with `.aarch64` suffix (e.g. `idtech3.aarch64`, `idtech3_server.aarch64`). Cross-compilation disables FFmpeg/AV1/VPX/Theora and may fail if SDL2/OpenAL cannot be found for the target.

**Build both x86_64 and aarch64** (native + cross, if cross-compiler installed):
```bash
./scripts/compile_engine.sh all-linux vulkan
```

**GitHub Actions** produces binaries for all platforms (Linux x86_64, Linux aarch64/armv7, macOS, Windows). Download artifacts from workflow runs for ready-to-use binaries when cross-compilation is not set up locally.

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

**Cross-compile for Linux aarch64** (manual):
```bash
cmake -S . -B build-aarch64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-aarch64.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DUSE_VULKAN=ON \
  -DSKIP_IDPAK_CHECK=ON \
  -Wno-dev
cmake --build build-aarch64 -j$(nproc)
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

### Steam API and SDR (Steam Datagram Relay)
```bash
# Full Steam API (achievements, overlay, rich presence, Steam Deck detection)
cmake .. -DUSE_STEAM=ON -DSTEAMWORKS_SDK=/path/to/steamworks_sdk ...

# Steam SDR transport (implies USE_STEAM when SDK found)
cmake .. -DUSE_STEAM_NETWORKING=ON -DSTEAMWORKS_SDK=/path/to/steamworks_sdk ...
```
Requires Steamworks SDK with `steam_api.h` (and `isteamnetworkingsockets.h` for SDR). Set `STEAMWORKS_SDK` to the SDK root.

- **USE_STEAM**: Achievements, overlay, rich presence, Steam Deck auto-detection. When Deck is detected, `base/steamdeck.cfg` is auto-exec'd.
- **USE_STEAM_NETWORKING**: SDR transport. Use `net_sdr 1` at runtime. Connect via `connect steam:STEAMID` when server advertises its SteamID.

### DTLS Network Encryption
```bash
cmake .. -DUSE_DTLS=ON ...
```
Requires OpenSSL. When enabled, use `net_dtls 1` and `net_dtls_key <shared-secret>` (same on client and server) for AES-256-GCM encrypted game traffic.

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
