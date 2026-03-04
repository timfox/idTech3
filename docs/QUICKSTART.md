# Quick Start (End Users)

Get the idTech3 engine running with game data in a few steps.

## 1. Download

Get the latest release from [Releases](https://github.com/timfox/idTech3/releases). Download the archive for your platform (e.g. `idtech3-v1.0.0-linux-x86_64.zip`).

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
./idtech3 +set r_renderer opengl
```

## Steam Deck

When running under Steam on Steam Deck, the engine auto-detects the device and loads `base/steamdeck.cfg` (gamepad enabled, Vulkan, 60 FPS cap). Ensure `steamdeck.cfg` exists in your base folder. Build with `-DUSE_STEAM=ON -DSTEAMWORKS_SDK=/path/to/sdk` for full Steam API support.

## Troubleshooting

- **"No game data"** — Ensure `baseq3/` (or `base/`) exists with at least one `.pk3` file.
- **Black screen / no render** — Try OpenGL: `+set r_renderer opengl`
- **Solid color / dark brown / dark green / no UI** — Ensure FBO is enabled: `+set r_fbo 1` and run `vid_restart`. If still broken, try `r_exposure_auto 0`, `r_volumetricFog 0`, then `vid_restart`. As last resort, `r_fbo 0` disables HDR/post-processing.
- **Missing libraries** — On Linux, install SDL2, OpenAL, and Vulkan drivers for your GPU.
