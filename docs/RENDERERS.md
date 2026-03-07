# Renderer Features

## Vulkan Renderer (Primary)

The Vulkan 1.4 renderer is the primary rendering backend, built as a shared library (`idtech3_vulkan.so`). Requests Vulkan 1.4 when available; validation layers (Khronos, then LUNARG fallback) are enabled in debug builds on all platforms.

### Physically Based Rendering (PBR)
- Metalness/roughness workflow with Cook-Torrance BRDF
- Image-based lighting (IBL) with prefiltered environment maps
- BRDF lookup table (LUT) generation
- Texture map types: normal, physical (RMO/ORM/RMOS variants), emissive, clearcoat, sheen, anisotropy, transmission, subsurface
- Spherical harmonics for diffuse irradiance
- Multi-scatter energy compensation
- **Glint NDF**: Procedural microfacet NDF for specular glints (replaces GGX D term on low-roughness surfaces). Cvars: `r_glint`, `r_glintMode`, `r_glintDensity`, `r_glintMicrofacetRoughness`, `r_glintPixelFilterSize`, `r_glintSampleBudget`, `r_glintMaxLodClamp`, `r_glintRoughnessLo`, `r_glintRoughnessHi`, `r_glintDMax`. Debug modes 5–8 via `r_pbr_debug`.
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
- Cvars: `r_volumetricFog`, `r_volumetricFogDensity`, `r_volumetricFogHeightFalloff`, `r_volumetricFogNoiseScale`, `r_volumetricFogGridDim`, `r_volumetricFogQuality`, etc. (Note: `r_vfog*` cvars in vk_vfog.c are legacy/unused; the pipeline uses `r_volumetricFog*` exclusively.)

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
- Cvars: `r_fogFluid` (enable), `r_fogFluidViscosity`, `r_fogFluidDissipation`, `r_fogFluidVorticity`, `r_fogFluidBuoyancy`, etc. Legacy `r_fluidsim*` deprecated.

### Lighting Robustness
- PBR Smith GGX visibility: division-by-zero guard at grazing angles (`CalcVisibility`)
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
- Compute runs at frame start; visual integration (draw from modified buffer) is a future enhancement

### RB_ColorMask (Vulkan)
- **Implemented**: Uses `VK_EXT_extended_dynamic_state3` when available. Enables `vkCmdSetColorWriteMaskEXT` for shadow volumes and other color-mask use cases. Gracefully no-ops when the extension is not supported.

### Anti-Aliasing
- **SMAA** (Sub-pixel Morphological Anti-Aliasing): edge detection, blend weight, and compose passes. Cvars: `r_ext_smaa` (enable), `r_smaa_preset` (0=Custom, 1=Low, 2=Medium, 3=High, 4=Ultra), `r_smaa_threshold` (0.01–0.5), `r_smaa_local_contrast` (1–4), `r_smaa_max_search_steps` (8–32), `r_smaa_corner_rounding` (0–1). Preset overrides manual params when non-zero. Edge detection uses max(left,right) and max(top,bottom) deltas (reference SMAA), HDR-safe luma, configurable corner rounding, and explicit LOD 0 sampling. Requires `r_fbo 1`.
- **MSAA**: Multi-sample anti-aliasing for geometry edges. Cvar `r_ext_multisample` (0|2|4|8|16). Requires `r_fbo 1`. `r_msaa_sample_shading` enables per-sample shading for better alpha/specular quality (~2x fragment cost). `r_ext_alpha_to_coverage` improves alpha-tested surfaces (foliage, grates) when MSAA is on. MSAA and SMAA can be used together: MSAA handles geometry edges, SMAA handles alpha/transparency edges.

### Order-Independent Transparency (OIT)
- Weighted Blended OIT (WBOIT) for correct blending of overlapping transparent surfaces
- Cvar `r_oit` (0=off, 1=on). Requires `r_fbo 1` and `vid_restart` after changing
- Opaque surfaces drawn first; transparent surfaces (alpha blend and additive) accumulated, then resolved
- Depth testing against opaque scene when MSAA off (transparent behind walls discarded)
- Additive blend (ONE/ONE) surfaces included for particles, sparks, etc.

### Screen-Space Ambient Occlusion (SSAO / HBAO)
- **SSAO** (`r_ssaoMethod 0`): Hemisphere sampling with Halton(2,3) low-discrepancy sequence for better distribution and less noise
- **HBAO** (`r_ssaoMethod 1`): Horizon-Based Ambient Occlusion — raymarches depth in multiple directions, tracks horizon angles; higher quality with fewer samples
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
- Panini projection with configurable parameters
- Ordered dithering (8x8 Bayer matrix)
- Greyscale mode
- sRGB gamma correction
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
| `r_post_contrast` | 1.0 | Post-tonemap contrast (1=neutral, >1=punchier, <1=flatter) |
| `r_post_saturation` | 1.0 | Post-tonemap saturation (1=neutral, >1=vivid, <1=desaturated) |
| `r_atmosphere` | 1 | Procedural atmospheric sky (Rayleigh+Mie). Replaces grey sky when no HDR skybox. |
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

Legacy note: `r_vfog*` cvars are still registered in `vk_vfog.c` for compatibility, but the active volumetric pipeline reads `r_volumetricFog*`.

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

Features:
- Classic Quake III Arena rendering
- Multi-texture support
- Vertex and fragment programs
- Dynamic lighting
- Stencil shadows
- Fog volumes (distance-based)

The OpenGL renderer does not include PBR, volumetric fog, SSAO, SMAA, bloom, fluid simulation, or IQM morph target evaluation.

## Future Renderers (Planned)

See [RENDERERS_FUTURE.md](RENDERERS_FUTURE.md) for architecture and implementation plans:

- **Vulkan RTX**: `VK_KHR_ray_tracing_pipeline` integration. Build with `-DUSE_VULKAN_RTX=ON`. Cvar `r_rtx` (0–3) reserved.
- **Metal**: Native Metal renderer for macOS/iOS (Apple Silicon). Option `USE_METAL_RENDERER` reserved.
- **DXR**: DirectX 12 + DirectX Raytracing for Windows. Option `USE_DXR_RENDERER` reserved.
