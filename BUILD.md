## Build Instructions

## Canonical build truth

This repository is built and validated through **CMake**. The two supported entry points are:

1. **`./scripts/compile_engine.sh`** - canonical developer workflow; handles the common Vulkan/OpenGL configurations, stages artifacts into `release/`, and mirrors CI closely.
2. **`cmake` / `cmake --preset`** - direct CMake workflow for IDEs, local iteration, and CI-style configure/build/test steps.

Legacy `make`-first instructions are no longer the primary build path for this fork.

## Toolchain notes

- CMake targets **C23** when supported by the active compiler/toolchain and falls back to **C17** otherwise.
- Set `C_STANDARD_STRICT=OFF` to disable the stricter warning set locally. CI keeps strict warnings enabled.
- The Vulkan path is the primary renderer configuration; OpenGL remains the compatibility fallback.
- `SKIP_IDPAK_CHECK=ON` is the default expectation for source-tree builds in this repository.

## Quick start

### Canonical script

```bash
# Vulkan + Release (recommended)
./scripts/compile_engine.sh vulkan

# OpenGL + Release
./scripts/compile_engine.sh opengl

# Vulkan + Debug
./scripts/compile_engine.sh vulkan debug

# Clean rebuild
./scripts/compile_engine.sh clean vulkan

# Optional shipping-style LTO build
./scripts/compile_engine.sh vulkan lto
```

Artifacts are copied to `release/` and the build trees live under `build-vk-Release/`, `build-vk-Debug/`, `build-gl-Release/`, and `build-gl-Debug/`.

**Minimal game data next to binaries:** see [docs/MINIMAL_GAME_SHELL.md](docs/MINIMAL_GAME_SHELL.md) (bootstrap `base/*.pk3` with `default.cfg`).

**Optional Git submodules** (not required to compile the engine):

```bash
git submodule update --init tools/tiled              # Tiled Map Editor (GPL-2.0)
git submodule update --init src/external/src/SparseVoxelOctree
```

See [docs/TILED.md](docs/TILED.md) and [tools/README.md](tools/README.md).

### CMake presets

`CMakePresets.json` exposes the same common configurations without having to remember the cache flags:

```bash
cmake --preset vulkan-release
cmake --build --preset build-vulkan-release
ctest --preset test-vulkan-release

cmake --preset vulkan-debug
cmake --build --preset build-vulkan-debug

cmake --preset opengl-release
cmake --build --preset build-opengl-release
```

The presets intentionally use their own `build/presets/<name>/` trees so they do not collide with the helper script's `build-vk-*` / `build-gl-*` directories.

Use the script when you want staged artifacts in `release/` and the standard helper behavior. Use presets when you want direct CMake/IDE workflows.

### Direct CMake

```bash
cmake -S . -B build-vk-Release -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DRENDERER_DEFAULT=vulkan \
  -DSKIP_IDPAK_CHECK=ON \
  -DUSE_VULKAN=ON \
  -Wno-dev
cmake --build build-vk-Release -j"$(nproc)"
ctest --test-dir build-vk-Release --output-on-failure -V
```

## Platform setup

### Linux / BSD

Primary development target:

```bash
sudo apt-get install cmake ninja-build pkg-config \
  libcurl4-openssl-dev mesa-common-dev libxxf86dga-dev libxrandr-dev \
  libxxf86vm-dev libasound-dev libsdl2-dev libopenal-dev \
  libfreetype6-dev lua5.4 liblua5.4-dev glslang-tools \
  libstdc++-14-dev
```

Recommended compilers:

- **Clang 18+**
- **GCC 15+**

Build with the canonical script or presets shown above.

### Windows (MSYS2 + CMake)

Use a modern **MINGW64** shell and install the package set used by CI:

```bash
pacman -S --needed \
  mingw-w64-x86_64-gcc \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-ninja \
  mingw-w64-x86_64-pkgconf \
  mingw-w64-x86_64-SDL2 \
  mingw-w64-x86_64-openal \
  mingw-w64-x86_64-freetype \
  mingw-w64-x86_64-lua \
  mingw-w64-x86_64-openssl \
  mingw-w64-x86_64-curl \
  mingw-w64-x86_64-bullet
```

Then use:

```bash
cmake --preset vulkan-release
cmake --build --preset build-vulkan-release
```

If you stage Windows binaries into a `bin/` directory outside MSYS2, run `./scripts/stage_mingw_runtime_dlls.sh bin` from a MINGW64 shell so the required runtime `.dll` files sit next to the executables.

To ship **OpenAL** without asking users to install Creative’s OpenAL (match CI release zips), from PowerShell run **`./scripts/stage_openal_windows_dlls.ps1 -BinDir bin -Arch x64`**. This downloads OpenAL Soft’s official `*-bin.zip` (cached under `.ci-openal-soft-cache/`, gitignored) and copies `OpenAL32.dll` + `soft_oal.dll` next to your `.exe` files. Optional env: `OPENAL_SOFT_BIN_VERSION`, `OPENAL_SOFT_DOWNLOAD_ATTEMPTS`, `SKIP_OPENAL_DLL_BUNDLE=1`.

### Windows (MSVC solution files)

The Visual Studio solution under `src/platform/win32/msvc2017/quake3e.sln` is still maintained for native MSVC workflows, but it is no longer the primary build truth for the repository.

- Open the solution in Visual Studio 2022 or newer.
- Build the desired configuration/platform.
- Outputs land under `src/platform/win32/msvc2017/output/`.
- **Audio:** the MSVC client uses the **Windows DMA path** (`win_snd.c`: **WASAPI** when `UseWasapi=0` is passed from CI, else build defaults apply)—it does **not** compile `snd_backend_openal.c`. For **OpenAL** + EFX/acoustics you need a **CMake** build with `USE_OPENAL=ON` and OpenAL installed/found (Linux/macOS/MinGW), or a custom MSVC project that defines `USE_OPENAL` and links OpenAL.
- **OpenAL Soft DLL zip (CI):** bundled for **MSVC x64** and **MinGW x64** artifacts only. **ARM64** MSVC zips skip it: upstream OpenAL Soft Windows bins ship **Win32/Win64** only; ARM64 Windows clients should rely on **WASAPI**.

Prefer the CMake path for documentation, CI parity, and cross-platform consistency.

### macOS

Install dependencies:

```bash
brew install coreutils sdl2 openal-soft cmake ninja freetype lua molten-vk bullet pkgconf
```

Then use the canonical script or presets:

```bash
./scripts/compile_engine.sh vulkan
# or
cmake --preset vulkan-release
cmake --build --preset build-vulkan-release
```

### Linux aarch64 / Raspberry Pi

For native ARM Linux builds, run the same script directly on the target machine:

```bash
./scripts/compile_engine.sh vulkan
```

For cross-compilation from x86_64 Linux:

```bash
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
./scripts/compile_engine.sh vulkan aarch64
```

Cross-build outputs are staged with `.aarch64` suffixes in `release/`. Video codecs are disabled by default for cross-builds and SDL/OpenAL discovery may still require a suitable ARM sysroot.

### ppc64le / ppc64 (PowerPC 64-bit)

Install the same dependencies as Linux, then build with the canonical CMake flow. The PowerPC JIT (`src/qcommon/vm_powerpc.c`) supports optional ISA-level tuning through compiler flags:

- ISA 2.07 (POWER8): direct-move instructions accelerate `OP_CVIF` / `OP_CVFI`.
- ISA 3.0 (POWER9): hardware modulo instructions accelerate `OP_MODI` / `OP_MODU`.

Examples:

```bash
cmake -S . -B build-power8 -G Ninja -DCMAKE_C_FLAGS="-mcpu=power8" -DCMAKE_CXX_FLAGS="-mcpu=power8"
cmake -S . -B build-power9 -G Ninja -DCMAKE_C_FLAGS="-mcpu=power9" -DCMAKE_CXX_FLAGS="-mcpu=power9"
```

Without an explicit `-mcpu`, optimization level depends on compiler defaults and the JIT falls back to baseline sequences when newer ISA features are unavailable.

## Validation workflow

The usual local validation sequence is:

```bash
./scripts/compile_engine.sh vulkan
cd build-vk-Release && ctest --output-on-failure -V
cd ..
./scripts/smoke_test.sh release
```

For a more CI-like local pass:

```bash
./scripts/validate_ci_build.sh
```

## Common cache variables

These are the CMake switches most developers touch during local builds:

- `RENDERER_DEFAULT=vulkan|opengl`
- `SKIP_IDPAK_CHECK=ON|OFF`
- `ENABLE_LTO=ON|OFF`
- `ENABLE_ASAN=ON|OFF` (Debug builds)
- `BUILD_FREETYPE=ON|OFF`
- `USE_LUA=ON|OFF`
- `USE_DUKTAPE=ON|OFF`

Platform-specific and feature-specific switches are documented below.

---

### OpenAL audio backend

OpenAL provides true 3D spatial audio with HRTF, environmental effects, and VOIP capture support.

#### Build-time

OpenAL is optional and disabled by default. Enable it with:

`cmake -DUSE_OPENAL=ON ..`

#### Runtime

Enable the OpenAL backend (requires engine restart):

`+set s_openal 1`

#### Console commands

`s_aldevices` - list all available OpenAL devices

`s_alinfo` - show detailed OpenAL device information and statistics

#### Configuration cvars

**Device selection:**
- `s_openalDevice` - device name (`"default"` uses the system default)

**Spatial audio:**
- `s_openalHrtf` - enable HRTF if supported (0/1, default: 1)
- `s_openalEfx` - enable EFX environmental audio effects (0/1, default: 1)
- `s_openalEfxPreset` - reverb preset (0=off, 1=generic, 2=hall, 3=cave, 4=underwater)

**Doppler effect:**
- `s_openalDopplerFactor` - doppler strength (0=off, 1=normal, higher=exaggerated, default: 1.0)
- `s_openalDopplerSpeed` - speed of sound in units/second (default: 9000)

**Distance attenuation:**
- `s_openalRolloff` - distance attenuation rolloff factor (default: 1.0)
- `s_openalMaxDistance` - maximum distance for sound attenuation (default: 2000)

**Occlusion/muffling:**
- `s_openalLowpass` - enable global low-pass filter (0=off, 1=full, default: 0.0)
- `s_openalLowpassHf` - low-pass high-frequency gain (0.05-1.0, default: 0.5)
- `s_openalOcclusion` - enable per-source occlusion tracing (0/1, default: 0)
- `s_openalOcclusionGain` - occluded direct gain multiplier (default: 0.5)
- `s_openalOcclusionHf` - occluded high-frequency gain (default: 0.2)

**VOIP:**
- `s_openalCapture` - enable audio capture for VOIP (0/1, default: 1)
- `s_openalVoipSpatial` - route VOIP through spatial sources (0/1, default: 1)
- `s_openalVoipGain` - VOIP gain multiplier (default: 1.0)

**Debugging:**
- `s_openalDebug` - debugging output level (0=off, 1=basic, 2=verbose, default: 0)

**Dynamic music layers:**
- `s_musicLayerEnabled` - enable secondary music layer (0/1, default: 0)
- `s_musicLayer` - secondary music layer track path (requires restart)
- `s_musicLayerVolume` - secondary layer volume multiplier (default: 1.0)
- `s_musicIntensity` - intensity value (0-1) for layer blending (default: 0.0)

---

### FreeType font rendering

FreeType font generation is disabled by default to keep the dependencies minimal. Enable it when you need the TrueType pipeline by passing `BUILD_FREETYPE=ON` to CMake:

```
cmake -S . -B build -DBUILD_FREETYPE=ON ...
```

Or let the helper script do it:

```
./scripts/compile_engine.sh freetype vulkan
```

The flag targets the renderer code under `src/renderers/common/tr_font.c`, links with your platform’s FreeType library, and defines `BUILD_FREETYPE` for the build. Make sure the FreeType headers/libraries (`libfreetype6-dev`, `freetype-devel`, etc.) are installed before you configure the project.

---

### SDF HUD text rendering

The client includes an SDF (signed-distance-field) HUD text path that can render UTF-8 glyphs from BMFont metrics + atlas assets.

Runtime cvars:

- `r_sdfEnable` (`0/1`) - enable SDF text path for supported HUD string rendering.
- `r_sdfFont` - base font path (for example `fonts/myfont` expects `fonts/myfont.fnt` and atlas image).
- `r_sdfFontMetrics` - optional explicit `.fnt` path override.
- `r_sdfFontAtlas` - optional explicit atlas image path override.
- `r_sdfSmoothing` - edge smoothing width hint for SDF content.

Behavior:

- When enabled and the configured SDF assets are valid, supported string draws use SDF glyph quads.
- If assets are missing/invalid or a glyph cannot be resolved, the renderer falls back to legacy bitmap text.
- Emoji rendering compatibility is preserved via the existing emoji atlas path.

---

### SVG image loading (librsvg)

The renderer can optionally rasterize `.svg` assets via **librsvg + cairo**.

Enable it with:

```bash
cmake -S . -B build -DUSE_LIBRSVG=ON ..
```

If dependencies are missing, CMake automatically disables the SVG loader and prints a status line.

Typical Linux packages:

- `librsvg2-dev`
- `libcairo2-dev`
- `pkg-config`

Runtime controls:

- `r_svgRasterScale` (default `1.0`) - rasterization scale multiplier.
- `r_svgMaxRasterSize` (default `4096`) - max rasterized width/height in pixels.
- `r_svgMaxFileBytes` (default `2097152`) - max accepted SVG source size.

Security behavior:

- Loads from virtual filesystem paths only.
- External resource resolution is disabled via empty base URI.
- Oversized files/raster targets are rejected safely with warnings.

---

### Lua scripting support

Lua support is enabled by default.

To disable it explicitly:

```bash
cmake -S . -B build -DUSE_LUA=OFF
```

`scripts/compile_engine.sh` also supports a `lua` flag, which passes `-DUSE_LUA=ON`.

The engine looks for common Lua installs, including `lua5.5` / `lua-5.5` (plus `5.4`..`5.1` variants). Lua support requires Lua 5.1 or newer.

When enabled, these commands are available:

- `script_reload [file1.lua ...]`
- `script_list`
- `script_dump [maxEntries]`

---

### JavaScript scripting support (Duktape)

Duktape support is enabled by default and uses a vendored internal static copy from `src/external/duktape`.

To force-enable it explicitly:

```bash
cmake -S . -B build -DUSE_DUKTAPE=ON
```

`scripts/compile_engine.sh` enables Duktape by default (vendored), and supports:

- `duktape` (explicit enable)
- `no-duktape` (disable, passes `-DUSE_DUKTAPE=OFF`)
- `system-duktape` (use system-installed library/headers, passes `-DUSE_SYSTEM_DUKTAPE=ON`)

When enabled, these commands are available:

- `js_reload [file1.js ...]`
- `js_list`
- `js_dump [maxEntries]`
- `js_exec <javascript-expression-or-code>`

Global JavaScript helpers are available under `idtech3`:

- `idtech3.print(...args)` - print to the engine console
- `idtech3.cvarGet(name)` - read cvar string value
- `idtech3.cvarSet(name, value)` - set cvar value (gated by `js_cvarSetMode`)
- `idtech3.exec(command[, mode])` - execute console command (`mode`: `append`, `insert`, `now`, gated by `js_allowExec`)
- `idtech3.readFile(path)` - read text from virtual filesystem (returns `null` if not found)
- `idtech3.writeFile(path, data)` - write file to virtual filesystem (gated by `js_allowFileWrite`)
- `idtech3.appendFile(path, data)` - append file to virtual filesystem (gated by `js_allowFileWrite`)
- `idtech3.include(path)` - evaluate another JS file from virtual filesystem
- `idtech3.require(path)` - load a JS module and return `module.exports` (cached when `js_requireCache=1`)
- `idtech3.on(event, fn)` - register callback for `frame`, `map_load`, `client_connect`, `ui_open`, `ui_close`, `menu_changed`, `input_key`, `mouse_move`, `console_open`
- `idtech3.off(event[, fn])` - remove one callback or all callbacks for an event
- `idtech3.textureLoad(path[, noMip])` - load texture/shader and return handle
- `idtech3.textureReload(path)` - force texture/shader lookup reload and return handle
- `idtech3.materialRegister(name)` - register material/shader and return handle
- `idtech3.hudDrawPic(x, y, w, h, shaderHandleOrName)` - draw image in HUD space
- `idtech3.hudDrawText(x, y, text[, size])` - draw HUD text

Event callback payloads:

- `frame`: `fn(msec, realMsec)`
- `map_load`: `fn(ev)` where `ev.map` is the map name
- `client_connect`: `fn(ev)` where `ev.address` and `ev.clientNum` identify the connection
- `ui_open`: `fn(ev)` when UI catcher opens
- `ui_close`: `fn(ev)` when UI catcher closes
- `menu_changed`: `fn(ev)` where `ev.menu` / `ev.menuId` describe active menu transitions
- `input_key`: `fn(ev)` where `ev.key`, `ev.keyNum`, `ev.down` report key transitions
- `mouse_move`: `fn(ev)` where `ev.dx`, `ev.dy` report mouse deltas
- `console_open`: `fn(ev)` when console catcher opens

JavaScript policy cvars:

- `js_allowEvents` (`0/1`) - allow/deny event registration
- `js_allowExec` (`0..3`) - command execution level: `0=off`, `1=append`, `2=append+insert`, `3=append+insert+now`
- `js_cvarSetMode` (`0..3`) - cvar write level: `0=off`, `1=existing writable only`, `2=also allow creating user cvars`, `3=unrestricted through Cvar_Set rules`
- `js_allowFileWrite` (`0/1`) - allow/deny `writeFile` and `appendFile`
- `js_maxEventCallbacks` (`1..1024`) - max callbacks per event
- `js_frameCallbackBudgetMs` (`0..1000`) - per-frame callback budget in ms (`0=unlimited`)
- `js_disableFaultyCallbacks` (`0/1`) - remove callbacks that throw errors
- `js_requireCache` (`0/1`) - cache module exports for `idtech3.require`
- `js_autoInit` (`0/1`) - initialize JS runtime automatically at startup
- `js_compatTarget` (read-only) - compatibility target string (default `es5.1-duktape`)

Script boundary policy:

- JavaScript (`js_reload`, `idtech3.include`) only accepts paths under: `ui/`, `client/`, `frontend/`, `scripts/js/`
- Lua (`script_reload`) only accepts paths under: `gameplay/`, `server/`, `vm/game/`, `scripts/lua/`

JavaScript module policy:

- `idtech3.require(path)` resolves explicit allowed-root paths and `scripts/js/<path>[.js]`
- It is intentionally minimal (CommonJS-style `module.exports`) and does not include Node/browser APIs
