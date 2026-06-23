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

**HTTP downloads (libcurl):** the client uses libcurl for HTTPS/FTP map and pak mirrors when enabled at build time. See **[CURL_NETWORKING.md](CURL_NETWORKING.md)** for `cl_dlURL`, `sv_dlURL`, `download` / `dlmap`, and security notes.

### Compiler Requirements
- **Clang 18+** (recommended) or **GCC 15+**
- C23 support required (falls back to C17 if unavailable)
- **C++20** for engine-owned `.cpp` modules (ECS, navigation, physics, world layer, ImGui inspector, FreeUSD bridges). Migrated world/open-world sources are listed in **`tests/scripts/test_cpp20_sources.sh`**; run **`ctest -R test_cpp20_sources`** after touching those files.
- C++23-only language features are deferred until the C++20 baseline is stable on all CI targets
- Third-party C++ dependencies (opus, flac, cflux2, ImGui, Bullet, EnTT) use their upstream standards

### Optional Video Codec Dependencies
```bash
# Or use the install script:
./scripts/install_video_codecs.sh

# Manual install:
# FFmpeg (H.264, H.265, VP9, AV1, and all FFmpeg-supported formats)
sudo apt-get install libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev

# dav1d (high-performance AV1 decoder)
sudo apt-get install libdav1d-dev

# libvpx (VP8/VP9)
sudo apt-get install libvpx-dev

# Theora
sudo apt-get install libtheora-dev
```

See [COMPATIBILITY.md](COMPATIBILITY.md) for platform-specific notes.

### Git submodules (optional)

The default engine build only **auto-inits** the **FreeUSD** submodule (`compile_engine.sh` with `USE_FREEUSD=ON`). Other submodules are optional:

```bash
# FreeUSD (also auto-inited on compile when USE_FREEUSD=ON)
./scripts/init_optional_submodules.sh --freeusd

# Game/backend tree (timfox/idtech3backend — not linked into CMake by default)
./scripts/init_optional_submodules.sh --backend

# QEMU fork for in-game OS sandbox (timfox/idTech3-Emulator — build inside submodule)
./scripts/init_optional_submodules.sh --emulator

# Tiled Map Editor (GPL-2.0, level design — not linked into idtech3)
./scripts/init_optional_submodules.sh --tiled

# Sparse voxel octree reference (optional, src/external)
./scripts/init_optional_submodules.sh --svo

# All optional submodules above
./scripts/init_optional_submodules.sh --all
```

See [FREEUSD.md](FREEUSD.md), [IDTECH3_BACKEND.md](IDTECH3_BACKEND.md), [IDTECH3_EMULATOR.md](IDTECH3_EMULATOR.md), [TILED.md](TILED.md), and [tools/README.md](../tools/README.md).

### C# scripting (optional, Mono)

```bash
sudo apt-get install libmono-2.0-dev mono-devel
./scripts/compile_engine.sh vulkan csharp
```

See [CSHARP.md](CSHARP.md). Lua `Engine.*` bindings register on the **client** when `USE_LUA=ON` (`LuaDebug_SetEngineRegisterCallback` in `CL_Init`).

### FreeUSD (default ON)

`USE_FREEUSD=ON` is the CMake default. The library lives in the **Git submodule** `src/external/FreeUSD` ([gopexllc/FreeUSD](https://github.com/gopexllc/FreeUSD)); `./scripts/compile_engine.sh` runs `git submodule update --init` when needed. If the submodule is missing, CMake **FetchContent** can fetch the same pin (network). Init manually with `./scripts/init_optional_submodules.sh --freeusd`. Disable with `./scripts/compile_engine.sh vulkan nofreeusd` or `-DUSE_FREEUSD=OFF`.

See [FREEUSD.md](FREEUSD.md) for mesh import cvars (`r_freeusd`, `r_freeusdPickLargest`, …) and console tools (`usd_info`, `usd_meshes`, `usd_load`, …). Test USDA files: `tests/data/usd/`.

### macOS
```bash
brew install coreutils sdl2 openal-soft cmake ninja freetype lua
```

### Windows (MSYS2)
```bash
pacman -S base-devel mingw-w64-x86_64-{gcc,cmake,ninja,pkgconf,SDL2,openal,freetype,lua}
```

MinGW links SDL2/OpenAL/etc. dynamically. If you copy `idtech3.exe` outside MSYS2 (or ship a zip), run `./scripts/stage_mingw_runtime_dlls.sh bin` from a **MINGW64** shell after copying binaries into `bin/` so required `.dll` files sit next to the executables. For **OpenAL** without relying on MSYS2’s `openal.dll` layout, match CI: **`pwsh ./scripts/stage_openal_windows_dlls.ps1 -BinDir bin -Arch x64`** (downloads OpenAL Soft’s official `OpenAL32.dll` + `soft_oal.dll` into `bin/`).

**Native DLL / `.so` load failures:** run with `+set com_nativeLibraryDebug 1` on the command line (before configs that matter). On filesystem startup the engine prints one cyan line confirming that mode. Every failed `FS_LoadLibrary` path then logs the **full path** and the OS loader message. On Windows, `Sys_GetLoadLibraryError` uses a **sticky** copy of `LoadLibrary` / `GetProcAddress` failures so the message is not wiped by unrelated API calls before you see it (including missing **`GetRefAPI`** on a renderer DLL). Game modules are searched under `modules/` and `vm/` for both packed names (`uix86_64.dll`) and dotted names (`ui.x86_64.dll`; same idea for `cgame` / `qagame`). For native filename probe order, logical-name aliases (`qagame` -> `game` / `server`, etc.), and `fs_restrict` behavior, see [ARCHITECTURE.md - Native game modules](ARCHITECTURE.md#native-game-modules-vm).

**PE architecture sanity:** from PowerShell, `pwsh ./scripts/windows_native_compat_check.ps1 -BinDir path\to\bin` fails if any `.exe`/`.dll` in the folder is not the same machine type as the reference executable (catches x86/x64/ARM64 mixups).

**PE export sanity:** `pwsh ./scripts/windows_pe_exports_check.ps1 -BinDir path\to\bin` - use **`-SkipRendererDlls`** for Windows CMake/MSVC/MinGW artifacts (`USE_RENDERER_DLOPEN` is forced **off** on `WIN32`, so renderer plugins are not shipped as separate DLLs). Use **`-ExpectRendererDlls`** only for trees that ship `idtech3_vulkan.dll` (e.g. Linux dlopen builds). Any `qagame`/`cgame`/`ui`/... native `.dll` present in the folder must export **`vmMain`** and **`dllEntry`**.

## Building

The primary build entry point is `scripts/compile_engine.sh`. The repository also ships `CMakePresets.json` for direct CMake / IDE workflows.

### Quick Start
```bash
# Vulkan renderer
./scripts/compile_engine.sh vulkan

# Vulkan + KHR ray-tracing demo (r_rtx 1, r_rtxDemo 1; requires RT-capable GPU)
./scripts/compile_engine.sh vulkan rtx

# Debug build
./scripts/compile_engine.sh vulkan debug

# Clean build
./scripts/compile_engine.sh clean vulkan

# Quiet build (suppress compiler output)
./scripts/compile_engine.sh vulkan quiet
```

### CMake Presets
```bash
# Configure + build Vulkan Release
cmake --preset vulkan-release
cmake --build --preset build-vulkan-release

# Run the matching ctest preset
ctest --preset test-vulkan-release

# Alternate configurations
cmake --preset vulkan-debug
```

Presets use dedicated `build/presets/<name>/` trees so they do not overwrite the helper script's `build-vk-*` directories.

Use the helper script when you want staged artifacts in `release/` and the same workflow documented throughout the repo. Use presets when you want direct CMake control or IDE integration.

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

**Optional link-time optimization (LTO / IPO)** for shipping builds on **GCC or Clang** (longer links; not enabled in default CI):

```bash
./scripts/compile_engine.sh vulkan lto
# or: cmake ... -DENABLE_LTO=ON
```

MSVC: `ENABLE_LTO` is currently not wired; use the Visual Studio LTO project settings if you need it there.

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
- **USE_VULKAN_RTX** (default OFF): KHR ray tracing (`r_rtx`, Hybrid1, path trace, GRTX, Raygun). Without it, `#else` stubs log at init and cvars remain inert.
- **USE_EXPERIMENTAL_RENDERERS** (default ON): Neural/scaffold paths (`r_niv`, `r_renderformer`, `r_vksplat`, `r_mgs`, `r_wpt`, etc.). Set OFF for lean renderer builds; `vk_experimental_renderer_stubs.c` supplies no-op symbols.
- **BUILD_FREETYPE** (default ON): TTF rasterization + GPU vector font load. OFF uses `tr_font_stub.c` / `tr_vector_font_stub.c` (cached `.dat` fonts only).

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
| `release/flux_cli` | FLUX.2 image generation tool |

### Minimal game data (engine-only trees)

The repo does not ship full game `.pk3` sets. For a **smallest valid `base/`** (one `.pk3` + `default.cfg`) so the client/server pass filesystem init without **“No game data”**, see [MINIMAL_GAME_SHELL.md](MINIMAL_GAME_SHELL.md). For a **demo layout** with optional `idtech3_demo.pk3`, see [examples/demo_skeleton/README.md](../examples/demo_skeleton/README.md). For **headless dedicated** smoke with maps + `qagame.qvm`, build [renderer_validation/devdata](renderer_validation/devdata/README.md) and point **`GAME_BASE`** at `rtest_base/`.

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

# Local CI validation: shaders, Vulkan Release build, smoke test, then ctest (same ordering as Ubuntu CI), renderer regression script, demo pk3 layout check
./scripts/validate_ci_build.sh

# CTest only (after `./scripts/compile_engine.sh vulkan` so `build-vk-Release/` exists)
cd build-vk-Release && ctest -C Release --output-on-failure
# or: make test

# CTest through presets
ctest --preset test-vulkan-release
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
