# RTX Build System Fixes

## Summary

Fixed build system issues to enable RTX modules. The main problems were:

1. **RTX modules were commented out** in CMakeLists.txt
2. **Missing include paths** - RTX files in subdirectory needed `../` for includes
3. **Symbol conflicts** - `GetRefAPI` defined in both main renderer and RTX renderer
4. **Undefined references** - Some functions need proper linkage

## Changes Made

### 1. Enabled RTX Modules in CMakeLists.txt
- Added RTX source files directly to `RENDERER_VK_SRCS` instead of separate library
- Files enabled:
  - `vk_rtx_main.cpp`
  - `vk_path_tracer.cpp`
  - `vk_denoiser.cpp`
  - `vk_rtx_raii.cpp`
  - `vk_raytracing.cpp`
  - `vk_compute_raytracing.c`
  - `vk_raymarching.cpp`

### 2. Fixed Include Paths
- `vk_raytracing.cpp`: Added `../` prefix to includes
- `vk_compute_raytracing.h`: Fixed `tr_local.h` include
- `vk_raymarching.h`: Fixed `vk.h` include
- `vk_raymarching.cpp`: Fixed includes
- `vk_compute_raytracing.c`: Fixed includes
- `vk_rtx_main.cpp`: Added `vk.h` and `tr_local.h` includes

### 3. Fixed Symbol Conflicts
- Commented out `GetRefAPI` export in `vk_rtx_main.cpp` since RTX is integrated, not standalone

## Remaining Issues

### Linker Errors (To Fix)

1. **Undefined references in vk_rtx_main.cpp**:
   - `RE_RegisterShader` - exists in `tr_shader.c`, may need `extern "C"` linkage
   - `RE_RegisterShaderNoMip` - same issue
   - `R_LoadWorld` - should be `RE_LoadWorldMap` or similar
   - `VK_ComputeRT_Shutdown` - needs implementation or forward declaration
   - `vk_rtx_acceleration_shutdown` - needs implementation
   - `vk_rt_update_uniform_buffer` - needs implementation

2. **Undefined references in vk_raytracing.cpp**:
   - `R_LoadPNG` - needs implementation or alternative
   - `R_CreateImage` - needs proper function name
   - `Matrix16InverseOptimized` - exists in `tr_math_optimized.c`, may need linkage
   - `vk_rtx_get_current_quality_preset` - needs implementation
   - `r_rtx_shadows`, `r_rtx_reflections`, `r_rtx_gi` - CVARs need to be defined
   - `vk_begin_post_bloom_render_pass` - needs implementation
   - `vk_rtx_bind_surface_indices_buffer` - needs implementation

3. **Undefined references in vk_compute_raytracing.c**:
   - `vk_rtx_acceleration_init` - needs implementation

## Next Steps

1. Add `extern "C"` guards for C functions called from C++
2. Fix function name mismatches (e.g., `R_LoadWorld` vs `RE_LoadWorldMap`)
3. Implement or stub missing functions
4. Define missing CVARs
5. Verify all RTX functions are properly linked

## Status

- ✅ RTX modules enabled in build
- ✅ Include paths fixed
- ✅ Symbol conflicts resolved
- ✅ Compilation errors fixed
- ⚠️ Linker errors remain (need missing function implementations/stubs)

## Remaining Linker Errors

The following functions need to be implemented or stubbed:

1. **vk_raytracing.cpp**:
   - `R_LoadPNG` - PNG loading function
   - `R_CreateImage` - Image creation function
   - `Matrix16InverseOptimized` - Matrix inversion (exists in tr_math_optimized.c, may need linkage fix)
   - `vk_rtx_get_current_quality_preset` - Quality preset getter
   - `r_rtx_shadows`, `r_rtx_reflections`, `r_rtx_gi` - CVARs need to be defined
   - `vk_begin_post_bloom_render_pass` - Post-processing function
   - `vk_rtx_bind_surface_indices_buffer` - Buffer binding function

2. **vk_compute_raytracing.c**:
   - `vk_rtx_acceleration_init` - Acceleration structure initialization

These can be stubbed out initially to get the build working, then implemented properly.
