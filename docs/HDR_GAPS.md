# HDR Pipeline Gaps and Risks

This document tracks known gaps, risks, and mitigations in the Vulkan HDR rendering pipeline.

## 6.1 Lightmap Color Space

**Issue**: BSP lightmaps are 8-bit; q3map2 may output gamma-encoded or linear values.

**Current**: Used as linear; `r_hdr_lightmap_scale` only scales intensity.

**Mitigation**: Added `r_lightmap_srgb_decode` (0=linear assumed, 1=sRGB→linear). When `r_hdr` 1/2 and lightmaps are gamma-encoded (e.g. q3map2 `-gamma`), set to 1 to avoid darkening in bright areas.

**How to tell if lightmaps are gamma-encoded**: Maps compiled with q3map2 `-gamma` (or `-gamma 2.2` etc.) produce gamma-encoded lightmaps. Many mod workflows use `-gamma` for a brighter look; if bright areas appear too dark or washed out with HDR on, try `r_lightmap_srgb_decode 1`. Linear lightmaps (no `-gamma`) are typical for vanilla Q3A and some modern maps.

## 6.1b Double-Precision (64-bit) Render Targets

**Option**: `r_hdr 3` requests RGBA64F (VK_FORMAT_R64G64B64A64_SFLOAT).

**Gating**: Requires `VkPhysicalDeviceFeatures::shaderFloat64` and format support for COLOR_ATTACHMENT, SAMPLED_IMAGE, SAMPLED_IMAGE_FILTER_LINEAR. Almost no consumer GPUs expose this for color render targets.

**Current**: Falls back to RGBA32F. 64-bit fragment shader output (dvec4) not yet implemented; would require separate pipeline/shaders. **In practice, `r_hdr 3` behaves like 32-bit HDR** because of this fallback; use `r_hdr 2` explicitly if you want to avoid the 64-bit request path.

## 6.2 No True 32-bit HDR Lightmaps

**Current**: 8-bit lightmaps + `r_hdr_lightmap_scale`. No real HDR range; scale can cause banding.

**Future**: Support q3map2 HDR lightmaps (3×16-bit or float) if available. Requires BSP format extension and loader changes.

## 6.3 Pre-exposure Scale

**Current**: `r_pre_exposure_scale` cvar (default 1.0) wired to gamma pass. Use for bloom/tonemap pipeline tweaks or future HDR pipeline adjustments.

**Typical use**: Scale bloom and tonemap input uniformly (e.g. 0.5 to dim highlights, 1.5 to brighten). Useful when tuning AgX/Filmic tonemap or when debug views (`r_post_debug`) show clipped highlights. Leave at 1.0 for normal use.

## 6.4 Bloom Knee

**Current**: `applyBloomKnee` in gamma pass applies a soft shoulder (smoothstep) to the main scene before tonemap. Bloom extraction uses `r_bloom_threshold` and `r_bloomKnee`; knee controls highlight rolloff. Both are applied; verified consistent with extraction intent.

## 6.5 Luminance Pass Timing

**Current**: Luminance from frame N used for frame N+1 (one-frame delay). Expected.

**Mitigation**: Camera cut detection-when view origin jumps >128 units, eye adaptation uses 4× faster blend (0.5) to snap within ~2 frames. Prevents slow adaptation after teleports or level loads.

**Death blowout**: On camera cut (e.g. death camera jump), target exposure is capped by `r_exposure_auto_cap_on_cut` (default 0.75) to avoid blowing out when the view suddenly faces a bright sky. Set to 0 to disable the cap.

## 6.5b Wobble / Underwater-like Distortion

Post-processing effects that can produce a "wobble" or underwater feel (especially during death or cutscenes):

| Cvar | Default | Effect |
|------|---------|--------|
| `r_chromaticAberration` | 0.22 | RGB separation on edges; can look like lens distortion. Set to 0 to disable. |
| `r_filmGrain` | 0.75 | Adds noise; with `r_filmLook` can create soft-light grain. Set to 0 to disable. |
| `r_paniniBarrelDistortion` | 0.0 | Barrel/pincushion warp. Non-zero adds visible distortion. |
| `cg_underwaterFovWarp` | 0 (OSP) | FOV warping when in water. If a mod enables it incorrectly, can cause wobble when not underwater. |

If the camera feels like it's underwater when it isn't, try `r_chromaticAberration 0` and `cg_underwaterFovWarp 0` (if using OSP).

## 6.5c Death-State and Console Artifacts

**Single-pixel artifacts when console is open**: Film grain (`r_filmGrain`, default 0.75) adds per-pixel noise across the screen. Set `r_filmGrain 0` to eliminate. Chromatic aberration can also cause fine color fringing.

**Streaky/smudged visuals when dead**: Volumetric fog temporal reprojection accumulates when the death camera is nearly static. `r_volumetricFogSkipStatic` (default 1) skips volumetrics when the view has been static for ~0.5 s, preventing the gradient/streak artifact. Set to 0 to always run fog. If artifacts persist, try `r_volumetricFog 0` to confirm, or `r_volumetricFogForceCameraCut 1` to force an immediate reset (debug).

**Quick isolation**: `r_filmGrain 0 r_volumetricFog 0 r_oit 0 r_ssao 0 r_ssr 0` then `vid_restart` to narrow down the source.

## 6.6 HDR Skybox vs Procedural Sky

- **HDR skybox**: EXR or Radiance .hdr with `r_skyboxHDR_exposure`, `r_skyboxHDR_intensity`
- **Procedural**: `r_atmosphere`, `r_atmosphere_scale`

Both feed into linear HDR; no conflict.

## 6.7 Dynamic Light Intensity

**Current**: All GPU light consumers (`projector`, Forward+, deferred, volumetric) share `R_DynamicLightColor()` in `tr_light.c`.

| Cvar | Default | Role |
|------|---------|------|
| `r_dynamicLightScale` | 1 | Global multiplier (0.25–4); use with HDR + `r_exposure` instead of `r_gamma`. |
| `r_lightGammaLink` | 1 | When 0, drop `pow(r_intensity,r_gamma)`; use `2*r_intensity` (+ scale) only. |

**Legacy path** (no hardware gamma, no FBO): `2 * pow(r_intensity, r_gamma) * r_dynamicLightScale`.

**HDR FBO path**: Raw `dl->color` unless `r_dynamicLightScale` ≠ 1 or `r_lightGammaLink` 0 (then `2*r_intensity` applies).

**Forward+ fix (2026)**: Tile SSBO packing now uses the same scale as projector dlights (was raw `dl->color` on legacy paths).

**Tuning**: Prefer `r_exposure` / `r_exposure_auto` for scene brightness; `r_dynamicLightScale` for muzzle flashes and explosions; `r_lightGammaLink 0` when `r_gamma` should affect display only.

## 6.9 Color pipeline tiers (gamma pass)

**World resolve** (menus/UI skip via `paniniPad1`): exposure (`r_exposure` / auto), pre-exposure (`r_pre_exposure_scale`), bloom knee, tonemap (`r_tonemap`), then display encode (`r_gamma` via `apply_srgb_gamma` when the swapchain is UNORM).

**Grading stack** (`r_post` 1): local exposure, white balance, LUT, contrast/saturation, motion blur, DOF, chromatic aberration, vignette, film grain, outline — all gated separately from resolve so `r_post 0` still tonemaps HDR instead of hard-clipping to 1.0.

**Swapchain**: `apply_srgb_gamma` is 0 for `*_SRGB` present formats (hardware encodes); linear UNORM gets shader gamma from `r_gamma`. Hardware gamma tables are bypassed when `vk.fboActive` (`s_gammatable_linear`).

**Lightmaps**: `r_lightmap_srgb_decode` 1 decodes gamma-encoded BSP lightmaps in `gen_frag` when `r_hdr` is on. Albedo PBR paths use `VK_FORMAT_R8G8B8A8_SRGB` via `vk_create_pbr_albedo_srgb`.

**Known legacy coupling**: `VK_SetLightParams` only applies `r_intensity`/`r_gamma` scaling when hardware gamma is off and FBO is off; HDR FBO uses raw `dl->color`.

## 6.8 TAA (`r_taa`)

**Current**: Optional temporal resolve after the post-fog source (SMAA / volumetrics), before luminance and gamma. Uses `vk_temporal` reset policy (resize, map load, camera cut, missing prev matrices) and skips the pass on portals, `RDF_NOWORLDMODEL`, first-person projection toggles, and **unreliable motion** (e.g. `customShader` vertex deformation).

**History clamp**: `taa.frag` uses a 3×3 neighborhood min/max on the **current** color before blending reprojected history (prevents fireflies when history is valid).

**Motion**: `r_taaMotionVectors` **1** (default) samples the main-pass motion attachment (`gen_frag` `out_motion`, same as volumetric fog). **0** uses depth + `prevViewProj` reprojection only.

**Tuning**: `r_taa_feedbackStationary` / `r_taa_feedbackMotion` / `r_taa_sharpen`. Use `r_temporalDebug 1` to log reset reasons. Default **off**; enable when SMAA alone is not enough.

---

## 7. Render Order

1. **Main pass** → scene to `vk.color_image` (RGBA16F or RGBA32F when `r_hdr` 1 or 2)
2. **OIT** (if `r_oit`): opaque copy to `fog_scene`, OIT accum for transparents, resolve to `color_image`. Runs during draw; output stays in HDR.
3. **Copy scene** → `vk.fog_scene_image` (for volumetric composite)
4. **Atmosphere pass** → additive sky where depth == far (only when `tr.world` and not `RDF_NOWORLDMODEL`; skipped for menus, videos)
5. **SSR** (if `r_ssr`): screen-space reflections; reads color + depth, writes back to `color_image`. Before bloom. The Vulkan SSR subpass is created only when SSR is enabled (`vk_update_post_process_pipelines`); toggling **`r_ssr`** (or bloom/SSAO/SMAA/OIT, HDR color format, or certain bloom cvars) schedules a **post-pipeline rebuild** at the next frame start (`PostFX_PostPipelinesNeedUpdate` in `vk_postfx.c`, refresh in `vk_post_process_refresh.c`). Quality tuning cvars do not require that rebuild.
6. **Bloom extraction** → from `color_image` to bloom chain (threshold + knee). **Bloom blend** → additive blend of blurred bloom back to `color_image`. Both before SSAO and luminance.
7. **SSAO** (if `r_ssao`): copy `color_image` to `fog_scene`, samples depth, blur, combine with scene into `fog_scene`. When SSAO is on, `fog_scene` becomes the post-fog source.
8. **Volumetric compute + composite** → fog over scene (when enabled). Reads and writes `fog_scene`.
9. **SMAA** (if enabled): edge detection, blend, resolve. Runs after volumetrics (or after 2D overlays if volumetrics skipped). Output is the post-fog source.
10. **TAA** (if `r_taa` 1): temporal resolve on post-fog source; history ping-pong in `taa_history` images. Skipped when temporal policy marks the frame unstable.
11. **Luminance pass** (if `r_exposure_auto`): compute pass on post-fog source (after TAA when enabled); result used next frame for eye adaptation.
12. **Gamma pass** → tonemap, exposure, gamma → swapchain.

**OIT / SSAO / SSR and HDR**: All operate in HDR space. OIT resolve outputs to `color_image`; SSAO combine writes to `fog_scene`; SSR modifies `color_image`. Luminance and gamma consume the final post-fog source (SMAA output or `fog_scene`/`color_image` when SMAA off).

---

## 8. HDR UI

**Current**: When `RDF_NOWORLDMODEL` or no world (menus, player config, videos), the gamma pass sets `paniniPad1 = 1.0`, which disables the HDR film pipeline (`noWorldLdr`). UI is **composited after tonemap**-menus and 2D overlays are authored as LDR and rendered on top of the (possibly tonemapped) framebuffer. Brightness and exposure are forced to 1.0 for these frames to avoid darkening or blowing out UI. No HDR-specific scaling or correction for UI; it is drawn in the same LDR output space as the gamma pass.

---

## 9. Screen-Space Effects: SSAO, SSR, Volumetric Fog

### 9.1 SSAO Edge Halos

**Issue**: Normals are derived from depth gradients (`dFdx`/`dFdy`). At depth discontinuities (object silhouettes, horizon), gradients are unreliable and produce dark/light halos.

**Mitigation**: `r_ssaoMaxDepthGradient` (default 0.08). Pixels where neighbor depth differs more than this threshold output full AO (1.0) instead of computing from bad normals. Set to 0 to disable. Lower = stricter edge rejection.

### 9.2 SSR Depth Discontinuities

**Issue**: Same depth-derived normal problem. Thin black/white lines can appear at object edges.

**Mitigation**: `r_ssr_maxDepthGradient` (default 0.08). SSR skips pixels at depth edges. Lower = stricter; may reduce reflection coverage near silhouettes.

### 9.3 Volumetric Fog Artifacts

**Sources**:
- **Transparent/alpha-tested**: Fog compositing assumes opaque depth; foliage, fences, grates can show incorrect fog density at alpha edges.
- **Temporal reprojection**: When view is nearly static (e.g. death cam), accumulated history can drift. Periodic camera cut (`vk_near_static_view_frames` ~90) resets history.
- **Motion vectors**: Animated entities without previous-frame skinning output zero velocity; entities with `customShader` (vertex deformation) also use zero motion to avoid ghosting.
