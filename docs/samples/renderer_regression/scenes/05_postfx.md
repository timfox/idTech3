# Scene 05 - PostFX toggles

## Goal

Catch **MSAA, SMAA, SSAO, bloom**, plus **HDR/tonemap/grade** ordering and resolution mismatches.

## Layout

- **High-contrast edges** (geometry silhouette against skybox).
- **Contact geometry** (objects near ground) for SSAO.
- **Small bright highlights** for bloom / tonemap rolloff.
- **Graded midtones** (warm/cool wall or decal) for LUT / white-balance checks.

## Pass criteria

- Each mode: **on vs off** does not change aspect ratio, viewport, or produce black buffer.
- SMAA/MSAA: edges stable; no **double image** or broken resolve. Prefer `r_ext_multisample 0` with modern profile for true G-buffer material export.
- SSAO: contact shadowing appears near contact; no **screen-wide** darkening bug.
- HDR: `r_hdr 2` holds highlight range; `r_hdr 3` must **alias to 32F** (startup warning) without format crashes.
- Tonemap: `r_tonemap` Filmic vs AgX (`3`) both respond to `r_grade_toe` / `r_grade_shoulder` / `r_grade_whitePoint` / `r_grade_highlightDesat`.
- Grade/LUT: `r_post 1` + optional `r_grade_lut` does not black-screen; capture with `r_filmGrain 0` / `r_chromaticAberration 0`.
- Hybrid1 overlay (Tier B / RTX build): after `exec vulkan_overlay_hybrid1.cfg` + `vid_restart`, `hybrid1_status` reports without crashes; composite still readable.

## Cvars / notes

| Area | Suggested toggles |
|------|-------------------|
| AA | `r_ext_multisample`, `r_ext_smaa`, `r_postAaAfterBloom` |
| AO/bloom | `r_ssao`, `r_bloom`, `r_bloom_threshold` |
| HDR | `r_hdr` 0/1/2/3 (3 = alias to 2) |
| Tonemap | `r_tonemap` 0–4; AgX = 3 |
| Grade | `r_grade_*`, `r_grade_lut`, `r_post` |
| Golden-safe | `r_filmGrain 0`, `r_chromaticAberration 0` (`gpu_golden_capture.cfg`) |
| Hybrid1 | `exec vulkan_overlay_hybrid1.cfg` (needs `USE_VULKAN_RTX`) |

Run toggles in one session and record order. See [GPU_GOLDEN_TIER_B.md](../../../GPU_GOLDEN_TIER_B.md).
