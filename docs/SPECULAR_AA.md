# Specular Anti-Aliasing (Toksvig + Geometric Roughness)

**Status:** Foundation Consolidation — shipping baseline for PBR specular stability.  
**Source:** `pbr_brdf_core.glsl` (`PbrSpecularAARoughness`) · **Debug:** `vk_shading_compare.c`

---

## Ownership

| Component | Owner | Applied in |
|-----------|-------|------------|
| Toksvig variance inflate | `pbr_brdf_core.glsl` | Forward+, deferred, gen_frag |
| Geometric roughness floor | `deferred_lighting_common.glsl`, `gen_frag.tmpl` | Screen-space normal deriv |
| Strength / kill switch | `r_pbr_specularAA`, `r_pbr_specularAAStrength` | CPU push constants |
| Frequency-aware boost | `vk_frequency_aware.c` | May raise strength per-tile |

No LEAN map in Foundation Consolidation sprint — deferred uses normal variance path only.

---

## Data flow

```text
Normal map + screen-space dN/dx,dN/dy
  → variance estimate
  → PbrSpecularAARoughness(roughness, variance, geoVariance, strength)
  → inflated roughness → GGX specular lobe
r_specularAADebug 1–6 → heatmaps / path split / side-by-side
```

Push constant `specularAA` in deferred lighting compute matches Forward+ `ApplySpecularAA` parity.

---

## Buffer formats

No dedicated specular AA buffer. Debug modes write false-color to SceneHDR or split viewport. Variance computed from G-buffer normals or tangent-space N in forward path.

---

## Lifecycle

- Init: `r_pbr_specularAA` default **1**, strength **0.5** (`tr_init.c`).
- Per draw / tile: strength clamped 0–2; frequency-aware may override locally.
- Disable: `r_pbr_specularAA 0` — zero push constant, raw authored roughness.

---

## Fallback behavior

- Classic non-PBR materials — specular AA skipped entirely.
- `r_pbr_specularAA 0` — kill switch, no global roughness rewrite.
- Deferred compact path — uses same variance block when normals available.
- Ultra LEAN (experimental) — separate deferred-only path; not Foundation default.

---

## Debug commands

| Cvar | Role |
|------|------|
| `r_specularAADebug` | 0 off, 1 variance heat, 2 roughness inflate, 3 normal deriv, 4 deferred, 5 Forward+, 6 side-by-side |
| `r_pbr_specularAA` | Enable / disable |
| `r_pbr_specularAAStrength` | 0–2 scale |
| `shading_compare_status` | Reports specular AA cvar state |

---

## Performance cost

Toksvig block is **~10 ALU** per light evaluation — negligible vs texture fetches. Debug overlays add optional full-screen pass when cheat enabled.

---

## Known limitations

- No LEAN / specular antialiasing map in Foundation Consolidation scope.
- Anisotropic materials use separate lobe — Toksvig applied to isotropic roughness only.
- Weapon / Surf paths inherit same core when PBR active.
- `r_specularAADebug` requires cheat; not available in competitive configs.

---

## Next milestone hooks

- Capture variance heat in reference lab rough-metal sweep.
- Tie `r_specularAADebug 6` to automated before/after screenshots.
- Optional LEAN deferred tier behind `r_pbr_specularAALean` (research profile).

Regression: `tests/scripts/test_specular_aa.sh`
