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
./scripts/compile_engine.sh vulkan rtx      # Same + USE_VULKAN_RTX=ON (RTX demo path)
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
- **Neural Dynamic GI (Vulkan, experimental)**: **`r_ndgi` 1** (latched) blends temporal baked-GI states from a neural feature atlas into merged BSP lightmaps (day/night, weather); **`r_ndgi_cycle`**, manifest **`ndgi/<map>.ndgi`**, commands **`ndgi_reload`** / **`ndgi_status`**. See **`docs/NEURAL_DYNAMIC_GI.md`**.
- **Neural Irradiance Volume (Vulkan, experimental)**: **`r_niv` 1** (latched) decodes a compact 3D neural probe field from G-buffer depth/normals into additive indirect (`r_niv_scale`, **`niv/<map>.niv`**). See **`docs/NEURAL_IRRADIANCE_VOLUME.md`**.
- **Neural Visibility Cache (Vulkan, experimental)**: **`r_nvc` 1** (latched) + **`r_forwardPlus 1`** — Forward+ tile lights with neural visibility cache and simplified ReSTIR DI refine at disocclusions (`r_nvc_scale`, **`nvc/<map>.nvc`**). See **`docs/NEURAL_VISIBILITY_CACHE.md`**.
- **Forget Superresolution / Sample Adaptively (Vulkan, experimental)**: **`r_fsa` 1** (latched) — importance map for sub-1-SPP path tracing + guided denoise; pair with **`r_rtx`**, **`r_rtxDemo`**, **`r_fsa_budget`**. See **`docs/FORGET_SUPERRESOLUTION_FSA.md`**.
- **Vertex Features Neural GI (Vulkan, experimental)**: **`r_vfgi` 1** (latched) + **`r_fbo` 1** — per-vertex GI features from BSP world meshes + coarse grid index (`r_vfgi_scale`, **`vfgi/<map>.vfgi`**). See **`docs/VERTEX_FEATURES_NEURAL_GI.md`**.
- **RenderFormer preview (Vulkan, experimental)**: **`r_renderformer` 1** (latched) + **`r_fbo` 1** — triangle-token transport + view decode for neural GI-style preview (`r_renderformer_scale`, **`renderformer/<map>.rfm`**). See **`docs/RENDERFORMER.md`**.
- **Neural renderer phases (hub)**: Phase 1–3 map (NIV, NSLM, NDGI, WPT, NIST, NVC, VFGI, MGS, GRTX, VUDA, RenderFormer) — **`docs/NEURAL_RENDERER_PHASES.md`**. Shared weight/volume loaders: **`vk_neural_io.c`** (`NIV1`/`NIV2`, `NSL1`/`NSL2`, `NIS1`, `NVC1`, `VFG1`).
- **Wavefront path experiment**: **`r_wpt` 1** + **`r_fbo` 1** — ray queue + screen-space bounce waves; pair with **`r_fsa`** / **`r_rtx`** for adaptive hardware traces. See **`docs/WAVEFRONT_PATH_TRACING.md`**.
- **VUDA CUDA-Vulkan multiplexing (experimental)**: build **`./scripts/compile_engine.sh vulkan vuda`**, then **`r_vuda` 1** + **`cl_vuda` 1** + **`vid_restart`** — KHR external memory fd interop + compute window after submit (`vuda_status`). See **`docs/VUDA.md`**.
- **Neural Six-way Lightmaps (Vulkan, experimental)**: **`r_nslm` 1** (latched) modulates volumetric froxel scatter after temporal fog (`r_volumetricFog 1`, **`nslm/<map>.nslm`**, **`nslm_reload`** / **`nslm_status`**). See **`docs/NEURAL_SIXWAY_LIGHTMAPS.md`**.
- **Neural Image Space Tessellation (Vulkan, experimental)**: **`r_nist` 1** (latched) refines low-poly silhouettes from G-buffer depth/normals (`r_nist_scale`, **`nist/<map>.nist`**, **`nist_reload`** / **`nist_status`**). See **`docs/NEURAL_IMAGE_SPACE_TESSELLATION.md`**.
- **Gaussian ray tracing / GRTX (Vulkan, experimental)**: **`r_grtx` 1** (latched, **`USE_VULKAN_RTX`** build) traces 3D Gaussian AABB proxies via KHR RT (`r_grtxDemo 1`, **`r_grtxMaxGaussians`**, **`grtx_status`**). See **`docs/GAUSSIAN_RAY_TRACING_GRTX.md`**.
- **Mobile-GS (Vulkan, experimental)**: **`r_mgs` 1–3** (latched) tiered compute splatting for mobile-class GPUs—no RTX (`mgs_status`, **`r_mgs_maxSplats`**, **`r_mgs_scale`**). See **`docs/MOBILE_GAUSSIAN_SPLATTING.md`**.
- **WebSplatter (Vulkan, experimental)**: **`r_wsp` 1–3** (latched) WebGPU-aligned **tile-binned** splats (`wsp_status`); takes precedence over **`r_mgs`** when both enabled. See **`docs/WEB_SPLATTER.md`**.
- **Vulkan Forward+ / TAA / deferred**: **`r_forwardPlus` 1** (default), up to **64** GPU dlights; **`r_renderMode 2`** latches Forward+ shade. **`r_renderMode 1`** + **`r_deferredLighting 1`**: G-buffer fill + additive dynamic (`r_deferredUnlitBase` 1); skips classic projector when active. **`r_taa 1`** optional temporal resolve. See **`docs/FORWARD_PLUS_PIPELINE_AUDIT.md`**, **`docs/RENDERERS.md`**, **`docs/HDR_GAPS.md`** §6.8.
- **Billboard / flipbook / imposter (engine-native)**: Map entities **`misc_*`** parsed on **`LoadWorld`** (`r_spriteProps 1`, auto-disabled when **`CS_ENGINE_SPRITE_META`** set). Server: **`sv_engineSprites`**, **`sv_engineSpritesSpawn`**, console **`sv_sprite_spawn`**. Client: **`cl_engineSprites`**, **`sprite_spawn`**, cgame trap **`trap_EngineSpriteAddLocal`**. QVM game traps: **`G_ENGINE_SPRITE_SHADER_INDEX`**, **`G_ENGINE_SPRITE_SPAWN`**. Lua: **`Engine.Sprites.spawnLocal` / `spawnServer`**. Demo: **`demo_sprites.cfg`**, **`lua_run demo_run_sprites()`**.
- **FreeUSD (default ON)**: Git submodule **`src/external/FreeUSD`** ([gopexllc/FreeUSD](https://github.com/gopexllc/FreeUSD)); **`USE_FREEUSD=ON`** in CMake and **`./scripts/compile_engine.sh`** (auto `git submodule update --init`; pass **`nofreeusd`** to disable). Targets link **`freeusd::runtime`** + **`freeusd::c`** via **`idtech3_freeusd`**. Renderer **`r_freeusd` 1**; client **`usd_*`** tools. Init: **`./scripts/init_optional_submodules.sh --freeusd`**. See **`docs/FREEUSD.md`**.

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

- **FreeUSD** (`src/external/FreeUSD`): default build; see `docs/FREEUSD.md`.
- **idTech3 Backend** (`src/external/idtech3backend`, [timfox/idtech3backend](https://github.com/timfox/idtech3backend)): `./scripts/init_optional_submodules.sh --backend` — optional game/backend tree; not linked into engine CMake by default; see `docs/IDTECH3_BACKEND.md`.
- **Tiled Map Editor** (`tools/tiled`, GPL-2.0): `./scripts/init_optional_submodules.sh --tiled` — not built by `compile_engine.sh`; see `docs/TILED.md`.
