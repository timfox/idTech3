# Raster Ultra 2.1 — Spatial-First Cinematic Rendering (Slice A)

**Candidate profile:** `config/modern_raster_cinematic.cfg` — **not** the boot default.  
**Recovery:** `exec modern_vulkan.cfg; vid_restart`  
**Certified fallback:** `modern_vulkan.cfg` (mode 2) and `modern_raster_ultra.cfg` / `modern_raster_ultra_2.cfg` remain unchanged.

Ray tracing and TAA remain **locked off** for this milestone. Slice A delivers the no-TAA image foundation only. Slices B–E (geometry, shadows, lighting, environments, USD workflow) are **not** started here.

## Slice A deliverables

| Piece | Status |
|-------|--------|
| Unified spatial AA controller (`vk_spatial_aa`) | **shipped** |
| Current-frame adaptive supersample (`spatial_adaptive_ss.frag`) | **shipped** (risk-classified, history-free) |
| SMAA residual edge cleanup | **certified** (`r_aaMode 2`) |
| Frequency-aware moiré path + selective SS responses | **wired** (highest no-TAA defect) |
| Selective MSAA architecture | **policy scaffold** (`r_spatialAaSelectiveMsaa`; deferred G-buffer stays 1×) |
| Spatial reference (4×/8×) | **opt-in** via `r_aaMode 6` / reference lab — not forced |
| `modern_raster_cinematic.cfg` | **candidate** |

## Spatial AA pipeline (cinematic)

```
stable content filtering (aniso / Toksvig / CorrectAlpha / mip floor)
  → current-frame adaptive SS (high-risk pixels only → TAA history[0] scratch)
  → SMAA 1x residual edge cleanup
  → HDR post / UI
```

No previous-frame color. No temporal history weight. Adaptive output uses `taa_history[0]` as **current-frame scratch** only while TAA is off.

### Surface policy (Slice A)

| Surface | Methods |
|---------|---------|
| Opaque deferred | Frequency-aware materials + adaptive SS + SMAA |
| Forward / transparent | WBOIT + alpha coverage; SMAA on resolve |
| Weapon | Forward after world AA (existing Ultra path); no world history |
| Alpha-tested foliage | CorrectAlpha + optional A2C when MSAA on (MSAA opt-in, not default for mode 3) |
| UI | Output resolution; no world jitter / AA history |

### Adaptive classifier (v0)

Inputs: luma gradient, depth gradient, risk threshold, sample budget, quality tier.  
Response: edge-aware neighborhood (cross + optional diagonals). Low-risk pixels pass through.  
Debug: `r_spatialAaDebug 1` = risk heat.

## Ownership (candidate)

| Signal | Owner |
|--------|--------|
| Opaque lighting | Mode 3 deferred (via ultra_2 / clustered) |
| Transparent | WBOIT + Forward+ |
| AA | `spatial_aa` → adaptive SS + SMAA |
| Temporal | **none** |
| RT | **none** |
| Diffuse GI | lightmaps + raster_gi (from ultra_2) |

## Commands / cvars

- `spatial_aa_status`
- `r_spatialAa` (latched), `r_spatialAaTier`, `r_spatialAaAdaptiveSS`, `r_spatialAaRisk`, `r_spatialAaBudget`
- `frequency_aware_status`, `renderer_sampler_status`

## Promotion table (after Slice A)

| Subsystem | Class |
|-----------|--------|
| SMAA zero-history | **Raster Ultra certified** |
| Frequency-aware (aniso / specular ND / alpha coverage) | **cinematic-profile certified** (with spatial AA) |
| Current-frame adaptive SS | **quality opt-in** (candidate; needs measured soak) |
| Selective MSAA | **experimental** (policy only) |
| Spatial reference 4×/8× | **quality opt-in** |
| Meshlet HLOD / virtual shadows / LTC / water / USD live-edit | **not in Slice A** |

## Validation (static)

`scripts/raster_ultra_2_1_check.sh` — profile locks, no TAA/RT, controller + shader present, boot unchanged.

Runtime soak, image metrics, and GPU timings are **not invented** here.

## Next defect after Slice A

Continue measuring no-TAA aliasing with cinematic profile; prefer fixing the largest remaining defect (texture moiré residuals, alpha shimmer, or WBOIT BRDF parity) before Slice B geometry work.
