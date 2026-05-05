# idtech3_demo - example “game” mod (config + pack)

This is **not** a standalone game: it is a **tiny mod** (`.pk3` of configs) you layer on top of a **full** compatible `base` / `baseq3` (Q3A-class assets, VMs, maps). It exists to **demo renderer and engine cvars** without maintaining a fork of `qagame`.

## What you get

| Artifact | Description |
|----------|-------------|
| `idtech3_demo.pk3` | Config mod + **minimal native UI** (inside the zip as **`vm/ui<arch>.so`** and **`vm/ui.<arch>.so`**, same binary): enough to open a window without retail `ui.qvm`. The engine packs native shared libraries only inside zips; it **extracts** them to **`vm/native_cache/`** under your game home path (toggle **`com_nativeLibraryExtractPk3`**, default **1**) then loads that file, because `dlopen` cannot read zip entries directly. Also **Duktape** (`demo_js.cfg`), optional **Lua** (`demo_lua.cfg` / `scripts/lua/` when built with `USE_LUA`), and gameplay hints. **Bootstrap renderer assets** ship in the same zip (`gameinfo.txt`, `scripts/demo_bootstrap.shader`, `gfx/2d/bigchars.png`, small `gfx/demo/*.png`, **Inter** `fonts/Inter_28pt-Regular.ttf` from repo `fonts/` + `fonts/LICENSE.txt`) so an **empty `base/`** still clears shader init; `demo_features.cfg` sets **`r_font`** to that file. For the filesystem **`default.cfg`** gate, use **`examples/demo_skeleton/base/z_minimal_bootstrap.pk3`** in **`base/`** (or retail paks). Regenerate PNGs with `python3 examples/demo_game/tools/gen_demo_bootstrap_media.py examples/demo_game/bootstrap_media`. |
| `idtech3_demo_helper` | Optional tiny host binary that prints launch hints (built when `BUILD_EXAMPLE_DEMO_GAME=ON`). |

### “Playable” without custom qagame

**Menus:** the pack includes a tiny **native `ui` shared library** (same API as `ui.qvm`) so the client can start with an **empty `base/`** - you get a window and on-screen hint text only.

**In-game:** you still need **stock** `qagame` / maps from your `base/` to join a map or use full UI. This mod adds:

1. **Renderer demo** - `demo_features.cfg` turns on PBR, volumetric fog, SSR, atmosphere, veg wind (Vulkan).
2. **Lightweight game code** - `demo_hooks.js` registers `idtech3.on('map_load')` and `idtech3.on('frame')` and draws an occasional HUD line (proves the `idtech3` Duktape API in `src/qcommon/js_debug.c`).
3. **Lua (optional)** - `demo_lua.cfg` runs `script_reload scripts/lua/demo_hooks.lua`. Requires a Lua-enabled engine build; otherwise the console reports Lua disabled.
4. **Subsystem hooks** - the engine already runs Director, Horde bridge, particles, nav crowd, behavior trees, etc. in `CL_GameFrame` when `cl_physicsEnabled` / `cl_navEnabled` / … are on - see `demo_gameplay.cfg` and `buildnavmesh`.

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

CI / pre-push: `test_demo_game_pk3` stages the same tree (configs + `cc`-built `vm/ui*.so` when available). Run `ctest -R test_demo_game_pk3` or `./tests/scripts/test_demo_game_pk3.sh`.

## Install and run

Quick path: use the **[demo skeleton](../demo_skeleton/README.md)** (`run_demo_client.sh`, `local.env`) for a ready folder layout and launch commands.

1. Copy **`idtech3_demo.pk3`** into a mod folder next to your game data, e.g. `idtech3_demo/` beside `base/`:

   ```
   YourInstall/
   ├── idtech3
   ├── base/              # or baseq3 - full game pk3s
   └── idtech3_demo/
       └── idtech3_demo.pk3
   ```

2. Launch (Linux example - adjust paths and `fs_basegame` if you use `baseq3`):

   ```bash
   ./idtech3 +set fs_basepath /path/to/YourInstall \
             +set fs_game idtech3_demo \
             +set cl_renderer vulkan
   ```

3. On load, `autoexec.cfg` runs `demo_features.cfg`, `demo_js.cfg`, `demo_lua.cfg`, and `demo_gameplay.cfg`. Edit files under `mod/` and rebuild `demo_game_pk3`.

**Console / HUD text:** `demo_features.cfg` sets **`r_font`** so **`cl_builtInTtf`** (default **1**) rasterizes glyphs at runtime via FreeType—no offline atlas. Optional **SDF** (sharper at extreme scales): set **`r_sdfEnable 1`** and **`r_sdfFont "fonts/demo_console_sdf"`**; regenerate that atlas with `python3 examples/demo_game/tools/gen_demo_console_sdf.py fonts/Inter_28pt-Regular.ttf examples/demo_game/bootstrap_media/fonts` (Pillow, numpy, scipy). If `autoexec.cfg` does not run, `exec demo_features` once or mirror the `r_font` line in `default.cfg`.

**JavaScript:** requires **USE_DUKTAPE** in the engine build. If `js_reload` fails, check the console; ensure `js_allowEvents` is `1` (default).

**Lua:** requires **USE_LUA** and a found Lua library at CMake configure time. If `script_reload` reports Lua disabled, reconfigure the engine with Lua dev packages installed.

## Maps

This pack does **not** ship BSPs. Use any map you have (e.g. `q3dm1`) after connect:

```bash
+map q3dm1
```

For renderer regression maps, use the official regression pack and docs under `docs/samples/renderer_regression/`.

## Legal

Do not redistribute commercial game `.pk3` files. This example ships **configs you edit in-tree**, **GPL engine-authored PNG placeholders**, the **demo UI native stub** you compile, and **Inter** (SIL Open Font License 1.1 — see `examples/demo_game/bootstrap_media/fonts/LICENSE.txt` in the source tree and `fonts/LICENSE.txt` inside the zip).
