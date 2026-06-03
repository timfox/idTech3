# ARM / Raspberry Pi Support

This document describes ARM and Raspberry Pi support for the idTech3 engine. See also [COMPATIBILITY.md](COMPATIBILITY.md) for platform-wide compatibility notes.

## Raspberry Pi 4 / 5 with Vulkan (V3DV)

Raspberry Pi OS ships Mesa V3DV (Vulkan 1.3) by default on Pi 4 and 5. The engine supports Vulkan on these devices.

### Known Issues (Mitigated)

- **"Couldn't get a visual" / "could not load Vulkan subsystem"**: On ARM + X11, SDL may fail to create a Vulkan window. The engine defaults to `r_vid_driver x11` on ARM and forces X11 when KMSDRM was selected (Vulkan has known issues with KMSDRM).
  - **Default renderer**: The engine uses **Vulkan**. On ARM, if SDL lacks Vulkan support, startup fails with a clear error until Vulkan is available.
  - **Force Vulkan attempt**: `+set cl_renderer_force 1` skips the probe and tries Vulkan anyway. Use only if you have SDL built with `-DSDL_VULKAN=ON`.
  - **SDL Vulkan requirement**: Raspberry Pi OS system SDL is often built without Vulkan. See [Build SDL with Vulkan](#build-sdl-with-vulkan-for-raspberry-pi) below.
  - **Fix path**: Build or install SDL with Vulkan enabled, use X11 (`r_vid_driver x11`), then `vid_restart`.
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

4. **Performance**: On RPi4/RPi5, the Vulkan driver (V3DV) has weaker shader performance. The engine disables high-quality dynamic lights by default (`r_dlightMode 0`) on ARM. You can try `r_dlightMode 1` for per-pixel lights if performance allows.

5. **Vulkan RPi5 tuning**: When Vulkan runs on V3DV (Raspberry Pi), the engine detects it and suggests `r_rpi_profile 1`. This preset disables SSAO, volumetric fog, bloom, SMAA, SSR, and fog fluid for better frame rates. Run:
   ```
   set r_rpi_profile 1
   vid_restart
   ```
   Or add `+set r_rpi_profile 1` to your launch command.

### Build SDL with Vulkan for Raspberry Pi

Raspberry Pi OS ships SDL built without Vulkan. To use the Vulkan renderer, rebuild SDL from source.

**Automated (recommended):**

```bash
./scripts/build_sdl_vulkan_rpi.sh
```

This installs SDL to `/usr/local`. Run the engine with the `run_vulkan.sh` launcher (sets `LD_LIBRARY_PATH` automatically):

```bash
./release/run_vulkan.sh +set cl_renderer vulkan
```

Or manually: `LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH ./release/idtech3.aarch64 +set cl_renderer vulkan`

For a user install (no sudo for the engine):

```bash
./scripts/build_sdl_vulkan_rpi.sh --prefix $HOME/sdl2-vulkan-install
# Then: LD_LIBRARY_PATH=$HOME/sdl2-vulkan-install/lib:$LD_LIBRARY_PATH ./release/idtech3.aarch64 +set cl_renderer vulkan
```

**Manual build** (if you prefer to run steps yourself):

```bash
# 1. Install build deps (libvulkan-dev provides Vulkan headers; Mesa V3DV provides libvulkan at runtime)
sudo apt-get install -y build-essential cmake ninja-build pkg-config \
  libasound2-dev libdbus-1-dev libdrm-dev libgbm-dev libibus-1.0-dev \
  libpulse-dev libudev-dev libx11-dev libxcb1-dev libxext-dev libxfixes-dev \
  libxinerama-dev libxrandr-dev libxss-dev libxxf86vm-dev libvulkan-dev

# 2. Clone SDL (use release tag for stability)
git clone https://github.com/libsdl-org/SDL.git -b release-2.30.0 sdl2-vulkan
cd sdl2-vulkan

# 3. Configure with Vulkan enabled
mkdir build && cd build
cmake .. -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DSDL_VULKAN=ON \
  -DSDL_X11=ON \
  -DSDL_WAYLAND=ON

# 4. Build and install
ninja
sudo ninja install
sudo ldconfig
```

Then rebuild the engine and run with Vulkan. **Use `LD_LIBRARY_PATH`** so the custom SDL is loaded instead of the system one:

```bash
cd /path/to/idTech3
./scripts/compile_engine.sh vulkan
LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH ./release/idtech3.aarch64 +set cl_renderer vulkan
```

If you used `--prefix $HOME/sdl2-vulkan-install`, use that path instead:

```bash
LD_LIBRARY_PATH=$HOME/sdl2-vulkan-install/lib:$LD_LIBRARY_PATH ./release/idtech3.aarch64 +set cl_renderer vulkan
```

To verify which SDL is used: `ldd ./release/idtech3.aarch64 | grep SDL` (should show your install path).

To use your custom SDL without replacing the system one, install to a prefix and set `LD_LIBRARY_PATH`:

```bash
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=$HOME/sdl2-vulkan-install \
  -DSDL_VULKAN=ON -DSDL_X11=ON -DSDL_WAYLAND=ON
ninja && ninja install
# Run engine with:
LD_LIBRARY_PATH=$HOME/sdl2-vulkan-install/lib:$LD_LIBRARY_PATH ./release/idtech3.aarch64 +set cl_renderer vulkan
```

### Video codecs (FFmpeg, AV1, VP8/VP9, Theora)

To enable all video codecs (fixes "No decoder available for codec FFmpeg"), install the development packages and rebuild:

```bash
./scripts/install_video_codecs.sh
./scripts/compile_engine.sh vulkan
```

This installs libavcodec-dev, libavformat-dev, libdav1d-dev, libvpx-dev, libtheora-dev. Build natively on the Raspberry Pi for codec support; cross-compilation disables codecs by default.

### Build Troubleshooting

- **Missing libstdc++-14-dev**: On Ubuntu 24.04 cross-compiling ARM, install `libstdc++-14-dev` so Clang can link C++.
- **Shader compilation**: Ensure `glslangValidator` and Python 3 are installed. Run `./scripts/compile_shaders.sh` before building.

### Full setup (recommended)

One script to install SDL with Vulkan and all video codecs:

```bash
./scripts/setup_rpi_full.sh
./scripts/compile_engine.sh vulkan
./release/run_vulkan.sh
```

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
