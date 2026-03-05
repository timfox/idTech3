# Vulkan FBO Rendering System Audit Report

**Date**: March 4, 2025  
**Context**: User reports `r_fbo 1` is "very broken" — solid colors, wrong rendering.  
**Scope**: Full pipeline trace from `vk.fboActive` through main pass, post-process, and swapchain.

---

## 1. vk.fboActive — Where Set and What Triggers Recreation

### Where Set
- **Location**: `vk.c` lines 6345–6353
- **Source**: `r_fbo->integer` cvar (CVAR_ARCHIVE | CVAR_LATCH)
- **Logic**:
  ```c
  if ( r_fbo->integer ) {
      vk.fboActive = qtrue;
      if ( r_ext_multisample->integer ) vk.msaaActive = qtrue;
  } else {
      vk.fboActive = qfalse;
  }
  vk.smaaActive = (vk.fboActive && r_ext_smaa->integer) ? qtrue : qfalse;
  ```

### When FBO Resources Are Recreated
- **vid_restart**: Triggers full Vulkan reinit; `r_fbo` is CVAR_LATCH so it takes effect on restart.
- **vk_create_framebuffers()**: Called from `vk_create_window()` (line 6245) and swapchain recreation (line 7241).
- **vk_alloc_attachments()**: Creates `vk.color_image`, `vk.fog_scene_image`, `vk.smaa_output_image`, etc., only when `vk.fboActive` (line 5443).
- **Render passes**: Created in `vk_create_render_passes()`; main pass layout depends on `fboActive`.

### Critical Dependency
`r_fbo` is **CVAR_LATCH** — changes require `vid_restart` to take effect. No hot-swap of FBO on/off.

---

## 2. Main Render Pass — Attachments, Formats, Layouts

### r_fbo 0 (fboActive = false)
| Attachment | Format | Samples | Layout |
|------------|--------|---------|--------|
| 0 (color) | `vk.present_format.format` | 1x | initSwapchainLayout → PRESENT_SRC_KHR |
| 1 (depth) | `vk.depth_format` | vkSamples | DEPTH_STENCIL_ATTACHMENT_OPTIMAL |

- **Framebuffer**: `vk.swapchain_image_views[n]` (direct to swapchain)
- **Size**: `gls.windowWidth` × `gls.windowHeight`

### r_fbo 1 (fboActive = true)
| Attachment | Format | Samples | Layout |
|------------|--------|---------|--------|
| 0 (resolve/color) | `vk.color_format` (HDR) | 1x | SHADER_READ_ONLY_OPTIMAL |
| 1 (depth) | `vk.depth_format` | vkSamples | DEPTH_STENCIL_ATTACHMENT_OPTIMAL |
| 2 (motion) | R16G16_SFLOAT | 1x | SHADER_READ_ONLY_OPTIMAL |
| 3 (MSAA color, if MSAA) | `vk.color_format` | vkSamples | COLOR_ATTACHMENT_OPTIMAL |
| 4 (MSAA motion, if MSAA) | R16G16_SFLOAT | vkSamples | COLOR_ATTACHMENT_OPTIMAL |

### HDR Format (r_hdr 0/1/2)
- **r_hdr -1**: B4G4R4A4 (testing)
- **r_hdr 0**: `vk.base_format.format` (8-bit)
- **r_hdr 1**: RGBA16F
- **r_hdr 2**: RGBA32F (fallback to 16F if unsupported)
- **r_hdr 3**: RGBA64F (shaderFloat64 enabled when supported; fragment shaders still vec4, falls back to 32F)

### Layout Transitions (fboActive)
- Main pass: `initialLayout = SHADER_READ_ONLY_OPTIMAL`, `finalLayout = SHADER_READ_ONLY_OPTIMAL`
- Post-bloom: `loadOp = LOAD`, reuses same attachments
- Volumetric composite: writes to `vk.color_image`, then transitions to SHADER_READ_ONLY
- Gamma pass: samples from `vk.color_descriptor` (color_image or smaa_output)

---

## 3. Descriptor Chain

### vk.color_descriptor
- **Updated by**: `vk_update_color_descriptor_image(view)` and `vk_update_attachment_descriptors()`
- **Points to** (at different stages):
  - After volumetric + SMAA: `vk.smaa_output_image_view`
  - After volumetric, no SMAA: `vk.color_image_view`
  - When volumetrics skipped: `vk.color_image_view` (fallback at 15290)

### vk.luminance_descriptor
- **Binding 0**: Input image (color or SMAA output) for luminance compute
- **Binding 1**: `vk.luminance_image_view` (1×1 storage)
- **Updated**:
  - In `vk_volumetric_fog_pass()` (lines 14684–14711): `smaa_output` or `color_image_view`
  - In volumetrics-skipped path (lines 15291–15321): `color_image_view` only

### When Volumetrics Are Skipped
- **luminance_descriptor binding 0** → `vk.color_image_view`
- **color_descriptor** → `vk.color_image_view`
- **Bug**: SMAA is never run when volumetrics are skipped. If SMAA was active in a previous frame, `color_descriptor` may have pointed to `smaa_output`; the skipped path always forces `color_image_view`, which is correct for the current frame (no SMAA ran). However, the **luminance** path does not account for SMAA when skipped — it always uses `color_image_view`, which is consistent.

### Other Descriptors
- **screenMap.color_descriptor** → `vk.screenMap.color_image_view`
- **cubeMap.color_descriptor** → `vk.cubeMap.color_image_view[0]`
- **smaa_edge_descriptor**, **smaa_blend_descriptor**, **smaa_compose_descriptor** — SMAA pipeline stages

---

## 4. Post-Process Flow

### Pipeline Flow (Text Diagram)

```
r_fbo 0:
  [Main Pass] → swapchain_image (direct)
       ↓
  [Present]

r_fbo 1:
  [Main Pass] → color_image (or msaa_image → resolve → color_image)
       ↓
  [Optional: SSAO] (if r_ssao)
       ↓
  [Optional: vk_prepare_2d] → end main, run volumetrics, begin post_bloom
       ↓
  [Post-bloom Pass] (2D overlays on same framebuffer)
       ↓
  [vk_end_render_pass]
       ↓
  [vk_volumetric_fog_pass] OR [volumetrics-skipped fallback]
       │
       ├─ IF volumetrics run:
       │     [Atmosphere Pass] (sky overlay)
       │     [Copy color_image → fog_scene_image]
       │     [Volumetric Compute]
       │     [Volumetric Composite] → color_image
       │     [SMAA] (if active, world, !RDF_NOWORLDMODEL) → smaa_output
       │     [Update color_descriptor] (smaa_output or color_image_view)
       │     [Update luminance_descriptor]
       │
       └─ IF volumetrics skipped:
             [Update color_descriptor] → color_image_view
             [Update luminance_descriptor] → color_image_view
       ↓
  [Luminance Pass] (if r_exposure_auto && r_hdr)
       ↓
  [Gamma Pass] → swapchain_image (samples color_descriptor)
       ↓
  [Present]
```

### Paths That Can Produce Solid Color / Wrong Output

1. **Descriptor points to wrong image**: If `color_descriptor` samples from an uninitialized, cleared, or wrong-layout image.
2. **Layout mismatch**: Gamma pass expects `SHADER_READ_ONLY_OPTIMAL`; wrong layout can cause undefined behavior.
3. **Volumetrics skipped + stale descriptor**: When volumetrics are skipped, the fallback updates descriptors. If that path is not taken (e.g. different control flow), descriptors can remain stale.
4. **r_exposure_auto + luminance**: Luminance pass writes to 1×1 image; if gamma shader accidentally sampled luminance, result would be solid color. Audit shows gamma uses `color_descriptor`, not luminance — so this is unlikely unless there is a shader bug.
5. **First-frame / init**: `vk.color_image` created with `SHADER_READ_ONLY_OPTIMAL`; main pass transitions to `COLOR_ATTACHMENT`. First-frame layout handling appears correct.
6. **gls.windowWidth vs glConfig.vidWidth**: Gamma framebuffer uses `gls.windowWidth/Height`; main pass uses `glConfig.vidWidth/Height`. Mismatch can cause scaling/black bars but not necessarily solid color.

---

## 5. r_fbo 0 vs r_fbo 1 — Render Path Difference

| Aspect | r_fbo 0 | r_fbo 1 |
|--------|---------|---------|
| Main target | Swapchain image | `vk.color_image` (or MSAA resolve) |
| HDR format | No (base_format) | Yes (r_hdr) |
| Post-process | None | Atmosphere, volumetrics, SMAA, luminance, gamma |
| Gamma pass | No | Yes (samples color_descriptor → swapchain) |
| vk.color_image_view | NULL | Allocated |
| vk.color_descriptor | NULL (not allocated) | Allocated |
| Framebuffer size | gls.windowWidth × gls.windowHeight | glConfig.vidWidth × glConfig.vidHeight |

**r_fbo 0**: Renders directly to swapchain; no FBO, no post-processing.  
**r_fbo 1**: Full offscreen pipeline with HDR, volumetrics, SMAA, gamma correction.

---

## 6. Known Fixes and Guards

### From Code Comments (lines 15287–15290)
```c
/* Volumetrics skipped (menu, no world, tier off): ensure gamma and
 * luminance passes sample from correct source. vk_volumetric_fog_pass
 * normally updates color_descriptor and luminance_descriptor; when
 * skipped, they may point at smaa_output from a previous frame. */
vk_update_color_descriptor_image( vk.color_image_view );
```

### QUICKSTART.md Workaround (line 57)
- `r_exposure_auto 0` — disables eye adaptation
- `r_volumetricFog 0` — disables volumetrics
- `vid_restart`
- If still broken: `r_fbo 0` as workaround

### OIT Draw Path (March 2025)
OIT re-enabled after wiring the OIT accum pipeline. When `r_oit 1` + `r_fbo 1`: opaque surfaces drawn first, then `vk_oit_pass` runs OIT accum (transparent surfaces with WBOIT), resolves to main color, then resumes post_bloom. The OIT accum pipeline uses `oit_accum.vert`/`oit_accum.frag` with additive blend and gen vertex layout.

### Identified Gaps (Fixed)
1. **SMAA when volumetrics skipped**: Fixed. SMAA now runs when volumetrics are skipped (tier off, resources missing, MSAA incomplete) and in menus/no-world (`vk_prepare_2d` menu path). Descriptors updated in all paths.
2. **Luminance when volumetrics skipped**: Luminance binding 0 is set to `color_image_view`. If `r_exposure_auto` is on and luminance was previously fed from `smaa_output`, the luminance pass now reads from `color_image_view`. That is correct for the current frame.
3. **Layout transition when volumetrics skipped**: The main/post_bloom pass leaves `color_image` in `SHADER_READ_ONLY_OPTIMAL` (finalLayout). No explicit transition is needed before gamma. Layout handling appears correct.

---

## 7. Deferred vs Forward Architecture

### Current Architecture: **Forward Rendering**
- **Main pass**: Forward rendering — geometry, lighting, fog drawn in a single pass.
- **No G-Buffer**: No deferred pass; no separate albedo/normal/position buffers.
- **Post-process**: Screen-space only (SSAO, SMAA, bloom, volumetrics, gamma).

### Pass Order
1. Shadow maps (sun, local spot, local point)
2. Screenmap (reflection/cubemap)
3. Main pass (forward scene)
4. Optional SSAO
5. Post-bloom (2D overlays)
6. Atmosphere (sky overlay)
7. Volumetric fog (compute + composite)
8. SMAA
9. Luminance (eye adaptation)
10. Gamma → swapchain

**No deferred or Forward+ path** — purely forward with screen-space post-processing.

---

## Identified Bugs and Risks

### High Priority (Fixed)
1. **Stale descriptor when volumetrics skip early** (fixed): If `vk_volumetric_fog_pass` returns early (e.g. `backEnd.doneFog`, tier off, missing resources), it sets `backEnd.doneFog = qtrue` and returns. The caller’s `else` branch then updates descriptors. Fixed: vk_prepare_2d menu path now calls vk_update_post_fog_descriptors. All skip paths update descriptors.
2. **Potential layout/transition gap**: When volumetrics are skipped, no explicit transition is recorded for `color_image` before gamma. The render pass `finalLayout` should handle this; worth validating with Vulkan validation layers.

### Medium Priority (Fixed)
3. **SMAA when volumetrics skipped**: Fixed. SMAA now runs when volumetrics are skipped (tier off, r_volumetricFog 0, missing resources) and in menus/no-world. Descriptors updated in all paths.
4. **r_exposure_auto + r_volumetricFog 0**: QUICKSTART suggests disabling both. Luminance pass runs independently; if there is a bug in luminance or tone mapping, it could contribute to solid/wrong colors.

### Low Priority (Addressed)
5. **gls vs glConfig size mismatch**: Gamma uses `gls.windowWidth/Height` (swapchain size); main uses `glConfig.vidWidth/Height` (render resolution). Intentional: gamma samples color_image (vid size) and outputs to swapchain (window size). Fallback to vid dimensions when window is 0 (minimized).

---

## Recommendations

### HDR
- Add validation that `vk.color_format` matches `r_hdr` and that the gamma shader’s tone mapping matches the format.
- Consider a fallback path when HDR format is unsupported (e.g. RGBA32F → RGBA16F).

### Post-Process
- **Unify descriptor update**: Factor descriptor updates into a single function used by both volumetric and non-volumetric paths to avoid divergence.
- **SMAA when volumetrics skipped**: Implemented. SMAA runs in menus and when volumetrics are skipped.
- **Logging**: Add `PRINT_DEVELOPER` logs when volumetrics are skipped and descriptors are updated, to aid debugging.

### Post-Process Pipeline Enhancements (March 2025)
- **Centralized post-fog source**: `vk_get_post_fog_source()` returns the correct `VkImageView` for luminance/gamma based on `backEnd.doneFog` and `vk.post_fog_color_source`. Used in `vk_end_frame` to avoid redundant logic.
- **r_fboDebug level 3**: Pipeline state logging (gamma pipeline, color_descriptor, layout, render pass, framebuffer handles) for debugging FBO issues.
- **Defensive null checks**: Gamma pass skips and logs a warning if pipeline, descriptor, render pass, or framebuffer is null, avoiding crashes when resources are missing.

### Deferred / Forward+
- Current design is forward-only. No change recommended unless deferred/Forward+ is a stated goal.
- If adding deferred later, keep the FBO/descriptor design in mind to avoid similar descriptor/layout issues.

### Debugging
- Enable Vulkan validation layers and check for:
  - Image layout transitions
  - Descriptor set bindings
  - Render pass compatibility
- Add a debug overlay (e.g. `r_fboDebug`) to show which image `color_descriptor` samples from.

---

## Summary

The FBO pipeline is complex, with multiple conditional paths (volumetrics on/off, SMAA, bloom, SSAO). The main risk for solid/wrong colors is **stale or incorrect descriptor bindings** when volumetrics are skipped.

### Fix Applied (March 2025)

- **vk_update_post_fog_descriptors()**: New centralized helper updates both `color_descriptor` and `luminance_descriptor` to sample from the given image view.
- **All volumetric skip paths** now call this helper before returning: tier/no-world skip, missing-resources skip, MSAA-depth-incomplete skip.
- **Success path** and **vk_end_frame fallback** both use the same helper.
- This ensures gamma and luminance passes always sample the correct image, eliminating the solid-color bug when volumetrics are skipped.

### Additional Fixes (March 2025 — r_fbo 1 solid rapidly-changing color)

- **Main pass color clear**: FBO color attachment now uses `VK_ATTACHMENT_LOAD_OP_CLEAR` instead of `DONT_CARE` to avoid uninitialized or stale content that could produce solid/wrong colors.
- **Belt-and-suspenders descriptor update**: `vk.post_fog_color_source` tracks the last source (color_image or smaa_output) used for gamma. Right before the gamma pass, `vk_update_post_fog_descriptors()` is called again with that source (or `color_image_view` if unset) to ensure the descriptor is never stale.

### SSAO Combine Fix (March 2025 — FBO color/post-process broken)

- **SSAO combine overwrote scene**: The ssao_combine shader previously output only the AO map (`vec4(ao,ao,ao,1)`), replacing the full scene with a grayscale AO image. Fixed to multiply scene × AO: `out_color = vec4(scene * ao, 1.0)`.
- **Pipeline and descriptors**: ssao_combine now uses `pipeline_layout_ssao_combine` (2 sets: scene + AO), `vk.color_format` for output, and binds `ssao_scene_descriptor` + `ssao_blur_descriptor`.
- **Read-modify-write avoidance**: Before combine, `color_image` is copied to `fog_scene_image`; the combine samples from `fog_scene_image` and writes to `color_image`, avoiding undefined behavior from sampling and writing the same image in one pass.
