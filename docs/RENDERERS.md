# Renderer Features

For a **build + manual validation checklist** (CI parity, shader coverage, GPU passes), see [RENDERER_CONFIDENCE.md](RENDERER_CONFIDENCE.md).

## Renderer backends (id Tech 7–style)

| Backend | Status | Notes |
|---------|--------|-------|
| **Vulkan** | **Shipping** | Only production renderer (`idtech3_vulkan.so` / static on Windows) |
| **Vulkan RTX** | Experimental | `-DUSE_VULKAN_RTX=ON` / `./scripts/compile_engine.sh vulkan rtx` |
| **DXR** | Roadmap scaffold | Windows plugin stub (`USE_DXR_RENDERER`); see [DXR_RENDERER.md](DXR_RENDERER.md) |
| **WebGPU** | Roadmap | Browser/Wasm target; portable compute shaders validated on Vulkan today — [WEBGPU_ROADMAP.md](WEBGPU_ROADMAP.md) |
| **OpenGL** | **Removed** | No `idtech3_opengl` target, no fallback; `cl_renderer opengl` maps to Vulkan with a warning |

Build: `./scripts/compile_engine.sh vulkan` (OpenGL/`opengl` arg is rejected).

## Vulkan Renderer (Primary)

The Vulkan 1.4 renderer is the primary rendering backend, built as a shared library (`idtech3_vulkan.so`). Requests Vulkan 1.4 when available; validation layers (Khronos, then LUNARG fallback) are enabled in debug builds on all platforms.

### Current Architecture
- Forward renderer with a large HDR/post-processing stack
- `r_renderMode`: **0** forward (classic projector; `r_forwardPlus` may still be 1), **1** deferred (scaffold + optional `r_deferredLighting`; **`r_deferredGBuffer 1`** allocates RTs), **2** Forward+ primary (`r_forwardPlus` 1, `r_forwardPlusShade` 1, latched via `vid_restart`)
- Vulkan is the supported rendering backend
- **Shared temporal reset policy** (`vk_temporal.c`): centralizes history invalidation for volumetrics, motion vectors, exposure. Resize, map load, camera cut, and missing prev-frame data trigger resets. Ready for future TAA/upscaler integration.
- See [RENDERER_2026_ARCHITECTURE_PASS.md](RENDERER_2026_ARCHITECTURE_PASS.md) for the focused 2026 renderer direction

### Vulkan Forward+ scaffolding

**GPU light packing + per-tile cull** on the forward path (`r_forwardPlus` default **1**; `r_renderMode 2` forces it on):

| Cvar | Role |
|------|------|
| `r_forwardPlus` | **1** (default) on; **0** off (**latched**; `vid_restart`). Packs up to **64** refdef dlights on GPU. |
| `r_forwardPlusMaxPerTile` | **4–8** lights indexed per **16×16** tile (**latched**; `vid_restart`). Lowers GPU work vs default **8**; tile SSBO keeps **8** `uint32` slots either way. |
| `r_forwardPlusDebug` | **0–1** float: PBR heatmap overlay (lights per tile + borders). |
| `r_forwardPlusShade` | **0–4** float: PBR shade for dlight indices **0–31** (not in `tess.dlightBits`); pipeline rebuild on change. |
| `r_forwardPlusOverflowShade` | **0–4** (default **0**): PBR shade for indices **32–63**; requires `r_classicLighting 0`. Try **0.5** with modern lighting. |
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

Code: `src/renderers/vulkan/vk_forward_plus.c`, `VK_FP_*` constants; cvar registration `src/renderers/vulkan/tr_init.c`.

**Audit:** [FORWARD_PLUS_PIPELINE_AUDIT.md](FORWARD_PLUS_PIPELINE_AUDIT.md).

### Compute shaders, mesh shaders (NV), and upscaling (DLSS)
- **Compute:** already central to Vulkan (volumetric fog stages, vegetation wind, fluid sim, terrain CBT, etc.).
- **Mesh shaders (NVIDIA):** optional device extension **`VK_NV_mesh_shader`** when **`r_vk_meshShaderNV 1`** (default **0**, **latched**, `vid_restart`). Enables `meshShader` in `VkPhysicalDeviceMeshShaderFeaturesNV` for future pipelines; **no mesh-shader draw path** is wired yet - safe on `main`.
- **DLSS / NGX:** **not** linked in this repository (proprietary NVIDIA SDK). Use **`r_renderScale`** / internal resolution, **TAA** (`r_taa`), **SMAA/FXAA/MSAA**, or **driver-level** scaling (e.g. NVIDIA NIS/DLSS in control panel) where applicable. Startup logs state that DLSS is not in-engine.
- **SP upscale preset:** `r_upscale 1` with `r_renderScale 0.75` (internal 75% res, spatial upscale). `r_upscale 2` is FSR2 experimental (falls back to spatial until SDK wired). See `demo_sp_slice.cfg` in idtech3_demo pk3.
- **Simulation profile (AMBF-Vulkan):** `sim_render_profile 1` then `vid_restart` — MSAA + FXAA, Reinhard tonemap, lightweight post. See [SIM_RENDER_PROFILE.md](SIM_RENDER_PROFILE.md).

### Physically Based Rendering (PBR)
- Metalness/roughness workflow with Cook-Torrance BRDF
- Image-based lighting (IBL) with prefiltered environment maps
- BRDF lookup table (LUT) generation
- Texture map types: normal, physical (RMO/ORM/RMOS variants), emissive, clearcoat, sheen, anisotropy, transmission, subsurface
- Spherical harmonics for diffuse irradiance
- Multi-scatter energy compensation
- **Glint NDF**: Procedural microfacet NDF for specular glints (replaces GGX D term on low-roughness surfaces). Cvars: `r_glint`, `r_glintMode`, `r_glintDensity`, `r_glintMicrofacetRoughness`, `r_glintPixelFilterSize`, `r_glintSampleBudget`, `r_glintMaxLodClamp`, `r_glintRoughnessLo`, `r_glintRoughnessHi`, `r_glintDMax`. Debug modes 5–8 via `r_pbr_debug`.
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
- **SMAA** (Sub-pixel Morphological Anti-Aliasing): edge detection, blend weight, and compose passes. Cvars: `r_ext_smaa` (enable), `r_smaa_preset` (0=Custom, 1=Low, 2=Medium, 3=High, 4=Ultra), `r_smaa_threshold` (0.01–0.5), `r_smaa_local_contrast` (1–4), `r_smaa_max_search_steps` (8–32), `r_smaa_corner_rounding` (0–1). Preset overrides manual params when non-zero. Edge detection uses max(left,right) and max(top,bottom) deltas (reference SMAA), HDR-safe luma, configurable corner rounding, and explicit LOD 0 sampling. Requires `r_fbo 1`.
- **MSAA**: Multi-sample anti-aliasing for geometry edges. Cvar `r_ext_multisample` (0|2|4|8|16). Requires `r_fbo 1`. `r_msaa_sample_shading` enables per-sample shading for better alpha/specular quality (~2x fragment cost). `r_ext_alpha_to_coverage` improves alpha-tested surfaces (foliage, grates) when MSAA is on. MSAA and SMAA can be used together: MSAA handles geometry edges, SMAA handles alpha/transparency edges.
- **TAA**: Optional temporal resolve for the Vulkan HDR/post path. Cvars: `r_taa`, `r_taa_feedbackStationary`, `r_taa_feedbackMotion`, `r_taa_sharpen`. Best used with world rendering plus internal-resolution rendering (`r_renderScale`) when you want a softer, more temporally stable presentation than pure SMAA/MSAA. TAA is intentionally conservative: portal views, non-world/menu/cinematic paths, missing history, and first-person projection transitions invalidate or bypass history rather than trying to blend through unstable motion.

### Internal Resolution / Present Scaling
- **Internal resolution controls**: `r_renderScale`, `r_renderWidth`, and `r_renderHeight` let the renderer shade at one resolution and present at another. This is the in-engine alternative to vendor upscalers.
- `r_renderScale 0` disables custom internal resolution.
- `r_renderScale 1/2` use nearest filtering; `3/4` use linear filtering. Modes `2/4` preserve aspect ratio with black bars; `1/3` stretch to the window.
- Recommended combinations:
  - sharp/default path: `r_taa 0`, `r_ext_smaa 1`, optional MSAA
  - softer temporal path: `r_taa 1`, `r_renderScale 3` or `4`, custom `r_renderWidth` / `r_renderHeight`
- Current renderer truth: internal-resolution presentation is supported, but some post paths are still being hardened around source-region tracking and active render-target sizing. Prefer modest scale reductions first.
- **Effective scene render target (Vulkan):** **`vk_get_render_target_width()` / `vk_get_render_target_height()`** in `src/renderers/vulkan/vk_view_state.c` return **`vk.mainColorWidth` / `mainColorHeight`** when **`vk.fboActive`** and those extents are set (main HDR color attachment); otherwise **`vk.renderWidth` / `vk.renderHeight`** if nonzero; otherwise **`glConfig.vidWidth` / `vidHeight`**. Sun shadow and other passes can temporarily change **`vk.renderWidth`**; packing and screen-space work that must match the **main color** image (Forward+ SSBO viewport, tile cull, SSAO/HBAO texel pushes, SSR → color copy, temporal history invalidation on resize—see `vk_temporal.c`, `vk_forward_plus.c`, `vk_postfx_passes.c`) uses this helper so dimensions stay aligned with the attachment the player sees, not transient globals.

### Order-Independent Transparency (OIT)
- Weighted Blended OIT (WBOIT) for correct blending of overlapping transparent surfaces
- Cvar `r_oit` (0=off, 1=on). Requires `r_fbo 1` and `vid_restart` after changing
- Opaque surfaces drawn first; transparent surfaces (alpha blend and additive) accumulated, then resolved
- Depth testing against opaque scene when MSAA off (transparent behind walls discarded)
- Additive blend (ONE/ONE) surfaces included for particles, sparks, etc.

### Screen-Space Ambient Occlusion (SSAO / HBAO)
- **SSAO** (`r_ssaoMethod 0`): Hemisphere sampling with Halton(2,3) low-discrepancy sequence for better distribution and less noise
- **HBAO** (`r_ssaoMethod 1`): Horizon-Based Ambient Occlusion - raymarches depth in multiple directions, tracks horizon angles; higher quality with fewer samples
- Configurable radius, bias, intensity, power; SSAO sample count or HBAO directions/steps
- Separable blur pass
- Combine pass with debug visualization modes

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
- Ordered dithering (8x8 Bayer matrix)
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
| `r_renderMode` | 0 | **0** forward, **1** deferred (scaffold RTs with `r_deferredGBuffer`; optional `r_deferredLighting`), **2** Forward+ primary. Latched; `vid_restart`. |
| `r_deferredGBuffer` | 0 | With `r_renderMode` 1: allocate albedo/normal/material/lighting G-buffer images. Latched; `r_fbo` 1. |
| `r_deferredGBufferFill` | 0 | With scaffold RTs: copy scene albedo + depth-derived normal/material after geometry. |
| `r_deferredGBufferDebug` | 0 | Before bloom: show G-buffer on scene color (1=albedo, 2=normal, 3=material, 4=lighting). |
| `r_deferredLighting` | 0 | Experimental deferred diffuse (Forward+ tiles, point+spot). Replaces scene color after geometry. Latches `r_forwardPlusShade` 0 with `vid_restart`. |
| `r_deferredUnlitBase` | 1 | Additive dynamic on static-lit scene copy; skips classic lit-surf pass. **0** = legacy multiply composite. |
| `r_deferredLightingStrength` | 1 | Scale deferred dynamic diffuse (0–4). |
| `r_deferredSpecular` | 1 | Blinn-Phong specular on dynamic lights in deferred pass (0=diffuse only). |
| `r_volumetricFog` | 0 | Volumetric fog enable (0=off, 1=on) |
| `r_vdbFog` | 0 | Blend GPU-uploaded bound VDB density (`vdb_bind_fog`) into global volumetric density (requires `r_volumetricFog` 1 and `VDB_UploadToGPU`) |
| `r_vdbFogBlend` | 0.5 | VDB density blend weight when `r_vdbFog` 1 |
| `r_volumetricFogDensity` | 0.35 | Volumetric density multiplier |
| `r_volumetricFogQuality` | 2 | Quality tier (0=low, 1=medium, 2=high, 3=ultra; latched) |
| `r_fogFluid` | 0 | Fluid-driven volumetric fog (0=off, 1=on). Vorticity/buoyancy: `r_fogFluidVorticity`, `r_fogFluidBuoyancy`. |
| `r_bloom` | 0 | Bloom enable |
| `r_bloom_threshold` | 0.6 | Bloom extraction threshold |
| `r_hdr` | 2 | HDR format (0=8-bit, 1=RGBA16F, 2=RGBA32F default, 3=RGBA64F optional/gated, -1=4-bit test) |
| `r_hdr_lightmap_scale` | 2.0 | HDR lightmap intensity (1=normal, 2+=brighter for 8-bit lightmaps) |
| `r_lightmap_srgb_decode` | 0 | When r_hdr 1/2: 0=linear (default), 1=sRGB→linear for gamma-encoded lightmaps |
| `r_ndgi` | 0 | **Neural Dynamic GI** (experimental): temporal baked lightmaps from neural feature atlas + VT page decode. See [NEURAL_DYNAMIC_GI.md](NEURAL_DYNAMIC_GI.md). Latched; needs BSP lightmaps. |
| `r_ndgi_time` | 0 | NDGI blend time in `[0,1]` (day/night, weather keys). |
| `r_ndgi_cycle` | 0 | Auto-cycle `r_ndgi_time` over `r_ndgi_cyclePeriod` seconds. |
| `r_niv` | 0 | **Neural Irradiance Volume** (experimental): G-buffer indirect from compact 3D neural probe field. See [NEURAL_IRRADIANCE_VOLUME.md](NEURAL_IRRADIANCE_VOLUME.md). |
| `r_niv_scale` | 1 | NIV decode resolution scale (0.25–1). |
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
| `r_tonemap` | 2 | Tonemapping (0=none, 1=Reinhard, 2=ACES) |
| `r_exposure` | 1.0 | Exposure multiplier |
| `r_exposure_auto` | 0 | Eye adaptation (0=manual, 1=temporal blend toward target) |
| `r_exposure_auto_target` | 0.5 | Target exposure for eye adaptation |
| `r_exposure_auto_speed` | 2.0 | Adaptation speed (higher = faster) |
| `r_taa` | 0 | Temporal resolve after post-fog, before luminance/gamma. Uses `vk_temporal` resets; skips portals, menus, unreliable motion. See [HDR_GAPS.md](HDR_GAPS.md) §6.8. |
| `r_taaMotionVectors` | 1 | TAA history UV from main-pass motion attachment (1) or depth reprojection (0). |
| `r_taa_feedbackStationary` | 0.92 | TAA history feedback for stable pixels. Higher = smoother, lower = more responsive. |
| `r_taa_feedbackMotion` | 0.72 | TAA history feedback for moving pixels. Lower helps reduce ghosting. |
| `r_taa_sharpen` | 0.12 | Post-resolve sharpening applied inside the TAA pass. |
| `r_renderScale` | 0 | Custom internal-resolution presentation mode: 0=off, 1/2=nearest, 3/4=linear; 2/4 preserve aspect ratio. |
| `r_renderWidth` | 800 | Internal render width used when `r_renderScale > 0`. |
| `r_renderHeight` | 600 | Internal render height used when `r_renderScale > 0`. |
| `r_post_contrast` | 1.0 | Post-tonemap contrast (1=neutral, >1=punchier, <1=flatter) |
| `r_post_saturation` | 1.0 | Post-tonemap saturation (1=neutral, >1=vivid, <1=desaturated) |
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
- **Raster quality:** `r_fontDpi` (default **72**, clamp **72–144**) feeds `FT_Set_Char_Size` device resolution — **96** is typical “desktop” DPI and yields a sharper atlas when the UI scales glyphs up (e.g. large console). **`r_fontHint`**: **0** = `FT_LOAD_DEFAULT`, **1** (default) = `FT_LOAD_TARGET_LIGHT`, **2** = `FT_LOAD_TARGET_NORMAL`. Outlines use **`FT_LOAD_NO_BITMAP`** so embedded strikes do not override hinted outlines. **`r_fontMipmap`** (default **1**, **0**/**1**): when **1**, each 256×256 FreeType atlas page is uploaded with a mip chain so minified UI text filters more cleanly; **0** restores a single mip (legacy). Apply changes with **`reloadTtf`** (clears the renderer’s TrueType registration cache and re-runs `CL_RegisterBuiltInTrueTypeFonts`) or **`vid_restart`** / **`vid_restart keep_window`**; startup logs raster settings when FreeType initializes. Each reload may leave prior `fonts/_ftg_*` atlases allocated until a full renderer reinit—use **`vid_restart`** if you toggle settings many times and want GPU memory reclaimed.
- **Pixel / virtual layout (client):** **`r_fontConsoleAlign`** (**1** default) baseline-aligns each TrueType glyph inside the fixed cell (console, notify, and 640×480 HUD strings) using FreeType `top` metrics; **0** restores legacy top-aligned quads. **`r_fontShadow`** (**0–8**, default **2**) is the drop-shadow offset in **screen pixels** (pixel path) or **virtual units** (HUD path); **0** skips the shadow pass. **`r_fontSubpixel`** (**0/1**, default **0**) adds a **0.375** bias after projection to soften stair-stepping with linear filtering on some panels (not Microsoft ClearType; optional because on-screen subpixel preference varies widely—see [bias2009-subpixel-preference.md](research/bias2009-subpixel-preference.md)).
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

- **Vulkan RTX**: demo path with `USE_VULKAN_RTX=ON`, `r_rtx` / `r_rtxDemo`, world BLAS; optional **`r_rtxEntities`** proxy entity boxes in TLAS. See [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md).
- **GRTX (Gaussian RT)**: `r_grtx` / `r_grtxDemo` over procedural 3D Gaussian AABB proxies (separate from BSP `r_rtx` demo). See [GAUSSIAN_RAY_TRACING_GRTX.md](GAUSSIAN_RAY_TRACING_GRTX.md).
- **Path trace experiment (C6)**: `r_pathtrace` + `r_pathtrace_arch` (`megakernel` / `wavefront`) over shared RTX world TLAS — requires `r_rtx 1`, `r_rtxDemo 1`, `USE_VULKAN_RTX`. Not SP ship lighting. See [PATHTRACE_ARCH_BENCHMARK.md](PATHTRACE_ARCH_BENCHMARK.md).
- **Mobile-GS**: `r_mgs` tiered compute splatting (Android-friendly; no RTX). See [MOBILE_GAUSSIAN_SPLATTING.md](MOBILE_GAUSSIAN_SPLATTING.md).
- **WebSplatter**: `r_wsp` tile-binned splats (WebGPU-portable compute layout). See [WEB_SPLATTER.md](WEB_SPLATTER.md).
- **Metal**: Roadmap scaffold — `USE_METAL_RENDERER=ON` (Apple) builds `idtech3_metal` dlopen plugin; native backend TBD. See `docs/METAL_RENDERER.md`.
- **DXR**: Roadmap scaffold — `USE_DXR_RENDERER=ON` (Windows) builds `idtech3_dxr` dlopen plugin; DX12 backend TBD. See `docs/DXR_RENDERER.md`.
- **WebGPU (Wasm)**: No native plugin; portable `wsp`/`mgs` compute on Vulkan today. See `docs/WEBGPU_ROADMAP.md`.

For practical renderer direction and priority order, see [RENDERER_2026_ARCHITECTURE_PASS.md](RENDERER_2026_ARCHITECTURE_PASS.md).
