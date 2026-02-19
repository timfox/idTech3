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

FreeType font generation is enabled by default. The `fonts/` directory is populated automatically during configure by downloading Source Sans 3 (the file is cached so it only hits the network once). When FreeType is enabled the renderer builds the UI fonts at runtime from these TrueType files, so you no longer need to pre-render or ship bitmap fonts.

```
cmake -S . -B build ...
```

or use the helper script:

```
./scripts/compile_engine.sh vulkan
```

If you need to turn FreeType off for minimal builds, pass `-DBUILD_FREETYPE=OFF` to CMake or run `./scripts/compile_engine.sh no-freetype`. The flag controls `src/renderers/rendercommon/tr_font.c` and requires your platform’s FreeType headers/libraries (`libfreetype6-dev`, `freetype-devel`, etc.).

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
