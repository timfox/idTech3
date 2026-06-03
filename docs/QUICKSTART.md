# Quick Start (End Users)

Get the idTech3 engine running with game data in a few steps.

## 1. Download

Get the latest release from [Releases](https://github.com/timfox/idTech3/releases). Download the archive for your platform:

| Platform | Archive |
|----------|---------|
| Linux x86_64 | `idtech3-<tag>-linux-x86_64.zip` |
| Linux ARM64 (aarch64) | `idtech3-<tag>-linux-aarch64.zip` |
| Linux ARMv7 | `idtech3-<tag>-linux-armv7.zip` |
| Windows x64 (MSVC) | `idtech3-<tag>-windows-msvc-x86_64.zip` |
| Windows ARM64 (MSVC) | `idtech3-<tag>-windows-msvc-arm64.zip` |
| Windows x64 (MinGW) | `idtech3-<tag>-windows-gcc-x86_64.zip` |
| macOS Apple Silicon | `idtech3-<tag>-macos-aarch64.zip` |
| Android arm64-v8a | `idtech3-<tag>-android-arm64-v8a.zip` |
| Android armeabi-v7a | `idtech3-<tag>-android-armeabi-v7a.zip` |

Binaries are built by CI when a release is published; allow 15–30 minutes after publishing for all platform archives to appear.

Extract the archive. You should see:
- `idtech3` (or `idtech3.exe` on Windows) - game client
- `idtech3_server` (or `idtech3_server.exe`) - dedicated server
- **Linux**: `idtech3_vulkan.so` - Vulkan renderer plugin (when built with `USE_RENDERER_DLOPEN`)
- **Windows (MSYS2/MinGW zip from CI)**: same `.exe` names plus several `.dll` files (SDL2, OpenAL Soft via `OpenAL32.dll` + `soft_oal.dll`, MinGW runtime). Keep them in the same folder as the executables. **OpenAL** does not require a separate Creative/OpenAL installer for these builds.
- **Windows (MSVC zip, x64)**: CI may ship **OpenAL Soft** router DLLs next to the `.exe` (`OpenAL32.dll` + `soft_oal.dll` + `OpenAL-Soft-COPYING.txt`) for users who run a **CMake/MinGW-style** client build linked against OpenAL, or who drop in an OpenAL-linked `idtech3.exe`. The **stock MSVC `quake3e.vcxproj` client does not compile in `snd_backend_openal.c`**, so out of the box it uses **WASAPI / DirectSound** (`win_snd.c`, `s_driver` / `s_openal` has no effect there). **Windows MSVC ARM64** zips omit the OpenAL Soft bundle: upstream **openal-soft *-bin.zip** has no **WinARM64** `soft_oal.dll`, and the MSVC client is **not** an OpenAL build—use **WASAPI** (default on Win7+) or **DirectSound** for audio.

## 2. Game Data

The engine needs game data (maps, textures, sounds). You must provide a compatible game base, for example:

**Smallest valid tree (bootstrap / tech demo):** at least one **`.pk3`** under **`base/`** (or your `fs_basegame` folder) that contains **`default.cfg`** and loads as a non-empty archive—see [MINIMAL_GAME_SHELL.md](MINIMAL_GAME_SHELL.md).

- **Quake III Arena** - copy or symlink the `baseq3` folder from your Q3A installation into the engine directory
- **Open Arena** or other Q3-based games - same idea: the engine expects a `baseq3` (or `base`) folder with `.pk3` files

Typical layout:
```
idtech3/
├── idtech3
├── idtech3_server
├── idtech3_vulkan.so
└── baseq3/          ← your game data
    ├── pak0.pk3
    ├── pak1.pk3
    └── ...
```

## 3. Run

**Client** (requires display and GPU):
```bash
./idtech3
```

**Dedicated server**:
```bash
./idtech3_server +set dedicated 1 +set com_hunkMegs 128
```

### Demo mod from source (optional)

If you **build the engine from this repository**, you can use the **`idtech3_demo`** config mod without editing C code:

1. `./examples/demo_game/build_demo_pack.sh` - builds `idtech3_demo.pk3`
2. Put your licensed game `.pk3` files under **`examples/demo_skeleton/base/`**
3. Copy the `.pk3` to **`examples/demo_skeleton/idtech3_demo/idtech3_demo.pk3`**, or run **`./examples/demo_skeleton/setup_demo_layout.sh`**
4. From the repo root: **`./scripts/run_demo.sh`**

Full walkthrough, **`baseq3`** layouts, and troubleshooting: [examples/demo_skeleton/README.md](../examples/demo_skeleton/README.md).

## 4. Renderer

The engine uses the **Vulkan** renderer. Ensure a working Vulkan driver and loader (Mesa/NVIDIA/AMD) on Linux.

**PBR (Physically Based Rendering)** is on by default with FBO. Ensure `r_fbo 1` (default) and `r_pbr 1` (default). If PBR is disabled at startup, the console will show why (e.g. "requires r_fbo 1"). Use `vid_restart` after changing these.

**Vulkan Forward+ (default on):** `r_forwardPlus` defaults to **1** (**latched**; set **0** + `vid_restart` to disable). GPU light records + per-tile compute cull and optional PBR debug/shade cvars—see [RENDERERS.md](RENDERERS.md#vulkan-forward-scaffolding) and [FORWARD_PLUS_PIPELINE_AUDIT.md](FORWARD_PLUS_PIPELINE_AUDIT.md). When many lights overlap a tile, **`r_forwardPlusLuminanceSort`** (**default 1**) keeps the brightest by RGB sum up to **`r_forwardPlusMaxPerTile`**.

## Steam Deck

When running under Steam on Steam Deck, the engine auto-detects the device and loads `base/steamdeck.cfg` (gamepad enabled, Vulkan, 60 FPS cap). Ensure `steamdeck.cfg` exists in your base folder. Build with `-DUSE_STEAM=ON -DSTEAMWORKS_SDK=/path/to/sdk` for full Steam API support.

## Raspberry Pi 5

For full compatibility (Vulkan + video codecs), run the setup script before building:

```bash
./scripts/setup_rpi_full.sh
./scripts/compile_engine.sh vulkan
./release/run_vulkan.sh
```

See [ARM_RASPBERRY_PI.md](ARM_RASPBERRY_PI.md) for details.

## Troubleshooting

- **"No game data"** - Ensure `baseq3/` (or `base/`) exists with at least one `.pk3` file.
- **Black screen / no render** - Confirm Vulkan works (`vulkaninfo` or your distro’s GPU tools), install current GPU drivers, then `vid_restart`. Check the console for renderer init errors.
- **Solid color / dark brown / dark green / no UI** - Ensure FBO is enabled: `+set r_fbo 1` and run `vid_restart`. If still broken, try `r_exposure_auto 0`, `r_volumetricFog 0`, then `vid_restart`. As last resort, `r_fbo 0` disables HDR/post-processing.
- **Missing libraries** - On Linux, install SDL2, OpenAL, and Vulkan drivers for your GPU. On Windows, if you copied only `idtech3.exe` out of a MinGW build folder, restore the accompanying `.dll` files from the same archive or re-run `./scripts/stage_mingw_runtime_dlls.sh bin` from an MSYS2 **MINGW64** shell after copying binaries into `bin/`.
- **Native game DLLs not found** - The engine looks under `baseq3`/`base` in `modules/` and `vm/` (and the gamedir root as a legacy fallback). It tries several basename patterns per slot (`ui.so` / `ui.x86_64.dll` / `uix86_64.dll`, etc.) and alternate logical names for some VMs. If a module exists **only inside a `.pk3`**, enable **`com_nativeLibraryExtractPk3`** (**1** by default): the engine copies it to **`vm/native_cache/`** under your game home path, then loads it. If load still fails, run with `+set com_nativeLibraryDebug 1` to print the full path and the OS loader error for each attempt. Details: [ARCHITECTURE.md#native-game-modules-vm](ARCHITECTURE.md#native-game-modules-vm).
