# How Dark is Dark — black materials BRDF scaffold

Filip & Vávra, *How Dark is Dark? A Reflectance and Scattering Analysis of Black Materials* ([arXiv:2601.05094](https://arxiv.org/abs/2601.05094), 2026).

This module is a **calibrated research scaffold**: summary metrics and rankings from the paper (Figs. 4–6, 8), console material-selection guidance, and an optional analytic PBR demo. It does **not** load measured Figshare EXR BRDFs or replace GGX metalness/roughness shading.

## Toggle

| Cvar | Default | Purpose |
|------|---------|---------|
| `cl_howdark_enable` | `0` | Gate console commands; startup log when research profile is built |

## Build

```bash
./scripts/compile_engine.sh vulkan full    # or research
ctest -R unit_howdark -V
./tests/scripts/test_howdark.sh build-vk-Release
```

Requires **`USE_RESEARCH_EXTENSIONS=ON`** (`full` / `research` profiles).

## Model mapping (paper → engine)

| Paper | Engine |
|-------|--------|
| Six materials (§3.1) | ids 0–5: Vantablack, Musou paint, black velvet, Musou fabric, acrylic, chalkboard |
| Effective albedo \(A\) (Fig. 4) | `HowDark_Albedo` — fabrics/ultra ≪ coatings (~10×) |
| Luminance P1/P50/P99 (Fig. 4) | `HowDark_LuminancePercentiles` |
| THR / TIS / \(R_s\) vs \(\theta_i\) (Fig. 6) | `HowDark_THR` / `HowDark_TIS` / `HowDark_Specular` |
| Perceived darkness @ I=1/10/100 (Fig. 8) | `HowDark_PerceivedDarkness` / `HowDark_RankByDarkness` |
| Material selection (§6) | `HowDark_SelectAdvice` |

Relative constants reproduce **ordering and magnitude trends**, not raw gonometric samples.

## Console

```
set cl_howdark_enable 1
howdark_paper
howdark_list
howdark_metrics vantablack
howdark_rank 100
howdark_compare velvet acrylic
howdark_advice stray
howdark_status
```

| Command | Role |
|---------|------|
| `howdark_paper` | Citation + one-line conclusions |
| `howdark_list` | Materials + class + albedo |
| `howdark_metrics <id\|name>` | Albedo, percentiles, THR/TIS/\(R_s\) samples, darkness scores |
| `howdark_rank [1\|10\|100]` | Perceived-darkness ranking (Fig. 8) |
| `howdark_compare a b` | Side-by-side metrics |
| `howdark_advice [optical\|calibration\|stray\|aesthetic]` | §6 selection guidance |
| `howdark_status` | Enable flag + material count |

## Demo

```
exec demo_howdark.cfg
```

Enables the module and prints paper / rank output. Visual appearance notes in the cfg are **analytic PBR proxies** (dark albedo + high roughness; velvet sheen), not paper Fig. 7 measured-BRDF renders.

## Validation

`unit_howdark` checks:

- Ranking at intensity 100: Musou fabric / Vantablack darkest; acrylic / chalkboard brightest
- Fabric/ultra albedo ≪ acrylic (~10×)
- Velvet mean TIS ≤ coating TIS
- Advice strings non-empty

## Limitations

- Not a measured BRDF evaluator; no Figshare EXR ingest in the build
- No psychophysical slider UI
- Analytic demo is an appearance proxy only
- No changes to Unified Clustered / Forward+ lighting ownership

## Follow-up (separate plan)

HDR measured BRDF texture + isotropic GPU lookup + `r_howdarkMeasured` debug mode (engine has `R_LoadEXR_HDR` but no BRDF table wiring today).

## References

- Filip & Vávra, arXiv:2601.05094 — BRDF, TIS, rendering, psychophysics
- Supplementary BRDF EXRs / rendering code: Optica Dataset / Figshare (external)
- Engine PBR baseline: [PBR_TEXTURES.md](PBR_TEXTURES.md)
