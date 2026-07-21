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
| `r_taa` | **1** | World TAA / Temporal Reconstruction **on** (shipping Surf profile) |
| `r_aaMode` | **4** | Native Temporal Reconstruction |
| `reconstruction` | **yes** | `vk_temporal_reconstruction_wanted() == true` |
| `weaponAfterWorldPost` | **yes** (default) | `r_weaponSsrIsolation 1` + SSR/SSAO live |
| `r_ssr` | **1** (Surf `surf.cfg`) | Screen-space reflections **on** unless gated |
| `r_bloom` / `r_motionBlur` / `r_oit` | 0 | Off in the measured session |

**Important:** Surf now boots through the Temporal Weapon Resolve path. The
zero-history renderer remains supported only through explicit comparison cfgs
such as `ghost_safe.cfg`; it is not the normal startup profile.

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

Old Surf archives could retain `r_firstPersonFovEnabled 0` and
`r_firstPersonZNear 0.125`. The renderer now warns once and migrates that exact
legacy combination instead of silently using it. The authoritative projection
inputs in `release/surf/surf.cfg` are:

| Input | Surf value | Effective meaning |
|---|---:|---|
| `cg_fov` | 90 | Nominal world horizontal FOV |
| `r_firstPersonFovEnabled` | 1 | Select custom weapon projection |
| `r_firstPersonFov` | 65 | Weapon horizontal FOV, degrees |
| `r_firstPersonZNear` | 4 | Weapon near plane, world units (clamped 0.01–8) |
| `r_firstPersonScaleEnabled` / `r_firstPersonScale` | 1 / 1.0 | Enabled, identity model-view scale |

`r_printViewmodelProjection`, captured on `surf_aztec` at 1280×720 after the
Architecture-B deferred weapon draw, reported:

```text
effective weapon FOV  : 65.000 deg horizontal
effective world FOV   : 90.000 x 58.733 deg (horizontal x vertical)
aspect-adjusted FOV   : 39.444 deg vertical (from 65.000 deg horizontal)
z-near / z-far        : 4.000 / 10043.434
projection mode       : custom horizontal weapon FOV (r_firstPersonFovEnabled=1)
reversed-Z state      : enabled (Vulkan 0..1 clip depth)
depth-range remap     : DEPTH_RANGE_WEAPON [0.600, 1.000]
jitter state          : current=(0.0000, 0.0000) px appliedToWeapon=no
previous-frame values : weaponFov=65.000 x 39.444 worldFov=90.000 x 58.733 z=4.000/dynamic
```

Weapon z-far has no independent cvar: it shares the dynamically computed world
view z-far, so its numeric value changes with map bounds/view. Aspect
compensation is automatic from the effective world FOV pair; there is no
aspect cvar. `r_zproj` is the projection-plane construction distance and does
not change the resulting FOV. Handedness/model offsets and ADS are owned by the
cgame entity/refdef transform—there is no renderer-side handedness or ADS
projection override. An ADS world-FOV change therefore affects the world
projection while the configured 65-degree weapon projection remains stable.

Weapon motion capture selects the same effective
`backEnd.firstPersonProjectionMatrix` used for the current draw. Its previous
MVP therefore uses the exact 65-degree, z-near-4 projection rather than the
world projection.

`release/surf/autoexec.cfg` also sets `com_nativeLibraryExtractPk3 0` so the loose
`release/surf/vm/game.x86_64.so` wins over the older `game.so` embedded in
`openarena.pk3` (pk3 extract was aborting in `PM_GroundTrace` on BSP30 maps).

### C. TAA path (Surf default)

Surf explicitly sets `r_aaMode 4`, `r_taa 1`, `r_taaMotionVectors 1`,
`r_temporalReactiveMask 1`, `r_temporalWeaponAfterTaa 1`, and
`r_weaponTemporalMode 1`. It also sets `r_bloom 1` and
`r_weaponBloomMode 1` so combined world/weapon HDR enters bloom once. Use
`r_temporalDebug` 1–33 and confirm
`r_temporalWeaponAfterTaa` defers the weapon until after world history.
Mode 2 adds dedicated class-gated weapon bloom after the flush (`vk_weapon_bloom`).
Presentation policies (`r_weaponAnalyticFog`, `r_weaponLocalReflection`,
`r_weaponLocalAO`, `r_weaponReadabilityLight`, `r_weaponThinSightReject`) default
restrained/off; dump with `r_printWeaponPresentation`. Depth-reject GPU counters
print via `r_dumpTemporalState`.

`taa.frag` samples a persistent R32F previous-frame depth image at the
reprojected UV. Current depth is never substituted for unavailable history;
invalid depth history forces current-frame color.

## Debug views (`r_temporalDebug`)

`r_temporalDebug` is range-checked to **0–33** (0 = off; values outside the
range are clamped at cvar registration in `tr_init.c`).

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
| 14 | Pre-weapon world resolve velocity (weapon has not been drawn yet) |
| 15 | Prior-class-gated world-resolve velocity; not weapon MVP |
| 16 | Current temporal class (gray=world, blue=reserved sky, white=weapon) |
| 17 | Previous temporal class |
| 18 | Reprojected previous class |
| 19 | Class rejection (red=reject, green=accept) |
| 20 | World-only velocity |
| 21 | Actual post-draw weapon MVP velocity |
| 22 | Final merged velocity |
| 23 | Raw reactive mask |
| 24 | Dilated reactive mask |
| 25 | Weapon temporal confidence |
| 26 | Weapon history validity |
| 27 | Current weapon composition coverage |
| 28 | Current weapon depth |
| 29 | Previous weapon depth |
| 30 | Reprojected previous weapon depth |
| 31 | Absolute depth difference |
| 32 | Relative depth error |
| 33 | Final depth rejection |

When reconstruction is **off**, modes 5 / 2 / 4 / 13 also run through the SSR pass as overlays (weapon depth tinted on the scene).

Modes 16–33 run the post-weapon diagnostic resolve and display a one-line
overlay containing effective weapon/world FOV, depth range, jitter, previous-MVP
state, independent validity bits, and temporal frame ID. Velocity colors use
RG for signed XY, yellow for out-of-range values, and magenta for non-finite
data. `r_temporalDebugVectorScale` controls vector display scale.

Legacy aliases: `r_debugHistoryRejection`, `r_debugMotionVectors` (still honored; override packing when set).

Commands:

```
temporal_status
temporal_ghost_status
surf_validateTemporalConfig
r_printViewmodelProjection
r_dumpTemporalState
r_captureTemporalDebug
```

On `fs_game surf`, renderer startup prints the effective TAA, weapon history
mode, class/reactive targets, MVP velocity availability, previous-depth status,
and weapon composition stage. `surf_validateTemporalConfig` repeats the
resource and cvar checks with PASS/WARN/FAIL diagnostics. Safe mode and
zero-history comparison cfgs intentionally produce warnings until `surf.cfg`
is restored and the renderer is restarted.

Surf configuration layering:

- Renderer-wide defaults remain conservative (`r_taa 0`) for non-Surf games.
- `config.cfg` loads before Surf `autoexec.cfg`; the packaged autoexec mirrors
  the temporal resource cvars so archived values cannot prevent allocation in
  `R_Init`.
- `mapscripts/g_default.cfg` re-applies authoritative `surf.cfg` on ordinary
  map loads. No shipping map-specific cfg overrides the temporal values.
- Command-line `+exec` comparison cfgs can intentionally disable TAA after the
  shipping profile. `ghost_safe.cfg` is the supported zero-history comparison.
- Engine safe mode skips archived config and autoexec; `gfx_safe.cfg` also
  explicitly keeps TAA off as the renderer recovery path.

## Independent subsystem gates

Each can be disabled without masking via blur:

| Cvar | Default | Effect |
|---|---|---|
| `r_taa` | 0 | Classic Temporal Reconstruction |
| `r_tsr` | 1 | Present adaptive / aaMode 3–5 / upscale temporal |
| `r_temporalAO` | 1 | SSAO pass |
| `r_temporalSSR` | 1 | SSR (even if `r_ssr 1`) |
| `r_weaponSsrIsolation` | 1 | Composite the view weapon after world SSR/SSAO |
| `r_weaponTemporalMode` | 1 | TAA-on weapon history: 0=current only, 1=classified shared (default), 2=independent weapon color/depth history |
| `r_weaponBloomMode` | 1 | 0=no weapon bloom, 1=combined HDR before one global bloom, 2=legacy world bloom + dedicated class-gated weapon bloom (`vk_weapon_bloom`) |
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

## TAA-on policy (Temporal Weapon Resolve)

When Temporal Reconstruction is active (`r_taa 1` / `r_aaMode` 3–5 /
temporal upscale), Architecture B resolves the weapon after world TAA, then
composites world and weapon HDR before the one global bloom pass. Additionally:

| Concern | Policy |
|---|---|
| Pass order | World draw → SSR/SSAO/TAA → weapon resolve/composite → combined HDR bloom → tone map/present |
| History ownership | World TAA history never includes weapon color (weapon after TAA) |
| Class mask | R8 `TEMPORAL_CLASS_WORLD` / `WEAPON` stamped after weapon flush from `DEPTH_RANGE_WEAPON` |
| History reject | `taa.frag` rejects WEAPON↔WORLD mismatch (and mode 0 forces no weapon history); 1–2 px motion-aware dilation |
| Weapon motion | Prev weapon MVP stored in `vk.temporal`; velocities are `currentUV - previousUV` (not world-depth reprojection) |
| Reactive | Weapon depth stamp with depth-aware dilation so gun edges prefer current samples |
| Cvar | `r_weaponTemporalMode` 0 / 1 (default) / 2 (separate weapon color/depth/coverage history) |

Live check: `r_taa 1` + `r_temporalDebug 5` while rotating — confirm `weaponAfterWorldPost=yes`, no dark wall trails, sharp weapon edges without multi-frame echoes.

True previous depth is now dual R32F history. TAA compares reprojected predicted
previous depth with the actual prior-frame sample; it never samples current
depth at the history UV as a substitute. Modes 16–33, frame-ID ownership,
descriptor fault injection, capture commands, memory accounting, and the formal
matrix are documented in [TEMPORAL_WEAPON_VALIDATION.md](TEMPORAL_WEAPON_VALIDATION.md).

World TAA no longer rejects a frame solely because first-person projection was
active. Architecture B intentionally uses a separate weapon projection, so the
old `firstPersonProjectionThisFrame == LastFrame` gate was incorrectly disabling
world history whenever the viewmodel FOV path ran. Weapon history still resets
independently on weapon switch / FOV discontinuity.

## What was intentionally not changed

- No raw-depth SSR hit-reject was added; ordering isolates weapon depth structurally.
- No blur / excessive confidence clamps as a cosmetic mask.
- `modern_vulkan.cfg` boot defaults untouched.
- SKY has a reserved debug color but is not stamped separately yet; unstamped
  sky follows WORLD ownership.

## Related

- `docs/TEMPORAL_RESOURCE_OWNERSHIP.md` — per-resource ownership: owner module,
  producer/consumer passes, formats, validity bits, frame IDs, reset scopes,
  destruction sites.
- `docs/MOMENT_OIT_STOCHASTIC_ALPHA.md` — prior OIT / distortion corruption work.
- `scripts/temporal_ghost_check.sh` — static regression gate.
- `renderers/vulkan/vk_view_state.c` — `DEPTH_RANGE_WEAPON`.
- `renderers/vulkan/shaders/glsl/ssr.frag` — reflection march + debug overlays.
- `renderers/vulkan/vk_temporal.c` — ownership / deferred weapon policy / `temporal_ghost_status`.
- `renderers/vulkan/vk_temporal_class.c` — WORLD/WEAPON class stamp for TAA rejection.
- `renderers/vulkan/shaders/glsl/taa.frag` — class mismatch + reactive resolve.
- `engine/core/cm_trace.c` — BSP30 recursion depth guard (`BSP30_MAX_TRACE_DEPTH`).
