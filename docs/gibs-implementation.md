# GIBS (Global Illumination Based on Surfels) Implementation

## Overview

This implementation adds GIBS (Global Illumination Based on Surfels) to the id Tech 3 Vulkan renderer, based on the SIGGRAPH 2021 paper and the [SurfelGI reference implementation](https://github.com/W298/SurfelGI).

GIBS provides efficient real-time global illumination by caching indirect lighting in surfels (surface elements) distributed throughout the scene. This approach is more efficient than pure path tracing while providing high-quality indirect lighting.

## Files Created

### Core Implementation
- `src/renderervk/vk_gibs.h` - Header file with data structures and API declarations
- `src/renderervk/vk_gibs.c` - Main implementation file with initialization, update, and shutdown logic

### Shaders
- `src/renderervk/shaders/glsl/gibs_surfel.glsl` - Surfel data structure and helper functions
- `src/renderervk/shaders/glsl/gibs_spawn.comp` - Compute shader for spawning surfels on surfaces
- `src/renderervk/shaders/glsl/gibs_update.comp` - Compute shader for updating surfel irradiance via ray tracing
- `src/renderervk/shaders/glsl/gibs_sampling.glsl` - Helper functions for sampling surfels in PBR shaders

## Features Implemented

### 1. Surfel Data Structure
- Position, normal, radius
- Cached irradiance (RGB)
- Confidence value
- Age and flags for lifecycle management

### 2. Buffer Management
- GPU storage buffer for surfels (up to 1M surfels by default)
- Indirect dispatch buffer for compute shaders
- Uniform buffer for camera and configuration data

### 3. CVars
- `r_gibs` - Enable/disable GIBS (0/1)
- `r_gibs_surfelRadius` - Surfel radius in world units (default 0.1)
- `r_gibs_maxSurfels` - Maximum number of surfels (default 1048576)
- `r_gibs_updateRate` - Update frequency in frames (default 4)
- `r_gibs_intensity` - Intensity multiplier (default 1.0)
- `r_gibs_samples` - Samples per surfel update (default 16)

### 4. Integration
- Initialization after ray tracing system
- Frame update in `vk_begin_frame()`
- Shutdown before ray tracing cleanup
- Integration with existing ray tracing acceleration structures

## Implementation Status

### ✅ Completed
- [x] Data structures and storage system
- [x] Buffer allocation and management
- [x] CVar system
- [x] Basic initialization and shutdown
- [x] Compute shader code (spawn and update)
- [x] Surfel sampling helper functions
- [x] Frame update logic

### ⚠️ Partially Implemented
- [ ] Pipeline creation (shader modules need to be compiled first)
- [ ] Descriptor set creation and binding
- [ ] Uniform buffer updates (needs proper matrix inversion)
- [ ] Surfel spawning integration (needs G-buffer access)

### ❌ Not Yet Implemented
- [ ] Shader compilation (shaders need to be compiled to SPIR-V)
- [ ] Pipeline creation functions (`vk_gibs_create_pipelines()`)
- [ ] Descriptor set layout creation
- [ ] Integration with PBR fragment shader (add surfel sampling)
- [ ] Spatial acceleration structure for efficient surfel lookup
- [ ] Surfel culling and removal of stale surfels
- [ ] Proper matrix inversion for view/projection matrices

## Usage

### Enabling GIBS

1. Enable ray tracing first:
   ```
   \r_raytracing 1
   \r_fbo 1
   ```

2. Enable GIBS:
   ```
   \r_gibs 1
   ```

3. Adjust settings as needed:
   ```
   \r_gibs_surfelRadius 0.1
   \r_gibs_maxSurfels 1048576
   \r_gibs_updateRate 4
   \r_gibs_intensity 1.0
   \r_gibs_samples 16
   ```

## Next Steps

### 1. Shader Compilation
The compute shaders need to be compiled to SPIR-V:
```bash
cd src/renderervk/shaders
glslc gibs_spawn.comp -o gibs_spawn.spv
glslc gibs_update.comp -o gibs_update.spv
```

### 2. Pipeline Creation
Implement `vk_gibs_create_pipelines()` function to:
- Load compiled shader modules
- Create descriptor set layouts
- Create compute pipelines
- Create and bind descriptor sets

### 3. PBR Integration
Add surfel sampling to the PBR fragment shader:
- Include `gibs_sampling.glsl`
- Call `sampleGIBSIrradiance()` in lighting calculation
- Blend with existing indirect lighting

### 4. Spatial Acceleration
Implement a spatial data structure (e.g., grid or BVH) for efficient surfel lookup instead of linear search.

### 5. Matrix Utilities
Add proper matrix inversion functions for view/projection matrices.

## Performance Considerations

- **Surfel Count**: More surfels = better quality but higher memory and computation cost
- **Update Rate**: Lower update rate = better performance but slower adaptation to lighting changes
- **Samples Per Surfel**: More samples = smoother indirect lighting but slower updates
- **Spatial Lookup**: Current linear search is O(n) - should be optimized with spatial acceleration

## References

- [SurfelGI GitHub Repository](https://github.com/W298/SurfelGI)
- SIGGRAPH 2021: "Global Illumination Based on Surfels"
- Falcor Framework (reference implementation)

## Notes

- GIBS requires hardware ray tracing support (Vulkan ray tracing extensions)
- Works best with static or slowly moving geometry
- Indirect lighting updates are distributed across multiple frames for performance
- Surfel confidence decays over time to handle dynamic lighting changes

