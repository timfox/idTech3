# HDR Pipeline Gaps and Risks

This document tracks known gaps, risks, and mitigations in the Vulkan HDR rendering pipeline.

## 6.1 Lightmap Color Space

**Issue**: BSP lightmaps are 8-bit; q3map2 may output gamma-encoded or linear values.

**Current**: Used as linear; `r_hdr_lightmap_scale` only scales intensity.

**Mitigation**: Added `r_lightmap_srgb_decode` (0=linear assumed, 1=sRGB→linear). When `r_hdr` 1/2 and lightmaps are gamma-encoded (e.g. q3map2 `-gamma`), set to 1 to avoid darkening in bright areas.

## 6.1b Double-Precision (64-bit) Render Targets

**Option**: `r_hdr 3` requests RGBA64F (VK_FORMAT_R64G64B64A64_SFLOAT).

**Gating**: Requires `VkPhysicalDeviceFeatures::shaderFloat64` and format support for COLOR_ATTACHMENT, SAMPLED_IMAGE, SAMPLED_IMAGE_FILTER_LINEAR. Almost no consumer GPUs expose this for color render targets.

**Current**: Falls back to RGBA32F. 64-bit fragment shader output (dvec4) not yet implemented; would require separate pipeline/shaders.

## 6.2 No True 32-bit HDR Lightmaps

**Current**: 8-bit lightmaps + `r_hdr_lightmap_scale`. No real HDR range; scale can cause banding.

**Future**: Support q3map2 HDR lightmaps (3×16-bit or float) if available. Requires BSP format extension and loader changes.

## 6.3 Pre-exposure Scale

**Current**: `r_pre_exposure_scale` cvar (default 1.0) wired to gamma pass. Use for bloom/tonemap pipeline tweaks or future HDR pipeline adjustments.

## 6.4 Bloom Knee

**Current**: `applyBloomKnee` in gamma pass applies a soft shoulder (smoothstep) to the main scene before tonemap. Bloom extraction uses `r_bloom_threshold` and `r_bloomKnee`; knee controls highlight rolloff. Both are applied; verified consistent with extraction intent.

## 6.5 Luminance Pass Timing

**Current**: Luminance from frame N used for frame N+1 (one-frame delay). Expected.

**Mitigation**: Camera cut detection—when view origin jumps >128 units, eye adaptation uses 4× faster blend (0.5) to snap within ~2 frames. Prevents slow adaptation after teleports or level loads.

## 6.6 HDR Skybox vs Procedural Sky

- **HDR skybox**: EXR/HDR with `r_skyboxHDR_exposure`, `r_skyboxHDR_intensity`
- **Procedural**: `r_atmosphere`, `r_atmosphere_scale`

Both feed into linear HDR; no conflict.

## 6.7 Dynamic Light Intensity

**Current**: `VectorScale(dl->color, 2 * powf(r_intensity->value, r_gamma->value), ...)`. `r_gamma` affects light intensity; intentional for legacy look. Consider separating display gamma from light intensity if needed.

---

## 7. Render Order

1. **Main pass** → scene to `vk.color_image` (RGBA16F or RGBA32F when `r_hdr` 1 or 2)
2. **Copy scene** → `vk.fog_scene_image` (for volumetric composite)
3. **Atmosphere pass** → additive sky where depth == far (only when `tr.world` and not `RDF_NOWORLDMODEL`; skipped for menus, videos)
4. **Volumetric compute + composite** → fog over scene (when enabled)
5. **SMAA** (if enabled)
6. **Luminance pass** (if `r_exposure_auto`)
7. **Gamma pass** → tonemap, exposure, gamma → swapchain
