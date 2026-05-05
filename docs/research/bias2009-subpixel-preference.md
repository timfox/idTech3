# Bias et al. (2009) — subpixel text preference and individual differences

**Citation:** Randolph G. Bias, Kevin Larson, Sheng-Cheng Huang, Paul R. Aumer-Ryan, Chris Montesclaros. *An Exploratory Study of Visual and Psychological Correlates of Preference for Onscreen Subpixel Rendered Text.* Journal of the American Society for Information Science and Technology (JASIST). DOI: [10.1002/asi.21273](https://doi.org/10.1002/asi.21273).

## Summary (engine-relevant)

- **Majority preference:** Most participants preferred ClearType-style subpixel variants over strict black-and-white bitmap-style edges, but preference was **not unanimous**—a consistent minority preferred black-and-white.
- **Color filtering trade-off:** “Grey scale” / default-style filtering (more luminance error, less chromatic fringe) was generally **more acceptable** than strongly “colorful” variants that maximize spatial accuracy but show **visible RGB fringing**—especially with **Verdana** and **Times New Roman**. **Consolas** (designed for subpixel) interacted more favorably with ClearType variants.
- **Hue / color sensitivity:** Higher hue discrimination tended to correlate with **less** preference for subpixel rendering (more notice of color artifacts). This supports exposing **subpixel-like tweaks as an optional toggle**, not a forced default.
- **Visual acuity:** Pilot partial correlations suggested people with **better acuity** leaned toward variants with **more** chromatic edge energy; people with **stronger color sensitivity** leaned toward **less** chromatic error—consistent with a luminance-vs-chroma trade-off in the display pipeline.
- **Personality (exploratory):** Among “ClearType haters” re-tested in Study 3, agreeableness and other traits were explored; sample sizes and **preference flip-flops** between sessions limit strong claims. The paper argues for **future individualized rendering** based on simple vision/personality probes.

## Relation to this engine

- **`r_fontSubpixel`** is a **small horizontal placement nudge** (0.375 px after projection) for FreeType-drawn console/HUD text, not full LCD RGB subpixel filtering or ClearType-style channel shaping.
- Empirical takeaway for users: if text looks “off” on your panel, try **`r_fontSubpixel 0`** first; if edges look too soft with linear filtering, try **`1`**. Preference and display geometry vary—as in Bias et al.’s ClearType studies.
