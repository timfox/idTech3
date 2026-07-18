# Renderer Phase 1 Checklist (2026 Q3)

**Date**: July 18, 2026  
**Scope**: actionable stabilization checklist for the 2026 H2 renderer roadmap

This document turns the Phase 1 renderer roadmap into an implementation-oriented checklist with concrete source files, tests, and instrumentation points.

Primary planning docs:

- [RENDERER_MODERNIZATION_ROADMAP_2026H2.md](RENDERER_MODERNIZATION_ROADMAP_2026H2.md)
- [RENDERER_2026_ARCHITECTURE_PASS.md](RENDERER_2026_ARCHITECTURE_PASS.md)
- [FORWARD_PLUS_PIPELINE_AUDIT.md](FORWARD_PLUS_PIPELINE_AUDIT.md)

---

## Phase 1 Goal

Make the shipping Vulkan path reliable enough that:

- `r_renderMode 2` remains the default product path
- late-frame post toggles do not black-screen or corrupt output
- device loss is easier to diagnose
- mode 1 and mode 3 remain clearly experimental until they are stable

---

## A. Pass Ownership And Resume Safety

### Target files

- `renderers/vulkan/vk_scene_pass.c`
- `renderers/vulkan/vk_render_pass.c`
- `renderers/vulkan/vk_2d_transition.c`
- `renderers/vulkan/vk_deferred_gbuffer.c`
- `renderers/vulkan/vk_vegetation_wind.c`

### Tasks

- [x] Audit every call path that ends the main pass and later resumes it.
- [x] Ensure every resume site goes through the shared scene-pass helpers rather than open-coded assumptions.
- [x] Verify the post-bloom continuation path always resumes the expected framebuffer and render-pass pair.
- [ ] Verify UI overlay and 2D transition paths cannot silently drift the active pass state.
- [x] Verify deferred and compute detours restore the main scene pass explicitly before transparent or overlay work resumes.

### Existing hooks worth extending

- `vk_scene_pass_validate_resume`
- `vk_scene_pass_validate_begin`
- `vk_scene_pass_resume_framebuffer`
- `vk_resume_current_render_pass`
- `vk_resume_main_render_pass`
- `vk_pass_diag_*` / `vk_report_device_lost_context`

### Desired instrumentation

- [x] Last successful pass name
- [x] Last requested resume target
- [x] Current framebuffer identity (via begin extent + pass name)
- [x] Whether the engine believed it was in-pass or out-of-pass
- [x] Whether the resume path had to self-heal/fallback

### Existing tests

- `tests/scripts/test_scene_pass_validation.sh`
- `tests/scripts/test_compute_break_validation.sh`
- `tests/scripts/test_renderer_self_heal.sh`
- `tests/scripts/test_unified_clustered.sh`
- `tests/scripts/test_deferred_lighting.sh`
- `tests/scripts/test_modern_renderer_profile_runtime.sh`

---

## B. Late Post / Bloom Safety

### Target files

- `renderers/vulkan/vk_postfx_passes.c`
- `renderers/vulkan/vk_postfx.c`
- `renderers/vulkan/vk_postfx_params.c`
- `renderers/vulkan/vk_frame_end.c`
- `renderers/vulkan/shaders/glsl/bloom.frag`
- `renderers/vulkan/shaders/glsl/blur.frag`
- `renderers/vulkan/shaders/glsl/gamma.frag`

### Current risk

As reproduced on **July 18, 2026**, a modern mode-2 stack could survive broad feature enablement and still fail only after the final bloom-tuning delta, producing:

- corrupted output
- black output
- `VK_ERROR_DEVICE_LOST`

That makes late post/bloom work a first-class stabilization target.

### Tasks

- [x] Log bloom path entry/exit with enough context to identify the active source image and expected destination.
- [x] Validate that bloom extraction only runs when its source image/view/layout are valid.
- [ ] Validate every bloom downsample and blur step against expected image dimensions and layouts.
- [ ] Validate the post-bloom continuation pass after bloom finishes, especially when toggles are applied from config startup.
- [ ] Audit bloom threshold/intensity parameter ranges and how they are pushed or specialized through the pipeline.
- [ ] Confirm gamma/final compose always samples the intended post-bloom source after bloom, SMAA, FXAA, and volumetric branches.

### Specific code points

- `vk_bloom`
- `vk_begin_bloom_extract_render_pass`
- `vk_begin_post_bloom_render_pass`
- `vk_barrier_post_fog_source_for_sampling`
- post-bloom refresh paths in `vk_postfx_passes.c`

### Desired instrumentation

- active post source image/view
- render target width/height at bloom entry
- bloom attachment width/height chain
- threshold/intensity values at runtime
- final source chosen by gamma / AA-after-bloom path

---

## C. Attachment And Descriptor Validity

### Target files

- `renderers/vulkan/vk_attachments.c`
- `renderers/vulkan/vk_descriptor_sets.c`
- `renderers/vulkan/vk_image_layout.c`
- `renderers/vulkan/vk_render_pass.c`

### Tasks

- [ ] Verify bloom attachments are created only when the pass graph really needs them and destroyed cleanly on reconfiguration.
- [ ] Verify descriptor updates for bloom images cannot lag behind attachment recreation.
- [ ] Add validation prints or assertions for null image views, stale descriptor sets, or mismatched dimensions in the bloom/post chain.
- [ ] Re-check layout transitions around `color_image`, bloom attachments, and post-fog sources.
- [ ] Confirm post-bloom compatibility assumptions in `vk_render_pass.c` still match real attachment usage.

### Existing evidence

- `vk_attachments.c` already builds the bloom image chain.
- `vk_descriptor_sets.c` already updates bloom descriptors conditionally.
- `docs/VULKAN_FBO_AUDIT.md` and `docs/FBO_BREAKAGE_ANALYSIS.md` already identify stale bindings and post-bloom transitions as risk areas.

---

## D. Device-Loss Diagnostics

### Target files

- `renderers/vulkan/vk_util.c`
- `renderers/vulkan/vk_shutdown.c`
- `renderers/vulkan/vk_frame_submit.c`
- `renderers/vulkan/vk_cmd.c`
- `renderers/vulkan/tr_init_diagnostics.inc`

### Tasks

- [x] Improve device-loss logging so the engine reports the active renderer profile and recent render toggles before recursive shutdown noise.
- [x] Record the last completed major Vulkan stage for the frame.
- [x] Record the last begun pass and the last ended pass.
- [x] Report whether the loss happened near bloom/post/AA/deferred/clustered work.
- [ ] Reduce duplicate recursive shutdown spam after `VK_ERROR_DEVICE_LOST` so the first useful error remains visible.

### Minimum crash-context payload

- active `r_renderMode`
- key toggles:
  - `r_bloom`
  - `r_hdr`
  - `r_fbo`
  - `r_pbr`
  - `r_forwardPlus`
  - `r_deferredGBuffer`
  - `r_ext_smaa`
  - `r_taa`
- last pass name
- last post stage name
- render target dimensions
- whether the frame was in a resumed continuation pass

---

## E. Runtime Profiles And Recovery Commands

### Target files

- `config/modern_vulkan.cfg`
- `config/deferred_vulkan.cfg`
- runtime overlay configs under `config/`
- startup diagnostics in `renderers/vulkan/tr_init_diagnostics.inc`

### Tasks

- [ ] Keep the shipping default conservative until the late-post/device-loss issue is fixed.
- [ ] Ensure safe recovery profiles stay documented and actually recover on live installs.
- [ ] Make runtime diagnostics clearly state whether the player is on the shipping mode-2 path, a deferred experimental path, or a unified-clustered experimental path.
- [ ] Ensure docs and config stacks do not drift on which path is considered default, safe, or experimental.

---

## F. Validation And Regression Coverage

### Existing tests to preserve

- `tests/scripts/test_modern_vulkan_default.sh`
- `tests/scripts/test_vulkan_runtime_regressions.sh`
- `tests/scripts/test_vulkan_renderer_guards.sh`
- `tests/scripts/test_scene_pass_validation.sh`
- `tests/scripts/test_compute_break_validation.sh`
- `tests/scripts/test_renderer_self_heal.sh`

### Cheap additions worth making

- [x] A source guard that bloom/post paths still validate resume ownership before returning to continuation passes.
- [ ] A source guard that gamma/final compose references the intended post-bloom source selection path.
- [x] A runtime diagnostics test that the renderer status print includes current mode/profile/post toggles.
- [x] A regression note/test for the July 18, 2026 late-bloom device-loss failure class.

---

## Recommended Implementation Order

1. Pass ownership and resume tracing
2. Bloom/post instrumentation
3. Descriptor/attachment validity assertions
4. Device-loss crash-context logging
5. Conservative config/profile cleanup
6. Additional source/runtime regression guards

---

## Exit Criteria For Phase 1

Phase 1 is done when:

- the shipping mode-2 path no longer black-screens or device-loses from ordinary late-post toggles
- pass resume diagnostics make it obvious where continuation state drift happened
- device-loss logs preserve useful context instead of collapsing into recursive shutdown noise
- the default config stack stays on a conservative known-good path until broader settings are re-promoted intentionally
