# Q2RTX Parity Plan for id Tech 3

## Overview
This document outlines the plan to bring id Tech 3's RTX renderer to parity with Q2RTX, adapting Q2-specific code to Q3 architecture while improving upon it.

## Key Q2RTX Features to Port

### 1. Path Tracing Architecture (HIGH PRIORITY)
**Current State:** Basic path tracer with single-stage rendering
**Q2RTX Approach:** Multi-stage path tracer:
- Stage 1: Primary rays (G-buffer generation)
- Stage 2: Reflect/Refract rays (recursive reflections/refractions)
- Stage 3: Direct lighting (local lights + sun)
- Stage 4: Indirect lighting (1-2 bounces)

**Implementation:**
- Port multi-stage pipeline from Q2RTX
- Adapt to Q3's renderer architecture
- Integrate with existing acceleration structures

### 2. ASVGF Denoising (HIGH PRIORITY)
**Current State:** Basic ASVGF implementation
**Q2RTX Approach:** Advanced ASVGF with:
- Gradient reconstruction and normalization
- Separate channels (HF, LF, SPEC)
- Spherical harmonics for indirect lighting
- Temporal accumulation with motion vectors

**Implementation:**
- Port gradient reconstruction shaders
- Implement multi-channel denoising
- Add SH support for indirect lighting

### 3. Physical Sky System (MEDIUM PRIORITY)
**Current State:** Basic sky rendering
**Q2RTX Approach:** Procedural physical sky with:
- Time-of-day simulation
- Sun position control
- Atmospheric scattering
- Space environment support

**Implementation:**
- Port physical_sky.c and shaders
- Adapt sky system to Q3's map format
- Add CVARs for sun control

### 4. God Rays / Volumetric Lighting (MEDIUM PRIORITY)
**Current State:** Not implemented
**Q2RTX Approach:** Volumetric lighting with:
- Inscatter accumulation
- Filtering passes
- Integration with path tracer

**Implementation:**
- Port god_rays.c and shaders
- Integrate with ray tracing pipeline

### 5. FSR Upscaling (MEDIUM PRIORITY)
**Current State:** Not implemented
**Q2RTX Approach:** FidelityFX Super Resolution 1.0:
- EASU (Edge Adaptive Spatial Upsampling)
- RCAS (Robust Contrast Adaptive Sharpening)

**Implementation:**
- Port FSR shaders
- Add upscaling pipeline

### 6. Advanced Tone Mapping (MEDIUM PRIORITY)
**Current State:** Basic tone mapping
**Q2RTX Approach:** Real-time noise-aware tone mapping:
- Histogram-based curve calculation
- Temporal blending
- HDR support

**Implementation:**
- Port tone_mapping.c and shaders
- Integrate with existing HDR pipeline

### 7. Material System Improvements (HIGH PRIORITY)
**Current State:** Basic PBR materials
**Q2RTX Approach:** Advanced material system with:
- .mat file parsing
- Automatic texture discovery
- Emissive texture synthesis
- Material flags and properties

**Implementation:**
- Port material.c and material.h
- Adapt .mat parser to Q3 shader system
- Integrate with existing material system

### 8. Transparency Handling (MEDIUM PRIORITY)
**Current State:** Basic transparency
**Q2RTX Approach:** Proper transparency with:
- Per-surface transparency
- Distortion effects
- Integration with path tracer

**Implementation:**
- Port transparency.c
- Add transparency shaders

### 9. Caustics (LOW PRIORITY)
**Current State:** Not implemented
**Q2RTX Approach:** Caustics approximation for glass/water

**Implementation:**
- Port caustics code from Q2RTX
- Add caustics shader support

### 10. Multi-GPU Support (LOW PRIORITY)
**Current State:** Not implemented
**Q2RTX Approach:** Checkerboard rendering for SLI

**Implementation:**
- Port mgpu.c and checkerboard shaders
- Add multi-GPU detection

## Architecture Adaptations

### Q2 vs Q3 Differences
1. **Renderer Interface:** Q2 uses refresh interface, Q3 uses refexport_t
2. **Material System:** Q2 uses .mat files, Q3 uses .shader files
3. **Map Format:** Q2 uses .bsp with different structure
4. **Entity System:** Q2 uses edict_t, Q3 uses gentity_t
5. **CVAR System:** Similar but different naming conventions

### Adaptation Strategy
1. Create adapter layer for Q2→Q3 conversions
2. Map Q2 material system to Q3 shader system
3. Adapt BSP loading to Q3 format
4. Port shaders with minimal changes (GLSL is mostly compatible)
5. Update CVAR names to Q3 conventions

## Implementation Order

### Phase 1: Core Path Tracing (Week 1)
- [x] Analyze Q2RTX path tracer architecture
- [ ] Port multi-stage path tracer
- [ ] Integrate with existing acceleration structures
- [ ] Test and validate

### Phase 2: Denoising (Week 1-2)
- [ ] Port advanced ASVGF
- [ ] Implement gradient reconstruction
- [ ] Add multi-channel support
- [ ] Test quality improvements

### Phase 3: Materials & Lighting (Week 2)
- [ ] Port material system
- [ ] Adapt .mat parser
- [ ] Integrate with Q3 shaders
- [ ] Port physical sky
- [ ] Port god rays

### Phase 4: Post-Processing (Week 2-3)
- [ ] Port FSR
- [ ] Port advanced tone mapping
- [ ] Add transparency improvements
- [ ] Test performance

### Phase 5: Polish (Week 3)
- [ ] Add caustics (optional)
- [ ] Multi-GPU support (optional)
- [ ] Performance optimization
- [ ] Documentation

## Files to Create/Modify

### New Files
- `src/renderers/vulkan/rtx/vk_path_tracer_multistage.cpp` - Multi-stage path tracer
- `src/renderers/vulkan/rtx/vk_asvgf_advanced.cpp` - Advanced ASVGF
- `src/renderers/vulkan/rtx/vk_physical_sky.cpp` - Physical sky
- `src/renderers/vulkan/rtx/vk_god_rays.cpp` - God rays
- `src/renderers/vulkan/rtx/vk_fsr.cpp` - FSR upscaling
- `src/renderers/vulkan/rtx/vk_tone_mapping_advanced.cpp` - Advanced tone mapping
- `src/renderers/vulkan/vk_material_pbr.c` - Material system

### Modified Files
- `src/renderers/vulkan/rtx/vk_raytracing.cpp` - Integrate multi-stage pipeline
- `src/renderers/vulkan/rtx/vk_denoiser.cpp` - Upgrade to advanced ASVGF
- `src/renderers/vulkan/rtx/vk_path_tracer.cpp` - Enhance with Q2RTX features
- `src/renderers/vulkan/vk_material_system.c` - Add .mat file support

### Shader Files
- Port all Q2RTX shaders from `reference/q2rtx/src/refresh/vkpt/shader/`
- Adapt to Q3's shader binding conventions
- Update texture binding indices

## Testing Strategy
1. Visual comparison with Q2RTX screenshots
2. Performance benchmarking
3. Quality metrics (noise, temporal stability)
4. Compatibility testing with Q3 maps
5. Regression testing for existing features

## Success Criteria
- [ ] Path tracer matches Q2RTX quality
- [ ] Denoising quality matches or exceeds Q2RTX
- [ ] Material system supports .mat files
- [ ] Physical sky renders correctly
- [ ] God rays work in appropriate scenes
- [ ] FSR provides quality upscaling
- [ ] Performance is acceptable (60fps+ on RTX 3060)
- [ ] No regressions in existing features
