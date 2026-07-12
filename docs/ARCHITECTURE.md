# Architecture Overview

## Directory Structure

Phase **5e** (2026): canonical sources live under **`engine/`**, **`runtime/`**, **`modules/`**, **`extensions/`**, **`renderers/`**, and **`third_party/`**. The old **`src/*` forwarding shims are dropped** (`src/README.md` only). Cross-domain `#include` paths may still use layout bridges until the include rewrite finishes — see [core/REPOSITORY_LAYOUT_2026.md](core/REPOSITORY_LAYOUT_2026.md).

```
idtech3/
├── engine/
│   ├── core/                 # qcommon — VM, FS, net, jobs, script emit
│   │   ├── vm.c              VM create, native load, QVM path
│   │   ├── vm_native_module.c Native `.so`/`.dll` filename candidates
│   │   ├── files.c           FS_LoadLibrary search (modules/vm/gamedir)
│   │   └── engine_phys_map.c Map `misc_phys_*` / `func_destructible` spawn
│   └── platform/             # unix, win32, android, SDL
├── runtime/
│   ├── client/               # Modular client (core / world / media / platform / shell)
│   │   ├── core/cl_main.c    Globals, reliable cmds, init glue (~240 LOC)
│   │   ├── core/cl_frame.c   Per-frame loop (CL_Frame)
│   │   ├── core/cl_gameframe.c  Game loop (ticks subsystems incl. Phys_StepSimulation)
│   │   ├── media/cl_cin*.c   ROQ + modern codec dispatch (FFmpeg, dav1d, dav2d, vvdec, VPX, Theora)
│   │   ├── world/cl_district.cpp  District USD residency + draw
│   │   └── shell/cl_phys_debug.c  Client physics debug draw
│   ├── server/               # sv_physics.c — dedicated-server Phys_StepSimulation
│   └── game/                 # g_lua_bindings.c, g_entity_bridge.c, AI middleware
├── modules/
│   ├── physics/              # Box3D Soft Step (default) + companion solvers
│   │   ├── phys_bullet.c/h   C API facade (`Phys_*`; backend-selected)
│   │   ├── phys_box3d_impl.c Box3D Soft Step backend (default)
│   │   ├── phys_bullet_impl.cpp  Optional Bullet backend
│   │   ├── phys_middleware.c Console commands + frame glue
│   │   ├── phys_solvers.c    PRE/POST registry (shadows, procanim, motors, dmm, cloth, fluid, …)
│   │   ├── phys_procedural_anim.c/h  Euphoria-like balance / stumble / getup
│   │   ├── phys_motor.c/h    Per-bone PD torques for active ragdolls
│   │   ├── phys_ragdoll_bind.c/h  `.rag` sidecar + MD3 frame blend
│   │   ├── phys_dmm.c/h      DMM-like fracture companion (rigid proxy + debris)
│   │   ├── phys_events.c/h   Gameplay event bus + `PhysEvent_Poll`
│   │   └── phys_cloth.c, phys_fluid.c, phys_particles.c, …
│   ├── navigation/           # Recast/Detour + BSP extract
│   ├── audio/                # OpenAL/SDL backends, Opus/FLAC/WebM codecs
│   └── world/                # Open world, districts, residency, Arc Blanc, fog biology
│       ├── world_open.cpp    Sector streaming (`r_openWorld`)
│       ├── world_district.cpp  USD district state machine (`r_district`)
│       └── world_residency.cpp Value-aware sector budgets
├── renderers/
│   └── vulkan/               # Vulkan 1.4 renderer (split across vk_*.c, tr_*.c)
│       ├── vk_forward_plus.c Forward+ tile cull + shade
│       ├── tr_bsp_stream.c   Visual BSP sector overlay (`r_bspStream`)
│       ├── vk_sparse.c       Sparse virtual texture residency (`r_vtSparse`)
│       └── shaders/glsl/     GLSL → SPIR-V at build time
├── extensions/
│   ├── generative/           # FLUX, TRELLIS, genetic GAN
│   └── research/             # RadiusFPS, VUDA, Mímir, …
├── third_party/              # box3d, FreeUSD, recast, …
├── cmake/                    # IdTech3Layout.cmake, profile manifests
├── examples/                 # demo_game, templates
├── tests/                    # scripts + ctest fixtures
└── docs/                     # Tiered index: docs/README.md
```

**Build profiles** (`IDTECH3_PROFILE`): `core` | `game` (default) | `full` | `research` — see [ENGINE_MODULE_MANIFEST.md](ENGINE_MODULE_MANIFEST.md).

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

## Physics (Box3D Soft Step)

Default rigid substrate is **Box3D Soft Step** (`modules/physics/phys_box3d_impl.c`, submodule `third_party/box3d`). The public C API is `Phys_*` in `phys_bullet.h` (historical name; backend selected at CMake via `IDTECH3_PHYSICS_BACKEND`).

```
Phys_StepSimulation (server: SV_Physics_Frame; listen: CL_GameFrame)
  ├─ PhysSolvers_PreStep   shadows, volumes, procanim, motors
  ├─ Box3D Soft Step       primary rigid solver
  ├─ contact / sensor / joint-break → PhysEvent_Post
  └─ PhysSolvers_PostStep  cloth, particles, softblob, fluid, dmm
```

Companion layers: **Euphoria-like** active ragdoll (`phys_procedural_anim`, `phys_motor`, `.rag` bind), **DMM-like** destructibles (`phys_dmm`), CastMover character (`phys_character`, `phys_pmove`). Full API, cvars, and console commands: [PHYSICS.md](PHYSICS.md).

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

**Vulkan Forward+** (`r_forwardPlus` default **1**): GPU light record SSBO packs up to **`VK_FP_MAX_GPU_LIGHTS` (64)** from `refdef.dlights`; **classic** surface **`dlightBits`** still only cover indices **0–31** (`MAX_DLIGHTS`). **16×16 px** tile cull compute, optional PBR tile debug / additive local-light shading. Per-tile index count is **`r_forwardPlusMaxPerTile`** (**4–8**, latched, default **8**; SSBO stride is fixed at 8 slots). Optional overload order: **`r_forwardPlusDistanceSort`** / **`r_forwardPlusLuminanceSort`**; optional **`r_forwardPlusDepthCull`** defers cull until after opaque and samples depth at light centers. Tile buffers follow **`vk_get_render_target_width/height`** (main FBO color extent when active, else `vk.render*` / window—`vk_view_state.c`) and **reallocate on resize** without `vid_restart`; toggling `r_forwardPlus` or `r_forwardPlusMaxPerTile` still needs **`vid_restart`**. Implementation: `renderers/vulkan/vk_forward_plus.c`, cvars in `renderers/vulkan/tr_init.c`. Full audit: [FORWARD_PLUS_PIPELINE_AUDIT.md](FORWARD_PLUS_PIPELINE_AUDIT.md).

**Temporal (TAA):** optional **`r_taa`** resolve after post-fog; **`r_taaMotionVectors 1`** samples main-pass motion when available. TAA is skipped on camera cuts, portals, and **`unreliableMotionThisFrame`**; history resets on unreliable motion (`vk_temporal_commit_frame_state`). See [HDR_GAPS.md](HDR_GAPS.md) §6.8.

For goals and longer notes, see [RENDERER_2026_ARCHITECTURE_PASS.md](RENDERER_2026_ARCHITECTURE_PASS.md) and [RENDERERS.md](RENDERERS.md#vulkan-forward-scaffolding).

**Bootstrap game data** (minimal `base/` layout that still satisfies the filesystem): [MINIMAL_GAME_SHELL.md](MINIMAL_GAME_SHELL.md).

## Native game modules (VM)

When `fs_restrict` is **0** (default), `VM_Create` always tries a **native** shared library before falling back to a `.qvm` (`engine/core/vm.c`). Native load is disabled when `fs_restrict` is set (demo-style restriction).

**Exported symbols:** the library must provide `dllEntry` and `vmMain` or the engine unloads it and continues to QVM.

**Filename probes** (`VM_TryLoadNativeModule` + `VM_BuildNativeModuleCandidates` in `engine/core/vm_native_module.c`), in order:

1. `<module>.so` (Linux/macOS-style name; still the first probe on Windows builds too)
2. `<module>.<ARCH_STRING><DLL_EXT>` (e.g. `client.x86_64.so`, `uix86_64.dll` - `ARCH_STRING` / `DLL_EXT` from `q_platform.h`)
3. `<module><ARCH_STRING><DLL_EXT>` (packed form, e.g. `clientx86_64.so`)

**Alternate logical names** for the same VM slot: if those candidates fail, `loadNative` tries additional base names before the final platform-specific `name + ARCH_STRING + DLL_EXT` path. Examples: `qagame` tries `game` then `server`; `cgame` tries `client`; `ui` tries `frontend`; `server` tries `game`; `client` tries `cgame`; `frontend` tries `ui`.

**Filesystem resolution** (`FS_LoadLibrary` in `engine/core/files.c`): for each static game directory on the search path, the engine tries `modules/<file>` then `vm/<file>`, then the file **directly in the gamedir** (legacy). If the requested name already looks like a dotted native (`ui.x86_64.dll`, `cgame.x86_64.so`, etc.), it also tries the dotted form under `modules/` and `vm/` for `ui`, `cgame`, and `qagame` prefixes.

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
