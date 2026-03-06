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

**Mitigation**: Camera cut detection—when view origin jumps >128 units, eye adaptation uses 4× faster blend (0.5) to snap within ~2 frames. Prevents slow adaptation after teleports or level loads.

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

## 6.6 HDR Skybox vs Procedural Sky

- **HDR skybox**: EXR/HDR with `r_skyboxHDR_exposure`, `r_skyboxHDR_intensity`
- **Procedural**: `r_atmosphere`, `r_atmosphere_scale`

Both feed into linear HDR; no conflict.

## 6.7 Dynamic Light Intensity

**Current**: `VectorScale(dl->color, 2 * powf(r_intensity->value, r_gamma->value), ...)`. `r_gamma` affects light intensity; intentional for legacy look. Consider separating display gamma from light intensity if needed.

**Desirable**: On dark monitors or legacy maps authored for CRT gamma, raising `r_gamma` brightens both display and lights—often what users expect. **Problematic**: When using HDR + tonemap, `r_gamma` changes light intensity without changing display gamma (handled by gamma pass), so lights can look too bright or too dim relative to the tonemapped scene. Prefer `r_exposure` or `r_exposure_auto` for HDR brightness control.

---

## 7. Render Order

1. **Main pass** → scene to `vk.color_image` (RGBA16F or RGBA32F when `r_hdr` 1 or 2)
2. **OIT** (if `r_oit`): opaque copy to `fog_scene`, OIT accum for transparents, resolve to `color_image`. Runs during draw; output stays in HDR.
3. **Copy scene** → `vk.fog_scene_image` (for volumetric composite)
4. **Atmosphere pass** → additive sky where depth == far (only when `tr.world` and not `RDF_NOWORLDMODEL`; skipped for menus, videos)
5. **SSR** (if `r_ssr`): screen-space reflections; reads color + depth, writes back to `color_image`. Before bloom.
6. **Bloom extraction** → from `color_image` to bloom chain (threshold + knee). **Bloom blend** → additive blend of blurred bloom back to `color_image`. Both before SSAO and luminance.
7. **SSAO** (if `r_ssao`): copy `color_image` to `fog_scene`, samples depth, blur, combine with scene into `fog_scene`. When SSAO is on, `fog_scene` becomes the post-fog source.
8. **Volumetric compute + composite** → fog over scene (when enabled). Reads and writes `fog_scene`.
9. **SMAA** (if enabled): edge detection, blend, resolve. Runs after volumetrics (or after 2D overlays if volumetrics skipped). Output is the post-fog source.
10. **Luminance pass** (if `r_exposure_auto`): compute pass on post-fog source; result used next frame for eye adaptation.
11. **Gamma pass** → tonemap, exposure, gamma → swapchain.

**OIT / SSAO / SSR and HDR**: All operate in HDR space. OIT resolve outputs to `color_image`; SSAO combine writes to `fog_scene`; SSR modifies `color_image`. Luminance and gamma consume the final post-fog source (SMAA output or `fog_scene`/`color_image` when SMAA off).

---

## 8. HDR UI

**Current**: When `RDF_NOWORLDMODEL` or no world (menus, player config, videos), the gamma pass sets `paniniPad1 = 1.0`, which disables the HDR film pipeline (`noWorldLdr`). UI is **composited after tonemap**—menus and 2D overlays are authored as LDR and rendered on top of the (possibly tonemapped) framebuffer. Brightness and exposure are forced to 1.0 for these frames to avoid darkening or blowing out UI. No HDR-specific scaling or correction for UI; it is drawn in the same LDR output space as the gamma pass.
