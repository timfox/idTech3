# Bilateral Upsampling

Shared helpers live in `renderers/vulkan/shaders/glsl/depth_view.glsl`:

* `Depth_LinearizeReversedZ` — device → positive view meters
* `Depth_BilateralWeight(center, sample, sharpness)` — **relative** view-depth error
* `Depth_NormalWeight`

AV mirrors these in `ambient_visibility/av_common.glsl` (`av_positive_view_depth`, `av_bilateral_depth_weight`) using the same `projInfo` reconstruction as GTAO.

## Production coefficients

| Pass | Sharpness | Notes |
|------|-----------|-------|
| AV filter | 48 (relative) | `texelFetch` samples (no LINEAR pre-mix) |
| AV temporal | 0.04 relative tol | Was device-Z 0.0025 |
| AV composite upsample | 48 | Half/quarter → full |
| SSAO blur | 48 | Depth-aware diamond blur |
| SSAO silhouette skip | relative view-Z | Legacy device-Z cvars remapped ×2.5 |
| RcGI denoise / upscale | 48 | `texelFetch` 4-tap + bilateral |
| Bloom extract firefly | 64 | Depth-gated neighborhood |
| SMAA compose | 0.05 rel reject | Drop blend weights across depth edges |
| Weapon TAA | 0.04 rel default | Was device-Z 0.025 |

Zero total weight → keep center sample (never invent neighbors).

## Forbidden

* Raw device-Z subtraction for edge rejection
* Plain bilinear of AO/GI/volumetrics across large depth deltas
* Normalizing from a near-zero weight sum
* Bloom extract seeding from far-side silhouette neighbors
