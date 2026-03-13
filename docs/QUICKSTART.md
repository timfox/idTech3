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
- `idtech3` (or `idtech3.exe` on Windows) — game client
- `idtech3_server` (or `idtech3_server.exe`) — dedicated server
- `idtech3_vulkan.so` / `idtech3_opengl.so` — renderer plugins (Linux)

## 2. Game Data

The engine needs game data (maps, textures, sounds). You must provide a compatible game base, for example:

- **Quake III Arena** — copy or symlink the `baseq3` folder from your Q3A installation into the engine directory
- **Open Arena** or other Q3-based games — same idea: the engine expects a `baseq3` (or `base`) folder with `.pk3` files

Typical layout:
```
idtech3/
├── idtech3
├── idtech3_server
├── idtech3_vulkan.so
├── idtech3_opengl.so
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

## 4. Renderer

The default renderer is Vulkan. To use OpenGL instead:
```bash
./idtech3 +set cl_renderer opengl
```

**PBR (Physically Based Rendering)** is on by default when using Vulkan with FBO. Ensure `r_fbo 1` (default) and `r_pbr 1` (default). If PBR is disabled at startup, the console will show why (e.g. "requires r_fbo 1"). Use `vid_restart` after changing these.

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

- **"No game data"** — Ensure `baseq3/` (or `base/`) exists with at least one `.pk3` file.
- **Black screen / no render** — Try OpenGL: `+set cl_renderer opengl`
- **Solid color / dark brown / dark green / no UI** — Ensure FBO is enabled: `+set r_fbo 1` and run `vid_restart`. If still broken, try `r_exposure_auto 0`, `r_volumetricFog 0`, then `vid_restart`. As last resort, `r_fbo 0` disables HDR/post-processing.
- **Missing libraries** — On Linux, install SDL2, OpenAL, and Vulkan drivers for your GPU.
