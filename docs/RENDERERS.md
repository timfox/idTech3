# Renderer Features

## Vulkan Renderer (Primary)

The Vulkan 1.4 renderer is the primary rendering backend, built as a shared library (`idtech3_vulkan.so`).

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
- Cvars: `r_fluidsim`, `r_fluidsim_viscosity`, `r_fluidsim_dissipation`, etc.

### Shadow Mapping
- Cascaded shadow maps (CSM) for directional/sun light
- Spot light shadow atlas
- Point light shadow cubemap array (6 faces per light)
- Per-frame shadow matrix computation

### Anti-Aliasing
- SMAA (Sub-pixel Morphological Anti-Aliasing) with edge detection, blend weight, and compose passes
- MSAA support (configurable sample count)

### Screen-Space Ambient Occlusion (SSAO)
- Hemisphere sampling with configurable radius, bias, intensity
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
- HDR rendering with RGBA16F color targets
- Bloom extraction with configurable threshold and knee
- 4-pass Gaussian blur at progressively lower resolutions
- ACES, Reinhard, Filmic, and AgX tonemapping
- Exposure control with per-frame push constant
- Eye adaptation (`r_exposure_auto`): temporal blend toward target exposure (luminance pass planned)
- Pre-exposure scaling

### Post-Processing
- Panini projection with configurable parameters
- Ordered dithering (8x8 Bayer matrix)
- Greyscale mode
- sRGB gamma correction
- Debug views (pre-tonemap HDR, luminance heatmap)

### First Person Rendering
- Custom FOV for first-person primitives (arms, weapons) separate from scene FOV
- Anti-clipping scale factor to shrink first-person geometry toward camera
- Applies to entities with `RF_FIRST_PERSON` + `RF_DEPTHHACK` (view weapon, arms)
- Cvars: `r_firstPersonFov` (default 90), `r_firstPersonScale` (default 1.0), `r_firstPersonFovEnabled`, `r_firstPersonScaleEnabled`

### GPU Occlusion Culling
- Entity occlusion culling via Vulkan occlusion queries (VkQueryPool)
- Depth-only world pass fills depth buffer; entity bounding boxes are drawn with occlusion queries
- Previous-frame visibility: query results read at frame end, used to skip occluded entities next frame
- Cvar: `r_occlusionCulling` (0=off, 1=on, default 0)
- Only entities (models) are culled; world geometry uses BSP/PVS

### Key Cvars
| Cvar | Default | Description |
|------|---------|-------------|
| `r_volumetricFog` | 0 | Volumetric fog enable (0=off, 1=on) |
| `r_volumetricFogDensity` | 0.35 | Volumetric density multiplier |
| `r_volumetricFogQuality` | 2 | Quality tier (0=low, 1=medium, 2=high, 3=ultra; latched) |
| `r_fluidsim` | 0 | Fluid simulation (0=off, 1=on) |
| `r_bloom` | 0 | Bloom enable |
| `r_bloom_threshold` | 0.6 | Bloom extraction threshold |
| `r_hdr` | 0 | HDR format (0=off, 1=RGBA16F) |
| `r_hdr_lightmap_scale` | 2.0 | HDR lightmap intensity (1=normal, 2+=brighter for 8-bit lightmaps) |
| `r_tonemap` | 2 | Tonemapping (0=none, 1=Reinhard, 2=ACES) |
| `r_exposure` | 1.0 | Exposure multiplier |
| `r_exposure_auto` | 0 | Eye adaptation (0=manual, 1=temporal blend toward target) |
| `r_exposure_auto_target` | 0.5 | Target exposure for eye adaptation |
| `r_exposure_auto_speed` | 2.0 | Adaptation speed (higher = faster) |
| `r_atmosphere` | 1 | Procedural atmospheric sky (Rayleigh+Mie). Replaces grey sky when no HDR skybox. |
| `r_atmosphere_scale` | 4.0 | HDR scale multiplier for sky brightness. Works with auto exposure; increase if sky appears dark. |
| `r_skyboxHDR` | "" | Path to HDR EXR/PNG skybox panorama (empty = use atmosphere or map skybox). |
| `r_ssao` | 0 | SSAO enable |
| `r_firstPersonFov` | 90 | Horizontal FOV (degrees) for first-person primitives (range 50–150) |
| `r_firstPersonScale` | 1.0 | Anti-clipping scale for first-person primitives |
| `r_firstPersonFovEnabled` | 1 | Use custom FOV for first-person (0=scene FOV) |
| `r_firstPersonScaleEnabled` | 1 | Apply scale for anti-clipping (0=no scale) |
| `r_occlusionCulling` | 0 | GPU occlusion culling for entities (0=off, 1=on) |

Legacy note: `r_vfog*` cvars are still registered in `vk_vfog.c` for compatibility, but the active volumetric pipeline reads `r_volumetricFog*`.

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
