# FBO Audit: Changes Since 20736ed (FBO-Working Baseline)

**Baseline commit**: 20736eddd0f5a5c06eb37e24e8442cde7f03a6cc  
**Date**: March 3, 2026 (Sky: enable procedural atmosphere by default)  
**Scope**: Identify what changed that could break FBO and ensure stability.

---

## 1. Critical Flow Comparison

### OLD (20736ed) Flow

**vk_volumetric_fog_pass** (when volumetrics run):
- Atmosphere → copy scene → compute → composite → layout transition
- If `vk.smaaActive && tr.world && !RDF_NOWORLDMODEL`: run SMAA, update `color_descriptor` to smaa_output
- Else: update `color_descriptor` to color_image_view

**vk_volumetric_fog_pass** (when skipped — tier/off/no-world):
- `vk_reset_volumetric_history()`, `backEnd.doneFog = qtrue`, **return**
- **BUG**: No descriptor update. `color_descriptor` stays at previous frame's value.

**vk_volumetric_fog_pass** (when skipped — resources missing / MSAA incomplete):
- **BUG**: Same — early return, no descriptor update.

**vk_prepare_2d** (no-world / menu):
- `vk_end_render_pass()`, `vk_begin_post_bloom_render_pass()`, return
- **BUG**: No descriptor update. Gamma will sample stale `color_descriptor`.

**vk_end_frame**:
- If `!backEnd.doneFog`: call `vk_volumetric_fog_pass()`
- Gamma pass uses `vk.color_descriptor`

### NEW (current) Flow

**vk_volumetric_fog_pass** (all skip paths):
- Call `vk_update_post_fog_descriptors(vk.color_image_view)` before return ✓

**vk_prepare_2d** (no-world):
- Call `vk_update_post_fog_descriptors(vk.color_image_view)` before `vk_begin_post_bloom_render_pass()` ✓

**vk_end_frame** (when `backEnd.doneFog`):
- Run SMAA if active (scene+2D in color_image)
- Call `vk_update_post_fog_descriptors(smaa_output or color_image_view)` ✓

---

## 2. Fixes That Should Stay (Correct)

| Fix | Commit | Description |
|-----|--------|-------------|
| Descriptor updates on skip | d94b5a3f, 58ff71aa | All volumetric-skip paths now update `color_descriptor` and `luminance_descriptor` |
| SSAO combine | 1815ea7b | Was outputting only AO map; now multiplies scene × AO |
| Main pass clear | ad6e9df9 | Color attachment uses CLEAR instead of DONT_CARE |
| SMAA blend descriptor | 5cf87f0a | Blend pass set 0 now uses `smaa_blend_descriptor` (edge map) not `smaa_edge_descriptor` (scene) |
| SMAA after 2D | 58ff71aa | SMAA runs in vk_end_frame when volumetrics skipped, so scene+2D get anti-aliased |
| Volumetric finalLayout | 70b6a297 | Composite pass finalLayout = SHADER_READ_ONLY_OPTIMAL |
| Gamma framebuffer dims | 70b6a297 | Uses swapchain_extent, zero-size guard |
| OIT accum descriptor | 6d9136e1 | OIT accum binds texture descriptor, not uniform |
| Scene copy before SSAO combine | 1815ea7b | Copy color_image → fog_scene to avoid read-modify-write |

---

## 3. Potential Regression Points

### 3.1 OIT Draw Path (r_oit 1)

When `r_oit 1` and `r_fbo 1`:
- Opaque surfaces → OIT accum (transparent) → resolve to color → post_bloom
- If OIT resolve or descriptor binding is wrong, could corrupt color_image.

**Mitigation**: OIT was disabled (9d41f165) then re-enabled (0d7afa9d) after descriptor fix. If FBO is still broken with `r_oit 0`, OIT is not the cause.

### 3.2 tier == 4 Skip

OLD: `tier >= 2` skipped. NEW: `tier >= 2 || tier == 4` skips.  
Tier 4 explicitly skips volumetrics. Unlikely to cause solid color.

### 3.3 Atmosphere Before Volumetrics

OLD: `vk_atmosphere_pass()` at start of vk_volumetric_fog_pass.  
NEW: Atmosphere only when `tr.world && !RDF_NOWORLDMODEL`; moved before the `backEnd.doneFog` check.

**Check**: Ensure atmosphere doesn't run when it shouldn't (e.g. menu) and doesn't corrupt color_image.

### 3.4 vk_get_post_fog_source / post_fog_color_source

NEW: `vk.post_fog_color_source` tracks the last source. `vk_get_post_fog_source()` returns it when set.  
**Risk**: If `post_fog_color_source` is never set or set incorrectly, gamma could sample wrong image.

**Check**: All paths that set descriptors should also set `vk.post_fog_color_source` (or ensure it's set before gamma).

### 3.5 Luminance Descriptor

When volumetrics skipped, `luminance_descriptor` binding 0 must point to the correct source (color_image or smaa_output).  
`vk_update_post_fog_descriptors()` updates both. ✓

---

## 4. Recommended Verification

1. **Minimal config**: `r_fbo 1`, `r_volumetricFog 0`, `r_ssao 0`, `r_oit 0`, `r_ext_smaa 0`  
   - Eliminates volumetrics, SSAO, OIT, SMAA. If still broken, issue is in core FBO path.

2. **Add volumetrics**: `r_volumetricFog 1`  
   - If breaks, issue is in volumetric path.

3. **Add SMAA**: `r_ext_smaa 1`  
   - If breaks, issue is in SMAA path.

4. **Add SSAO**: `r_ssao 1`  
   - If breaks, issue is in SSAO path.

5. **Add OIT**: `r_oit 1`  
   - If breaks, issue is in OIT path.

---

## 5. Quick Reference: Descriptor Update Points

| Path | Updates color_descriptor? | Updates luminance_descriptor? |
|------|---------------------------|-------------------------------|
| vk_volumetric_fog_pass (full run) | ✓ via vk_update_post_fog_descriptors | ✓ |
| vk_volumetric_fog_pass (tier/off/no-world skip) | ✓ vk_update_post_fog_descriptors(color) | ✓ |
| vk_volumetric_fog_pass (resources missing skip) | ✓ | ✓ |
| vk_volumetric_fog_pass (MSAA incomplete skip) | ✓ | ✓ |
| vk_prepare_2d (no-world) | ✓ vk_update_post_fog_descriptors(color) | ✓ |
| vk_end_frame (backEnd.doneFog, SMAA on) | ✓ vk_update_post_fog_descriptors(smaa_output) | ✓ |
| vk_end_frame (backEnd.doneFog, SMAA off) | ✓ vk_update_post_fog_descriptors(color) | ✓ |

---

## 6. Summary

The OLD code had **missing descriptor updates** when volumetrics were skipped. The NEW code fixes this. The SSAO combine fix was also critical (was overwriting scene with AO).

If FBO is still broken, the cause is likely:
1. A path that doesn't call `vk_update_post_fog_descriptors` when it should
2. `vk.post_fog_color_source` not set correctly before gamma
3. A layout transition or barrier missing
4. OIT/SSR/other new pass corrupting color_image

Run with `r_fboDebug 2` and `com_developer 1` to see which path is taken and which source is used for gamma.

---

## 7. Recent Stability Improvements (Post-Audit)

- **FBO startup log**: When `r_fbo 1`, log "FBO enabled (HDR, post-process, gamma, PBR-ready)" at init.
- **Attachment init**: Call `vk_update_post_fog_descriptors(color_image_view)` when updating attachment descriptors so luminance_descriptor is initialized for eye adaptation.
- **Gamma pass guards**: Skip gamma when `post_fog_src` and `color_image_view` are both null; skip when pipeline/descriptor/renderpass/framebuffer missing or zero size.
- **Gamma pass barrier (belt-and-suspenders)**: Call `vk_barrier_post_fog_source_for_sampling(gamma_src)` immediately before the gamma pass when gamma_src is valid, ensuring color-attachment writes are visible to the fragment shader.
- **SMAA blend pass descriptors**: Blend pass set 0 = scene (color_image) for texelSize, set 1 = edge map. Use `smaa_edge_descriptor` (color) and `smaa_blend_descriptor` (edge) instead of both blend.
