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

### Bloom and HDR
- HDR rendering with RGBA16F color targets
- Bloom extraction with configurable threshold and knee
- 4-pass Gaussian blur at progressively lower resolutions
- ACES and Reinhard tonemapping
- Exposure control
- Pre-exposure scaling

### Post-Processing
- Panini projection with configurable parameters
- Ordered dithering (8x8 Bayer matrix)
- Greyscale mode
- sRGB gamma correction
- Debug views (pre-tonemap HDR, luminance heatmap)

### Key Cvars
| Cvar | Default | Description |
|------|---------|-------------|
| `r_vfog` | 0 | Volumetric fog mode (0=off, 1+=on) |
| `r_fluidsim` | 0 | Fluid simulation (0=off, 1=on) |
| `r_bloom` | 0 | Bloom enable |
| `r_bloom_threshold` | 0.6 | Bloom extraction threshold |
| `r_hdr` | 0 | HDR format (0=off, 1=RGBA16F) |
| `r_tonemap` | 2 | Tonemapping (0=none, 1=Reinhard, 2=ACES) |
| `r_exposure` | 1.0 | Exposure multiplier |
| `r_ssao` | 0 | SSAO enable |

## OpenGL Renderer (Fallback)

The OpenGL renderer provides compatibility for systems without Vulkan support. It implements the same `refexport_t` interface with classic OpenGL fixed-function and shader-based rendering.

Features:
- Classic Quake III Arena rendering
- Multi-texture support
- Vertex and fragment programs
- Dynamic lighting
- Stencil shadows
- Fog volumes (distance-based)

The OpenGL renderer does not include PBR, volumetric fog, SSAO, SMAA, bloom, or fluid simulation.
