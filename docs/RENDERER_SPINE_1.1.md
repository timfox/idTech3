# Renderer Spine 1.1

**Status:** Implementation complete; **GPU certification pending** until the lifecycle soak passes on a display + Vulkan client.

Spine 1.1 certifies one opt-in stack on top of Spine 1.0 shipping defaults. It does **not** change boot (`modern_vulkan.cfg` → [`modern_vulkan_stable.cfg`](../config/modern_vulkan_stable.cfg): Forward+ mode **2**, SMAA, GTAO, TAA/OIT off).

## Audit notes (ownership)

- Frame order is already correct: mode-3 opaque → OIT resolve into HDR → late post (SSR/bloom) → TAA → weapon flush → luminance → gamma → UI.
- Resolved-OIT-only as a **buffer** contract is met (moments/accum never bind as TAA history); reactive stamp affects blend weight only.
- Weapon/world separation is **temporal ordering**, not dual history buffers — presentation-only weapon; no `weapon_history` resource.
- Soft-demote without weapon-after remains; the former every-frame “experimental OIT×TAA” violation is cleared for the certified combo.

## Certified combination

| Pin | Value |
|-----|-------|
| `r_renderMode` | **3** (Unified Clustered) |
| `r_oit` | **1** (WBOIT) |
| `r_aaMode` / `r_taa` | **4** / **1** (Temporal Reconstruction) |
| `r_temporalWeaponAfterTaa` | **1** |
| `r_temporalSmaaCleanup` | **0** |
| Bloom / eye adapt | `r_bloom 1`, `r_exposure_auto 1` |
| Asserts | `r_spineCert 1`, `r_spineValidate 2` |

Entry points:

```text
exec vulkan_overlay_spine_1_1_cert.cfg
vid_restart
# or:
renderer_spine_1_1_cert
vid_restart
```

Recovery: `exec modern_vulkan.cfg`

## Key invariants

1. **TAA may consume only fully resolved world color** — never raw OIT accum, revealage, or moments.
2. **Weapon renders after world Temporal Reconstruction** (presentation-only; no weapon history buffer).
3. **OIT resources are single-frame** and must never be treated as temporal history.
4. **Recreated attachments invalidate and rebuild every dependent descriptor** (`vk_spine_note_descriptors_rebound` after `vk_update_attachment_descriptors`).
5. **Skipped OIT** leaves HDR `color_image` as the valid scene-color producer.
6. **Resize / `vid_restart` / focus / profile switches** invalidate incompatible history (sticky temporal resets + `vk_spine_cert_check_history_invalidated`).

## Pass order (cert path)

```text
mode3 opaque deferred → WBOIT accum → OIT resolve → HDR
→ SSR / bloom (as enabled) → Temporal Reconstruction (TAA)
→ weapon flush → luminance / eye adapt → tonemap/gamma → UI overlay
```

## Unsupported / experimental (not certified)

| Stack | Behavior |
|-------|----------|
| OIT + TAA without weapon-after | Soft-demote world TAA + violation |
| MBOIT (`r_oit 2`) + TAA | Experimental warn; not cert |
| WBOIT + TAA without mode3/aaMode4 pins | Experimental warn; no perpetual fail |
| Stochastic alpha | Off for cert; remains experimental |
| AV mode 4 / RT / Hybrid1 | Out of scope |

## Automation

| Check | Command |
|-------|---------|
| Static contracts | `./scripts/spine_1_1_cert_check.sh` (also via `spine_stability_check.sh` / regression) |
| GPU soak | `spine_1_1_stress [vid_restarts] [resizes] [focus] [maps]` then `spine_1_1_stress_report` |
| Optional script | `./scripts/spine_1_1_lifecycle_stress.sh` (SKIP 77 without client/display) |

Certification **fails** on: validation / ownership violations, black-frame streak, NaN luminance, stale attachment/descriptor generation, resource growth past baseline, unrebound descriptors after recreate, history still valid after lifecycle reset, or DEVICE_LOST.

Full GPU bar (doc “certified”): ≥20 `vid_restart`, ≥50 resize, ≥20 focus pulses, ≥20 map changes when data available, plus a longer camera soak (`IDTECH3_SPINE_SOAK=1` when wired). Until that run is green, keep status as **GPU cert pending**.

## Related

- [RENDERER_SPINE_1.0.md](RENDERER_SPINE_1.0.md) — shipping spine
- [UNIFIED_CLUSTERED_RENDERER.md](UNIFIED_CLUSTERED_RENDERER.md) — mode 3
- [MOMENT_OIT_STOCHASTIC_ALPHA.md](MOMENT_OIT_STOCHASTIC_ALPHA.md) — OIT
- [HDR_GAPS.md](HDR_GAPS.md) — late-post order
