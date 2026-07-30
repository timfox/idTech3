# Renderer Features

For a **build + manual validation checklist** (CI parity, shader coverage, GPU passes), see [RENDERER_CONFIDENCE.md](RENDERER_CONFIDENCE.md).

## Renderer backends (id Tech 7–style)

| Backend | Status | Notes |
|---------|--------|-------|
| **Vulkan** | **Shipping** | Only production renderer (`idtech3_vulkan.so` / static on Windows) |
| **Vulkan RTX** | Chocolate RT tier | `-DUSE_VULKAN_RTX=ON` / `./scripts/compile_engine.sh vulkan rtx` — Hybrid1/Raygun supported demos; neural scaffolds still experimental |
| **DXR** | Roadmap scaffold | Windows plugin stub (`USE_DXR_RENDERER`); see [DXR_RENDERER.md](DXR_RENDERER.md) |
| **WebGPU** | Roadmap | Browser/Wasm target; portable compute shaders validated on Vulkan today — [WEBGPU_ROADMAP.md](WEBGPU_ROADMAP.md) |
| **OpenGL** | **Removed** | No `idtech3_opengl` target, no fallback; `cl_renderer opengl` maps to Vulkan with a warning |

Build: `./scripts/compile_engine.sh vulkan` (OpenGL/`opengl` arg is rejected).

## Vulkan Renderer (Primary)

The Vulkan 1.4 renderer is the primary rendering backend, built as a shared library (`idtech3_vulkan.so`). Requests Vulkan 1.4 when available; validation layers (Khronos, then LUNARG fallback) are enabled in debug builds on all platforms.

### Current Architecture
- **Modern Vulkan default:** `exec modern_vulkan.cfg` → `modern_vulkan_stable.cfg` (Unified Clustered mode 3, SMAA, GTAO, bloom, SH/IBL, WBOIT). Quality: `modern_vulkan_quality.cfg`. Recovery: `gfx_safe.cfg`. See [RENDERER_SPINE_1.0.md](RENDERER_SPINE_1.0.md).
- `r_renderMode`: **0** forward (classic projector; `r_forwardPlus` may still be 1), **1** deferred lighting mode, **2** Forward+ legacy recovery, **3** Unified Clustered Renderer (**Spine default**: deferred opaque + clustered Forward+/OIT transparent), **4/5** hybrid/reference modes. North-star 2027 stack builds on mode 3 — see [RENDERER_2027.md](RENDERER_2027.md).
- **Visibility buffer + material classify:** production mode 3 enables `r_visibilityBuffer 1`, `r_visibilityBufferFill 2`, `r_materialClassify 1`, and `r_deferredMaterialClassify 1`. The path prefers PrimID/drawId MRT export and falls back to depth-proxy fill; deferred opaque lighting consumes the class map by default.
- **Surfel GI default request:** `modern_vulkan.cfg` requests `r_surfelGi 1` with balanced density. It activates only in `USE_VULKAN_RTX` builds on ray-query devices with a ready TLAS; otherwise it is inert/fail-open. `gfx_safe.cfg` disables it.
- **Deferred G-buffer sidecar and lighting:** the deferred G-buffer sidecar (`r_deferredGBuffer 1` + `r_deferredGBufferFill 1`) works with `r_renderMode` 1, 2, and 3. In default mode 3, opaque surfaces use deferred lighting while transparent/OIT surfaces consume the same clustered Forward+ lists. `r_deferredLighting` runs in modes **1** and **3** (ignored in mode 2).
- **Day/night world lighting:** `r_dayNight 1` updates the canonical `tr.sunDirection` / `tr.sunLight` from local real time or an accelerated cycle, so atmosphere, volumetrics, sun shadows, deferred sun lighting, and RTX/Hybrid paths share one current world sun. See [DAY_NIGHT_WORLD_LIGHTING.md](DAY_NIGHT_WORLD_LIGHTING.md) and `config/vulkan_overlay_day_night.cfg`.
- Vulkan is the supported rendering backend
- **Shared temporal reset policy** (`vk_temporal.c`): centralizes history invalidation for volumetrics, motion vectors, exposure. Resize, map load, camera cut, and missing prev-frame data trigger resets. Used by Temporal Reconstruction (`r_taa` / `r_aaMode` 4–5) and volumetric history.
- See [RENDERER_SPINE_1.0.md](RENDERER_SPINE_1.0.md) for the production-spine milestone (stabilize before adding techniques)
- See [RENDERER_2026_ARCHITECTURE_PASS.md](RENDERER_2026_ARCHITECTURE_PASS.md) for the focused 2026 renderer direction

### Modern Vulkan Default

Use this for native/full-conversion games:

```cfg
exec modern_vulkan.cfg
vid_restart
```

`modern_native.cfg` inherits this profile automatically when `cl_autoGraphicsProfile 1` loads a native cgame. The shipping default is the **Spine stable Unified Clustered** profile (`modern_vulkan_stable.cfg`): mode 3, **SMAA 1x**, GTAO, WBOIT, visibility buffer + material classify, and default Surfel GI request, no TAA/Hybrid1/RcGI/late-shade/open-world. Quality (SSR + volumetrics): `exec modern_vulkan_quality.cfg`. `exec modern_clustered.cfg` reasserts the same clustered defaults after experiments. Temporal Reconstruction: `exec vulkan_overlay_temporal_recon.cfg`. Bisect with `exec gfx_safe.cfg` or `renderer_modern_safe`.

For an in-session recovery back to the documented modern baseline after deferred or clustered experiments:

```cfg
renderer_modern_safe
vid_restart
```

This restores the mode 3 clustered default, depth-cull, 8 logarithmic Z slices, deferred opaque lighting, clustered-lit WBOIT, HDR/PBR, SMAA baseline (TAA off), and turns Hybrid1 / open-world back off.

The CI confidence target for this path is `test_modern_renderer_profile_runtime`: source-only checks run on normal hosted CI, while the self-hosted renderer Tier B workflow launches the client and exercises `renderer_profile`, `renderer_status`, and `renderer_compatibility` against the minimal renderer validation pack. `renderer_status` includes dedicated `lighting` and `gi/neural` rows so Forward+/SSAO/volumetrics/IBL plus NDGI/NIV/VFGI/NVC readiness can be checked without digging through individual cvars.

| Area | Default Contract |
|------|------------------|
| Framebuffer/HDR | `r_fbo 1`, `r_hdr 2` |
| Materials | `r_pbr 1`, `r_materialBlend 1` |
| Lighting | `r_renderMode 3`, `r_forwardPlus 1`, `r_forwardPlusShade 1`, `r_forwardPlusDepthCull 1`, `r_forwardPlusZSlices 8`, `r_forwardPlusZSliceMode 1`, `r_deferredLighting 1` |
| Deferred data | `r_deferredGBuffer 1`, `r_deferredGBufferFill 1` |
| Visibility/classify | `r_visibilityBuffer 1`, `r_visibilityBufferFill 2`, `r_materialClassify 1`, `r_deferredMaterialClassify 1`, `r_visibilityBufferLateShade 0` |
| Indirect RT request | `r_surfelGi 1`, `r_surfelGiDensity 2`, `r_surfelGi_hybrid1Fusion 1`; active only with `USE_VULKAN_RTX` + ray query |
| Ambient visibility | `r_ambientVisibilityMode 2` production GTAO; legacy `r_ssao 0` |
| Presentation AA | `r_aaMode 2` (SMAA 1x), `r_taa 0`, `r_taaMotionVectors 1` |
| Post AA | `r_ext_smaa 1`, `r_postAaAfterBloom 1` |

### Vulkan Overlays

The renderer profile rule is: start from **one** modern base (`modern_vulkan.cfg` → stable Unified Clustered mode 3), then apply an overlay only for the experimental path you want to test.

| Overlay | Use | Notes |
|---------|-----|-------|
| `vulkan_overlay_deferred.cfg` | Mode-1 deferred lighting development | Switches to `r_renderMode 1`, enables `r_deferredLighting 1`, disables `r_forwardPlusShade`, and turns TAA/SMAA/FXAA off by default so temporal/post-AA artifacts do not mask deferred lighting bugs. |
| `vulkan_overlay_aa_sharp.cfg` | Competitive SMAA 1x | `r_aaMode 2`, `r_taa 0` |
| `vulkan_overlay_temporal_recon.cfg` | Modern native temporal | `r_aaMode 5` + conservative history weight + SMAA cleanup |
| `vulkan_overlay_temporal_perf.cfg` | Perf reconstruction | `r_aaMode 4` + `r_renderScale` / upscale |
| `vulkan_overlay_unified_clustered_safe.cfg` | Unified Clustered safe baseline | Mode 3 with TAA/SMAA/FXAA/OIT off and MSAA pinned off so clustered lighting and pass ordering can be debugged in isolation. |
| `vulkan_overlay_oit_clustered.cfg` | Mode 3 + WBOIT | Unified Clustered + production `r_oit 1` (MBOIT is experimental-only via `vulkan_overlay_mboit.cfg`). See [MOMENT_OIT_STOCHASTIC_ALPHA.md](MOMENT_OIT_STOCHASTIC_ALPHA.md). |
| `vulkan_overlay_visibility_2027.cfg` | Visibility foundation re-apply/debug | Mode 3 + G-buffer + `r_visibilityBuffer` + material classify. Already enabled by `modern_vulkan.cfg`; useful after experiments. See [RENDERER_2027.md](RENDERER_2027.md). |
| `vulkan_overlay_rtx.cfg` | Plain RTX demo pass + Surfel GI | Requires `USE_VULKAN_RTX`; keeps the modern Forward+ base and enables shared TLAS/entity BLAS plus Surfel GI. |
| `vulkan_overlay_hybrid1.cfg` | Hybrid1 ray/raster path + Surfel fusion | Requires `USE_VULKAN_RTX`; enables shared TLAS/entity BLAS, Surfel GI fusion, Hybrid1 channels, and adaptive Ambient Visibility mode 4. |

Examples:

```cfg
exec vulkan_overlay_deferred.cfg
vid_restart

exec vulkan_overlay_rtx.cfg
vid_restart

exec vulkan_overlay_hybrid1.cfg
vid_restart
```

`deferred_vulkan.cfg` remains as a direct standalone deferred profile for older docs/scripts, but new renderer work should prefer the overlay configs so the modern base stays single-source.

### Deferred Vulkan Profile

Use this when working directly on the mode-1 deferred renderer:

```cfg
exec deferred_vulkan.cfg
vid_restart
```

For an in-session recovery path, run:

```cfg
renderer_deferred_safe
vid_restart
```

This profile sets `r_renderMode 1`, `r_deferredGBuffer 1`, `r_deferredGBufferFill 1`, and `r_deferredLighting 1`. It uses the same **opaque deferred + Forward+ transparent** split as mode 3 (`r_forwardPlusShade 1` on transparent; opaque handoff skips Forward+ add). Prefer `r_ext_multisample 0` for direct material export; MSAA uses depth-fallback fill with `deferredMsaaSafeMaterials` confidence floor. Safe debug: `renderer_deferred_safe`. Shipping default remains `modern_vulkan.cfg` (Spine stable mode 3).

The G-buffer fill copies scene albedo. On **non-MSAA** FBO frames, opaque PBR shaders **directly export** normals and material (metalness/roughness/AO). **MSAA** forces the depth-derived fallback (default metal/rough/AO=1). Prefer `r_ext_multisample 0` with `modern_vulkan.cfg` for true material export. Fallback defaults: `r_deferredDefaultMetalness`, `r_deferredDefaultRoughness`, `r_deferredNormalEdgeThreshold`.

### Vulkan Forward+ scaffolding

**GPU light packing + per-cluster cull** on the default clustered path (`r_forwardPlus` default **1**; `r_renderMode 3` / `modern_vulkan.cfg` force it on):

| Cvar | Role |
|------|------|
| `r_forwardPlus` | **1** (default) on; **0** off (**latched**; `vid_restart`). Packs up to **64** refdef dlights on GPU. |
| `r_forwardPlusMaxPerTile` | **4–8** lights indexed per **16×16** tile (**latched**; `vid_restart`). Lowers GPU work vs default **8**; tile SSBO keeps **8** `uint32` slots either way. |
| `r_forwardPlusDebug` | **0–1** float: PBR heatmap overlay (lights per tile + borders). |
| `r_forwardPlusShade` | **0–4** float: PBR shade for dlight indices **0–31** (not in `tess.dlightBits`); pipeline rebuild on change. |
| `r_forwardPlusOverflowShade` | **0–4** (default **0**; mode 2/3 latch **1** when not classic): PBR shade for indices **32–63**; requires `r_classicLighting 0`. |
| `r_forwardPlusZSlices` | **1–16** (default **1** = 2D tiled; shipping configs **8**): depth-partitioned cluster count. Latched; `vid_restart`. |
| `r_forwardPlusZSliceMode` | **0** linear / **1** log view-depth (default **1**). |
| `r_forwardPlusSpecularStrength` | **0–4** (default **0.65**): Forward+ dynamic specular scale (`modern_vulkan.cfg`). |
| `r_forwardPlusEnergyRenorm` | **0–2** (default **0**): Legacy dual-path renorm; unused when Forward+ shade owns dynamics (projector skipped). |
| `r_forwardPlusDepthCull` | **0/1** (code default **0**; **`modern_vulkan.cfg` 1**): post-opaque tile cull with 5 depth probes. |
| `r_forwardPlusLuminanceSort` | **0/1** (default **1**): when a tile is overloaded, keep brightest lights by RGB sum. |
| `r_forwardPlusDistanceSort` | **0/1** (default **0**): when overloaded, prefer nearest lights to the camera (`vieworg`). |
| `r_forwardPlusDepthCull` | **0/1** (default **1**): **0** = tile cull before draws; **1** = depth prepass + reject lights behind surfaces. |
| `r_dynamicLightScale` | **0.25–4** (default **1**): global dynamic-light multiplier (projector, Forward+, deferred, volumetric). |
| `r_lightGammaLink` | **0/1** (default **1**): **0** decouples dynamic lights from display `r_gamma` (HDR tuning). |
| `r_classicLighting` | **1** (default): retail/baseq3 look — disables SH world tint, PBR sun shadow, Forward+ overflow. Auto **0** for native cgame via **`modern_native.cfg`** when **`cl_autoGraphicsProfile 1`**. |
| `r_pbrSunShadow` | **0/1** (default **0**): PBR deluxe direct × sun shadow map; requires `r_classicLighting 0`. |
| `r_pbrSunShadowStrength` | **0–1** (default **1**): Blend sun shadow into PBR direct lighting. |

**Caps:** up to **`VK_FP_MAX_GPU_LIGHTS` (64)** on GPU; **`tess.dlightBits`** skip still applies to indices **0–31** only (classic projector remains **32** lights).

**SH world lighting** (`r_shWorldLighting` 1, default): light-grid SH tints world vertex colors on lightmapped and vertex-lit BSP (`R_WorldSHVertexColor`). Tune with `r_shWorldStrength` (0–2) and `r_directedScale`. Sun shadow map: `r_pbrSunShadow` for PBR direct, `r_fog_shadows` for volumetric; `r_fogShadowSnap` 1 (default) stabilizes cascades.

Code: `renderers/vulkan/vk_forward_plus.c`, `VK_FP_*` constants; cvar registration `renderers/vulkan/tr_init.c`.

**Audit:** [FORWARD_PLUS_PIPELINE_AUDIT.md](FORWARD_PLUS_PIPELINE_AUDIT.md).

### Compute shaders, mesh shaders (NV), and upscaling (DLSS)
- **Compute:** already central to Vulkan (volumetric fog stages, vegetation wind, fluid sim, terrain CBT, etc.).
- **Mesh shaders / virtual geometry:** stable profile requests portable meshlet virtual geometry (**`r_virtualGeometry 1`**, **`r_meshletsMdiDraw 1`**, **`r_meshletsLod 1`**). Optional **`VK_NV_mesh_shader`** remains explicit opt-in via **`r_vk_meshShaderNV 1`** (default **0**, **latched**, `vid_restart`) for NVIDIA mesh-shader experiments and diagnostics; indexed MDI is the production fallback.
- **DLSS / NGX:** **not** linked in this repository (proprietary NVIDIA SDK). Use **`r_renderScale`** / internal resolution, **TAA** (`r_taa`), **SMAA/FXAA/MSAA**, or **driver-level** scaling (e.g. NVIDIA NIS/DLSS in control panel) where applicable. Startup logs state that DLSS is not in-engine.
- **SP upscale preset:** `r_upscale 1` auto-enables `r_renderScale 3` at ~75% of window (`r_renderWidth`/`r_renderHeight`) with spatial blit. `r_upscale 2` is **engine temporal upsample** (Halton jitter + TAA at internal res, then gamma upsample)—no FidelityFX/DLSS SDK. TAA history framebuffers are created **without requiring SMAA**. `r_upscaleDisplayHistory 1` adds resolve sharpen bias. Console: `upscale_status`. Demo: `exec demo_upscale.cfg`. Full look stack: `exec demo_idtech8_look.cfg` (Forward+ + TAA). See `demo_sp_slice.cfg` / [NEURAL_RENDERER_PHASES.md](NEURAL_RENDERER_PHASES.md) chocolate table.
- **Simulation profile (AMBF-Vulkan):** `sim_render_profile 1` then `vid_restart` — MSAA + FXAA, Reinhard tonemap, lightweight post. See [SIM_RENDER_PROFILE.md](SIM_RENDER_PROFILE.md).

### Physically Based Rendering (PBR)
- Metalness/roughness workflow with Cook-Torrance BRDF
- Image-based lighting (IBL) with prefiltered environment maps
- BRDF lookup table (LUT) generation
- Texture map types: normal, physical (RMO/ORM/RMOS variants), emissive, clearcoat, sheen, anisotropy, transmission, subsurface
- Spherical harmonics for diffuse irradiance
- Multi-scatter energy compensation
- **Glint NDF**: Procedural microfacet NDF for specular glints (replaces GGX D term on low-roughness surfaces). Cvars: `r_glint`, `r_glintMode`, `r_glintDensity`, `r_glintMicrofacetRoughness`, `r_glintPixelFilterSize`, `r_glintSampleBudget`, `r_glintMaxLodClamp`, `r_glintRoughnessLo`, `r_glintRoughnessHi`, `r_glintDMax`. Debug modes 5–8 via `r_pbr_debug`. Hybrid1: **`r_hybrid1_glint`** + **`r_glint`** (defaults **1**) weight specular RT by the same NDF (screen-UV jacobian proxy; inactive until Hybrid1 is enabled).
- **Parallax Occlusion Mapping (POM)**: With `r_pom` on, height is ray-marched from **either** the packed ORM/physical map’s **occlusion (R)** channel (when a metalness/roughness physical map is bound) **or** the **normal map alpha** when the stage uses `normalHeightMap` (decouples height from AO). Base UVs (`texcoord0`) are displaced for normal/ORM/detail/emissive/subsurface/anisotropy sampling; lightmaps still sample their own UV set. Shader keywords `parallaxDepth` (height scale, stored in `normalScale[3]`) and `parallaxBias` still apply. Cvars: `r_pom`, `r_pomSteps`, `r_pomScale`, `r_pomShadow`, `r_pomShadowSteps`. Startup prints a one-line POM summary when PBR initializes.
- See [PBR_TEXTURES.md](PBR_TEXTURES.md) for texture naming conventions

### Volumetric Fog
- Froxel-based volumetric lighting (configurable grid: default 160x90x64)
- Composite robustness: NaN/Inf guard on final output; explicit textureLod for scene/depth/froxel sampling
- Multi-stage compute pipeline: density, volume, local lights, sun light, temporal blend
- Exponential height fog with noise turbulence (FBM)
- Henyey-Greenstein phase function for anisotropic scattering
- Temporal reprojection for stable results
- Integration with Navier-Stokes fluid simulation for dynamic fog
- Shadowed volumetrics via sun CSM and local shadow maps
- **Composite accuracy**: froxel Z aligned to view-depth march (Z-interpolation between slice bounds); MSAA depth resolve uses nearest surface per pixel
- Cvars: `r_volumetricFog`, `r_volumetricFogDensity`, `r_volumetricFogHeightFalloff`, `r_volumetricFogNoiseScale`, `r_volumetricFogGridDim`, `r_volumetricFogQuality`, `r_volumetricFogCompositeMode` (0=physical), `r_volumetricFogSteps`, etc. See [SIM_RENDER_PROFILE.md](SIM_RENDER_PROFILE.md) for simulation presets.

### Water Flowmap
- Flowmap textures drive per-pixel UV offset for water surfaces (rivers, pools, wakes)
- Shader keywords: `flowmapTex <path>`, `flowSpeed <value>`
- Flowmap RG channels encode flow direction (0.5 = no flow); shader offsets main texture UV by flow × speed × time
- Requires extended shader support; single-texture stages only
- Sample assets: [docs/samples/flowmap/](samples/flowmap/README.md)

### Navier-Stokes Fluid Simulation
- GPU compute-based Stable Fluids (Jos Stam 1999)
- Semi-Lagrangian advection (unconditionally stable)
- Jacobi pressure solver (configurable iterations)
- Helmholtz-Hodge decomposition for incompressibility
- 2D velocity, density, and pressure fields (resolution derived from froxel grid)
- Emitter system (up to 16 emitters) for smoke, fire, fog injection
- Wind and buoyancy forces
- Cvars: `r_fogFluid` (enable), `r_fogFluidViscosity`, `r_fogFluidDissipation`, `r_fogFluidVorticity`, `r_fogFluidBuoyancy`, etc.

### Lighting Robustness
- PBR Smith GGX visibility: division-by-zero guard at grazing angles (`CalcVisibility`)
- PBR anisotropy map: **direct** specular uses anisotropic GGX NDF and **anisotropic Smith visibility** when a map is bound (`r_pbr_anisotropicSpecular`). **IBL** can use the same map to **stretch** environment roughness (`r_pbr_iblAnisoStretch`, default 1). Regenerate SPIR-V after editing `gen_frag.tmpl` (`scripts/compile_shaders.sh`).
- PBR clearcoat / sheen: clearcoat dims the base lobe before adding the coat; sheen uses a **Charlie** lobe with optional fourth `sheenScale` component for roughness (see [PBR_TEXTURES.md](PBR_TEXTURES.md)).
- Dynamic light radius: clamped to minimum 0.001 before `1/r²` to avoid inf
- Light grid: `lightGridSize` clamped to ≥1 before inverse to avoid division by zero

### Shadow Mapping
- Cascaded shadow maps (CSM) for directional/sun light
- Spot light shadow atlas
- Point light shadow cubemap array (6 faces per light)
- Per-frame shadow matrix computation

### Vegetation Wind (Vulkan)
- GPU compute shader deforms vertices for grass, leaves, foliage
- Surfaces with `surfaceparm vegetation` in shaders feed the VegetationVertex buffer
- Flexibility from vertex normal Y; phase from position hash
- Cvars: `r_vegWind` (PostFX panel), wind direction/strength, primary/detail frequency and amplitude
- Compute **dispatches after each vegetation tess batch** uploads staging (so the GPU sees the correct `vertexCount`). **Vertex shaders still draw from the original tess buffer** - binding the modified SSBO for final positions remains a future enhancement.

### RB_ColorMask (Vulkan)
- `RB_ColorMask` is wired through `vk_set_color_write_mask()`, but the `VK_EXT_extended_dynamic_state3` path is currently disabled due to validation/driver issues.
- Current behavior falls back to full color writes when the dynamic mask path is unavailable.
- Treat this as partial infrastructure rather than a production-ready Vulkan parity feature.

### Anti-Aliasing
- **Policy (`r_aaMode`)**: unified presentation AA applicator (`vk_aa_policy.c`). **0** none, **1** FXAA, **2** SMAA 1x (**shipping default**), **3** Present-Time Adaptive Reconstruction (migrated from SMAA T2x; see [RASTER_ULTRA_1.5.md](RASTER_ULTRA_1.5.md)), **4** Temporal Reconstruction, **5** Temporal Reconstruction + SMAA cleanup, **6** supersample (`r_ext_supersample`). Applies `r_ext_smaa` / `r_ext_fxaa` / `r_taa`. Latched modes need `vid_restart` for SMAA/FXAA resource rebuild.
- **SMAA** (Sub-pixel Morphological Anti-Aliasing): edge detection, blend weight, and compose passes. Cvars: `r_ext_smaa` (enable), `r_smaa_preset` (0=Custom, 1=Low, 2=Medium, 3=High, 4=Ultra), `r_smaa_threshold` (0.01–0.5), `r_smaa_local_contrast` (1–4), `r_smaa_max_search_steps` (8–32), `r_smaa_corner_rounding` (0–1). Preset overrides manual params when non-zero. Edge detection uses max(left,right) and max(top,bottom) deltas (reference SMAA), HDR-safe luma, configurable corner rounding, and explicit LOD 0 sampling. Requires `r_fbo 1`. Extent uses render-target size (`vk_get_render_target_width/height`), not window size alone.
- **MSAA**: Multi-sample anti-aliasing for geometry edges. Cvar `r_ext_multisample` (0|2|4|8|16). Requires `r_fbo 1`. `r_msaa_sample_shading` enables per-sample shading for better alpha/specular quality (~2x fragment cost). `r_ext_alpha_to_coverage` improves alpha-tested surfaces (foliage, grates) when MSAA is on. MSAA and SMAA can be used together: MSAA handles geometry edges, SMAA handles alpha/transparency edges.
- **Temporal Reconstruction** (`r_taa` / `r_aaMode` 4–5): confidence-guided resolve (`taa.frag`) with YCoCg variance clipping, depth/velocity/luma/reactive confidence, and optional post-recon SMAA cleanup (`r_aaMode 5`). Not the shipping default — `modern_vulkan.cfg` keeps **`r_aaMode 2` / `r_taa 0`**. First-person weapons do not poison whole-frame history; near-depth reactive prefers current. Debug: `r_debugMotionVectors`, `r_debugHistoryRejection`. Presets: `vulkan_overlay_aa_sharp.cfg`, `vulkan_overlay_temporal_recon.cfg`, `vulkan_overlay_temporal_perf.cfg`.

### Internal Resolution / Present Scaling
- **Internal resolution controls**: `r_renderScale`, `r_renderWidth`, and `r_renderHeight` let the renderer shade at one resolution and present at another. This is the in-engine alternative to vendor upscalers.
- `r_renderScale 0` disables custom internal resolution.
- `r_renderScale 1/2` use nearest filtering; `3/4` use linear filtering. Modes `2/4` preserve aspect ratio with black bars; `1/3` stretch to the window.
- Recommended combinations:
  - modern default path: `exec modern_vulkan.cfg` (SMAA 1x)
  - sharp/classic path: `exec vulkan_overlay_aa_sharp.cfg` (`r_aaMode 2`)
  - modern temporal: `exec vulkan_overlay_temporal_recon.cfg` (`r_aaMode 5`)
  - performance reconstruction: `exec vulkan_overlay_temporal_perf.cfg` (`r_aaMode 4` + renderScale)
- Current renderer truth: internal-resolution presentation is supported, but some post paths are still being hardened around source-region tracking and active render-target sizing. Prefer modest scale reductions first.
- **Effective scene render target (Vulkan):** **`vk_get_render_target_width()` / `vk_get_render_target_height()`** in `renderers/vulkan/vk_view_state.c` return **`vk.mainColorWidth` / `mainColorHeight`** when **`vk.fboActive`** and those extents are set (main HDR color attachment); otherwise **`vk.renderWidth` / `vk.renderHeight`** if nonzero; otherwise **`glConfig.vidWidth` / `vidHeight`**. Sun shadow and other passes can temporarily change **`vk.renderWidth`**; packing and screen-space work that must match the **main color** image (Forward+ SSBO viewport, tile cull, SSAO/HBAO texel pushes, SSR → color copy, temporal history invalidation on resize—see `vk_temporal.c`, `vk_forward_plus.c`, `vk_postfx_passes.c`) uses this helper so dimensions stay aligned with the attachment the player sees, not transient globals.

### Order-Independent Transparency (OIT)
- **Production:** WBOIT (`r_oit 1`) — weighted blended OIT for glass, smoke, particles, overlapping translucent layers; mode 3 overlay `vulkan_overlay_oit_clustered.cfg`; Spine 1.1 cert uses `r_oit 1`
- **Experimental:** MBOIT / Moment Transparency (`r_oit 2`) — `modern_vulkan_experimental.cfg` / `vulkan_overlay_mboit.cfg` only; not Spine 1.1 certified
- Stochastic alpha-clipped materials (`r_stochasticAlpha` 0–2) for foliage, grates, hair cards, fabric holes, and decals (separate from OIT)
- Cvar `r_oit` (0=off, 1=WBOIT production, 2=MBOIT experimental). Requires `r_fbo 1` and `vid_restart` after changing. Future tracks: [OIT_FUTURE_TRACKS.md](OIT_FUTURE_TRACKS.md)
- Opaque surfaces drawn first; transparent surfaces (alpha blend and additive) accumulated, then resolved
- Depth testing against opaque scene when MSAA off (transparent behind walls discarded)
- Additive blend (ONE/ONE) surfaces included for particles, sparks, etc.

### Directional Ambient Visibility (GTAO / RTAO / Reference AO)

Vulkan deferred rendering uses `vk_ambient_visibility.c` as the modern ambient-light visibility path. Its shared RGBA16F result stores diffuse visibility, an octahedrally encoded world-space bent normal, and a visibility-cone measure; a companion target stores average hit distance, variance, temporal confidence, and rejection reason. It runs after opaque G-buffer/class capture and before deferred dynamic lights, so it attenuates the indirect/static-lit scene without darkening direct lights. Transmission and emissive material classes are skipped. NIV, VFGI, RcGI, Surfel GI, and Hybrid1 RcGI/Surfel fusion sample the same visibility result for their diffuse indirect contribution; explicit Hybrid1 traced diffuse/specular paths do not.

- `r_ambientVisibilityMode 0`: off.
- `1`: compatibility SSAO/HBAO post path (`r_ssao` / `r_ssaoMethod`). This mode retains the old whole-scene combine and is not the preferred physically correct path.
- `2`: production GTAO. Paired horizon slices use a view-space radius, thickness compensation, open/missing-data edge fallback, directional bent-normal estimation, temporal accumulation, and a depth/normal/hit-distance bilateral filter.
- `3`: production ray-query RTAO using the shared Hybrid1 TLAS. If the build/device lacks ray query or the TLAS is not ready, it logs the reason and falls back to GTAO.
- `4`: adaptive hybrid. GTAO fills uncertain/contact regions and RTAO owns high-confidence visibility; the estimates are confidence-selected rather than multiplied.
- `5`: deterministic full-resolution Reference AO, 16–256 cosine-weighted rays per pixel, fixed seed, finite radius, and no temporal/spatial filtering. This is a validation path, not a shipping preset.

RTAO uses dedicated history (never TAA history), motion reprojection, depth/normal/bent-normal/hit-distance validation, local variance clipping, bounded history age, and shared temporal reset reasons for cuts, teleports, FOV/projection changes, resolution changes, and renderer-mode changes. The ray-query default treats geometry present in the shared TLAS as opaque and relies on TLAS population to omit ordinary blended transparency. Per-candidate alpha-test evaluation is not yet available because the Ambient Visibility descriptor set does not bind shared opacity/UV metadata; foliage represented in the TLAS therefore follows the TLAS opaque policy.

`r_rtaoDebug 1..20` exposes GTAO, raw/final visibility, bent normals, cone, average hit distance, variance, temporal confidence/weight, rejection reason, GTAO/RTAO difference, Reference AO scalar/bent-normal error, the cone-based specular-occlusion proxy, and screen-edge fallback. Debug 15/16 also run a sparse GPU reduction against the deterministic reference and report scalar visibility MAE plus bent-normal angular MAE in degrees. Use `ambient_visibility_status` for those metrics plus requested/effective mode, ray-query/TLAS state and fallback reason, image extents, approximate memory, history state, rays per pixel, and GPU timings; use `ambient_visibility_reset` to invalidate only this subsystem's history. The normal screenshot command captures any debug view.

The old SSAO/HBAO implementation remains available only as mode 1 and as a fallback for older configurations. Its controls are `r_ssao`, `r_ssaoMethod`, `r_hbaoDirections`, and `r_hbaoSteps`.

### IQM Morph Targets (Vulkan)
- IQM-only morph sidecar loading (`.morph`) with additive position and normal deltas
- Per-entity runtime weights (`re.SetEntityMorphWeight`)
- Top-K active channel evaluation (`r_morphMaxActive`, hard capped at 4)
- Distance-based LOD fade (`r_morphLodStart`, `r_morphLodEnd`)
- Debug visualization (`r_morphDebug`)
- Optional procedural breathing demo (`r_morphBreath`, `r_morphBreathAmp`, `r_morphBreathFreq`)

### Bloom and HDR
- HDR rendering with RGBA16F, RGBA32F, or optional RGBA64F color targets (r_hdr 1, 2, or 3; 64-bit gated, falls back to 32F)
- Bloom extraction with soft knee (Karis/UE4 style): `r_bloom_threshold`, `r_bloomKnee` for smooth highlight rolloff
- 4-pass Gaussian blur at progressively lower resolutions
- ACES, Reinhard, Filmic, and AgX tonemapping
- Exposure control with per-frame push constant
- Eye adaptation (`r_exposure_auto`): luminance pass + temporal blend; camera cut detection for fast snap
- Pre-exposure scaling (`r_pre_exposure_scale`)

### Post-Processing
- **Module**: `vk_postfx.c` / `vk_postfx.h` register and own most HDR-adjacent post cvars (SSR, procedural atmosphere, vegetation wind, vignette, chromatic aberration, film grain / look, motion blur, depth of field, sharpen, color grading + LUT). They are grouped for UI as **`CVG_RENDERER`** (renderer options).
- **Vulkan pipeline refresh**: At the start of each rendered frame, `PostFX_PostPipelinesNeedUpdate()` decides whether `vk_update_post_process_pipelines()` must run (`vk_post_process_refresh.c`). A rebuild happens when **bloom / SSAO / SMAA / SSR / OIT** are toggled, when **HDR FBO color format** changes (e.g. `r_hdr` path), when related **bloom** cvars change (`r_bloom`, `r_bloom_intensity`, `r_bloom_threshold`, `r_bloom_threshold_mode`, `r_bloom_modulate`, `r_bloomKnee`, `r_bloom_scatter`, `r_bloom_energyPreserve`), or after **Forward+ shade** invalidates pipelines (`r_forwardPlusShade`). Other PostFX tuning (SSR quality, atmosphere, grading, etc.) updates **uniforms** each frame and does **not** require a full post-pipeline rebuild.
- Panini projection with configurable parameters
- Final display-space ordered dithering (8x8 Bayer matrix, enabled by default with `r_dither 1`)
- Greyscale mode
- sRGB gamma correction
- **Screen-space reflections (SSR)**: `r_ssr` (default **1**, requires **`r_fbo 1`**). Tuners: `r_ssr_maxDistance`, `r_ssr_stepSize`, `r_ssr_thickness`, `r_ssr_fadeEdge`, `r_ssr_intensity`, `r_ssr_maxDepthGradient` (depth-edge rejection for silhouette artifacts), `r_ssr_roughnessThreshold` / `r_ssr_fresnelExponent` (view-dependent weighting without a roughness buffer). Turning SSR on/off triggers the post-pipeline refresh above. Render order vs bloom/SSAO: [HDR_GAPS.md](HDR_GAPS.md) section 7.
- **Procedural atmosphere**: `r_atmosphere` (default **0**; when **1**, Rayleigh+Mie sky; requires **`r_fbo 1`**). Sun direction and scattering cvars are registered alongside SSR in `vk_postfx.c`.
- **Camera motion blur**: Per-pixel velocity from depth + prev/curr view-proj; samples along motion vector. Cvars: `r_motionBlur`, `r_motionBlurStrength`, `r_motionBlurMaxRadius`, `r_motionBlurSamples`.
- **Depth of field**: Circle-of-confusion from linear depth; radial blur for out-of-focus areas. Cvars: `r_depthOfField`, `r_dofFocusDistance`, `r_dofFocusRange`, `r_dofAperture`, `r_dofMaxBlur`.
- Debug views (pre-tonemap HDR, luminance heatmap)
- Shader robustness: NaN/Inf guard in gamma pass, explicit `textureLod(..., 0.0)` in post-process shaders (gamma, blur, bloom, blend, SSAO), `to_src_uv` zero-scale fallback

### First Person Rendering
- Custom FOV for first-person primitives (arms, weapons) separate from scene FOV
- Anti-clipping scale factor to shrink first-person geometry toward camera
- Dedicated near clip plane for first-person to reduce clipping of arms/weapons
- Applies to entities with `RF_FIRST_PERSON` + `RF_DEPTHHACK` (view weapon, arms)
- Cvars: `r_firstPersonFov`, `r_firstPersonScale`, `r_firstPersonZNear`, `r_firstPersonFovEnabled`, `r_firstPersonScaleEnabled`

### GPU Occlusion Culling
- Entity occlusion culling via Vulkan occlusion queries (VkQueryPool)
- Depth-only world pass fills depth buffer; entity bounding boxes are drawn with occlusion queries
- Previous-frame visibility: query results read at frame end, used to skip occluded entities next frame
- Cvar: `r_occlusionCulling` (0=off, 1=on, default 0)
- Only entities (models) are culled; world geometry uses BSP/PVS

### Key Cvars
| Cvar | Default | Description |
|------|---------|-------------|
| `r_fbo` | 1 | Framebuffer objects (required for PBR, HDR, bloom, MSAA, SMAA, SSAO). Use vid_restart after changing. |
| `r_pbr` | 1 | Physically Based Rendering (metalness/roughness, IBL). Requires r_fbo 1. |
| `r_renderMode` | 0 | **0** forward, **1** deferred lighting mode, **2** Forward+ legacy recovery, **3** Unified Clustered (**Spine shipping default** via `modern_vulkan.cfg` → `modern_vulkan_stable.cfg`). Latched; `vid_restart`. Path ownership: [RENDERER_PATH_OWNERSHIP.md](RENDERER_PATH_OWNERSHIP.md). |
| `r_deferredGBuffer` | 0 | With `r_renderMode` 1/2/3: allocate albedo/normal/material/lighting G-buffer images. `modern_vulkan.cfg` sets **1** as a sidecar. Latched; `r_fbo` 1. |
| `r_deferredGBufferFill` | 0 | With G-buffer RTs: copy scene albedo after geometry (mode 3: after opaque). On non-MSAA FBO frames, opaque PBR material shaders directly export normals and material; MSAA/legacy paths keep the depth-derived fallback. Material is RGBA16F: metalness, roughness, AO, source confidence. `modern_vulkan.cfg` sets **1**. |
| `r_deferredGBufferDebug` | 0 | Before bloom: show G-buffer on scene color (1=albedo, 2=normal, 3=material, 4=lighting, 5=normal confidence, 6=motion vectors from the main material pass). |
| `r_deferredLighting` | 0 | Deferred dynamic lights (Forward+ clusters, point+spot): Disney/Burley **Fd** + Fresnel **kD**, GGX specular. Modes **1** and **3** (also 4). Mode 1/3 keep Forward+ shade for transparent; opaque handoff skips Forward+ add when path-ready. Enabled by the mode-3 modern default. |
| `r_deferredUnlitBase` | 1 | Additive dynamic on static-lit scene copy; skips classic lit-surf pass. **0** = legacy multiply composite. |
| `r_deferredLightingStrength` | 1 | Scale deferred dynamic diffuse (0–4). |
| `r_deferredSpecular` | 1 | GGX + Smith + Fresnel specular on deferred dynamic lights (0=diffuse only). |
| `r_deferredSpecularStrength` | 1 | Scale deferred dynamic specular (0–4). |
| `r_deferredAOCoupling` | 0.65 | Attenuate deferred dynamic light by the G-buffer **material** AO channel (0=off, 1=full). Does not sample SSAO. |
| `r_ambientVisibilityMode` | 2 | Directional Ambient Visibility: 0 off, 1 legacy SSAO/HBAO, 2 GTAO, 3 ray-query RTAO, 4 adaptive hybrid, 5 deterministic Reference AO. Latched; `vid_restart`. |
| `r_ambientVisibilityStrength` | 1 | Blend indirect visibility from open (0) to the computed result (1). Direct/emissive/transmission and explicitly traced light paths are excluded. |
| `r_rtaoRaysPerPixel` | 1 | Production RTAO rays per trace pixel (1–8). |
| `r_rtaoRadius` | 96 | Environmental visibility radius in world/view units. |
| `r_rtaoMinRadius` | 12 | Contact visibility band radius. |
| `r_rtaoRayBias` | 0.02 | Ray-query minimum distance. |
| `r_rtaoNormalBias` | 0.05 | World-space origin offset along the surface normal. |
| `r_rtaoResolutionScale` | 0.5 | RTAO trace scale (0.25–1). Latched; `vid_restart`. |
| `r_rtaoContactVisibility` | 1 | Calibrated near-contact band; tightens one estimator and is not multiplied as a second AO layer. |
| `r_rtaoTemporal` | 1 | Dedicated Ambient Visibility temporal accumulation. |
| `r_rtaoSpatialFilter` | 1 | Adaptive depth/normal/hit-distance bilateral filtering. |
| `r_rtaoMaxHistory` | 16 | Maximum temporal history age (1–64 frames). |
| `r_rtaoDebug` | 0 | Ambient Visibility/reference comparison view (0 normal, 1–20 diagnostics). |
| `r_referenceAORays` | 64 | Reference AO rays per pixel (16–256). |
| `r_referenceAOSeed` | 1 | Fixed deterministic Reference AO sample seed. |
| `r_gtaoSlices` | 4 | GTAO horizon slice count (2–8). |
| `r_gtaoSteps` | 6 | GTAO steps per half-slice (2–8). |
| `r_gtaoThickness` | 2 | GTAO depth-layer thickness compensation in view units. |
| `r_gtaoHalfRes` | 0 | Run GTAO at half resolution. Latched; `vid_restart`. |
| `r_deferredDefaultMetalness` | 0 | Fallback metalness for MSAA/depth-derived G-buffer export. |
| `r_deferredDefaultRoughness` | 0.55 | Fallback roughness for MSAA/depth-derived G-buffer export. |
| `r_deferredNormalEdgeThreshold` | 0.08 | View-space depth delta used to reject silhouette-crossing neighbors during deferred normal reconstruction. |
| `r_volumetricFog` | 0 | Volumetric fog enable (0=off, 1=on) |
| `r_vdbFog` | 0 | Blend GPU-uploaded bound VDB density (`vdb_bind_fog`) into global volumetric density (requires `r_volumetricFog` 1 and `VDB_UploadToGPU`) |
| `r_vdbFogBlend` | 0.5 | VDB density blend weight when `r_vdbFog` 1 |
| `r_volumetricFogDensity` | 0.35 | Volumetric density multiplier |
| `r_volumetricFogQuality` | 2 | Quality tier (0=low, 1=medium, 2=high, 3=ultra; latched) |
| `r_fogFluid` | 0 | Fluid-driven volumetric fog (0=off, 1=on). Vorticity/buoyancy: `r_fogFluidVorticity`, `r_fogFluidBuoyancy`. |
| `r_bloom` | 0 | Bloom enable |
| `r_bloom_threshold` | 0.6 | Bloom extraction threshold |
| `r_hdr` | 2 | HDR format (0=8-bit, 1=RGBA16F, 2=RGBA32F default, 3=aliases to 32-bit with startup warning; RGBA64F not implemented) |
| `r_hdr_lightmap_scale` | 2.0 | HDR lightmap intensity (1=normal, 2+=brighter for 8-bit lightmaps) |
| `r_lightmap_srgb_decode` | 0 | When r_hdr 1/2: 0=linear (default), 1=sRGB→linear for gamma-encoded lightmaps |
| `r_ndgi` | 0 | **Neural Dynamic GI** (experimental): temporal baked lightmaps from neural feature atlas + VT page decode. See [NEURAL_DYNAMIC_GI.md](NEURAL_DYNAMIC_GI.md). Latched; needs BSP lightmaps. |
| `r_ndgi_time` | 0 | NDGI blend time in `[0,1]` (day/night, weather keys). |
| `r_ndgi_cycle` | 0 | Auto-cycle `r_ndgi_time` over `r_ndgi_cyclePeriod` seconds. |
| `r_niv` | 0 | **Neural Irradiance Volume** (experimental): G-buffer indirect from compact 3D neural probe field. See [NEURAL_IRRADIANCE_VOLUME.md](NEURAL_IRRADIANCE_VOLUME.md). |
| `r_niv_scale` | 1 | NIV decode resolution scale (0.25–1). |
| `r_niv_normalAtten` | 0.6 | NIV normal-facing attenuation to reduce indirect GI leaks. |
| `r_niv_ao` | 0.75 | NIV SSAO/HBAO coupling for contact-aware indirect attenuation. |
| `r_nslm` | 0 | **Neural Six-way Lightmaps** (experimental): froxel scatter modulation for fog/smoke/dust. Requires `r_volumetricFog 1`. See [NEURAL_SIXWAY_LIGHTMAPS.md](NEURAL_SIXWAY_LIGHTMAPS.md). |
| `r_nslm_strength` | 1 | NSLM scattering scale in froxels. |
| `r_nslm_sixWaySharpness` | 2 | Six-way axis lobe sharpness from view direction. |
| `r_nist` | 0 | **Neural Image Space Tessellation** (experimental): screen-space silhouette smoothing. See [NEURAL_IMAGE_SPACE_TESSELLATION.md](NEURAL_IMAGE_SPACE_TESSELLATION.md). |
| `r_nist_strength` | 1 | NIST silhouette blend strength. |
| `r_nist_scale` | 1 | NIST refine resolution scale (0.25–1). |
| `r_nvc` | 0 | **Neural Visibility Cache** (experimental): Forward+ ReSTIR-style direct refine with neural visibility. Requires `r_forwardPlus 1`. See [NEURAL_VISIBILITY_CACHE.md](NEURAL_VISIBILITY_CACHE.md). |
| `r_nvc_strength` | 1 | NVC direct lighting refine scale. |
| `r_nvc_scale` | 1 | NVC cache/ReSTIR resolution scale (0.25–1). |
| `r_nvc_restirMode` | 1 | NVC `0`=cache only, `1`=ReSTIR refine + composite. |
| `r_fsa` | 0 | **Forget Superresolution / Sample Adaptively** (experimental): importance-guided sub-1-SPP RTX + denoise. See [FORGET_SUPERRESOLUTION_FSA.md](FORGET_SUPERRESOLUTION_FSA.md). |
| `r_fsa_budget` | `0.25` | FSA target samples per pixel (may be &lt;1). |
| `r_fsa_rtxAdaptive` | 1 | Stochastic RTX traces from FSA importance map. |
| `r_vfgi` | 0 | **Vertex Features Neural GI** (experimental): per-vertex features + spatial index decode. See [VERTEX_FEATURES_NEURAL_GI.md](VERTEX_FEATURES_NEURAL_GI.md). |
| `r_vfgi_strength` | 1 | VFGI indirect irradiance scale. |
| `r_vfgi_scale` | 1 | VFGI decode resolution scale (0.25–1). |
| `r_vfgi_normalAtten` | 0.6 | VFGI normal-facing attenuation to reduce indirect GI leaks. |
| `r_vfgi_ao` | 0.75 | VFGI SSAO/HBAO coupling for contact-aware indirect attenuation. |
| `r_vfgi_vertCap` | `524288` | Max unique world vertices for VFGI (latched). |
| `r_renderformer` | 0 | **RenderFormer** neural triangle preview (experimental): transport + view decode. See [RENDERFORMER.md](RENDERFORMER.md). |
| `r_renderformer_strength` | 1 | RenderFormer transport/decode/composite scale. |
| `r_renderformer_scale` | 1 | RenderFormer decode resolution (0.25–1). |
| `r_renderformer_triCap` | `32768` | Max world triangle tokens (latched). |
| `r_wpt` | 0 | **Wavefront path experiment** (queued rays + bounce waves). See [WAVEFRONT_PATH_TRACING.md](WAVEFRONT_PATH_TRACING.md). |
| `r_wpt_bounces` | `1` | WPT extension waves (0–2). |
| `r_vuda` | 0 | **VUDA** CUDA-Vulkan spatial multiplexing scaffold (requires `USE_VUDA` build). See [VUDA.md](VUDA.md). |
| `r_vuda_mux` | 1 | Open post-submit CUDA compute window. |
| `r_vuda_slotMb` | 64 | Exported shared buffer per slot (MiB). |
| `cl_vuda` | 0 | Client CUDA scheduler + import of Vulkan fds. |
| `r_mgs` | 0 | **Mobile-GS** (experimental): tiered Gaussian splatting (`1`=mobile … `3`=high). See [MOBILE_GAUSSIAN_SPLATTING.md](MOBILE_GAUSSIAN_SPLATTING.md). |
| `r_mgs_strength` | 0.85 | Mobile-GS splat / composite strength. |
| `r_squeezeme` | 0 | **SqueezeMe** (experimental): distilled animatable Gaussian avatars (linear+GCS, arXiv:2412.15171). See [SQUEEZEME.md](SQUEEZEME.md). |
| `r_squeezeme_tier` | 2 | Mobile-GS splat tier when `r_mgs=0` and SqueezeMe is on. |
| `r_squeezeme_avatars` | 1 | Concurrent SqueezeMe avatars (1–3). |
| `r_wsp` | 0 | **WebSplatter** (experimental): WebGPU-aligned tile splats (`1`=mobile … `3`=high). See [WEB_SPLATTER.md](WEB_SPLATTER.md). |
| `r_wsp_strength` | 0.85 | WebSplatter composite strength. |
| `r_pre_exposure_scale` | 1.0 | Pre-exposure scale for bloom/tonemap pipeline |
| `r_tonemap` | 3 | Tonemapping (0=none, 1=Reinhard, 2=ACES, 3=Filmic, 4=AgX) |
| `r_exposure` | 1.0 | Exposure multiplier |
| `r_exposure_auto` | 0 | Eye adaptation (0=manual, 1=temporal blend toward target) |
| `r_exposure_auto_target` | 0.5 | Target exposure for eye adaptation |
| `r_exposure_auto_speed` | 2.0 | Adaptation speed (higher = faster) |
| `r_taa` | 0 | Temporal resolve after post-fog, before luminance/gamma. `modern_vulkan.cfg` sets **1**. Uses `vk_temporal` resets; skips portals, menus, unreliable motion. See [HDR_GAPS.md](HDR_GAPS.md) §6.8. |
| `r_taaMotionVectors` | 1 | TAA history UV from main-pass motion attachment (1) or depth reprojection (0). |
| `r_taa_feedbackStationary` | 0.92 | TAA history feedback for stable pixels. Higher = smoother, lower = more responsive. |
| `r_taa_feedbackMotion` | 0.72 | TAA history feedback for moving pixels. Lower helps reduce ghosting. |
| `r_taa_sharpen` | 0.12 | Post-resolve sharpening applied inside the TAA pass. |
| `r_renderScale` | 0 | Custom internal-resolution presentation mode: 0=off, 1/2=nearest, 3/4=linear; 2/4 preserve aspect ratio. |
| `r_renderWidth` | 800 | Internal render width used when `r_renderScale > 0`. |
| `r_renderHeight` | 600 | Internal render height used when `r_renderScale > 0`. |
| `r_post_contrast` | 1.0 | Post-tonemap contrast (1=neutral, >1=punchier, <1=flatter) |
| `r_post_saturation` | 1.0 | Post-tonemap saturation (1=neutral, >1=vivid, <1=desaturated) |
| `r_grade_hue` | 0.0 | Display-referred hue rotation in degrees (-180 to 180) |
| `r_grade_vibrance` | 0.0 | Selective saturation boost for muted colors (-1 to 1; 0=neutral) |
| `r_atmosphere` | 0 | Procedural atmospheric sky (Rayleigh+Mie). **1** replaces grey sky when no HDR skybox; requires `r_fbo 1`. |
| `r_atmosphere_scale` | 4.0 | HDR scale multiplier for sky brightness. Works with auto exposure; increase if sky appears dark. |
| `r_skyboxHDR` | "" | Path to HDR skybox panorama: EXR or Radiance .hdr (empty = use atmosphere or map skybox). |
| `r_ssao` | 0 | SSAO/HBAO enable |
| `r_ssaoMethod` | 0 | AO algorithm: 0=SSAO (hemisphere), 1=HBAO (horizon-based). Requires vid_restart. |
| `r_hbaoDirections` | 8 | HBAO ray directions (4=fast, 8=default, 16=quality) |
| `r_hbaoSteps` | 8 | HBAO steps per direction (4=fast, 8=default, 16=quality) |
| `r_firstPersonFov` | 90 | Horizontal FOV (degrees) for first-person primitives (range 1–179) |
| `r_firstPersonScale` | 1.0 | Anti-clipping scale for first-person primitives |
| `r_firstPersonZNear` | 0.5 | Near clip plane for first-person (smaller = less clipping of arms/weapons) |
| `r_firstPersonFovEnabled` | 1 | Use custom FOV for first-person (0=scene FOV) |
| `r_firstPersonScaleEnabled` | 1 | Apply scale for anti-clipping (0=no scale) |
| `r_occlusionCulling` | 0 | GPU occlusion culling for entities (0=off, 1=on) |

See [HDR_GAPS.md](HDR_GAPS.md) for HDR pipeline gaps, risks, and render order.

### Engine HUD / console text (FreeType vs SDF)
- **Default path:** `r_font` (TrueType) + `cl_builtInTtf` **1** (default) — FreeType rasterizes glyphs into a runtime atlas (no offline bake). Used for engine console, small HUD strings, and bigchars-style paths when the font loads.
- **Raster quality:** `r_fontDpi` (default **96**, clamp **72–144**) feeds `FT_Set_Char_Size` device resolution. **`r_fontHint`**: **0** = `FT_LOAD_DEFAULT`, **1** (default) = `FT_LOAD_TARGET_LIGHT`, **2** = `FT_LOAD_TARGET_NORMAL`. Outlines use **`FT_LOAD_NO_BITMAP`** so embedded strikes do not override hinted outlines. **`r_fontAtlasSize`** (default **512**) snaps atlas pages to **256**, **512**, **1024**, or **2048** so high-DPI and LCD glyphs are not forced through a cramped legacy page size. **`r_fontMipmap`** (default **1**, **0**/**1**): when **1**, each FreeType atlas page is uploaded with a mip chain so minified UI text filters more cleanly; **0** restores a single mip (legacy). Apply changes with **`reloadTtf`** (clears the renderer’s TrueType registration cache and re-runs `CL_RegisterBuiltInTrueTypeFonts`) or **`vid_restart`** / **`vid_restart keep_window`**; startup logs raster settings when FreeType initializes. Each reload may leave prior `fonts/_ftg_*` atlases allocated until a full renderer reinit—use **`vid_restart`** if you toggle settings many times and want GPU memory reclaimed.
- **Pixel / virtual layout (client):** **`r_fontConsoleAlign`** (**1** default) baseline-aligns each TrueType glyph inside the fixed cell (console, notify, and 640×480 HUD strings) using FreeType `top` metrics; **0** restores legacy top-aligned quads. **`r_fontShadow`** (**0–8**, default **2**) is the drop-shadow offset in **screen pixels** (pixel path) or **virtual units** (HUD path); **0** skips the shadow pass. **`r_fontSubpixel`** (**0/1**, default **0**) adds a **0.375** bias after projection to soften stair-stepping with linear filtering on some panels (not Microsoft ClearType; optional because on-screen subpixel preference varies widely—see [bias2009-subpixel-preference.md](research/bias2009-subpixel-preference.md)).
- **LCD/subpixel path:** `r_fontLcd 1` builds RGB LCD glyph atlases; `r_fontSubpixelPos 1` enables fractional horizontal placement in the Vulkan subpixel text shader; `r_fontLcdWeight` (default **0.35**) blends between monochrome alpha coverage and full per-channel LCD coverage. This keeps the modern path configurable instead of forcing color fringing.
- **Optional SDF:** BMFont `.fnt` + distance atlas (`r_sdfEnable`, `r_sdfFont`, optional `r_sdfFontAtlas`, `r_sdfSmoothing`). Vulkan **`r_sdfScreenAa`** (default **2**, **0** = off) scales **`fwidth(distance)`** in `frag_ui_sdf_text` so edge anti-aliasing widens under magnification (resolution-independent AA; see Green’s Valve SDF notes and Alvin 2020 on derivative-based bands; local PDF: [independent-fonts-in-games.pdf](research/independent-fonts-in-games.pdf)). When both FreeType and SDF are configured, the engine **draws FreeType first**; set `cl_builtInTtf` **0** to prefer SDF for those paths.
- **Autopick:** `r_sdfAuto` (default **0**) may preset `r_sdfFont` to the packaged demo metrics when enabled; it does **not** turn on `r_sdfEnable`.
- **Fallback:** legacy 16×16 bitmap charset when neither path applies.
- **GPU vector outlines:** `r_vectorFont 1` — Lengyel JCGT 2017 curve texture + winding shader (no atlas). `r_vectorFontMode`: **0** = Lengyel (default); **2** = planned Loop & Blinn + mesh glyphlets ([AMD GPUOpen](https://gpuopen.com/learn/mesh_shaders/mesh_shaders-font-rendering/), not implemented). See [VECTOR_FONT.md](VECTOR_FONT.md).
- **Further reading (CPU/GPU font workload):** Recker, Beretta & Lin (HP Labs **HPL-2009-181**) describe a print RIP that keeps **outline scan conversion** on the CPU (Ghostscript) and feeds the GPU **monochrome horizontal spans** from glyph bitmaps for parallel fill—same *separation* as our **FreeType atlas → textured quads** path, though we do not use their span display list. Summary: [recker2009-gpu-rip-fonts.md](research/recker2009-gpu-rip-fonts.md).

### SVG Graphics
- Vector SVG assets load as textures (icons, logos, UI graphics)
- Uses librsvg when available; NanoSVG fallback on all platforms
- Cvars: `r_svgRasterScale`, `r_svgMaxRasterSize`, `r_svgMaxFileBytes`
- Use `.svg` files in textures/ or gfx/ for scalable UI elements

## Future Renderers (Planned)

See [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md) for architecture and implementation plans:

- **Vulkan RTX**: demo path with `USE_VULKAN_RTX=ON`, `r_rtx` / `r_rtxDemo`, world BLAS; optional **`r_rtxEntities`** packs MD3 LOD0 + **CPU-skinned IQM/MDR** + static/CPU-skinned glTF (AABB on pack fail) into the entity BLAS; Hybrid1 quality tiers via **`r_hybrid1Quality`**. See [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md), [HYBRID_RENDERING1.md](HYBRID_RENDERING1.md).
- **GRTX (Gaussian RT)**: `r_grtx` / `r_grtxDemo` over procedural 3D Gaussian AABB proxies (separate from BSP `r_rtx` demo). See [GAUSSIAN_RAY_TRACING_GRTX.md](GAUSSIAN_RAY_TRACING_GRTX.md).
- **Path trace experiment (C6)**: `r_pathtrace` + `r_pathtrace_arch` (`megakernel` / `wavefront`) over shared RTX world TLAS — requires `r_rtx 1`, `r_rtxDemo 1`, `USE_VULKAN_RTX`. Not SP ship lighting. See [PATHTRACE_ARCH_BENCHMARK.md](PATHTRACE_ARCH_BENCHMARK.md).
- **Mobile-GS**: `r_mgs` tiered compute splatting (Android-friendly; no RTX). See [MOBILE_GAUSSIAN_SPLATTING.md](MOBILE_GAUSSIAN_SPLATTING.md).
- **WebSplatter**: `r_wsp` tile-binned splats (WebGPU-portable compute layout). See [WEB_SPLATTER.md](WEB_SPLATTER.md).
- **Metal**: Roadmap scaffold — `USE_METAL_RENDERER=ON` (Apple) builds `idtech3_metal` dlopen plugin; native backend TBD. See `docs/METAL_RENDERER.md`.
- **DXR**: Roadmap scaffold — `USE_DXR_RENDERER=ON` (Windows) builds `idtech3_dxr` dlopen plugin; DX12 backend TBD. See `docs/DXR_RENDERER.md`.
- **WebGPU (Wasm)**: No native plugin; portable `wsp`/`mgs` compute on Vulkan today. See `docs/WEBGPU_ROADMAP.md`.

For practical renderer direction and priority order, see [RENDERER_2026_ARCHITECTURE_PASS.md](RENDERER_2026_ARCHITECTURE_PASS.md).
