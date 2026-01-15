# Q3RTX Integration Plan

## Overview
This document outlines the integration of relevant features from Q3RTX (Quake III Arena RTX) into our id Tech 3 RTX renderer.

## Key Features from Q3RTX

### 1. ASVGF Denoising System (HIGH PRIORITY)
Q3RTX implements a complete ASVGF (Adaptive Spatio-Temporal Variance-Guided Filtering) pipeline:

**Stages:**
- **RNG Generation** (`asvgf_rng.comp`): Generates random number seeds for denoising
- **Forward Pass** (`asvgf_forward.comp`): Forward reprojection pass
- **Gradient Computation** (`asvgf_grad.comp`): Computes gradients for edge detection
- **Gradient Atrous Filtering** (`asvgf_grad_atrous.comp`): Multi-pass gradient filtering
- **Temporal Accumulation** (`asvgf_temporal.comp`): Temporal reprojection and accumulation
- **Atrous Filtering** (`asvgf_atrous.comp`): Multi-pass spatial filtering (5 iterations)
- **TAA** (`asvgf_taa.comp`): Temporal Anti-Aliasing pass

**Key Differences from Q2RTX:**
- Uses NV ray tracing extension (older API)
- Simpler gradient computation
- Direct integration with Q3's cluster system
- Optimized for Q3's specific rendering needs

### 2. G-Buffer System (HIGH PRIORITY)
Q3RTX uses a comprehensive G-buffer for deferred rendering:

**Buffers:**
- `albedo`: Surface albedo/color
- `position`: World-space position
- `normals`: Surface normals
- `viewDir`: View direction
- `objectInfo`: Object/material information
- `reflection`: Reflection/refraction data
- `transparent`: Transparency information

### 3. Cluster-Based Lighting (MEDIUM PRIORITY)
Q3RTX integrates with Q3's PVS (Potentially Visible Set) cluster system:

- Uses `currentCluster` to cull lights efficiently
- Cluster-based light assignment
- Efficient light list generation per cluster

### 4. Compositing and Tone Mapping (MEDIUM PRIORITY)
- **Compositing** (`compositing.comp`): Combines direct + indirect lighting
- **Tone Mapping** (`tonemapping.comp`): Reinhard tone mapping with mipmap generation
- **Max Mipmap** (`maxmipmap.comp`): Generates mipmaps for tone mapping

### 5. Depth of Field (LOW PRIORITY)
- Aperture and focal length controls
- DOF rendering support

### 6. Accumulation Rendering (LOW PRIORITY)
- Frame accumulation for photo mode
- Sample accumulation for noise reduction

## Integration Strategy

### Phase 1: ASVGF Denoising
1. Port ASVGF compute shaders
2. Create ASVGF image buffers
3. Integrate ASVGF pipeline into rendering loop
4. Add CVARs for ASVGF control

### Phase 2: G-Buffer Enhancement
1. Enhance existing G-buffer with Q3RTX buffers
2. Update path tracer to write to G-buffer
3. Add reflection buffer support

### Phase 3: Cluster Integration
1. Integrate Q3's cluster system with lighting
2. Add cluster-based light culling
3. Optimize light sampling per cluster

### Phase 4: Post-Processing
1. Port compositing shader
2. Port tone mapping with mipmap generation
3. Add DOF support (optional)

## Files to Create/Modify

### New Files
- `src/renderers/vulkan/rtx/vk_asvgf_q3rtx.cpp` - ASVGF denoising system
- `src/renderers/vulkan/rtx/vk_asvgf_q3rtx.h` - ASVGF header
- `src/renderers/vulkan/rtx/vk_gbuffer_q3rtx.cpp` - Enhanced G-buffer system
- `src/renderers/vulkan/rtx/vk_compositing_q3rtx.cpp` - Compositing and tone mapping

### Shader Files
- Port all ASVGF compute shaders from `reference/q3rtx/shader/glsl/compute/`
- Adapt to use KHR ray tracing extensions (Q3RTX uses NV extension)

### Modified Files
- `src/renderers/vulkan/rtx/vk_path_tracer_multistage.cpp` - Integrate G-buffer writes
- `src/renderers/vulkan/rtx/vk_raytracing.cpp` - Add ASVGF pipeline
- `src/renderers/vulkan/vk.h` - Add ASVGF and G-buffer structures

## Key Data Structures

### GlobalUbo (from Q3RTX)
- Camera matrices (view, proj, inverses)
- Portal support matrices
- Cluster information
- Rendering settings (bounces, samples, denoiser, TAA, DOF)
- Frame index and accumulation

### ASInstanceData
- Instance ID tracking
- Material/texture indices
- Cluster assignment
- Transform matrices

### VertexBuffer
- Position, normal
- Material index
- UV coordinates (4 stages)
- Color data (4 stages)
- Texture indices
- Cluster assignment

## Notes

1. **API Differences**: Q3RTX uses `VK_NV_ray_tracing` extension, we use `VK_KHR_ray_tracing`. Need to adapt shader bindings and API calls.

2. **Cluster System**: Q3RTX deeply integrates with Q3's PVS cluster system. We should maintain this integration for efficient light culling.

3. **Shader Compatibility**: Q3RTX shaders are GLSL and should be mostly compatible, but need binding index updates.

4. **Performance**: Q3RTX is optimized for Q3's specific rendering needs. Some optimizations may not apply directly to our use case.
