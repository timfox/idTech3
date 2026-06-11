# AGENTS.md

## Cursor Cloud specific instructions

### Overview

This is an **idTech3 engine fork**

### Client modularization (in progress)

`cl_main.c` is slim glue (~240 LOC): globals, reliable commands, `CL_Init`. Satellite modules: `cl_lifecycle.c` (shutdown/memory/map), `cl_frame.c` (`CL_Frame`), `cl_cvars.c`, plus earlier splits `cl_connect`, `cl_cmds`, `cl_demo`, `cl_download`, `cl_ref`, `cl_gameframe`. Wiring test: `tests/scripts/test_client_modular.sh`. Renderer splits (`tr_bsp.c`, shader init) are **not** started. - a C/C++ game engine based on Quake III Arena with Vulkan 1.4 + RTX rendering, PBR, audio codecs (Opus/FLAC/WebM/MP3), Lua/Duktape/Python scripting, and ImGui debug UI (optional **Studio** session + command strips via `r_studio_tools`; see `docs/IN_ENGINE_STUDIO_TOOLS.md`). It produces a client (`idtech3`), dedicated server (`idtech3_server`), and the Vulkan renderer plugin (`idtech3_vulkan.so`).

### Building

See `CLAUDE.md` for canonical build commands. The primary build script is `./scripts/compile_engine.sh`. **Build profiles** (`IDTECH3_PROFILE`, default **`game`**): `core` | `game` | `full` | `research` — see **`docs/ENGINE_MODULE_MANIFEST.md`**.

```
./scripts/compile_engine.sh vulkan          # game profile (default), Release
./scripts/compile_engine.sh vulkan full     # kitchen-sink parity (all extensions)
./scripts/compile_engine.sh vulkan core     # Q3/OA fast path (no open world / research)
./scripts/compile_engine.sh vulkan debug    # Debug build
./scripts/compile_engine.sh vulkan demo     # Also builds idtech3_demo.pk3 → release/demo_game/
./scripts/compile_engine.sh vulkan rtx      # Same + USE_VULKAN_RTX=ON (RTX demo path)
./scripts/compile_engine.sh clean vulkan    # Clean build
```

Build artifacts go to `build-vk-Release/` and are copied to `release/`.

**Repository layout (2026):** [docs/core/REPOSITORY_LAYOUT_2026.md](docs/core/REPOSITORY_LAYOUT_2026.md) — `src/extensions/`, `samples/`, `third_party/` symlinks, profile matrix in [BUILD.md](BUILD.md). Feature docs: [docs/README.md](docs/README.md) tier hubs (not duplicated below).

### Gotchas

- **C++ linker dependency**: The build requires `libstdc++-14-dev` because Clang 18 (the default `c++` on Ubuntu 24.04) selects the GCC 14 installation but only GCC 13 dev files are installed by default. The update script installs this.
- **C++20 engine modules**: First-party `.cpp` (ECS, nav, physics, **`src/world/*`**, **`cluster_graph.cpp`**, ImGui inspector, FreeUSD) build with **`CMAKE_CXX_STANDARD 20`**; public headers keep **`extern "C"`** ABI. Core engine remains **C23**. Revert guard: **`tests/scripts/test_cpp20_sources.sh`** (`ctest -R test_cpp20_sources`, also in **`smoke_test.sh`**).
- **No game data**: The engine repo does not include Quake III Arena game data (`.pk3` files). The dedicated server will print "No game data" and exit cleanly - this is expected. `SKIP_IDPAK_CHECK=ON` is set by default in `compile_engine.sh`.
- **Test suite**: Run `make test` or `ctest` from the build directory to execute the smoke test (binary checks, server startup, shader validation). Full validation is via build matrix (`.github/workflows/build.yml`) and manual testing.
- **Headless environment**: The client executable (`idtech3`) requires a display server (X11/SDL2) and GPU. In headless Cloud Agent VMs, only the dedicated server (`idtech3_server`) can run. The client binary can still be verified via `file` and `ldd` checks.
- **Shader compilation**: Vulkan GLSL shaders are compiled to SPIR-V during the CMake build via `scripts/compile_shaders.sh`. This requires `glslangValidator` (from `glslang-tools`) and Python 3.
- **SDF UI (Vulkan):** `r_sdfScreenAa` scales `fwidth`-based edge AA for `uiSdfText`; re-run `compile_shaders.sh` after editing `frag_ui_sdf_text.frag` / `sdf_text.frag`.
- **Console / HUD fonts**: With **`cl_builtInTtf` 1** (default) and a valid **`r_font`** `.ttf`, FreeType draws engine console and small HUD text before optional pre-baked SDF (`r_sdfEnable`). Use **`cl_builtInTtf 0`** to prefer SDF when both are configured. Optional **`r_vectorFont 1`** (Vulkan) uses GPU vector outlines: mode **0** = Lengyel 2017 winding; mode **2** = Loop & Blinn glyphlets (vertex path; NV mesh string dispatch pending mesh SPIR-V compile). See **`docs/VECTOR_FONT.md`**. Tune rasterization with **`r_fontDpi`** (e.g. **96**), **`r_fontHint`** (default **1**), and **`r_fontMipmap`** (default **1**, atlas mip chain for minified text); apply with **`reloadTtf`** or **`vid_restart`**. Client cvars: **`r_fontConsoleAlign`** (baseline in cell), **`r_fontShadow`** (0–8, 0=no shadow), **`r_fontSubpixel`** (optional 0.375px nudge).
- **FonTS (ICCV 2025) + FLUX**: In-engine image generation uses **`flux_generate`** (cflux2). The separate [FonTS](https://github.com/ArtmeScienceLab/FonTS) typography pipeline is optional: set **`cl_fonts_enable` 1**, **`cl_fonts_repo`**, and **`cl_fonts_cmd`**, then run **`fonts_pipeline`** (see **`docs/FONTS.md`**).
- **TRELLIS.2 (image-to-3D)**: Runtime pipeline mirroring FLUX: **`trellis_generate`** (async default), **`trellis_status`** / **`trellis_cancel`**, **`trellis_view`**, **`trellis_from_prompt`** (FLUX→TRELLIS chain). Set **`cl_trellis_enable` 1** and **`cl_trellis_repo`** (see **`docs/TRELLIS.md`**). Requires Linux + NVIDIA GPU (24GB+); Python/CUDA runs out-of-process like external FLUX.
- **Genetic GAN (procedural body evolution)**: In-engine genome slots (**`genome_create`**, **`genome_breed`**, **`genome_mutate`**) + optional async GAN decode (**`genome_generate`**, **`genome_view`**) via engine **job pool** + **`Defer_Add`** main-thread import. Queue depth 8; **`cl_mlSerial`** / **`cl_mlUseJobs`**. Set **`cl_geneticGan` 1**; Lua **`Engine.Genome.*`**; see **`docs/GENETIC_GAN.md`**.
- **VDB volumetric fog (Vulkan)**: **`r_vdb` 1** (default) loads `.nvdb` (NanoVDB leaf/blind CPU decode → GPU 3D texture); console **`vdb_load`**, **`vdb_upload`**, **`vdb_bind_fog`**, **`vdb_list`**, **`vdb_rebuild_majorant`**. Typical path: `vdb_load path/to/grid.nvdb` → `vdb_upload 0` → `vdb_bind_fog 0` → `r_volumetricFog 1` + **`r_vdbFog 1`**. **`r_volumetricFogIntegration 3`** + **`r_vdbMajorantBrick`** enable OpenVDB majorant grid + Woodcock/delta tracking (arXiv:2211.09997-style, real-time). See **`docs/VDB_WOODCOCK_VOLUMETRICS.md`**. Unit test: **`unit_nanovdb_decode`**. Lua: `VDB.load` / `bindAsFog` in game module when enabled.
- **Fog bioaerosol ecology (Evans et al. 2019)**: **`r_fogBiology` 0** (default). Demo **`demo_fog_biology.cfg`** (Maine ~30 m) / **`demo_fog_biology_namib.cfg`** (50 km) / **`demo_fog_biology_openworld.cfg`**; console **`fog_biology_paper`**, **`fog_biology_genera`**, **`fog_biology_poll`**, **`fog_biology_sweep`**; optional **`r_fogBiologyCoastAuto 1`**; Lua **`Engine.FogBiology.poll()`**; ImGui reads **`r_fogBiologySync*`** mirrors. SRA **SRP155760** (not reproduced in-engine). See **`docs/FOG_BIOLOGY.md`**.
- **Physics middleware (Bullet substrate + Euphoria/DMM layers)**: **`phys_enabled` 1**, **`cl_physicsEnabled` 1**; **`phys_debugDraw` 1** → Vulkan wireframe; **`phys_status`**; event bus, gameplay materials, active ragdoll motors, ProcAnim tick in **`PhysMiddleware_Frame`**. See **`docs/PHYSICS.md`**.
- **Neural Dynamic GI (Vulkan, experimental)**: **`r_ndgi` 1** (latched) blends temporal baked-GI states from a neural feature atlas into merged BSP lightmaps (day/night, weather); **`r_ndgi_cycle`**, manifest **`ndgi/<map>.ndgi`**, commands **`ndgi_reload`** / **`ndgi_status`**. See **`docs/NEURAL_DYNAMIC_GI.md`**.
- **Neural Irradiance Volume (Vulkan, experimental)**: **`r_niv` 1** (latched) decodes a compact 3D neural probe field from G-buffer depth/normals into additive indirect (`r_niv_scale`, **`niv/<map>.niv`**). See **`docs/NEURAL_IRRADIANCE_VOLUME.md`**.
- **Neural Visibility Cache (Vulkan, experimental)**: **`r_nvc` 1** (latched) + **`r_forwardPlus 1`** — Forward+ tile lights with neural visibility cache and simplified ReSTIR DI refine at disocclusions (`r_nvc_scale`, **`nvc/<map>.nvc`**). See **`docs/NEURAL_VISIBILITY_CACHE.md`**.
- **Vulkan RTX (experimental)**: **`USE_VULKAN_RTX=ON`**, **`r_rtx` 1–3** (latched), **`r_rtxDemo` 1** — world BSP BLAS + depth-guided primary trace; **`r_rtxEntities` 1** adds entity proxy BLAS/TLAS; **`r_rtxTlasUpdate` 1** (default) uses TLAS UPDATE when instance count stable. Console **`rtx_status`**. Hybrid frame order: Hybrid1 → Raygun → RTX demo. See **`docs/RENDERERS_FUTURE.md`**.
- **Forget Superresolution / Sample Adaptively (Vulkan, experimental)**: **`r_fsa` 1** (latched) — importance map for sub-1-SPP path tracing + guided denoise; pair with **`r_rtx`**, **`r_rtxDemo`**, **`r_fsa_budget`**. See **`docs/FORGET_SUPERRESOLUTION_FSA.md`**.
- **Vertex Features Neural GI (Vulkan, experimental)**: **`r_vfgi` 1** (latched) + **`r_fbo` 1** — per-vertex GI features from BSP world meshes + coarse grid index (`r_vfgi_scale`, **`vfgi/<map>.vfgi`**). See **`docs/VERTEX_FEATURES_NEURAL_GI.md`**.
- **Conditional stubs**: **`USE_VULKAN_RTX` OFF** → RTX/Hybrid1/PathTrace `#else` no-ops; **`USE_EXPERIMENTAL_RENDERERS` OFF** (CMake) → `vk_experimental_renderer_stubs.c` for NIV/RenderFormer/VkSplat/MGS/etc.; **`BUILD_FREETYPE` OFF** → `tr_font_stub.c` + `tr_vector_font_stub.c`; **`USE_STEAM` OFF** → `cl_steam.c` deck-env stub. See **`docs/DEVELOPMENT_SETUP.md`**.
- **RenderFormer preview (Vulkan, experimental)**: **`r_renderformer` 1** (latched) + **`r_fbo` 1** — triangle-token transport + view decode for neural GI-style preview (`r_renderformer_scale`, **`renderformer/<map>.rfm`**). See **`docs/RENDERFORMER.md`**.
- **Neural renderer phases (hub)**: Phase 1–3 map (NIV, NSLM, NDGI, WPT, NIST, NVC, VFGI, MGS, GRTX, VUDA, RenderFormer) — **`docs/NEURAL_RENDERER_PHASES.md`**. Shared weight/volume loaders: **`vk_neural_io.c`** (`NIV1`/`NIV2`, `NSL1`/`NSL2`, `NIS1`, `NVC1`, `VFG1`).
- **Wavefront path experiment**: **`r_wpt` 1** + **`r_fbo` 1** — ray queue + screen-space bounce waves; pair with **`r_fsa`** / **`r_rtx`** for adaptive hardware traces. See **`docs/WAVEFRONT_PATH_TRACING.md`**.
- **VUDA CUDA-Vulkan multiplexing (experimental)**: build **`./scripts/compile_engine.sh vulkan vuda`**, then **`r_vuda` 1** + **`cl_vuda` 1** + **`vid_restart`** — KHR external memory fd interop + compute window after submit (`vuda_status`). See **`docs/VUDA.md`**.
- **Neural Six-way Lightmaps (Vulkan, experimental)**: **`r_nslm` 1** (latched) modulates volumetric froxel scatter after temporal fog (`r_volumetricFog 1`, **`nslm/<map>.nslm`**, **`nslm_reload`** / **`nslm_status`**). See **`docs/NEURAL_SIXWAY_LIGHTMAPS.md`**.
- **Neural Image Space Tessellation (Vulkan, experimental)**: **`r_nist` 1** (latched) refines low-poly silhouettes from G-buffer depth/normals (`r_nist_scale`, **`nist/<map>.nist`**, **`nist_reload`** / **`nist_status`**). See **`docs/NEURAL_IMAGE_SPACE_TESSELLATION.md`**.
- **Gaussian ray tracing / GRTX (Vulkan, experimental)**: **`r_grtx` 1** (latched, **`USE_VULKAN_RTX`** build) traces 3D Gaussian AABB proxies via KHR RT (`r_grtxDemo 1`, **`r_grtxMaxGaussians`**, **`grtx_status`**). See **`docs/GAUSSIAN_RAY_TRACING_GRTX.md`**.
- **Mobile-GS (Vulkan, experimental)**: **`r_mgs` 1–3** (latched) tiered compute splatting for mobile-class GPUs—no RTX (`mgs_status`, **`r_mgs_maxSplats`**, **`r_mgs_scale`**). See **`docs/MOBILE_GAUSSIAN_SPLATTING.md`**.
- **VkSplat (Vulkan, experimental)**: **`r_vksplat` 1** (latched) — Eurographics 2026 Vulkan compute 3DGS training scaffold (`vksplat_train_step`, **`vksplat_status`**); paper Table 2 benchmark via **`vksplat_model`** (no GPU). See **`docs/VKSPLAT.md`**. Upstream full trainer: [harry7557558/vksplat](https://github.com/harry7557558/vksplat).
- **CuRast (Vulkan, experimental)**: **`r_curast` 1** (latched) — software rasterization scaffold with visibility buffer (`curast_render`, **`curast_status`**); paper Table 2 benchmark via **`curast_model`**. See **`docs/CURAST.md`**. Upstream CUDA: [m-schuetz/CuRast](https://github.com/m-schuetz/CuRast).
- **Mímir (Vulkan, experimental)**: **`r_mimir` 1** (latched) — CUDA/Vulkan interop point-cloud viz scaffold (`mimir_step`, **`mimir_status`**); paper Fig. 9 benchmark via **`mimir_model`**. Optional CUDA import: `./scripts/compile_engine.sh vulkan mimir`. See **`docs/MIMIR.md`**.
- **Iris (Vulkan, experimental)**: **`r_iris` 1** (latched) — WSI tile renderer (`iris_pan`, **`iris_load`** / **`iris_save`** `.iris` atlas, **`iris_spd_step`**); PiP overlay via **`r_iris_overlay` 1**; **`r_iris_bilinear` 1** for mip upsample; TeFOV/TPT via **`iris_model`**. See **`docs/IRIS.md`** (Landvater & Balis, J Pathol Inform 2025).
- **SqueezeMe (Vulkan, experimental)**: **`r_squeezeme` 1** (latched) mobile-ready **animatable Gaussian avatars** — linear pose correctives + GCS (arXiv:2412.15171); splats via Mobile-GS (`sqz_status`, **`r_squeezeme_avatars`** 1–3). See **`docs/SQUEEZEME.md`**.
- **Hybrid Rendering 1 (Vulkan RTX, experimental)**: **`r_hybrid1` 1** (latched) Granja/Pereira thesis pipeline — separate 1-SPP **shadow + specular + optional diffuse** RT, SVGF temporal/variance clamp (optional **`r_hybrid1_motion`** motion-vector reprojection), separable A-trous, **IBL cubemap** on secondary hits (`r_hybrid1_ibl`), composite with raster direct light, optional post **TAA** via **`r_hybrid1_taa`**. Console: **`hybrid1_status`**, **`hybrid1_reset`**. Requires **`USE_VULKAN_RTX`**, **`r_rtxDemo` 1**, deferred G-buffer fill. Demo: **`exec demo_hybrid1.cfg`**. See **`docs/HYBRID_RENDERING1.md`**.
- **WebSplatter (Vulkan, experimental)**: **`r_wsp` 1–3** (latched) WebGPU-aligned **tile-binned** splats (`wsp_status`); takes precedence over **`r_mgs`** when both enabled. See **`docs/WEB_SPLATTER.md`**, **`docs/WEBGPU_ROADMAP.md`** (`scripts/check_webgpu_shader_portability.sh`).
- **Metal / DXR (roadmap, not shipping)**: **`USE_METAL_RENDERER=ON`** / **`USE_DXR_RENDERER=ON`** build optional **`idtech3_metal`** / **`idtech3_dxr`** dlopen scaffolds (`tr_platform_renderer_stub.c`). **`cl_renderer metal|dxr`** loads scaffold when built; **`cl_renderer webgpu`** is Wasm-only (client falls back to Vulkan). Shipping backend remains **`vulkan`** only.
- **Vulkan Forward+ / TAA / deferred**: **`r_forwardPlus` 1** (default), up to **64** GPU dlights; **`r_renderMode 2`** latches Forward+ shade. **`r_renderMode 1`** + **`r_deferredLighting 1`**: G-buffer fill + dynamic lighting (`r_deferredUnlitBase` 1 composites scene base + dynamic; **`r_deferredSpecular` 1** optional spec); skips classic projector when active. **`r_taa 1`** optional temporal resolve; **`r_taaMotionVectors 1`** prefers main-pass motion vectors. **`r_temporalCpuSkinPrev 1`** (default) uses per-entity motion fallback for CPU-skinned anim instead of disabling TAA for the whole frame; **`r_temporalCustomShaderMotion 1`** for billboard/decals. **`r_temporalDebug 2`** logs reset reasons and per-entity motion fallbacks. See **`docs/FORWARD_PLUS_PIPELINE_AUDIT.md`**, **`docs/RENDERERS.md`**, **`docs/HDR_GAPS.md`** §6.8, **`docs/RENDERER_2026_ARCHITECTURE_PASS.md`**.
- **Billboard / flipbook / imposter (engine-native)**: Map entities **`misc_*`** parsed on **`LoadWorld`** (`r_spriteProps 1`, auto-disabled when **`CS_ENGINE_SPRITE_META`** set). Server: **`sv_engineSprites`**, **`sv_engineSpritesSpawn`**, console **`sv_sprite_spawn`**. Client: **`cl_engineSprites`**, **`sprite_spawn`**, cgame trap **`trap_EngineSpriteAddLocal`**. QVM game traps: **`G_ENGINE_SPRITE_SHADER_INDEX`**, **`G_ENGINE_SPRITE_SPAWN`**. Lua: **`Engine.Sprites.spawnLocal` / `spawnServer`**. Demo: **`demo_sprites.cfg`**, **`lua_run demo_run_sprites()`**.
- **FreeUSD (default ON)**: Git submodule **`src/external/FreeUSD`** ([gopexllc/FreeUSD](https://github.com/gopexllc/FreeUSD)); **`USE_FREEUSD=ON`** in CMake and **`./scripts/compile_engine.sh`** (auto `git submodule update --init`; pass **`nofreeusd`** to disable). Targets link **`freeusd::runtime`** + **`freeusd::c`** via **`idtech3_freeusd`**. Renderer **`r_freeusd` 1**; client **`usd_*`** tools. Init: **`./scripts/init_optional_submodules.sh --freeusd`**. See **`docs/FREEUSD.md`**.
- **World districts + proxy meshes**: **`r_district` 1** (default) — USD manifest via **`district_load world/playfield.usda`**, FreeUSD snapshot parse, proxy residency (`r_districtProxy`), optional **`cm_districtStream`** sector prefetch. Console: **`district_list`**, **`district_proxy`**, **`district_load_full`**. Demo: **`exec demo_districts.cfg`**. See **`docs/DISTRICTS.md`**.
- **Infinite open worlds**: **`r_openWorld` 1** — view-driven sector residency: **`cm_stream`** + **`cm_streamMerge` 1** (overlay sector BSP collision), per-chunk **`nav/sector_X_Y.nav`**, **`sprites/sector_X_Y.ents`** billboard scatter. Console: **`openworld_start`**, **`openworld_sector`**, **`openworld_status`**. Demo: **`exec demo_openworld.cfg`**. See **`docs/OPEN_WORLD.md`**. CI fidelity: **`ctest -R test_sector_stream_fidelity`** (collision + visual BSP lumps + sync list + nav walkable + stress).
- **Procedural patterns**: **`r_proc` 1** — deterministic Voronoi/grid/hex/radial/stripe/noise sector typing. Console: **`proc_pattern`**, **`proc_map`**, **`proc_sample`**. Scatter fallback: **`sprites/region_<id>.ents`** when **`r_procScatterRegion` 1**. Demo: **`exec demo_proc.cfg`**. See **`docs/PROC_PATTERNS.md`**.
- **Sector BSP fixtures**: **`python3 scripts/tools/gen_sector_bsp.py maps/sector_0_0.bsp`** — minimal collision overlay for **`cm_streamMerge`**. **`ctest -R test_cm_stream_merge`**.
- **Nav sector bake**: **`nav_bake_sector 0 0`** / **`nav_bake_view`** — Recast tile from sector BSP → **`nav/sector_X_Y.nav`**. Server collision residency: **`sv_openWorld 1`**. **`ctest -R test_nav_bake`**.
- **MP sector sync**: **`sv_openWorldSync` 1** → **`CS_ENGINE_OPENWORLD_SECTORS`**; client **`cl_openWorldSync` 1**. Visual overlay: **`r_bspStream` 1**. **`ctest -R test_openworld_sync`**; full stream path: **`ctest -L sector_stream`** or **`make test-sector-stream`** (from build dir).
- **Research modules (Python + C console)**: **RadiusFPS** (`cl_radiusfps_*`, `src/extensions/research/radiusfps/`); **x3DPRA**, **GCC-FER**, **DaX** under `src/extensions/research/` — see respective docs.

### Linting / Static Analysis

`scripts/run_clang_tidy.sh` and `scripts/run_cppcheck.sh` are available for optional local static analysis. CI primarily enforces quality through compiler warnings (`-Wall -Wextra -Wpedantic` and more), with `CI_BUILD=OFF` in the current workflow so warnings are not treated as errors by default.

### Running

- **Dedicated server**: `./release/idtech3_server +set dedicated 1 +set com_hunkMegs 64`
- **Client** (requires display): `./release/idtech3`
- Both require game data in a `base/` directory to do anything meaningful.

### Game data / base

- **Standalone full conversion**: Ship your own `fs_game` mod with native or QVM modules; do not rely on third-party demo content in this repository.
- **Legacy compatible installs**: Retail QVM mods remain supported — see [COMPATIBILITY.md](docs/COMPATIBILITY.md).
- **Smallest valid data tree** (bootstrap `.pk3` + `default.cfg`): see `docs/MINIMAL_GAME_SHELL.md`.

### Scripting

- **Lua:** `script_reload` (default `USE_LUA=ON`); **`com_scriptWatch 1`** auto-reloads tracked scripts. Mod manifests: **`game.idproj`**, see [docs/MOD_MANIFEST.md](docs/MOD_MANIFEST.md).
- **App CRDT (distributed Lua updates, opt-in)**: **`com_app_crdt 1`** — server **`app_crdt_publish`**, **`app_crdt_emit`**; client/server **`Engine.AppCrdt`**. **idtech3backend** submodule auto-publishes **`app_crdt/manifest.json`** on map load when **`com_app_crdt_auto 1`**. Example mod: **`examples/app_crdt/`**. Tests: **`unit_app_crdt`**, **`test_app_crdt`**. See **`docs/APP_CRDT.md`**, **`docs/IDTECH3_BACKEND.md`**.
- **idTech3Radiant:** external BSP editor; `./scripts/install_radiant_gamepack.sh <mod>` + [docs/RADIANT.md](docs/RADIANT.md) (s&box-style `Editor/` Python bridge).
- **JavaScript (Duktape):** `js_reload` (default `USE_DUKTAPE=ON`).
- **Python (CPython, optional):** `py_reload` — `./scripts/compile_engine.sh vulkan python`; Infernux batch bridge. See **`docs/PYTHON.md`**, **`infernux_model`**.
- **C# (Mono):** `cs_reload` when built with `./scripts/compile_engine.sh vulkan csharp` or `-DUSE_CSHARP=ON` + `libmono-2.0-dev` / `mono-devel`; see `docs/CSHARP.md`.

### Optional submodules

- **FreeUSD** (`src/external/FreeUSD`): default build; see `docs/FREEUSD.md`.
- **idTech3 Backend** (`src/external/idtech3backend`, [timfox/idtech3backend](https://github.com/timfox/idtech3backend)): `./scripts/init_optional_submodules.sh --backend` — optional game/backend tree; not linked into engine CMake by default; see `docs/IDTECH3_BACKEND.md`.
- **Tiled Map Editor** (`tools/tiled`, GPL-2.0): `./scripts/init_optional_submodules.sh --tiled` — not built by `compile_engine.sh`; see `docs/TILED.md`.
