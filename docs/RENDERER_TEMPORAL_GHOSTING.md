# First-Person Weapon Temporal Ghosting

**Status:** First screen-space offender fixed with weapon-after-world-post isolation; residual non-temporal silhouette noted.  
**Date:** 2026-07-20  
**Constraint:** No broad renderer algorithm fixes until the first pass is confirmed (this document). Debug toggles and views are in place.

## Symptoms

Observed on Surf (`fs_game surf`, `surf_aztec`) and similar first-person views:

- Dark translucent trails extend behind the weapon silhouette.
- Thin geometry (rails / sights) shows stacked “echo” copies.
- Nearby world pixels look contaminated by previous weapon positions.
- The weapon MD3 itself is valid (confirmed offline; geometry is not duplicated in the asset).

## Runtime state of the Surf repro (measured)

| Consumer | Typical Surf value | Notes |
|---|---|---|
| `r_taa` | **0** | World TAA / Temporal Reconstruction **off** |
| `r_aaMode` | **0** | No SMAA / present adaptive recon |
| `reconstruction` | **no** | `vk_temporal_reconstruction_wanted() == false` |
| `weaponAfterTaa` | no | Deferral idle because reconstruction is off |
| `r_ssr` | **1** (Surf `surf.cfg`) | Screen-space reflections **on** unless gated |
| `r_bloom` / `r_motionBlur` / `r_oit` | 0 | Off in the measured session |

**Important:** With reconstruction off, this cannot be classic TAA history ghosting until `r_taa` / `r_aaMode` 3–5 / upscale temporal is enabled.

## Pass order (relevant slice)

From `vk_end_frame` (`vk_frame_submit.c`):

Legacy order:

1. World + first-person weapon draws complete (`backEnd.doneSurfaces`).
2. Weapon uses `DEPTH_RANGE_WEAPON` → viewport depth **[0.6, 1.0]** (reverse-Z near), see `vk_view_state.c`.
3. **`vk_ssr_pass()`** samples the shared color + depth, including weapon depth-hack values.

Corrected order (`r_weaponSsrIsolation 1`, default):

1. World draws complete.
2. RDF_NOWORLDMODEL weapon draw command is deferred.
3. SSR and other world post passes run without weapon color/depth.
4. Existing deferred-weapon pass composites the weapon after world post.
5. Luminance/presentation continue from the weapon-composited HDR source.

## Bisect results

### A. First screen-space offender when SSR is enabled

Clean-homepath A/B (`exec ghost_on.cfg` → `exec ghost_off.cfg`):

| Step | State | Result |
|---|---|---|
| SSR on | `r_ssr 1` + `r_temporalSSR 1`, `r_taa 0` | Soft dark contamination around weapon; floor/gun region changes |
| SSR off | `r_ssr 0` + `r_temporalSSR 0` | Mean \|Δ\| ≈ **28** in gun region, ≈ **18** on floor — SSR was rewriting those pixels |

**First pass that introduces screen-space weapon→world contamination: `vk_ssr_pass` / `ssr.frag`.**

Mechanism:

1. First-person weapon writes depth with `DEPTH_RANGE_WEAPON` ([0.6, 1.0]).
2. SSR reconstructs normals from that depth and marches reflections.
3. Hits that involve weapon depth / weapon color paste dark gun-shaped energy onto nearby floor/wall pixels.
4. View motion changes which pixels get those hits → reads as trails / ghost copies, especially on thin rails.

### Structural correction

The renderer now uses Architecture B for SSR:

- `r_weaponSsrIsolation 1` (default) defers the first-person view weapon whenever SSR is live.
- The existing `r_temporalWeaponAfterTaa` path remains responsible for TAA/TSR isolation.
- Both policies converge on the same deferred weapon composite after world post.
- The deferred path queues every RDF_NOWORLDMODEL draw command. Surf emits two
  commands per frame (2 surfaces, then 1 surface); the old single-command slot
  silently overwrote the first command.
- `r_weaponSsrIsolation 0` restores legacy ordering for A/B comparison.

This does not infer weapon identity from raw reversed-Z values, so nearby world geometry cannot be accidentally rejected by an unsafe `depth > 0.6` heuristic.

Rotating-camera validation (`weapon_ssr_rotate_isolated.cfg`, 36 captured
frames): 72 commands were deferred and all 36 flushes replayed exactly two
commands; no queue overflow, unknown command, build error, or linter error.

### B. Residual silhouette with all temporal / post consumers off

`ghost_safe.cfg` forced:

`r_ssr 0`, `r_temporalSSR 0`, `r_ssao 0`, `r_temporalAO 0`, `r_taa 0`, `r_aaMode 0`, `r_tsr 0`, bloom/motionBlur/DoF/sharpen/fog/transparency off, `r_pbr 0`.

**Result:** Soft dark under-gun banding / rail “echo” **still readable** in screenshots. Quantitative luma strip under the gun shows only ~2 strong dark features (not a long temporal history ladder).

So after SSR is gated, remaining “duplication” is **not** explained by TAA/SSR/SSAO/bloom. Leading hypotheses for the residual (not fixed yet):

1. Surf disables `r_firstPersonFovEnabled` / `r_firstPersonScaleEnabled` and uses a very small `r_firstPersonZNear` → extreme close-up perspective on multi-part MD3 (hand + weapon + barrel + sight).
2. Same-frame multi-surface draws (rail / barrel ribs) misread as ghosts.
3. A still-ungated main-pass effect (to be bisected next with `r_temporalDebug` once reconstruction is intentionally enabled).

### C. TAA path (not active in Surf default)

When `r_taa 1` or `r_aaMode` 3–5 is enabled, use `r_temporalDebug` 1–6 and confirm `r_temporalWeaponAfterTaa` defers the weapon until after world history.

Note: `taa.frag` currently samples **current** depth at the reprojected UV for `histDepth` — there is no true previous-depth RT yet. Debug mode “previous-frame depth” is therefore approximate.

## Debug views (`r_temporalDebug`)

| Value | View |
|---|---|
| 0 | Off |
| 1 | Final motion vectors (velocity) |
| 2 | Depth / history rejection |
| 3 | Temporal history weight |
| 4 | Disocclusion / reactive mask |
| 5 | Weapon mask (TAA near-weapon, or SSR weapon-range depth overlay when TAA off) |
| 6 | Current vs history contribution |
| 7 | History UV |
| 8 | World vs reactive ownership |
| 9 | Adaptive sample mask |
| 10 | Current vs history (alias) |
| 11 | Neighborhood variance |
| 12 | History delta |
| 13 | NaN / Inf detection (magenta) |
| 14 | Weapon-only motion vectors |
| 15 | World-only motion vectors |
| 16 | Current depth |

When reconstruction is **off**, modes 5 / 2 / 4 / 13 also run through the SSR pass as overlays (weapon depth tinted on the scene).

Legacy aliases: `r_debugHistoryRejection`, `r_debugMotionVectors` (still honored; override packing when set).

Commands:

```
temporal_status
temporal_ghost_status
```

## Independent subsystem gates

Each can be disabled without masking via blur:

| Cvar | Default | Effect |
|---|---|---|
| `r_taa` | 0 | Classic Temporal Reconstruction |
| `r_tsr` | 1 | Present adaptive / aaMode 3–5 / upscale temporal |
| `r_temporalAO` | 1 | SSAO pass |
| `r_temporalSSR` | 1 | SSR (even if `r_ssr 1`) |
| `r_weaponSsrIsolation` | 1 | Composite the view weapon after world SSR |
| `r_temporalFog` | 1 | Volumetric froxel history weight |
| `r_temporalTransparency` | 1 | OIT / transparent reactive stamp |
| `r_motionBlur` | 0 | Camera motion blur |
| `r_dof` / `r_depthOfField` | 0 | Thin-lens DoF |
| `r_bloom` | 0 | Bloom |
| `r_sharpen` | 0 | Sharpen |

Recommended bisect while moving / turning:

```
temporal_ghost_status
set r_weaponSsrIsolation 0   # legacy SSR contamination A/B
set r_weaponSsrIsolation 1   # corrected weapon-after-world-post order
set r_temporalDebug 5        # weapon depth mask overlay (with SSR path live)
set r_taa 1                  # only after SSR gated — check TAA separately
```

Example cfgs live under `release/surf/ghost_on.cfg` and `ghost_off.cfg` / `ghost_safe.cfg`.

## What was intentionally not changed

- No raw-depth SSR hit-reject was added; ordering isolates weapon depth structurally.
- No TAA history / motion-vector broad fix.
- No blur / excessive confidence clamps as a cosmetic mask.
- `modern_vulkan.cfg` boot defaults untouched.

## Next fix candidates (when approved)

1. Verify `r_weaponSsrIsolation 1` during deterministic camera rotation.
2. **Residual:** re-enable sane first-person projection for Surf (`r_firstPersonFovEnabled` / scale) and/or audit multi-part view-weapon draws.
3. Enable `r_taa 1` and verify weapon deferral + debug views 1–6 separately.

## Related

- `docs/MOMENT_OIT_STOCHASTIC_ALPHA.md` — prior OIT / distortion corruption work.
- `renderers/vulkan/vk_view_state.c` — `DEPTH_RANGE_WEAPON`.
- `renderers/vulkan/shaders/glsl/ssr.frag` — reflection march + debug overlays.
- `renderers/vulkan/vk_temporal.c` — ownership / deferred weapon policy / `temporal_ghost_status`.
