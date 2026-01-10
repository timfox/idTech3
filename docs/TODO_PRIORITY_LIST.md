# Vulkan Renderer TODO Priority List

**Total TODOs: 127** (as of 2026-01-09)

## Priority Categories

### 🔴 CRITICAL (Fix Immediately)
**Issues that affect stability, memory safety, or core functionality**

1. **Memory Leaks & Resource Cleanup** (5 items)
   - `gltf_loader.c:178` - TODO: Free descriptor set
   - `tr_rtx.c:105` - TODO: Clean up Vulkan resources if needed
   - `rtx/vk_rtx_main.cpp:250` - TODO: Shutdown RTX-specific resources
   - Various cleanup TODOs in RTX renderer

2. **Core Functionality Issues** (3 items)
   - `tr_backend.c:1837` - TODO: implement! ZZZZZZZZZZZ (critical missing implementation)
   - `tr_font_vk.c:38` - TODO: Implement proper Vulkan font texture creation
   - `tr_lightclusters.c:15` - TODO: Implement light binning (clustered/forward+)

### 🟠 HIGH (Fix Soon)
**Important features or code quality issues**

3. **Shader & Rendering Issues** (8 items)
   - `tr_shader.c:1021` - FIXME: assumes max exponent of 8192 and min of 1
   - `tr_shade.c:1449` - FIXME: we can't do that if going to lighting/fog later?
   - `tr_shade_calc.c:1175` - FIXME: track dynamically (hardcoded light origin)
   - Various shader-related FIXMEs

4. **Animation & Mesh Issues** (4 items)
   - `tr_animation.c:153` - FIXME: non-normalized axis issues
   - `tr_mesh.c:257` - FIXME: non-normalized axis issues
   - `tr_model_iqm.c:1074` - FIXME: non-normalized axis issues

5. **BSP & Geometry Issues** (3 items)
   - `tr_bsp.c:1161` - FIXME: write generalized version that also avoids cracks
   - `tr_bsp.c:263` - FIXME: check range (color code by intensity)
   - `tr_curve.c:415` - FIXME: also check midpoints of adjacent patches

### 🟡 MEDIUM (Fix When Time Permits)
**Optional features or improvements**

6. **RTX/Ray Tracing** (25+ items)
   - All TODOs in `rtx/vk_rtx_main.cpp` - RTX implementation stubs
   - `rtx/vk_raymarching.cpp:302` - TODO: Bind descriptor set when implemented
   - Various RTX feature TODOs

7. **Terrain & Decals** (30+ items)
   - All TODOs in `vk_terrain.c` - Terrain system implementation
   - All TODOs in `vk_decals.c` - Decal system implementation
   - All TODOs in `vk_surface_sprites.c` - Surface sprite system

8. **Advanced Features** (10+ items)
   - `vk_god_rays.c` - God rays implementation TODOs
   - `vk_sem.c:132` - TODO: Implement actual texture array loading
   - `vk_sync.c` - GPU timing query TODOs

### 🟢 LOW (Future Enhancements)
**Nice-to-have or incomplete features**

9. **GLTF & Model Loading** (2 items)
   - `gltf_loader.h:150` - TODO: Animation support
   - GLTF-related improvements

10. **Code Quality & Comments** (5 items)
    - `tr_backend.c:1199` - FIXME: not exactly backend (comment clarification)
    - `tr_backend.c:1622` - TODO: Maybe check for rdf_noworld stuff
    - `tr_local.h:877` - FIXME: use this! (culling information)
    - `tr_renderer_vulkan.c:83` - TODO: Implement if needed

11. **Shader Binding** (1 item)
    - `shaders/spirv/shader_binding.c:7` - TODO: Add shader bindings here

## Summary by File

- **RTX Renderer**: ~30 TODOs (all MEDIUM - incomplete feature)
- **Terrain System**: ~25 TODOs (all MEDIUM - incomplete feature)
- **Decals/Surface Sprites**: ~20 TODOs (all MEDIUM - incomplete feature)
- **Core Rendering**: ~15 TODOs (mix of CRITICAL and HIGH)
- **Shader System**: ~10 TODOs (mostly HIGH)
- **Animation/Mesh**: ~8 TODOs (mostly HIGH)
- **Other**: ~19 TODOs (mix of priorities)

## Recommended Fix Order

1. **Week 1**: Fix all CRITICAL items (memory leaks, core functionality)
2. **Week 2**: Fix HIGH priority items (shader issues, animation fixes)
3. **Week 3+**: Address MEDIUM items as features are needed
4. **Future**: Complete LOW priority items when time permits

## Notes

- Many TODOs in RTX, Terrain, and Decals are for incomplete optional features
- Core rendering TODOs are more critical and should be prioritized
- Memory leak TODOs should be fixed immediately to prevent resource exhaustion
- FIXME items typically indicate bugs or issues, while TODO items are for missing features
