# Vulkan Shaders Reference

This document lists all Vulkan shaders available in the id Tech 3 renderer.

## Ray Tracing Shaders

### Ray Generation
- **rt_primary_rays.rgen** - Primary ray generation for path tracing

### Miss Shaders
- **rt_miss.rmiss** - Default miss shader (sky sampling)
- **rt_shadow.rmiss** - Shadow ray miss (fully lit)
- **rt_ao.rmiss** - Ambient occlusion miss
- **rt_reflection.rmiss** - Reflection ray miss (sky reflection)
- **rt_refraction.rmiss** - Refraction ray miss

### Closest Hit Shaders
- **rt_closesthit.rchit** - Main closest hit shader with PBR lighting
- **rt_shadow.rchit** - Shadow ray closest hit (occluded)
- **rt_ao.rchit** - Ambient occlusion closest hit
- **rt_reflection.rchit** - Reflection ray closest hit
- **rt_refraction.rchit** - Refraction ray closest hit

### Any Hit Shaders
- **rt_anyhit.rahit** - Alpha testing and transparency handling

### Intersection Shaders
- **rt_intersection.rint** - Procedural geometry intersection (spheres, boxes, cylinders)

### Callable Shaders
- **rt_callable_material.rchit** - Material evaluation callable shader

## Compute Shaders

### Post-Processing
- **gamma.comp** - Gamma correction
- **tonemap.comp** - Tone mapping
- **aces_filmic_tonemap.comp** - Enhanced ACES filmic tone mapping with shoulder rolloff (cinematic)
- **taa.comp** - Temporal Anti-Aliasing
- **fxaa.comp** - Fast Approximate Anti-Aliasing
- **smaa.comp** - Subpixel Morphological Anti-Aliasing
- **smaa_edges.comp** - SMAA edge detection pass
- **cas_sharpen.comp** - Contrast Adaptive Sharpening (AMD FidelityFX)
- **luma_sharpen.comp** - Luma-based sharpening (avoids color artifacts, cinematic)
- **chromatic_aberration.comp** - Chromatic aberration effect
- **vignette.comp** - Vignette effect
- **lens_flare.comp** - Lens flare rendering
- **color_grading.comp** - Color grading with LUT support
- **color_grading_cinematic.comp** - Advanced cinematic color grading with 9 presets
- **deband.comp** - Debanding to remove color banding artifacts (cinematic)
- **upscale.comp** - Image upscaling (bilinear, Lanczos, FSR-style)

### Global Illumination
- **ssgi.comp** - Screen Space Global Illumination
- **ssr.comp** - Screen Space Reflections
- **gibs_spawn.comp** - GIBS surfel spawning
- **gibs_update.comp** - GIBS surfel irradiance updates

### Effects
- **motion_blur.comp** - Motion blur based on velocity buffer
- **depth_of_field.comp** - Depth of field with bokeh
- **volumetric_fog.comp** - Volumetric fog/lighting
- **volumetric_clouds.comp** - Ray-marched volumetric clouds with lighting (cinematic)
- **volumetric_aurora.comp** - Volumetric aurora and lightning effects (cinematic)
- **bloom.comp** - Bloom extraction
- **bloom_enhanced.comp** - Multi-octave enhanced bloom with color weighting (cinematic)
- **blur.comp** - Gaussian blur
- **blur_subgroup.comp** - Optimized blur using subgroups
- **blur_gather.frag** - Blur gather pass

### Denoising
- **rt_relax.comp** - ReLAX denoising for ray tracing

### Utility
- **histogram.comp** - Luminance histogram generation
- **auto_exposure.comp** - Automatic exposure adjustment
- **particles.comp** - GPU particle system update

### Mesh Shaders
- **meshlet.task** - Task shader for meshlet culling
- **meshlet.mesh** - Mesh shader for meshlet rendering

## Fragment Shaders

### Rendering
- **gen_frag.tmpl** - Template for generated fragment shaders
- **light_frag.tmpl** - Template for lighting fragment shaders
- **color.frag** - Simple color rendering
- **dot.frag** - Dot pattern
- **fog.frag** - Fog rendering
- **blend.frag** - Blending operations
- **bloom.frag** - Bloom fragment shader
- **gamma.frag** - Gamma correction fragment shader

### PBR
- **brdflut.frag** - BRDF lookup table generation
- **irradiancecube.frag** - Irradiance map generation
- **prefilterenvmap.frag** - Prefiltered environment map generation
- **rt_composite.frag** - Ray tracing composite pass

## Vertex Shaders

- **gen_vert.tmpl** - Template for generated vertex shaders
- **light_vert.tmpl** - Template for lighting vertex shaders
- **color.vert** - Simple color vertex shader
- **dot.vert** - Dot pattern vertex shader
- **fog.vert** - Fog vertex shader
- **gamma.vert** - Gamma correction vertex shader

## Geometry Shaders

- **filtercube.geom** - Cube map filtering geometry shader
- **filtercube.vert** - Cube map filtering vertex shader

## Helper/Include Files

- **rt_defines.glsl** - Ray tracing constants and defines
- **rt_helpers.glsl** - Ray tracing helper functions
- **rt_random.glsl** - Random number generation for ray tracing
- **gibs_surfel.glsl** - GIBS surfel data structures
- **gibs_sampling.glsl** - GIBS surfel sampling functions
- **shader_constants.glsl** - Common shader constants

## Shader Compilation

All shaders need to be compiled to SPIR-V before use:

```bash
cd src/renderervk/shaders
./compile.sh
```

Or manually:
```bash
glslc shader_name.comp -o shader_name.spv
glslc shader_name.frag -o shader_name.spv
glslc shader_name.vert -o shader_name.spv
glslc shader_name.rgen -o shader_name.spv
glslc shader_name.rchit -o shader_name.spv
glslc shader_name.rmiss -o shader_name.spv
glslc shader_name.rahit -o shader_name.spv
glslc shader_name.rint -o shader_name.spv
```

## Usage Notes

- Ray tracing shaders require `GL_EXT_ray_tracing` extension
- Mesh shaders require `GL_EXT_mesh_shader` extension
- Compute shaders use local workgroup sizes optimized for modern GPUs
- Most post-processing shaders support HDR input/output
- Many effects can be chained together in a post-processing pipeline

## Performance Considerations

- Use TAA for better quality with lower sample counts
- FXAA is fastest but lower quality
- SMAA provides best quality but requires multiple passes
- CAS sharpening is very efficient and can be used with upscaling
- SSGI/SSR are faster than full ray tracing but have limitations
- Volumetric effects are expensive - use sparingly

