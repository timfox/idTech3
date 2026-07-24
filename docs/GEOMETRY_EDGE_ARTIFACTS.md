# Geometry Edge Artifacts / Mesh Silhouette Halos

**Status:** Active remediation (engine Vulkan path)  
**Related:** [DEPTH_CONTRACT.md](DEPTH_CONTRACT.md), [BILATERAL_UPSAMPLING.md](BILATERAL_UPSAMPLING.md), [MESH_SILHOUETTE_HALO.md](MESH_SILHOUETTE_HALO.md)

## Symptom

Depth-discontinuity haloing: bright/dark fringes, background dilation, and contour ringing around silhouettes. Follows geometry boundaries, not texture boundaries. The filled mesh is correctly shaped — contamination is image-space.

## Classification

```text
SILHOUETTE_CORONA
DEPTH_EDGE_HALO
FOREGROUND_DILATION
BACKGROUND_BLEED
BLOOM_EDGE_EXPANSION
AO_GI_EDGE_BLEED
TEMPORAL_DISOCCLUSION_LEAK
```

## First corrupting owners (Surf defaults)

With `r_ambientVisibilityMode 2` (GTAO), `r_taa 0`, `r_volumetricFog 0`, `r_ssr 1`, `r_ext_smaa 1`:

| Rank | Pass | Failure |
|------|------|---------|
| 1 | Bloom extract (when enabled) | Depth-blind firefly / far-side silhouette seeding into pyramid |
| 2 | SMAA compose | Neighborhood blend across depth discontinuities |
| 3 | AV spatial filter | LINEAR sample before bilateral weights |
| 4 | Weapon TAA (mode 2) | Raw device-Z depth confidence |
| 5 | Legacy SSAO silhouette skip | Device-Z gradient |
| 6 | RcGI upsample | Bilinear `texture()` of low-res irradiance |
| 7 | Post sharpen | Soft overshoot when depth gate is partial |

## Fix policy

* All bilateral / temporal depth decisions use **positive reversed-Z view depth**.
* Reduced-res upsample: 4-tap `texelFetch` + depth weights; nearest fallback when weight sum ≈ 0.
* Bloom extract: depth-aware firefly neighborhood + far-side silhouette gate (does not globally reduce bloom intensity).
* SMAA compose: reject blends when relative view-depth error exceeds threshold.
* Do not “fix” by blurring, disabling bloom/AO permanently, or widening thresholds without unit verification.

## Isolation A/B

```text
mesh_halo_capture
mesh_halo_pass_bisect
r_bloom 0
r_ssr 0
r_ambientVisibilityMode 0 ; r_ssao 0
r_ext_smaa 0
r_sharpen 0
r_weaponTemporalMode 0
```

## Debug

* `mesh_halo_status` / `mesh_halo_validate`
* `r_meshHaloDebug` — reserved silhouette views
* `r_rtaoDebug` — AV views
* `r_ssaoDebugView` — legacy AO
