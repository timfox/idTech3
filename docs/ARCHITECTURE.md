# Architecture Overview

## Directory Structure

```
src/
├── client/              Client systems
│   ├── cl_main.c                 Client init + frame loop
│   ├── cl_gameframe.c/h          Game loop (ticks all subsystems)
│   ├── cl_cin.c                  ROQ cinematic + modern codec dispatch
│   ├── cl_cin_modern.c/h         Modern video codec dispatcher
│   ├── cl_cin_ffmpeg/dav1d/vpx/theora.c  Codec backends
│   ├── cl_particles.c/h          Particle system (8192 pool)
│   ├── cl_demo.c/h               Demo record/playback (record, demo, stoprecord)
│   ├── cl_map_background.c/h     Background maps for menus
│   └── cl_window_title.c/h       Dynamic window title
├── game/                Gameplay systems
│   ├── g_director.c/h            AI Director (intensity, phases, spawns)
│   ├── g_goap.c/h                Goal-Oriented Action Planning
│   ├── g_horde.c/h               Horde AI (512 agents, LOD)
│   ├── g_response.c/h            Response rules (dialogue)
│   ├── g_choreography.c/h        Scripted scene system
│   ├── g_facial.c/h              Facial animation (flex + phonemes)
│   ├── g_dismember.c/h           Dismemberment + gibs
│   └── g_lua_bindings.c/h        Lua API (64 functions)
├── physics/             Physics + simulation
│   ├── phys_bullet.c/h           Bullet Physics C API (35 functions)
│   ├── phys_bullet_impl.cpp      Bullet C++ backend
│   ├── phys_procedural_anim.c/h  Procedural animation controller
│   ├── phys_ik.c/h               IK solvers
│   ├── phys_dmm.c/h              DMM deformation engine
│   ├── phys_dmm_materials.c/h    Material library + prefabs
│   └── phys_cloth.c/h            XPBD cloth simulation
├── navigation/          Pathfinding
│   ├── nav_recast.cpp/h          Recast/Detour navmesh + crowd
│   └── nav_bsp_extract.c         BSP triangle extraction
├── audio/               Audio
│   ├── backends/                  OpenAL, SDL, null
│   ├── codecs/                    WAV, MP3, Opus, FLAC, WebM
│   ├── effects/                   EFX reverb, acoustics
│   └── snd_music_adaptive.c/h    Adaptive music layers
├── renderers/
│   ├── vulkan/            Vulkan 1.4 renderer
│   │   ├── vk.h                  Core Vulkan state + public API (implementation split across vk_*.c)
│   │   ├── vk_init_device.c      vk_initialize (device bootstrap after logical device)
│   │   ├── vk_frame_submit.c     vk_begin_frame / vk_end_frame / vk_present_frame
│   │   ├── vk_raster_samples.c   MSAA sample counts + vk_get_msaa_min_sample_shading
│   │   ├── vk_sun_shadow_pass.c  Sun shadow map render pass
│   │   ├── vk_pipelines_bootstrap.c vk_create_pipelines (+ BRDF LUT pipeline helper)
│   │   ├── vk_pbr_ibl_validate.c   vk_validate_pbr_ibl_resources (startup IBL checks)
│   │   ├── vk_procs.c/h          `qvk*` Vulkan entry points (storage + declarations)
│   │   ├── vk_shader_modules.c/h SPIR-V `VkShaderModule` creation + `vk_create_shader_modules`
│   │   ├── vk_pipelines_persistent.c/h Long-lived pipelines (skybox, fog, debug tools)
│   │   ├── vk_attachments.c/h    Render targets, pooled image memory, shadows, froxels (split from vk.c)
│   │   ├── vk_resource_destroy.c/h VkRenderPass + long-lived pipeline teardown (split from vk.c)
│   │   ├── vk_framebuffers.c/h       VkFramebuffer create/destroy (split from vk.c)
│   │   ├── vk_descriptor_sets.c/h    Descriptor pool alloc + attachment/volumetric writes (split from vk.c)
│   │   ├── vk_texture_image.c/h    Texture image create/upload + per-image descriptor (split from vk.c)
│   │   ├── vk_pipeline_helpers.c/h Post-process pipelines: atmosphere, OIT accum, blur; vk_set_shader_stage_desc (split from vk.c)
│   │   ├── vk_occlusion.c/h       GPU occlusion queries + entity visibility buffer (split from vk.c)
│   │   ├── vk_create_pipeline.c/h Vk_Pipeline_Def graphics pipeline factory + pipeline table lookup (split from vk.c)
│   │   ├── vk_draw_state.c/h      Tess upload, vertex/index/descriptor/pipeline bind, draws (split from vk.c)
│   │   ├── vk_volumetric_pipelines.c Volumetric fog / fluid / luminance / CBT / veg-wind pipeline setup (split from vk.c)
│   │   ├── vk_volumetric_internal.c/h MSAA depth resolve, fluid sim dispatch, volumetric perf queries (split from vk.c)
│   │   ├── vk_volumetric_pass_compute.c Local volumetric shadows, froxel compute, composite, SMAA (split from vk.c)
│   │   ├── vk_shutdown.c          vk_shutdown, wait-idle, release_resources (split from vk.c)
│   │   ├── vk_postfx_passes.c     Bloom, SSAO/HBAO, OIT, SSR passes (split from vk.c)
│   │   ├── vk_clear_attachments.c In-pass color/depth clear + dynamic color write mask (split from vk.c)
│   │   ├── vk_cubemap_prefilter.c IBL cubemap prefilter, SH extraction, vk_generate_cubemaps, vk_begin_cubemap_render_pass, vk_create_brfdlut (split from legacy vk.c)
│   │   ├── vk_fluidsim.c/h       Fluid simulation module
│   │   ├── vk_postfx.c/h         PostFX cvars + `PostFX_PostPipelinesNeedUpdate` (SSR/bloom/SSAO/SMAA/OIT/FBO fmt)
│   │   ├── vk_post_process_refresh.c  `vk_update_post_process_pipelines` — rebuild post VkPipelines when needed
│   │   ├── vk_flashlight.c/h     Projected texture system
│   │   ├── vk_skybox_hdr.c/h     HDR EXR skybox + IBL
│   │   ├── tr_model_gltf.c/h     glTF 2.0 loader (Vulkan GPU path; CPU tess fallback - see docs/GLTF.md)
│   │   ├── tr_model_obj.c        OBJ loader
│   │   ├── tr_model_md5.c        MD5 loader
│   │   ├── inspector/             ImGui inspector overlay
│   │   └── shaders/glsl/          GLSL shaders
│   └── common/              Shared (images, fonts, types)
│       └── tr_image_exr.cpp       OpenEXR loader
├── platform/
│   ├── unix/            Linux/macOS
│   ├── win32/           Windows
│   └── sdl/             SDL2 (windowing, input, gamma)
├── qcommon/             Shared engine (VM, filesystem, network)
│   ├── vm.c / vm_local.h         VM create, native load, QVM path
│   ├── vm_native_module.c/h      Native `.so`/`.dll` filename candidates
│   ├── cm_stream.c/h             Sector pk3 prefetch + collision merge overlay
│   └── files.c                   FS_LoadLibrary search (modules/vm/gamedir)
├── world/               Open-world streaming (engine layer)
│   ├── world_open.c/h            Sector residency: collision, nav tiles, scatter
│   ├── world_residency.c/h       Value-aware cardinality planner (submodular proxy)
│   ├── world_district.c/h        USD district manifest + sector streaming hooks
│   ├── world_proc.c/h            Procedural region/palette typing per sector cell
│   └── sector_graph.c/h          k-hop sector grid reachability (CPU + optional GPU BFS)
├── server/              Dedicated server
├── botlib/              Bot AI (Q3 AAS pathfinding)
└── external/            Vendored libraries
    └── src/recast/      Recast/Detour (zlib)
```

## Game Loop

```
CL_Init()
  └── CL_InitGameSystems()    -- init all 16 subsystems

CL_Frame(msec)
  ├── input, networking, cgame
  ├── SCR_UpdateScreen()       -- renderer draws frame
  ├── SCR_RunCinematic()       -- video playback
  ├── CL_GameFrame(dt)         -- tick all subsystems:
  │   ├── Phys_StepSimulation
  │   ├── Nav_UpdateCrowd
  │   ├── Particles_Update
  │   ├── Director_Update
  │   ├── Music_Update
  │   ├── Cloth_SimulateAll
  │   ├── Face_Update
  │   ├── Dismember_Update
  │   ├── GOAP_Update
  │   ├── Choreo_Update
  │   ├── Horde_Update
  │   ├── BgMap_Frame
  │   └── WinTitle_Update
  └── Con_RunConsole()
```

## Renderer Pipeline (Vulkan)

The shipping Vulkan renderer is **forward-only** with a layered HDR/post-processing pipeline.

1. Shadow passes (sun CSM, spot atlas, point cubemaps)
2. Main forward scene pass
3. Optional OIT resolve for transparent surfaces
4. SSR (SSR pass pipelines are created only when `r_ssr` is on; toggling it triggers a frame-start post-pipeline rebuild)
5. Bloom
6. SSAO/HBAO
7. Atmosphere + volumetric fog
8. SMAA
9. Luminance / eye adaptation
10. Gamma / tonemap / lens effects
11. Present

**`r_renderMode`** (latched, `vid_restart`): **0** classic forward, **1** deferred scaffold with optional **`r_deferredLighting 1`** (G-buffer fill + Forward+ tile diffuse; latches `r_forwardPlus` 1, `r_forwardPlusShade` 0). **`r_deferredGBuffer 1`** allocates RTs; **`r_deferredGBufferFill 1`** captures albedo/normal/material after geometry. **2** Forward+ primary.

**Vulkan Forward+** (`r_forwardPlus` default **1**): GPU light record SSBO packs up to **`VK_FP_MAX_GPU_LIGHTS` (64)** from `refdef.dlights`; **classic** surface **`dlightBits`** still only cover indices **0–31** (`MAX_DLIGHTS`). **16×16 px** tile cull compute, optional PBR tile debug / additive local-light shading. Per-tile index count is **`r_forwardPlusMaxPerTile`** (**4–8**, latched, default **8**; SSBO stride is fixed at 8 slots). Optional overload order: **`r_forwardPlusDistanceSort`** / **`r_forwardPlusLuminanceSort`**; optional **`r_forwardPlusDepthCull`** defers cull until after opaque and samples depth at light centers. Tile buffers follow **`vk_get_render_target_width/height`** (main FBO color extent when active, else `vk.render*` / window—`vk_view_state.c`) and **reallocate on resize** without `vid_restart`; toggling `r_forwardPlus` or `r_forwardPlusMaxPerTile` still needs **`vid_restart`**. Implementation: `src/renderers/vulkan/vk_forward_plus.c`, cvars in `src/renderers/vulkan/tr_init.c`. Full audit: [FORWARD_PLUS_PIPELINE_AUDIT.md](FORWARD_PLUS_PIPELINE_AUDIT.md).

**Temporal (TAA):** optional **`r_taa`** resolve after post-fog; **`r_taaMotionVectors 1`** samples main-pass motion when available. TAA is skipped on camera cuts, portals, and **`unreliableMotionThisFrame`**; history resets on unreliable motion (`vk_temporal_commit_frame_state`). See [HDR_GAPS.md](HDR_GAPS.md) §6.8.

For goals and longer notes, see [RENDERER_2026_ARCHITECTURE_PASS.md](RENDERER_2026_ARCHITECTURE_PASS.md) and [RENDERERS.md](RENDERERS.md#vulkan-forward-scaffolding).

**Bootstrap game data** (minimal `base/` layout that still satisfies the filesystem): [MINIMAL_GAME_SHELL.md](MINIMAL_GAME_SHELL.md).

## Open world and streaming

View-driven **sector residency** loads collision (optional BSP merge), Detour nav tiles, and billboard scatter per grid cell. Core modules live under `src/world/` and integrate with `cm_stream` (collision overlay), Recast/Detour (`nav/sector_X_Y.nav`), and renderer BSP overlay (`r_bspStream`).

```
CL_OpenWorld_Frame / SV_OpenWorld_Frame
    └── WorldOpen_UpdateView
            ├── r_openWorldResidency 0 → radius disk (r_openWorldRadius)
            └── r_openWorldResidency 1 → WorldResidency (budget + bounded swaps)
                    ├── optional SectorGraph k-hop filter (r_graphStreamReach)
                    ├── CM_Stream_MergeSector (cm_streamMerge)
                    ├── Nav_LoadSectorTile
                    └── scatter parse (sprites/sector_X_Y.ents)
```

| Layer | Guide |
|-------|-------|
| Streaming overview | [OPEN_WORLD.md](OPEN_WORLD.md) |
| Value-aware budgets | [WORLD_RESIDENCY.md](WORLD_RESIDENCY.md) |
| Sector graph BFS | [GRAPH_COMPUTE.md](GRAPH_COMPUTE.md) |
| USD districts | [DISTRICTS.md](DISTRICTS.md) |
| Procedural region typing | [PROC_PATTERNS.md](PROC_PATTERNS.md) |

**MP:** server publishes loaded cells via `CS_ENGINE_OPENWORLD_SECTORS`; clients with `cl_openWorldSync 1` merge authoritative collision. Optional `sv_openWorldResidency 1` plans collision from the union of player origins.

**Client frame hook:** `CL_OpenWorld_Frame` (client) and `SV_OpenWorld_Frame` (dedicated server) run after valid snapshots / player origins are available.

## Native game modules (VM)

When `fs_restrict` is **0** (default), `VM_Create` always tries a **native** shared library before falling back to a `.qvm` (`src/qcommon/vm.c`). Native load is disabled when `fs_restrict` is set (demo-style restriction).

**Exported symbols:** the library must provide `dllEntry` and `vmMain` or the engine unloads it and continues to QVM.

**Filename probes** (`VM_TryLoadNativeModule` + `VM_BuildNativeModuleCandidates` in `src/qcommon/vm_native_module.c`), in order:

1. `<module>.so` (Linux/macOS-style name; still the first probe on Windows builds too)
2. `<module>.<ARCH_STRING><DLL_EXT>` (e.g. `client.x86_64.so`, `uix86_64.dll` - `ARCH_STRING` / `DLL_EXT` from `q_platform.h`)
3. `<module><ARCH_STRING><DLL_EXT>` (packed form, e.g. `clientx86_64.so`)

**Alternate logical names** for the same VM slot: if those candidates fail, `loadNative` tries additional base names before the final platform-specific `name + ARCH_STRING + DLL_EXT` path. Examples: `qagame` tries `game` then `server`; `cgame` tries `client`; `ui` tries `frontend`; `server` tries `game`; `client` tries `cgame`; `frontend` tries `ui`.

**Filesystem resolution** (`FS_LoadLibrary` in `src/qcommon/files.c`): for each static game directory on the search path, the engine tries `modules/<file>` then `vm/<file>`, then the file **directly in the gamedir** (legacy). If the requested name already looks like a dotted native (`ui.x86_64.dll`, `cgame.x86_64.so`, etc.), it also tries the dotted form under `modules/` and `vm/` for `ui`, `cgame`, and `qagame` prefixes.

**Native modules stored only inside `.pk3`:** `dlopen` / `LoadLibrary` cannot load directly from zip-backed file handles. When **`com_nativeLibraryExtractPk3`** is **1** (default, archived), `FS_LoadLibrary` first looks up the requested basename via `FS_ReadFile` (virtual paths such as `vm/uix86_64.so`). If the bytes exist only in a pack, it writes them under **`<fs_homepath>/<fs_game>/vm/native_cache/<basename>`** (CRC32 match skips rewrite when the cache file already matches), then loads that OS path. Set **`com_nativeLibraryExtractPk3`** to **0** to disable extraction (fall back to loose files only). Startup prints one line when extraction is enabled.

**Debugging failed loads:** `+set com_nativeLibraryDebug 1` logs each failed path and the OS loader message. See [DEVELOPMENT_SETUP.md](DEVELOPMENT_SETUP.md#prerequisites) (native DLL troubleshooting). Unit coverage: `ctest -R unit_vm_native_module` exercises candidate ordering.

## Client HTTP downloads (libcurl)

The **client** links **libcurl** when `USE_CURL` is enabled at build time. It powers **HTTPS/FTP** fetches of **`.pk3`** archives: server redirect downloads (`sv_dlURL` + `CL_cURL_*`) and manual or auto map downloads (`cl_dlURL` + `Com_DL_*`, commands `download` / `dlmap`). Protocols are restricted to **http, https, ftp, ftps**; there is no generic HTTP API exposed to game VMs without additional code. Full tutorial: [CURL_NETWORKING.md](CURL_NETWORKING.md).

## JavaScript / UI Debug (Duktape)

When `USE_DUKTAPE` is enabled, the engine provides a JavaScript runtime (`idtech3` namespace) with event callbacks and HUD bindings. **Game events** (emitted from snapshot parsing): `entity_spawn`, `entity_death`, `weapon_fire` - payloads include `entityNum`, `eType`, `attacker`, `weapon`. See [JS_HUD_DRAWING.md](JS_HUD_DRAWING.md#game-events). Other events: `frame`, `menu_changed`, `ui_open`, `ui_close`, `map_load`, `input_key`, `mouse_move`, etc. For debugging UI and script issues:

- **`js_verbose`** (0/1): Toggle verbose info when at a menu.
- **`js_verboseMenu`** (main|ingame|all|none|off): Which menu to show verbose for. Default `main`.
- **`js_list`**: Shows policy cvars, callback counts, current menu, and error count.
- **`js_clearErrors`**: Reset the JavaScript error log.
- **`js_reload`**, **`js_exec`**, **`js_dump`**: Reload scripts, run code, dump globals.

When verbose is on and you are at the specified menu, the console prints once per second: menu id, callback counts, and any JavaScript errors (with counts). Errors are always printed to console when they occur; the log tracks them for the verbose summary.
