# ARM / Raspberry Pi Support

This document describes ARM and Raspberry Pi support for the idTech3 engine.

## Raspberry Pi 4 / 5 with Vulkan (V3DV)

Raspberry Pi OS ships Mesa V3DV (Vulkan 1.3) by default on Pi 4 and 5. The engine supports Vulkan on these devices.

### Known Issues (Mitigated)

- **"Couldn't get a visual" / "could not load Vulkan subsystem"**: On ARM + X11, SDL may fail to create a Vulkan window. The engine defaults to `r_vid_driver x11` on ARM and forces X11 when KMSDRM was selected (Vulkan has known issues with KMSDRM).
  - **Default renderer**: On ARM, `cl_renderer` defaults to `opengl` (not `vulkan`) because many systems ship SDL without Vulkan support. Use `+set cl_renderer vulkan` only if you have Vulkan-capable SDL.
  - **Automatic fallback**: If you explicitly select Vulkan and SDL lacks Vulkan support, the engine falls back to OpenGL and prints `[VK] Vulkan not available in SDL, falling back to OpenGL`.
  - **Manual fallback**: If Vulkan still fails: `./idtech3.aarch64 +set cl_renderer opengl`
- **KMS/DRM**: Vulkan with SDL's KMSDRM backend has known issues on Raspberry Pi (see [SDL#3997](https://github.com/libsdl-org/SDL/issues/3997)). The engine automatically overrides to X11 when Vulkan is requested with KMSDRM on ARM.
- **r_mode -2 fails on RPi5**: Desktop resolution mode may fail. On ARM, the engine defaults to `r_mode -1` with 640x480 for better reliability. Fallbacks (r_mode 3, -1 800x600, windowed, wayland) are also tried automatically.

### Recommended Setup

1. **Use X11**: Run under X11 (default desktop session). If you get "Couldn't get a visual", set:
   ```
   set r_vid_driver x11
   vid_restart
   ```
   Or run with: `SDL_VIDEODRIVER=x11 ./idtech3`

2. **r_vid_driver cvar**: The engine supports `r_vid_driver` (auto, x11, wayland, kmsdrm). On ARM with Vulkan, "auto" defaults to "x11" for compatibility. If x11 fails, the engine automatically retries with wayland. Requires `vid_restart` to take effect.

3. **If display still fails**: Try launching with explicit safe settings:
   ```
   ./idtech3 +set r_mode -1 +set r_customWidth 640 +set r_customHeight 480 +set r_fullscreen 0
   ```
   This forces a 640x480 windowed mode, which often works when desktop/fullscreen modes fail.

4. **Performance**: On RPi4, the Vulkan driver has poor GLSL shader performance. The engine disables high-quality dynamic lights by default (`r_dlightMode 0`) on ARM. You can try `r_dlightMode 1` for per-pixel lights if performance allows.

### Build Troubleshooting

- **Missing libstdc++-14-dev**: On Ubuntu 24.04 cross-compiling ARM, install `libstdc++-14-dev` so Clang can link C++.
- **Shader compilation**: Ensure `glslangValidator` and Python 3 are installed. Run `./scripts/compile_shaders.sh` before building.

### Build for ARM

```bash
# Native on Raspberry Pi (recommended)
./scripts/compile_engine.sh vulkan

# Cross-compile from x86_64 (requires gcc-aarch64-linux-gnu)
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
./scripts/compile_engine.sh vulkan aarch64

# Or use CMake directly with the toolchain
cmake -S . -B build-aarch64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-aarch64.cmake \
  -DCMAKE_BUILD_TYPE=Release -DUSE_VULKAN=ON -DSKIP_IDPAK_CHECK=ON -Wno-dev
cmake --build build-aarch64 -j$(nproc)
```

See [DEVELOPMENT_SETUP.md](DEVELOPMENT_SETUP.md#building-for-multiple-platforms) for full platform build options.

### Debugging

- **SDL video driver**: The engine prints `SDL video driver: x11` (or wayland, kmsdrm) at startup.
- **Vulkan load**: If Vulkan fails, the engine prints `SDL_GetError()` output.
- **Environment**: Use `SDL_DEBUG=1 SDL_VIDEODRIVER=x11 ./idtech3` for verbose SDL output.

## Platform Notes

- **Android**: Uses a separate Vulkan init path (`android_main.c`).
- **Wayland**: May work on ARM; if you see visual issues, try `r_vid_driver x11`.
