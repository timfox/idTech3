# idtech3_demo — example “game” mod (config + pack)

This is **not** a standalone game: it is a **tiny mod** (`.pk3` of configs) you layer on top of a **full** compatible `base` / `baseq3` (Q3A-class assets, VMs, maps). It exists to **demo renderer and engine cvars** without maintaining a fork of `qagame`.

## What you get

| Artifact | Description |
|----------|-------------|
| `idtech3_demo.pk3` | Config mod: renderer cvars, **Duktape** `scripts/js/demo_hooks.js` (map_load / frame / HUD), and gameplay-system hints (`buildnavmesh`, `cl_*` cvars). |
| `idtech3_demo_helper` | Optional tiny host binary that prints launch hints (built when `BUILD_EXAMPLE_DEMO_GAME=ON`). |

### “Playable” without custom qagame

You still need **stock** `qagame` / maps from your `base/`. This mod adds:

1. **Renderer demo** — `demo_features.cfg` turns on PBR, volumetric fog, SSR, atmosphere, veg wind (Vulkan).
2. **Lightweight game code** — `demo_hooks.js` registers `idtech3.on('map_load')` and `idtech3.on('frame')` and draws an occasional HUD line (proves the `idtech3` Duktape API in `src/qcommon/js_debug.c`).
3. **Subsystem hooks** — the engine already runs Director, Horde bridge, particles, nav crowd, behavior trees, etc. in `CL_GameFrame` when `cl_physicsEnabled` / `cl_navEnabled` / … are on — see `demo_gameplay.cfg` and `buildnavmesh`.

## Build the demo pack

From a configured build directory:

```bash
cmake -S . -B build-vk-Release -DBUILD_EXAMPLE_DEMO_GAME=ON
cmake --build build-vk-Release --target demo_game_pk3
```

The `.pk3` is written to **`build-vk-Release/idtech3_demo.pk3`** (same dir as other outputs).

Or use the wrapper:

```bash
./examples/demo_game/build_demo_pack.sh
```

CI / pre-push: the repo test `test_demo_game_pk3` packs the same file set with `cmake -E tar` (no full engine configure). Run `ctest -R test_demo_game_pk3` or `./tests/scripts/test_demo_game_pk3.sh`.

## Install and run

Quick path: use the **[demo skeleton](../demo_skeleton/README.md)** (`run_demo_client.sh`, `local.env`) for a ready folder layout and launch commands.

1. Copy **`idtech3_demo.pk3`** into a mod folder next to your game data, e.g. `idtech3_demo/` beside `base/`:

   ```
   YourInstall/
   ├── idtech3
   ├── base/              # or baseq3 — full game pk3s
   └── idtech3_demo/
       └── idtech3_demo.pk3
   ```

2. Launch (Linux example — adjust paths and `fs_basegame` if you use `baseq3`):

   ```bash
   ./idtech3 +set fs_basepath /path/to/YourInstall \
             +set fs_game idtech3_demo \
             +set cl_renderer vulkan
   ```

3. On load, `autoexec.cfg` runs `demo_features.cfg`, `demo_js.cfg` ( **`js_reload scripts/js/demo_hooks.js`** ), and `demo_gameplay.cfg`. Edit files under `mod/` and rebuild `demo_game_pk3`.

**JavaScript:** requires **USE_DUKTAPE** in the engine build. If `js_reload` fails, check the console; ensure `js_allowEvents` is `1` (default).

## Maps

This pack does **not** ship BSPs. Use any map you have (e.g. `q3dm1`) after connect:

```bash
+map q3dm1
```

For renderer regression maps, use the official regression pack and docs under `docs/samples/renderer_regression/`.

## Legal

Do not redistribute commercial game `.pk3` files. This example only ships **text configs** you build yourself.
