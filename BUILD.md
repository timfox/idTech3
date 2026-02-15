## Build Instructions

## Toolchain Notes

- CMake targets C23 when supported by the active compiler/toolchain and falls back to C17 otherwise.
- Set `C_STANDARD_STRICT=OFF` to disable the strict warning set locally. CI uses strict warnings with warnings-as-errors.

### windows/msvc

Install Visual Studio Community Edition 2017 or later and compile `quake3e` project from solution

`src/platform/win32/msvc2017/quake3e.sln`

Copy resulting exe from `src/platform/win32/msvc2017/output` directory

To compile with Vulkan backend - clean solution, right click on `quake3e` project, find `Project Dependencies` and select `renderervk` instead of `renderer`

---

### windows/msys2

Install the build dependencies:

`MSYS2 MSYS`

* pacman -Syu
* pacman -S make mingw-w64-x86_64-gcc mingw-w64-i686-gcc

Use `MSYS2 MINGW32` or `MSYS2 MINGW64` depending on your target system, then copy resulting binaries from created `build` directory or use command:

`make install DESTDIR=<path_to_game_files>`

---

### windows/mingw

All build dependencies (libraries, headers) are bundled-in

Build with either `make ARCH=x86` or `make ARCH=x86_64` commands depending on your target system, then copy resulting binaries from created `build` directory or use command:

`make install DESTDIR=<path_to_game_files>`

---

### generic/ubuntu linux/bsd

You may need to run the following commands to install packages (using fresh ubuntu-18.04 installation as example):

* sudo apt install make gcc libcurl4-openssl-dev mesa-common-dev
* sudo apt install libxxf86dga-dev libxrandr-dev libxxf86vm-dev libasound-dev
* sudo apt install libsdl2-dev
* sudo apt install libopenal-dev

Build with: `make`

Copy the resulting binaries from created `build` directory or use command:

`make install DESTDIR=<path_to_game_files>`

---

### Arch Linux

The package `quake3e-git` can either be installed through your favourite AUR helper, or manually using these commands:

Download the snapshot from AUR:

`curl -O https://aur.archlinux.org/cgit/aur.git/snapshot/quake3e-git.tar.gz`

Extract the snapshot:

`tar xfz quake3e-git.tar.gz`

Enter the extracted directory:

`cd quake3e-git`

Build and install `quake3e-git`:

`makepkg -risc`

---

### raspberry pi os

Install the build dependencies:

* apt install libsdl2-dev libxxf86dga-dev libcurl4-openssl-dev

Build with: `make`

Copy the resulting binaries from created `build` directory or use command:

`make install DESTDIR=<path_to_game_files>`

---

### macos

* install the official SDL2 framework to /Library/Frameworks
* `brew install molten-vk` or install Vulkan SDK to use MoltenVK library

Build with: `make`

Copy the resulting binaries from created `build` directory

---

### ppc64le / ppc64 (PowerPC 64-bit)

Install the same dependencies as generic Linux, then build normally.

The PowerPC JIT (`src/qcommon/vm_powerpc.c`) supports optional ISA-level optimizations that are enabled by compiler target flags:

- ISA 2.07 (POWER8): uses direct-move instructions (`mtvsrwa`, `mfvsrwz`, `xscvdpsxws`) to avoid memory round-trips in float/int conversions (`OP_CVIF`, `OP_CVFI`).
- ISA 3.0 (POWER9): uses hardware modulo instructions (`modsw`, `moduw`) to optimize `OP_MODI` and `OP_MODU`.

Examples:

- `make CFLAGS='-mcpu=power8'`
- `make CFLAGS='-mcpu=power9'`
- `make CFLAGS='-mcpu=native'` (may reduce portability to older CPUs)

Without an explicit `-mcpu`, optimization level depends on compiler defaults, and the JIT falls back to baseline sequences when newer ISA features are unavailable.

---

Several Makefile options are available for linux/mingw/macos builds:

`BUILD_CLIENT=1` - build unified client/server executable, enabled by default

`BUILD_SERVER=1` - build dedicated server executable, enabled by default

`USE_SDL=0`- use SDL2 backend for video, audio, input subsystems, enabled by default, enforced for macos

`USE_VULKAN=1` - build vulkan modular renderer, enabled by default

`USE_OPENGL=1` - build opengl modular renderer, enabled by default

`USE_OPENGL2=0` - build opengl2 modular renderer, disabled by default

`USE_RENDERER_DLOPEN=1` - do not link single renderer into client binary, compile all enabled renderers as dynamic libraries and allow to switch them on the fly via `\cl_renderer` cvar, enabled by default

`RENDERER_DEFAULT=opengl` - set default value for `\cl_renderer` cvar or use selected renderer for static build for `USE_RENDERER_DLOPEN=0`, valid options are `opengl`, `opengl2`, `vulkan`

`USE_SYSTEM_JPEG=0` - use current system JPEG library, disabled by default

Example:

`make BUILD_SERVER=0 USE_RENDERER_DLOPEN=0 RENDERER_DEFAULT=vulkan` - which means do not build dedicated binary, build client with single static vulkan renderer

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

The flag targets the renderer code under `src/renderers/rendercommon/tr_font.c`, links with your platform’s FreeType library, and defines `BUILD_FREETYPE` for the build. Make sure the FreeType headers/libraries (`libfreetype6-dev`, `freetype-devel`, etc.) are installed before you configure the project.
