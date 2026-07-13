# Platform Compatibility

This document summarizes compatibility considerations, known issues, and mitigations across supported platforms.

## Platform Support Matrix

| Platform | Vulkan | Video Codecs | Notes |
|----------|--------|--------------|-------|
| Linux x86_64 | ✅ | FFmpeg, dav1d, vpx, Theora (if deps installed) | Primary development target |
| Linux aarch64 (RPi 4/5) | ✅ (SDL with Vulkan) | If deps installed; cross-compile disables by default | See [ARM_RASPBERRY_PI.md](ARM_RASPBERRY_PI.md) |
| Linux armv7 | Varies | Same as aarch64 | Limited Vulkan; see platform notes |
| Windows x64 | ✅ | Same | |
| macOS Apple Silicon | ✅ | Same | |
| Android | ✅ | Varies by build | Separate init path |

## Compatibility Principles

1. **Graceful degradation**: High-quality features → lower quality or disabled when hardware or deps are missing
2. **Graceful degradation**: Missing optional deps (codecs, etc.) disable features without crashing
3. **Backward compatibility**: Existing mods, configs, and game data continue to work — see [core/LEGACY_AND_MODERN.md](core/LEGACY_AND_MODERN.md) for the 2026 profile/symlink contract.
4. **Clear diagnostics**: Startup logs and error messages point to fixes

## Platform-Specific Notes

### Raspberry Pi 5

- **SDL**: System SDL often lacks Vulkan. Run `./scripts/build_sdl_vulkan_rpi.sh` and use `./release/run_vulkan.sh`.
- **Video codecs**: Run `./scripts/install_video_codecs.sh` before building.
- **Performance**: Use `r_rpi_profile 1` for V3DV-friendly defaults.
- **Display**: Prefer X11; r_mode -1 with 640x480 is more reliable than desktop resolution.

### Low-Memory Systems

- Reduce `com_hunkMegs` (default 256): `+set com_hunkMegs 128` for dedicated server or constrained clients.
- Disable heavy features: `r_ssao 0`, `r_volumetricFog 0`, `r_fogFluid 0`, `r_rpi_profile 1`.

### Cross-Compilation

- aarch64 cross-build disables FFmpeg/dav1d/vpx/Theora by default. Add `codecs` to try: `./scripts/compile_engine.sh vulkan aarch64 codecs` (requires ARM dev packages in sysroot).
- Native build on target is recommended for full codec support.

## Cvar Compatibility

| Cvar | Purpose | Fallback |
|------|---------|----------|
| `cl_renderer` | Renderer selection (`vulkan`) | Requires working Vulkan + SDL Vulkan support |
| `renderer` | Alias for `cl_renderer` | Synced to `cl_renderer` |
| `r_vid_driver` | SDL video driver (x11, wayland, kmsdrm) | Auto-retry wayland if x11 fails on ARM |
| `r_fbo` | Enable HDR/post-processing | 0 disables if issues |
| `r_rpi_profile` | RPi performance preset | Disables SSAO, volumetrics, etc. |

## VM / Game Module Loading

- **Native first**: Engine looks for `module.so`, `module.arch.so` (e.g. `client.aarch64.so`), `modulearch.so`.
- **QVM fallback**: If native not found, tries `module.qvm`.
- **Diagnostics**: Missing `ui.qvm` or native UI prints clear messages.

See also [Q3_OPENARENA_VULKAN.md](Q3_OPENARENA_VULKAN.md) for a short play + validation checklist.

### Legacy Quake 3 and OpenArena-style mods (QVM)

Retail **Quake III Arena**, **OpenArena**, and most classic **`.pk3` mods** expect **bytecode QVMs** (`qagame.qvm`, `cgame.qvm`, `ui.qvm`) and the original filesystem / protocol surface. This fork **keeps the QVM path** (`Q3_VM` / `vm.c`) and native-vs-QVM selection so those games and mods remain loadable without forcing a native-only toolchain. Engine and renderer enhancements must **not** remove or silently break QVM execution for this class of content unless a deliberate, documented compatibility decision is made (see project constitution).

### Full conversions (native game code)

**Standalone** or **full-conversion** games may ship **only native** game/UI modules (e.g. `vm/qagame*.so`, `ui*.dll`) and omit `.qvm` files. `VM_Create` tries **native** shared libraries first when allowed (`fs_restrict` off); if a valid native module is present, QVM is not required. This is the supported path for modern total conversions that own their entire `base`/gamedir and build native game logic.

Details: [ARCHITECTURE.md](ARCHITECTURE.md#native-game-modules-vm) (`vm.c`, `vm_native_module.c`, `FS_LoadLibrary`).

## Troubleshooting Quick Reference

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| "No decoder available for codec FFmpeg" | Codec libs not installed | `./scripts/install_video_codecs.sh` + rebuild |
| "Couldn't get a visual" / Vulkan fails | SDL without Vulkan | `./scripts/build_sdl_vulkan_rpi.sh`; use `run_vulkan.sh` |
| Vulkan init fails on Pi | System SDL without Vulkan | `./scripts/build_sdl_vulkan_rpi.sh`; `run_vulkan.sh` or `LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH` |
| "VM_Create on UI failed" | Missing ui.qvm / native UI | Ensure game provides `ui.qvm` or `ui.aarch64.so` in base/ |
| Black screen / wrong colors | FBO/HDR issue | `r_fbo 0` or `r_exposure_auto 0` `r_volumetricFog 0`; `vid_restart` |
| Lighting looks “modern” on baseq3 | Modern renderer features enabled | Default **`r_classicLighting 1`** preserves retail look; **`exec modern_lighting.cfg`** for PBR sun shadow + SH world tint |

### Classic vs modern lighting (Vulkan)

Retail **baseq3** with **`cgame.qvm`** auto-loads **`classic_baseq3.cfg`** when **`cl_autoGraphicsProfile 1`** (default): classic lighting, no open-world overlays, physics middleware off.

**Native cgame** modules auto-load **`modern_native.cfg`**, which inherits **`modern_vulkan.cfg`**: Forward+ primary lighting, HDR/PBR/material blending, TAA, deferred G-buffer sidecar, then native-game additions such as PBR sun shadows, volumetrics, SSR, and open world (followed by **`vid_restart`** for latched cvars).

Other **QVM mods** (e.g. OpenArena) keep the conservative renderer defaults (**`r_classicLighting 1`**) without forcing a cfg.

Disable auto selection with **`cl_autoGraphicsProfile 0`**. Manual presets: **`config/modern_vulkan.cfg`** (stable renderer default), **`config/deferred_vulkan.cfg`** (experimental mode-1 deferred lighting), **`config/modern_lighting.cfg`** (lighting only), **`config/modern_native.cfg`**, **`config/classic_baseq3.cfg`**.


- [ARM_RASPBERRY_PI.md](ARM_RASPBERRY_PI.md) - Raspberry Pi setup and Vulkan
- [QUICKSTART.md](QUICKSTART.md) - End-user quick start
- [DEVELOPMENT_SETUP.md](DEVELOPMENT_SETUP.md) - Build environment
- [RENDERERS.md](RENDERERS.md) - Renderer features and architecture
