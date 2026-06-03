# AGENTS.md

## Cursor Cloud specific instructions

### Overview

This is an **idTech3 engine fork** - a C/C++ game engine based on Quake III Arena with Vulkan 1.4 + RTX rendering, PBR, audio codecs (Opus/FLAC/WebM/MP3), Lua/Duktape scripting, and ImGui debug UI (optional **Studio** session + command strips via `r_studio_tools`; see `docs/IN_ENGINE_STUDIO_TOOLS.md`). It produces a client (`idtech3`), dedicated server (`idtech3_server`), and the Vulkan renderer plugin (`idtech3_vulkan.so`).

### Building

See `CLAUDE.md` for canonical build commands. The primary build script is `./scripts/compile_engine.sh`. Key examples:

```
./scripts/compile_engine.sh vulkan          # Vulkan renderer, Release
./scripts/compile_engine.sh vulkan debug    # Vulkan renderer, Debug
./scripts/compile_engine.sh vulkan demo     # Also builds idtech3_demo.pk3 → release/demo_game/
./scripts/compile_engine.sh clean vulkan    # Clean build
```

Build artifacts go to `build-vk-Release/` and are copied to `release/`.

### Gotchas

- **C++ linker dependency**: The build requires `libstdc++-14-dev` because Clang 18 (the default `c++` on Ubuntu 24.04) selects the GCC 14 installation but only GCC 13 dev files are installed by default. The update script installs this.
- **No game data**: The engine repo does not include Quake III Arena game data (`.pk3` files). The dedicated server will print "No game data" and exit cleanly - this is expected. `SKIP_IDPAK_CHECK=ON` is set by default in `compile_engine.sh`.
- **Test suite**: Run `make test` or `ctest` from the build directory to execute the smoke test (binary checks, server startup, shader validation). Full validation is via build matrix (`.github/workflows/build.yml`) and manual testing.
- **Headless environment**: The client executable (`idtech3`) requires a display server (X11/SDL2) and GPU. In headless Cloud Agent VMs, only the dedicated server (`idtech3_server`) can run. The client binary can still be verified via `file` and `ldd` checks.
- **Shader compilation**: Vulkan GLSL shaders are compiled to SPIR-V during the CMake build via `scripts/compile_shaders.sh`. This requires `glslangValidator` (from `glslang-tools`) and Python 3.
- **SDF UI (Vulkan):** `r_sdfScreenAa` scales `fwidth`-based edge AA for `uiSdfText`; re-run `compile_shaders.sh` after editing `frag_ui_sdf_text.frag` / `sdf_text.frag`.
- **Console / HUD fonts**: With **`cl_builtInTtf` 1** (default) and a valid **`r_font`** `.ttf`, FreeType draws engine console and small HUD text before optional pre-baked SDF (`r_sdfEnable`). Use **`cl_builtInTtf 0`** to prefer SDF when both are configured. Optional **`r_vectorFont 1`** (Vulkan) uses GPU vector outlines (Lengyel 2017); **`r_vectorFontMode 2`** reserves Loop & Blinn + mesh glyphlets per AMD GPUOpen (not implemented yet). See **`docs/VECTOR_FONT.md`**. Tune rasterization with **`r_fontDpi`** (e.g. **96**), **`r_fontHint`** (default **1**), and **`r_fontMipmap`** (default **1**, atlas mip chain for minified text); apply with **`reloadTtf`** or **`vid_restart`**. Client cvars: **`r_fontConsoleAlign`** (baseline in cell), **`r_fontShadow`** (0–8, 0=no shadow), **`r_fontSubpixel`** (optional 0.375px nudge).
- **FonTS (ICCV 2025) + FLUX**: In-engine image generation uses **`flux_generate`** (cflux2). The separate [FonTS](https://github.com/ArtmeScienceLab/FonTS) typography pipeline is optional: set **`cl_fonts_enable` 1**, **`cl_fonts_repo`**, and **`cl_fonts_cmd`**, then run **`fonts_pipeline`** (see **`docs/FONTS.md`**).
- **TRELLIS.2 (image-to-3D)**: Runtime pipeline mirroring FLUX: **`trellis_generate`** (async default), **`trellis_status`** / **`trellis_cancel`**, **`trellis_view`**, **`trellis_from_prompt`** (FLUX→TRELLIS chain). Set **`cl_trellis_enable` 1** and **`cl_trellis_repo`** (see **`docs/TRELLIS.md`**). Requires Linux + NVIDIA GPU (24GB+); Python/CUDA runs out-of-process like external FLUX.
- **VDB volumetric fog (Vulkan)**: **`r_vdb` 1** (default) loads `.nvdb` (NanoVDB leaf/blind CPU decode → GPU 3D texture); console **`vdb_load`**, **`vdb_upload`**, **`vdb_bind_fog`**, **`vdb_list`**, **`vdb_rebuild_majorant`**. Typical path: `vdb_load path/to/grid.nvdb` → `vdb_upload 0` → `vdb_bind_fog 0` → `r_volumetricFog 1` + **`r_vdbFog 1`**. **`r_volumetricFogIntegration 3`** + **`r_vdbMajorantBrick`** enable OpenVDB majorant grid + Woodcock/delta tracking (arXiv:2211.09997-style, real-time). See **`docs/VDB_WOODCOCK_VOLUMETRICS.md`**. Unit test: **`unit_nanovdb_decode`**. Lua: `VDB.load` / `bindAsFog` in game module when enabled.
- **FreeUSD (default ON)**: **`USE_FREEUSD=ON`** in CMake and **`./scripts/compile_engine.sh`** (pass **`nofreeusd`** to disable). Renderer **`r_freeusd` 1** + **`r_freeusdShaderMap` 1** load `.usda`/`.usd` (UsdGeom.Mesh, PreviewSurface → Q3 shader, `primvars:st`); client **`usd_*`** tools including **`usd_shader_map`** / **`usd_load`**. Demo pk3 ships `models/*.usda` + `demo_usd.cfg`. See **`docs/FREEUSD.md`**.

### Linting / Static Analysis

`scripts/run_clang_tidy.sh` and `scripts/run_cppcheck.sh` are available for optional local static analysis. CI primarily enforces quality through compiler warnings (`-Wall -Wextra -Wpedantic` and more), with `CI_BUILD=OFF` in the current workflow so warnings are not treated as errors by default.

### Running

- **Dedicated server**: `./release/idtech3_server +set dedicated 1 +set com_hunkMegs 64`
- **Client** (requires display): `./release/idtech3`
- Both require game data in a `base/` directory to do anything meaningful.

### Game data / base

- **Standalone full conversion**: Do not assume Q3A, OpenArena, or other generic bases. The base is either Unwaking or a game explicitly defined by the user.
- **Smallest valid data tree** (bootstrap `.pk3` + `default.cfg`): see `docs/MINIMAL_GAME_SHELL.md`.

### Scripting

- **Lua:** `script_reload` (default build `USE_LUA=ON`).
- **JavaScript (Duktape):** `js_reload` (default `USE_DUKTAPE=ON`).
- **C# (Mono):** `cs_reload` when built with `./scripts/compile_engine.sh vulkan csharp` or `-DUSE_CSHARP=ON` + `libmono-2.0-dev` / `mono-devel`; see `docs/CSHARP.md`.

### Optional submodules

- **Tiled Map Editor** (`tools/tiled`, GPL-2.0): `./scripts/init_optional_submodules.sh --tiled` — not built by `compile_engine.sh`; see `docs/TILED.md`.
