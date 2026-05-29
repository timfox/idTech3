# Quake III Arena and OpenArena on Vulkan

Quick reference for playing classic QVM-based `.pk3` mods on this engine’s default Vulkan renderer.

## Game data

| Game | Typical folder | Engine default |
|------|----------------|----------------|
| Quake III Arena | `baseq3/` with `pak0.pk3` … | `fs_basegame` defaults to `baseq3/base` |
| OpenArena | Your OA gamedir (often `base/` or `baseoa/`) | Set `+set fs_basegame <name>` if not under `baseq3/` |

See [QUICKSTART.md](QUICKSTART.md) and [COMPATIBILITY.md](COMPATIBILITY.md#legacy-quake-3-and-openarena-style-mods-qvm).

## After building the engine

```bash
./scripts/q3_openarena_compat_check.sh release
./scripts/smoke_test.sh release
./scripts/run_renderer_tier_b_devdata.sh   # optional: no retail pk3
```

With a full install:

```bash
export GAME_BASE=/path/to/baseq3   # or your OA base folder
./scripts/renderer_regression_maps.sh
```

## Optional in-game tuning

Copy [examples/q3_vulkan_compat.cfg](../examples/q3_vulkan_compat.cfg) into your gamedir and run `+exec q3_vulkan_compat`.

| Issue | Try |
|-------|-----|
| Mod maps too dark/bright (HDR) | `r_lightmap_srgb_decode 1` |
| Black screen / broken UI | `r_fbo 1`, then `r_exposure_auto 0`, `r_volumetricFog 0`; last resort `r_fbo 0` |
| Fog on alpha foliage (OA) | `r_volumetricFog 0` |
| Vulkan won’t start | Auto-fallback to OpenGL when SDL has no Vulkan window (log: `falling back to OpenGL`). Or `./release/run_vulkan.sh`, install a Vulkan ICD, or `+set cl_renderer opengl` |

## What automation does *not* prove

- Retail menu art, full UI flows, or GPU framebuffer parity (Tier C — [renderer_validation/FINDINGS.md](renderer_validation/FINDINGS.md)).
- Multiplayer against arbitrary Internet servers (protocol surface is preserved; test your targets).

## Related

- [COMPATIBILITY.md](COMPATIBILITY.md) — platform matrix and troubleshooting
- [HDR_GAPS.md](HDR_GAPS.md) — lightmaps, dynamic lights, volumetrics
- [PRODUCTION_CERTIFICATION.md](PRODUCTION_CERTIFICATION.md) — Tier A–D evidence
