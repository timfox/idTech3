# Build & Run

This repository is CMake-based, but the **recommended** path is the wrapper script in `tools/`.

## Build (recommended)

- **OpenGL (Release)**:

```bash
bash tools/compile_engine.sh opengl Release
```

- **Vulkan (Release)**:

```bash
bash tools/compile_engine.sh vulkan Release
```

- **Debug**:

```bash
bash tools/compile_engine.sh opengl Debug
```

Build outputs land in renderer-specific build dirs (`build-gl-<type>` / `build-vk-<type>`) and are copied into `release/`.

## Run

This repo does **not** ship Quake III Arena content. To actually play, you need game data (e.g. `pak0.pk3`) in:

- `release/base/`

### Client

```bash
cd release
./idtech3.x86_64
```

### Debug Vulkan pipeline logging
- To enable verbose Vulkan pipeline creation logs, run the engine with the environment variable `VK_VERBOSE_PIPELINE_LOGS` set.
- Example:
```bash
VK_VERBOSE_PIPELINE_LOGS=1 ./release/idtech3.x86_64 +set fs_game mymod +set cl_renderer vulkan
```
- Expected output: additional DEBUG lines around pipeline allocation, e.g. "Allocated pipeline def ..." after vk_find_pipeline_ext allocations.

### Wayland Support
- **Linux only**: Wayland backend is supported via SDL with automatic X11 fallback
- **Configuration**: Use `+set r_wayland 1` to force Wayland, `+set r_wayland 0` for X11 (default)
- **Requirements**: `WAYLAND_DISPLAY` environment variable must be set for Wayland to work
- **Fallback**: If Wayland initialization fails, engine automatically falls back to X11
- **Window management**: Wayland uses server-side decorations by default (libdecor disabled)

#### Wayland with X11 fallback for Vulkan
- On Wayland sessions, if SDL_Vulkan_CreateSurface fails, the engine automatically retries by restarting the video backend to X11 and recreating the Vulkan surface.
- Runtime log hints:
  - "Using Wayland display driver (WAYLAND_DISPLAY=...)"
  - "SDL_Vulkan_CreateSurface failed on Wayland (...); retrying with X11..."
  - "VK_CreateSurface: Wayland fallback to X11 path engaged"
- How to test:
  - **CI testing**: Run `scripts/ci_wayland_fallback.sh` to validate the fallback logic
  - **Manual testing**: Set `SDL_VIDEODRIVER=wayland ./idtech3.x86_64 +set cl_renderer vulkan` to force Wayland and observe fallback behavior
  - **Cvar method**: Use `+set r_wayland 1` to enable Wayland with automatic fallback
- Notes:
  - This fallback path is intended for environments with limited Wayland support; modern Wayland setups should render directly.
### Dedicated server

```bash
cd release
./idtech3.server.x86_64 +set dedicated 1
```

### Helpful startup flags

- **Set base/home paths explicitly** (useful when launching from elsewhere):

```bash
./idtech3.x86_64 +set fs_basepath . +set fs_homepath .
```

- **Use command-line `+` commands** (supported):

```bash
./idtech3.server.x86_64 +set fs_basepath . +set fs_homepath . +set dedicated 1 +quit
```

