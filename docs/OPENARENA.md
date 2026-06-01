# OpenArena and classic QVM mods

This fork targets a **modern Vulkan renderer** while keeping **Quake III Arena** and **OpenArena**-style `.pk3` stacks loadable via **QVM** (`qagame.qvm`, `cgame.qvm`, `ui.qvm`). Gameplay and network protocol are unchanged unless you change them in mod data.

## Quick start

1. Build the engine: `./scripts/compile_engine.sh vulkan`
2. Point at your game data (OpenArena example):

```bash
export OA_BASE=/path/to/openarena/base
./scripts/run_openarena.sh
```

Or manually:

```bash
./release/idtech3 +set fs_basegame base +set fs_game "$OA_BASE" +exec q3_vulkan_compat
```

3. In-game, for a conservative Vulkan preset on classic maps:

```
classic_mod
vid_restart
```

Or persist: `seta r_classicMod 1` then `vid_restart`.

## Presets

| Control | Purpose |
|---------|---------|
| `r_classicMod 1` | Disables heavy Vulkan features at init (Forward+, volumetrics, RTX demo, VDB, veg wind, etc.) |
| `classic_mod` | Sets `r_classicMod 1`, turns off FLUX/TRELLIS/spec_energy client hooks, prompts `vid_restart` |
| `r_rpi_profile 1` | Raspberry Pi (V3DV) performance preset (see [ARM_RASPBERRY_PI.md](ARM_RASPBERRY_PI.md)) |
| `examples/q3_vulkan_compat.cfg` | Optional HDR/FBO tuning; copy into your gamedir |

`r_classicMod` defaults to **0** so new projects keep modern features until you opt in.

## Validation (no retail pk3)

```bash
./scripts/q3_openarena_compat_check.sh release
./scripts/smoke_test.sh release
./scripts/run_renderer_tier_b_devdata.sh
```

With a full OpenArena or Q3A tree:

```bash
export GAME_BASE=/path/to/your/base
./scripts/renderer_regression_maps.sh
```

## Troubleshooting

See [Q3_OPENARENA_VULKAN.md](Q3_OPENARENA_VULKAN.md) and [COMPATIBILITY.md](COMPATIBILITY.md#vulkan-with-quake-iii-arena--openarena).

| Symptom | Fix |
|---------|-----|
| Vulkan fails at startup | Engine falls back to OpenGL when SDL has no Vulkan; or `+set cl_renderer opengl` |
| Wrong brightness on mod BSPs | `r_lightmap_srgb_decode 1`; `vid_restart` |
| Heavy fog on foliage | `r_volumetricFog 0` or `classic_mod` + `vid_restart` |
| Compare to stock | `+set cl_renderer opengl` on the same install |

## Related

- [COMPATIBILITY.md](COMPATIBILITY.md) — QVM vs native modules
- [SPEC_ENERGY.md](SPEC_ENERGY.md) — optional generative pipeline (off by default)
- [QUICKSTART.md](QUICKSTART.md) — engine install
