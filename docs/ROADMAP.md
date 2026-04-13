# Development Roadmap

## Execution focus (main branch, 2026)

Priorities that keep **CI green** and **README/build truth** aligned:

1. **Watch GitHub Actions on `main`** — especially **Android** (CMake + Gradle `assembleDebug`, OpenSSL/Lua/FetchContent caches) and **MSYS curl**.
2. **Renderer validation** — Tier B (self-hosted `GAME_BASE`) and Tier C (manual GPU notes) when you have hardware/content; keep `renderer_regression_check` passing on default CI (manifest + GLSL + **IQM_MORPH_TOP_K** C/GLSL parity + **glTF vs IQM** joint/morph `#define` parity).
3. **glTF on Vulkan** — **GPU skinning/morph** (PBR + `r_gltfGpu`) uses **top-8** morph weights per draw (aligned with `GLTF_MAX_MORPH_TARGETS`), including **`RE_SetEntityMorphWeight`** with clip-driven weights; **`r_gltfGpuTangentFix`** (default on) re-orthonormalizes tangents on the GPU path after skin+morph; next polish: full **MikkTSpace**-style qtangent on GPU (neighborhood-aware). **OpenGL** now registers glTF/OBJ/MD5 with a **CPU tess** path (no `r_gltfGpu`).
4. **Android product** — missing `base/` / no pk3: logcat path + **Toast** with `…/base` and apkassets hint before exit; optional APK smoke.

## Current Status (`main`)

### Renderer -- Complete
- [x] Vulkan 1.4 with PBR (metalness/roughness, IBL, BRDF LUT)
- [x] Shader discovery: scripts/ (legacy) + shaders/ (priority override)
- [x] Volumetric fog (froxel compute, temporal reprojection, fluid sim)
- [x] Navier-Stokes fluid simulation (GPU compute)
- [x] Shadow mapping (sun CSM, spot atlas, point cubemaps)
- [x] SMAA anti-aliasing
- [x] SSAO with blur and combine
- [x] Bloom with HDR and tonemapping (ACES/Reinhard)
- [x] Glint NDF (research paper implementation)
- [x] Post-processing: panini projection, lens effects (vignette, chromatic aberration, film grain)
- [x] SSR shader, atmospheric scattering shader, vegetation wind compute shader
- [x] GoPro-style camera lens presets (7 presets)
- [x] Flashlight / projected texture system
- [x] HDR EXR skybox with IBL (equirectangular, cubemap, spherical)
- [x] OpenEXR image format support
- [x] Water flowmap (flow vectors offset texture UVs for rivers, pools, wakes)

### Physics -- Complete
- [x] Bullet Physics (35 API functions, C++ backend)
- [x] Procedural animation controller (11-state ragdoll)
- [x] IK solvers (two-bone, CCD, foot placement, aim, look-at)
- [x] DMM deformation (FEM, Voronoi fracture, thermal, 12 materials, 10 prefabs)
- [x] Cloth simulation (XPBD, wind, pinning, sleep)

### Gameplay -- Complete
- [x] AI Director (intensity, phases, spawn budgets, zones)
- [x] GOAP (A* action planning)
- [x] Horde/swarm AI (512 agents, 4-tier LOD, flocking)
- [x] Response rules (14 criteria, weighted responses)
- [x] Choreography (timeline scenes)
- [x] Facial animation (33 flex, 25 phonemes, 11 expressions)
- [x] Dismemberment + extended gibs (16 limbs, physics gibs)
- [x] Navigation mesh (Recast/Detour, crowd simulation)
- [x] Particle system (8192 pool, billboard rendering)
- [x] Background map for menus
- [x] Dynamic window title

### Audio -- Complete
- [x] OpenAL with HRTF and EFX reverb
- [x] Geometry-based acoustics
- [x] 6 audio codecs (WAV, MP3, Opus, FLAC, WebM, Ogg)
- [x] Adaptive music with intensity-driven layers

### Video -- Complete
- [x] ROQ + modern codecs (FFmpeg, dav1d, libvpx, Theora)
- [x] OpenEXR HDR image loading

### Assets -- Complete
- [x] Multiple model formats (glTF primary on Vulkan for GPU path; OpenGL loads glTF/OBJ/MD5 with CPU tess — see `docs/GLTF.md`)
- [x] 6 image formats (EXR, PNG, TGA, JPG, PCX, BMP)

### Integration -- Complete
- [x] All 16 systems wired into game loop
- [x] 64 Lua-callable engine functions
- [x] ImGui inspector (optional)
- [x] Android platform support (NativeActivity, Vulkan lifecycle, touch HUD, logcat, optional `apkassets`)
- [x] CI for Linux, Windows, macOS, ARM, **Android** (CMake cross-build + **Gradle debug APK** on Release matrix cells)
- [x] **glTF (Vulkan):** runtime skeletal clip sampling, morph blending, mesh default weights; **GPU skin/morph** on PBR path (`r_gltfGpu`); PBR qtangent recompute on **CPU** tess path (`docs/GLTF.md`)
- [x] **Android engine parity (no game pk3):** Lua, Duktape, curl, Recast, Bullet, FreeType, FLUX lib, DTLS via **static OpenSSL autobuild** when prefab not present

### Tooling -- Complete
- [x] Smoke test script (10 checks)
- [x] CI with smoke test step
- [x] 8 documentation files

## Remaining Work

### Next on the roadmap (ordered)

| Priority | Item | Notes |
|----------|------|--------|
| P0 | **CI stability** | Fix any red matrix on `main`; Android OpenSSL/Lua first-build time — tune cache keys if needed |
| P1 | **glTF GPU path (polish)** | Entity morph + top-8 GPU morph (done); GPU tangent Gram–Schmidt after skin+morph via **`r_gltfGpuTangentFix`** (done); optional full MikkTSpace qtangent on GPU; validate on real assets |
| P1 | **Renderer validation** | Tier B/C as optional gates; `renderer_regression_check`: manifest (**`docs/GLTF.md`**, **`docs/RENDERERS_FUTURE.md`**, Tier B doc), GLSL validate, **IQM_MORPH_TOP_K** parity, **GLTF_MAX_* vs IQM_*** header parity; **`tr_model_gltf.c`** `STATIC_ASSERT` for joint/morph caps |
| P2 | **OpenGL glTF polish** | CPU tess + **`r_gltfCpuQtangent`** qtangent recompute when shader uses a `norm` image; **`_norm`** shader try from `normalTexture`; optional: wire qtangents into fixed-function tangents if a normal-mapped ARB path appears |
| P2 | **Engine systems hardening** | Telemetry / replay / save / quest / dialogue — define stable APIs + minimal tests |
| P3 | **GOAP content** | Data-driven actions; perf limits; debug draw |
| P3 | **Vulkan architecture pass** | Clustered Forward+, motion history — see below |

### Renderer 2026 Architecture Pass
- [ ] Lighting scalability: move Vulkan from legacy dynamic-light selection toward clustered Forward+; decouple Vulkan light scale from `MAX_DLIGHTS` and surface-bit assumptions. See `docs/RENDERER_2026_ARCHITECTURE_PASS.md`.
- [ ] Temporal robustness: introduce shared history invalidation and stronger motion-vector coverage before adding TAA/upscaling or RT reuse systems. See `docs/RENDERER_2026_ARCHITECTURE_PASS.md`.
- [ ] Platform strategy: keep Vulkan primary, freeze OpenGL as compatibility-only, prioritize Metal ahead of DXR, and treat RTX as a Vulkan feature tier. See `docs/RENDERER_2026_ARCHITECTURE_PASS.md`.

### Short-Term (completed)
- [x] Connect BSP geometry extraction to map loading for automatic navmesh
- [x] PostFX specialization constants in gamma pipeline (vk_frame_end.c / post-process path)
- [x] Wire SSR/atmosphere shaders into Vulkan render passes
- [x] Wire vegetation wind compute into Vulkan pipeline (surfaceparm vegetation, real geometry from tess)
- [x] SMAA when volumetrics skipped (r_volumetricFog 0, tier off, no world with world)
- [x] glTF GPU upload (MOD_GLTF, tess path, baseColorTexture shader, bounds)
- [x] CBT-inspired GPU terrain tessellation (compute shader LOD, r_cbtTerrain)

### Medium-Term
- [x] GPU occlusion culling (r_occlusionCulling, entity bbox queries, previous-frame visibility)
- [x] Texture compression (BC7/KTX2)
- [x] Optional USD format for entities and shaders (com_usdEntities, com_usdShaders; GPLv2 parser)
- [x] DTLS network encryption (USE_DTLS=ON, net_dtls/net_dtls_key cvars; AES-256-GCM)
- [x] Steam SDR (USE_STEAM_NETWORKING=ON, Steamworks SDK; net_sdr 1, connect steam:STEAMID)
- [x] Steam API + Steam Deck (USE_STEAM=ON; achievements, overlay, rich presence, Deck auto-detect, steamdeck.cfg)
- [x] clang-tidy / cppcheck static analysis (CI job, continue-on-error)

### Long-Term (future work, not in active development)
- [ ] Vulkan RTX — VK_KHR_ray_tracing_pipeline integration (BLAS/TLAS, raygen/miss/closest-hit). See docs/RENDERERS_FUTURE.md.
- [ ] Metal renderer — native Metal backend for macOS/iOS (Apple Silicon). See docs/RENDERERS_FUTURE.md.
- [ ] DXR renderer — DirectX 12 + DirectX Raytracing for Windows. See docs/RENDERERS_FUTURE.md.
- [ ] WebAssembly + WebGPU — browser deployment via Emscripten + WebGPU
