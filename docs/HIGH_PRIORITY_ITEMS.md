# High Priority Items - Vulkan Renderer

**Last Updated**: 2026-01-09  
**Status**: Active Development

This document lists all high-priority TODOs, FIXMEs, and critical issues that need to be addressed in the Vulkan renderer. Items are categorized by priority and impact.

---

## 🔴 CRITICAL PRIORITY

**Issues that affect stability, memory safety, or core rendering functionality. Must be fixed immediately.**

### 1. Memory Leaks & Resource Cleanup

#### `tr_rtx.c:105` - RTX Resource Cleanup
- **Issue**: RTX renderer may not properly clean up Vulkan resources on shutdown
- **Impact**: Memory leaks, resource exhaustion
- **Status**: TODO - Clean up Vulkan resources if needed
- **Action Required**: Implement proper cleanup in `RTX_Shutdown()`

#### `rtx/vk_rtx_main.cpp:250` - RTX-Specific Resources
- **Issue**: RTX-specific resources not properly cleaned up
- **Impact**: Memory leaks on renderer shutdown
- **Status**: TODO - Shutdown RTX-specific resources
- **Action Required**: Add cleanup for acceleration structures, shader binding tables, descriptor sets

#### `gltf_loader.c:178` - Descriptor Set Cleanup
- **Issue**: Descriptor sets allocated but not freed
- **Impact**: Descriptor pool exhaustion
- **Status**: TODO - Free descriptor set
- **Action Required**: Track and free descriptor sets when models are unloaded

### 2. Core Rendering Functionality

#### `tr_backend.c:1161` - Render Pass State
- **Issue**: CRITICAL comment - Ensure main render pass is started before UI rendering
- **Impact**: Rendering corruption, crashes
- **Status**: Documented but needs verification
- **Action Required**: Add validation to ensure render pass is active before UI rendering

#### `tr_backend.c:1837` - Missing Implementation
- **Issue**: TODO: implement! ZZZZZZZZZZZ (critical missing implementation)
- **Impact**: Unknown - needs investigation
- **Status**: Critical missing implementation
- **Action Required**: Identify what functionality is missing and implement it

#### `tr_font_vk.c:38` - Font Texture Creation
- **Issue**: TODO - Implement proper Vulkan font texture creation
- **Impact**: Font rendering may not work correctly
- **Status**: Currently uses STB fallback
- **Action Required**: Implement native Vulkan font texture creation

### 3. Image Layout & Synchronization

#### Multiple CRITICAL comments in `vk.c` - Image Layout Handling
- **Locations**: Lines 6246, 8529, 8551, 8695, 8710, 8730, 8756, 8775, 8848, 8908, 9442, 9487, 9522
- **Issue**: Image layout transitions must be handled correctly to prevent corruption
- **Impact**: Rendering artifacts, driver errors, crashes
- **Status**: Documented with CRITICAL comments
- **Action Required**: 
  - Verify all layout transitions are correct
  - Add validation for layout state
  - Ensure proper barriers between layout transitions

#### `tr_shade.c:77, 93` - ScreenMap Capture
- **Issue**: CRITICAL comments about screenMap usage and capture
- **Impact**: Incorrect screenMap usage can cause rendering corruption
- **Status**: Documented but needs verification
- **Action Required**: Ensure screenMap is only used if captured THIS frame

---

## 🟠 HIGH PRIORITY

**Important features or code quality issues that should be fixed soon.**

### 4. Light Clustering System

#### `tr_lightclusters.c:22` - Buffer Storage
- **Issue**: TODO - Add Vulkan buffer storage for cluster headers and indices
- **Impact**: Clustered forward+ lighting not functional
- **Status**: Stub implementation
- **Action Required**:
  1. Declare static VkBuffer lcHeaderBuffer, lcIndexBuffer
  2. Allocate buffers with VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
  3. Use device-local memory with staging buffer for uploads
  4. Bind buffers to descriptor sets (bindings 6 and 7 per tr_lightclusters.glsl)
  5. Update buffers each frame in R_BuildLightClusters()

#### `tr_lightclusters.c:52, 80` - Light Binning Implementation
- **Issue**: TODO - Implement full light binning (clustered/forward+)
- **Impact**: Dynamic lighting performance degradation
- **Status**: No-op implementation
- **Action Required**:
  1. Allocate/resize Vulkan buffers (see TODO above)
  2. Reset cluster headers (lightOffset=0, lightCount=0 for all clusters)
  3. For each dynamic light in tr.refdef.dlights:
     - Compute screen-space AABB of light influence
     - Map depth range to Z slices using logarithmic distribution
     - For each affected cluster (tileX, tileY, sliceZ):
       - Append light index to cluster's light list
  4. Upload cluster data to GPU buffers via staging buffer
  5. Bind buffers to shader descriptor sets for use in lighting shaders

### 5. Shader System Issues

#### `tr_shader.c:2616` - Modulated Add Collapse
- **Issue**: TODO - Verify and fix modulated add + modulated add collapse logic
- **Impact**: Visual artifacts with shaders using multiple modulated add stages
- **Status**: Known issue documented
- **Action Required**: 
  - Test all combinations of modulated add blending modes
  - Fix collapse table if incorrect
  - Consider disabling multitexture collapse for problematic shaders

#### `tr_sky.c:600, 608` - Sky fullClouds Field
- **Issue**: TODO - Add fullClouds field to skyParms_t structure
- **Impact**: Sky rendering may not correctly handle full cloud coverage
- **Status**: Hardcoded to always enable full clouds
- **Action Required**:
  1. Add `qboolean fullClouds;` to skyParms_t typedef in tr_local.h
  2. Parse fullClouds from shader files in tr_shader.c
  3. Replace hardcoded (1) with shader->sky.fullClouds check in tr_sky.c

### 6. Pipeline & State Management

#### `tr_backend.c:1865` - Extended Dynamic State
- **Issue**: TODO - Implement full support using VK_EXT_extended_dynamic_state3
- **Impact**: Pipeline recreation overhead for color mask changes
- **Status**: Currently requires pipeline recreation
- **Action Required**:
  1. Check for VK_EXT_extended_dynamic_state3 extension support
  2. Enable VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT in pipeline creation
  3. Call vkCmdSetColorWriteMaskEXT() instead of storing state
  4. Alternatively, track color mask state and include it in pipeline hash/key

#### `tr_surface.c:1447` - Vertex Layout Optimization
- **Issue**: TODO - Use common vertex layout and avoid TESS_ST0 binding if not needed
- **Impact**: Minor performance optimization
- **Status**: Currently includes TESS_ST0 even when not needed
- **Action Required**: Update axis pipeline to not require texture coordinates, change to `vk_bind_geometry( TESS_XYZ | TESS_RGBA0 )`

### 7. Compute Scheduler

#### `vk_compute_scheduler.cpp:250` - Dependency Checking
- **Issue**: TODO - Implement proper dependency checking by verifying dependency jobs are completed
- **Impact**: Jobs may execute out of order, causing incorrect results
- **Status**: Currently assumes dependencies are resolved
- **Action Required**:
  1. For each dependency in internal_job->dependencies[]:
     - Look up dependency job in compute_scheduler.active_jobs map
     - Check if dependency job state is JOB_STATE_COMPLETED
     - If any dependency is not completed, mark dependencies_resolved = qfalse
  2. Only mark job as ready when all dependencies are completed

#### `vk_compute_scheduler.cpp:796, 809` - Semaphore Arrays
- **Issue**: TODO - Add semaphore array to vk_compute_job_t structure
- **Impact**: Cannot use semaphore synchronization for compute jobs
- **Status**: Functions exist but don't store semaphores
- **Action Required**:
  - For wait semaphores: Add VkSemaphore wait_semaphores[MAX_WAIT_SEMAPHORES], VkPipelineStageFlags wait_stages[MAX_WAIT_SEMAPHORES], uint32_t wait_semaphore_count
  - For signal semaphores: Add VkSemaphore signal_semaphores[MAX_SIGNAL_SEMAPHORES], uint32_t signal_semaphore_count
  - Store semaphores in arrays when functions are called
  - Use arrays in vkQueueSubmit() when submitting the job

### 8. GPU Timing & Profiling

#### `vk_sync.c:247, 252, 257` - GPU Timing Queries
- **Issue**: TODO - Implement GPU timing queries for performance profiling
- **Impact**: Cannot measure GPU performance, difficult to optimize
- **Status**: Stub implementation
- **Action Required**:
  1. Allocate query pool if not already created (one per frame in flight)
  2. Reset query pool: vkCmdResetQueryPool() or VK_EXT_host_query_reset
  3. Record begin timestamp: vkCmdWriteTimestamp(vk.cmd->command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, query_pool, query_index)
  4. Record end timestamp: vkCmdWriteTimestamp(vk.cmd->command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, query_pool, query_index)
  5. Retrieve timestamps: vkGetQueryPoolResults(device, query_pool, query_index, 2, sizeof(uint64_t)*2, timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT)
  6. Calculate delta: (end_timestamp - begin_timestamp) * timestamp_period
  7. Convert to milliseconds: delta_ns / 1e6

---

## 🟡 MEDIUM PRIORITY

**Optional features or improvements that can be addressed when time permits.**

### 9. Ray Tracing Integration

#### `vk_raytracing.cpp:28, 39, 50` - RTX Renderer Integration
- **Issue**: TODO - Call RTX renderer functions when fully integrated
- **Impact**: Hardware ray tracing not functional
- **Status**: Interface stubs exist
- **Action Required**:
  1. RTX renderer module must be compiled and linked
  2. RTX_vk_rt_init(), RTX_vk_rt_shutdown(), RTX_vk_rt_trace_rays() must be implemented
  3. Check for ray tracing extension support before calling
  4. Uncomment function calls in vk_raytracing.cpp

### 10. Terrain System

#### `vk_terrain.c` - Multiple TODOs
- **Issues**: Heightmap loading, height editing, material painting, ray tracing
- **Impact**: Terrain system incomplete
- **Status**: Core rendering works, editing features missing
- **Action Required**: See individual TODOs in vk_terrain.c for implementation steps

### 11. Surface Sprites

#### `vk_surface_sprites.c:265, 414` - Sprite Management
- **Issues**: Sprite type removal, ray tracing for interaction
- **Impact**: Limited sprite system functionality
- **Status**: Basic rendering works, advanced features missing
- **Action Required**: See individual TODOs in vk_surface_sprites.c

---

## ✅ RECENTLY FIXED

### Fog Collapse FIXME - `tr_shader.c:4215`
- **Status**: ✅ FIXED
- **Fix**: Added NULL checks for mirror pipelines, proper fallback logic
- **Date**: 2026-01-09
- **Details**: Fog collapse now works safely even when mirror pipelines are disabled

---

## Summary Statistics

- **Critical Items**: 8
- **High Priority Items**: 12
- **Medium Priority Items**: 15+
- **Recently Fixed**: 1

## Recommended Fix Order

1. **Week 1**: Fix all CRITICAL items (memory leaks, core functionality, image layouts)
2. **Week 2**: Fix HIGH priority items (light clustering, shader issues, compute scheduler)
3. **Week 3+**: Address MEDIUM items as features are needed (RTX, terrain editing)

## Notes

- Many CRITICAL items are related to resource cleanup and image layout handling
- HIGH priority items focus on performance and feature completeness
- MEDIUM priority items are mostly optional features (RTX, terrain editing)
- All items have been documented with implementation steps where applicable
- Regular review and updates to this document are recommended

---

**Document Maintainer**: Development Team  
**Review Frequency**: Weekly during active development
