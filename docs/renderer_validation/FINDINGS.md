# Renderer validation findings (devdata bring-up)

Date: 2026-04-03  
Environment: Linux headless agent (no GPU / no client run).

## What was brought up

- **Minimal IBSP v46 stubs** (`scripts/tools/gen_rtest_bsp.py`): six `maps/rtest_*.bsp` (collision-only empty world, ~400 bytes each). **Not** authored visual regression geometry — replace with real maps from `docs/samples/renderer_regression/scenes/` when available.
- **`qagame.qvm`**: ioquake3 `Release/baseq3/vm/qagame.qvm` (GPLv2+). See `NOTICE-ioquake3-qvm.txt`.
- **`default.cfg`**: `bot_enable 0` — required because stock qagame otherwise initializes botlib and fails without `scripts/weapons.c`, `bots.txt`, etc.
- **`z_renderer_regression.pk3`**: built by `scripts/build_renderer_devdata.sh` (includes `default.cfg`, maps, `vm/qagame.qvm`).

## Automated results (this environment)

| Check | Result |
|-------|--------|
| `GAME_BASE=.../devdata/rtest_base` + `GAME_ASSETS_LIST=.../devdata/OPTIONAL_GAME_ASSETS.txt` → `renderer_regression_check.sh` | PASS |
| `renderer_regression_maps.sh` (with `+set vm_game 2 +set bot_enable 0`) | PASS — all six maps reach `Server: <map>` without `ERROR:` / `CM_LoadMap` failures |

## Manual / visual (Vulkan vs OpenGL)

**Not executed here** — no display client run. The devdata BSPs are also **not drawable** client content (no draw surfaces). Visual parity and volumetric behavior remain **unproven** until real maps + client session.

## Engine changes made

1. **`renderer_regression_maps.sh`**: `+set vm_game 2` (force QVM when native `qagame` absent), `+set bot_enable 0` (avoid botlib on minimal bases).
2. **`renderer_regression_check.sh`**: optional `GAME_ASSETS_LIST` env override for alternate asset manifests (e.g. devdata list vs samples template).

## Known limitations / next steps

1. Replace stub BSPs with **authored** regression maps + textures/shaders; keep `bot_enable 0` or ship minimal bot scripts if needed.
2. Run **client** Vulkan/OpenGL passes per `docs/RENDERER_CONFIDENCE.md` on a machine with GPU + display.
3. Optional: CI job that runs `build_renderer_devdata.sh` then both regression scripts (requires ioq3 checkout or cached `qagame.qvm`).

## Volumetric / Navier–Stokes

No new proof: dedicated path never exercises GPU froxel/fog. Stubs only ensure map name `rtest_volumetric` loads through **CM + qagame** init.
