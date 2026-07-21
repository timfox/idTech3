# First-Person Weapon Temporal Ghosting

**Status:** Structural fix verified. SSR/SSAO isolation + Surf first-person presentation defaults are in place.  
**Date:** 2026-07-21  
**Constraint:** Smallest structurally correct fix first; no blur/clamp masks.

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
| `r_aaMode` | **2** | SMAA (spatial only; not temporal reconstruction) |
| `reconstruction` | **no** | `vk_temporal_reconstruction_wanted() == false` |
| `weaponAfterWorldPost` | **yes** (default) | `r_weaponSsrIsolation 1` + SSR/SSAO live |
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
3. SSR/SSAO and other world post passes run without weapon color/depth.
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

The renderer now uses Architecture B for SSR/SSAO:

- `r_weaponSsrIsolation 1` (default) defers the first-person view weapon whenever SSR **or** SSAO is live.
- The existing `r_temporalWeaponAfterTaa` path remains responsible for TAA/TSR isolation.
- Both policies converge on the same deferred weapon composite after world post.
- The deferred path queues every RDF_NOWORLDMODEL draw command. Surf emits two
  commands per frame (2 surfaces, then 1 surface); the old single-command slot
  silently overwrote the first command.
- `r_weaponSsrIsolation 0` restores legacy ordering for A/B comparison.

This does not infer weapon identity from raw reversed-Z values, so nearby world geometry cannot be accidentally rejected by an unsafe `depth > 0.6` heuristic.

### Verification (2026-07-21)

Same-pose A/B with the multi-command queue (`r_weaponSsrIsolation 0` → `1`):

| Region | Mean \|Δ\| |
|---|---|
| Full frame | 4.73 |
| Gun crop | **16.16** |
| Floor-left (control) | 0.63 |

Isolation changes the gun silhouette region while leaving distant floor nearly unchanged. Live status with isolation on:

`reconstruction OFF + SSR ON + isolation ON` → `weaponAfterWorldPost=yes`.

Static gate: `scripts/temporal_ghost_check.sh` (`ctest -R test_temporal_ghost`).

### B. Residual silhouette was first-person projection, not temporal

Surf previously archived:

`r_firstPersonFovEnabled 0`, `r_firstPersonScaleEnabled 0`, `r_firstPersonZNear 0.125`

That placed the MD3 millimetres from the camera at scene FOV, which made the rear of the machinegun look stretched / “duplicated.” Defaults are now:

| Cvar | Value | Where |
|---|---|---|
| `r_firstPersonFovEnabled` | 1 | `release/surf/surf.cfg`, `autoexec.cfg` |
| `r_firstPersonScaleEnabled` | 1 | same |
| `r_firstPersonFov` | 65 | same |
| `r_firstPersonZNear` | 4 | same |

`release/surf/autoexec.cfg` also sets `com_nativeLibraryExtractPk3 0` so the loose
`release/surf/vm/game.x86_64.so` wins over the older `game.so` embedded in
`openarena.pk3` (pk3 extract was aborting in `PM_GroundTrace` on BSP30 maps).

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
| `r_weaponSsrIsolation` | 1 | Composite the view weapon after world SSR/SSAO |
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
- No TAA history / motion-vector broad fix beyond the existing weapon-after-TAA path.
- No blur / excessive confidence clamps as a cosmetic mask.
- `modern_vulkan.cfg` boot defaults untouched.

## Related

- `docs/MOMENT_OIT_STOCHASTIC_ALPHA.md` — prior OIT / distortion corruption work.
- `scripts/temporal_ghost_check.sh` — static regression gate.
- `renderers/vulkan/vk_view_state.c` — `DEPTH_RANGE_WEAPON`.
- `renderers/vulkan/shaders/glsl/ssr.frag` — reflection march + debug overlays.
- `renderers/vulkan/vk_temporal.c` — ownership / deferred weapon policy / `temporal_ghost_status`.
- `engine/core/cm_trace.c` — BSP30 recursion depth guard (`BSP30_MAX_TRACE_DEPTH`).
