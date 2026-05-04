# Renderer Features

For a **build + manual validation checklist** (CI parity, shader coverage, GPU passes), see [RENDERER_CONFIDENCE.md](RENDERER_CONFIDENCE.md).

## Vulkan Renderer (Primary)

The Vulkan 1.4 renderer is the primary rendering backend, built as a shared library (`idtech3_vulkan.so`). Requests Vulkan 1.4 when available; validation layers (Khronos, then LUNARG fallback) are enabled in debug builds on all platforms.

### Current Architecture
- Forward renderer with a large HDR/post-processing stack
- `r_renderMode 1/2` remain placeholders (no full deferred / mode-switched Forward+ path)
- Vulkan is the primary feature backend; OpenGL is compatibility fallback
- **Shared temporal reset policy** (`vk_temporal.c`): centralizes history invalidation for volumetrics, motion vectors, exposure. Resize, map load, camera cut, and missing prev-frame data trigger resets. Ready for future TAA/upscaler integration.
- See [RENDERER_2026_ARCHITECTURE_PASS.md](RENDERER_2026_ARCHITECTURE_PASS.md) for the focused 2026 renderer direction

### Vulkan Forward+ scaffolding

Optional **GPU light packing + per-tile cull** on the existing forward path (not `r_renderMode`):

| Cvar | Role |
|------|------|
| `r_forwardPlus` | **0** (default) off; **1** on (**latched**; `vid_restart` to apply). Startup logs buffer size when enabled. |
| `r_forwardPlusMaxPerTile` | **4–8** lights indexed per **16×16** tile (**latched**; `vid_restart`). Lowers GPU work vs default **8**; tile SSBO keeps **8** `uint32` slots either way. |
| `r_forwardPlusDebug` | **0–1** float: PBR heatmap overlay (lights per tile + borders). |
| `r_forwardPlusShade` | **0–4** float: experimental **additive** PBR from tile-culled dynamics; skips indices already in the Forward+ `tess.dlightBits` mask (first 32); changing it **invalidates graphics pipelines** (logged). |

**Caps:** at most **`MAX_DLIGHTS` (32)** packed lights so GPU indices match `tess.dlightBits`. If the refdef supplies more, extras are dropped and a **developer** log can note it.

**Resolution:** tile grid uses **`vk_get_render_target_width/height`** (FBO / internal scale). The tile SSBO **reallocates when that size changes**; toggling Forward+ itself still needs **`vid_restart`**.

Code: `src/renderers/vulkan/vk_forward_plus.c`, `VK_FP_*` constants; cvar registration `src/renderers/vulkan/tr_init.c`.

**Audit:** [FORWARD_PLUS_PIPELINE_AUDIT.md](FORWARD_PLUS_PIPELINE_AUDIT.md).

### Compute shaders, mesh shaders (NV), and upscaling (DLSS)
- **Compute:** already central to Vulkan (volumetric fog stages, vegetation wind, fluid sim, terrain CBT, etc.).
- **Mesh shaders (NVIDIA):** optional device extension **`VK_NV_mesh_shader`** when **`r_vk_meshShaderNV 1`** (default **0**, **latched**, `vid_restart`). Enables `meshShader` in `VkPhysicalDeviceMeshShaderFeaturesNV` for future pipelines; **no mesh-shader draw path** is wired yet - safe on `main`.
- **DLSS / NGX:** **not** linked in this repository (proprietary NVIDIA SDK). Use **`r_renderScale`** / internal resolution, **TAA** (`r_taa`), **SMAA/MSAA**, or **driver-level** scaling (e.g. NVIDIA NIS/DLSS in control panel) where applicable. Startup logs state that DLSS is not in-engine.

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
- Cvars: `r_volumetricFog`, `r_volumetricFogDensity`, `r_volumetricFogHeightFalloff`, `r_volumetricFogNoiseScale`, `r_volumetricFogGridDim`, `r_volumetricFogQuality`, etc.

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
| `r_renderMode` | 0 | Rendering path: 0=forward, 1=deferred (placeholder), 2=forward+ (placeholder). Deferred and forward+ would need G-buffers, light culling, and separate passes; they are not implemented yet. |
| `r_volumetricFog` | 0 | Volumetric fog enable (0=off, 1=on) |
| `r_volumetricFogDensity` | 0.35 | Volumetric density multiplier |
| `r_volumetricFogQuality` | 2 | Quality tier (0=low, 1=medium, 2=high, 3=ultra; latched) |
| `r_fogFluid` | 0 | Fluid-driven volumetric fog (0=off, 1=on). Vorticity/buoyancy: `r_fogFluidVorticity`, `r_fogFluidBuoyancy`. |
| `r_bloom` | 0 | Bloom enable |
| `r_bloom_threshold` | 0.6 | Bloom extraction threshold |
| `r_hdr` | 2 | HDR format (0=8-bit, 1=RGBA16F, 2=RGBA32F default, 3=RGBA64F optional/gated, -1=4-bit test) |
| `r_hdr_lightmap_scale` | 2.0 | HDR lightmap intensity (1=normal, 2+=brighter for 8-bit lightmaps) |
| `r_lightmap_srgb_decode` | 0 | When r_hdr 1/2: 0=linear (default), 1=sRGB→linear for gamma-encoded lightmaps |
| `r_pre_exposure_scale` | 1.0 | Pre-exposure scale for bloom/tonemap pipeline |
| `r_tonemap` | 2 | Tonemapping (0=none, 1=Reinhard, 2=ACES) |
| `r_exposure` | 1.0 | Exposure multiplier |
| `r_exposure_auto` | 0 | Eye adaptation (0=manual, 1=temporal blend toward target) |
| `r_exposure_auto_target` | 0.5 | Target exposure for eye adaptation |
| `r_exposure_auto_speed` | 2.0 | Adaptation speed (higher = faster) |
| `r_taa` | 0 | Optional temporal resolve for Vulkan HDR/post-processing. Best for world rendering; history is conservatively invalidated on unstable paths. |
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

### SDF Text Rendering
- Signed Distance Field (SDF) fonts for resolution-independent sharp text
- Applied to HUD, menu, console, key bindings, and all UI text paths
- BMFont-format metrics (`.fnt`) + SDF atlas textures
- Cvars: `r_sdfEnable`, `r_sdfFont`, `r_sdfFontAtlas`, `r_sdfSmoothing`
- Fallback to bitmap charset when SDF font unavailable

### SVG Graphics
- Vector SVG assets load as textures (icons, logos, UI graphics)
- Uses librsvg when available; NanoSVG fallback on all platforms
- Cvars: `r_svgRasterScale`, `r_svgMaxRasterSize`, `r_svgMaxFileBytes`
- Use `.svg` files in textures/ or gfx/ for scalable UI elements

## OpenGL Renderer (Fallback)

The OpenGL renderer provides compatibility for systems without Vulkan support. It implements the same `refexport_t` interface with classic OpenGL fixed-function and shader-based rendering.

### OpenGL vs Vulkan Feature Parity

| Feature | Vulkan | OpenGL |
|---------|--------|--------|
| Model formats **glTF/GLB**, **OBJ**, **MD5** (registration + draw) | ✓ (full; GPU glTF options on Vulkan) | ✓ (CPU tess; no `r_gltfGpu`) |
| PBR (metalness/roughness, IBL) | ✓ | - |
| Volumetric fog | ✓ | - |
| SSAO / HBAO | ✓ | - |
| SMAA | ✓ | - |
| Bloom, HDR tonemapping | ✓ | - |
| OIT (order-independent transparency) | ✓ | - |
| IQM morph targets | ✓ | - |
| Fluid simulation (fog) | ✓ | - |
| Vegetation wind (GPU compute) | ✓ | - |
| SDF text | ✓ | ✓ |
| Dynamic lighting | ✓ | ✓ |
| Stencil shadows | ✓ | ✓ |
| Fog volumes | ✓ | ✓ |
| Multi-texture, vertex/fragment programs | ✓ | ✓ |

OpenGL is the compatibility fallback; Vulkan is the primary feature backend. Use Vulkan when available for PBR, HDR, and advanced effects.

## Future Renderers (Planned)

See [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md) for architecture and implementation plans:

- **Vulkan RTX**: `VK_KHR_ray_tracing_pipeline` integration. Build with `-DUSE_VULKAN_RTX=ON`. Cvar `r_rtx` (0–3) reserved.
- **Metal**: Native Metal renderer for macOS/iOS (Apple Silicon). Option `USE_METAL_RENDERER` reserved.
- **DXR**: DirectX 12 + DirectX Raytracing for Windows. Option `USE_DXR_RENDERER` reserved.

For practical renderer direction and priority order, see [RENDERER_2026_ARCHITECTURE_PASS.md](RENDERER_2026_ARCHITECTURE_PASS.md).
