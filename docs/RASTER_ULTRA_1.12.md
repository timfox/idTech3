# Raster Ultra 1.12 — Frequency-Aware Rendering + Moiré Suppression

Continuation of [RASTER_ULTRA_1.11.md](RASTER_ULTRA_1.11.md). **Ray tracing stays off.** Does **not** solve moiré with global blur, forced TAA, FXAA spam, or indiscriminate detail loss.

**Certification:** experimental / opt-in. Boot remains `modern_vulkan.cfg`. SMAA (`r_aaMode 2`) stays the certified zero-history AA path.

## Enable

Configs must be on the game search path (`release/openarena/`, `release/base/`, …). `r_frequencyAware` is latched.

**Shell (prefer `keep_window` on Wayland — avoids libdecor destroy crashes):**
```
./release/idtech3 +set fs_game openarena \
  +exec modern_raster_ultra.cfg \
  +exec vulkan_overlay_frequency_aware.cfg \
  +vid_restart keep_window
```

In-console: `exec modern_raster_ultra.cfg` → `exec vulkan_overlay_frequency_aware.cfg` → `vid_restart keep_window`.

Confirm: `[VK][FrequencyAware] enabled`, then `frequency_aware_status` / `renderer_sampler_status`.

Do **not** stack plain `+vid_restart` (destroy window) with OpenArena auto-profile or SDL resize handling — that used to loop (`RE_Shutdown( 2 )`) when archived `r_customHeight` (e.g. 1011) fought FitWindowedSize (1010). Prefer `vid_restart keep_window`.
## Core principle

Moiré is undersampling. Classify the earliest producing signal, then apply the **smallest correct** filter.

| Class | Typical mitigation |
|-------|-------------------|
| Texture | Correct mips, anisotropy, mip-bias floor |
| Normal / specular | Toksvig-style NDF variance (not global roughness) |
| Alpha | Coverage-preserving threshold (`CorrectAlpha`) |
| Procedural | Octave / frequency cutoff (policy) |
| Geometry | Variance from vertex normals; thin-geo catalog |
| Shadow / water | Decorrelate / spectrum cutoff (policy flags) |
| Reconstruction | Do not lock moiré into history (TAA stays off by default) |

## What shipped in 1.12

| Item | Status |
|------|--------|
| Frequency controller + tiers | **yes** (`vk_frequency_aware`) |
| Moiré scene catalog (20) | **yes** (`frequency_aware_scenes`) |
| Sampler audit command | **yes** (`renderer_sampler_status`) |
| Anisotropy policy + mip-bias floor | **yes** |
| Toksvig-style specular AA (Forward+ / deferred) | **yes** |
| Geometric normal variance | **yes** |
| Coverage-preserving alpha (opt-in) | **yes** |
| Material IR frequency metadata | **yes** |
| Selective current-frame SS | **scaffolded, default off** |
| Stochastic complete-material filter | **scaffolded, default off** |
| Full GPU tile FFT diagnostic | **not in production path** |
| Signal-specific offline mip bake | **documented; CPU mips remain** |

## Tiers (`r_frequencyTier`)

| Tier | Behavior |
|------|----------|
| 1 Low | mips/aniso, basic specular AA, alpha coverage |
| 2 Medium (overlay default) | + procedural/water/shadow policy flags |
| 3 High | + selective SS eligible |
| 4 Ultra | + stochastic eligible |
| 5 Reference | pair with Reference Lab spatial SS |

## Debug

- `r_frequencyDebug` 0–20 (modes documented in `frequency_aware_status`)
- `r_colorMipLevels` (existing mip tint)
- `r_pbr_debug` (material channels)

## Validation

```
./scripts/raster_ultra_1_12_check.sh
```

Manual: oblique floors, fences/grates, brushed metal, water at distance, SMAA-only (no TAA), `renderer_sampler_status`, compare vs Reference Lab spatial SS when available.

## Promotion decision

| Mitigation | Class |
|------------|--------|
| Anisotropy + mip-bias floor | **quality opt-in** |
| Toksvig specular AA strengthen | **quality opt-in** (builds on certified `r_pbr_specularAA`) |
| Coverage-preserving alpha | **quality opt-in** |
| Selective SS / stochastic | **experimental** (default off) |
| Promote to Ultra default | **no** — overlay only |
| Boot unchanged | **yes** |

## Highest-impact fix

Aggressive negative mip LOD bias + missing anisotropy on grazing floors were the largest reproducible texture-moiré drivers. Ultra 1.12 clamps bias while active and certifies high anisotropy without enabling TAA.
