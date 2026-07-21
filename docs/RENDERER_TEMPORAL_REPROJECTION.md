# Temporal Reprojection / Velocity-Space Correctness

**Status:** Instrumentation + structural fixes for 2×–4× velocity scale bugs.  
**Date:** 2026-07-21  
**Constraint:** Fix velocity space / frame ownership / jitter — do **not** mask with lower history weight or more blur.

## Observed symptom

Moving or camera-relative world meshes produce multiple offset silhouettes arranged along the motion direction. Separation is systematically exaggerated (~2×–4×), consistent with history reprojection using overscaled motion vectors. Geometry itself is valid.

Related weapon-trail work remains in [RENDERER_TEMPORAL_GHOSTING.md](RENDERER_TEMPORAL_GHOSTING.md); this document covers **world** velocity scale / ownership.

## Canonical velocity convention

Single renderer-wide space (`renderers/vulkan/vk_velocity_space.h`):

| Enum | Meaning |
|---|---|
| `VK_VELOCITY_SPACE_UV` | **Canonical.** Normalized `[0,1]` UV at the **render target** extent |
| `VK_VELOCITY_SPACE_PIXELS` | Derived only: `velocityUV * renderExtent` |
| `VK_VELOCITY_SPACE_NDC` | Never stored (NDC delta is exactly **2×** UV) |

Produce:

```glsl
vec2 currUV = currentClip.xy / currentClip.w * 0.5 + 0.5;
vec2 prevUV = previousClip.xy / previousClip.w * 0.5 + 0.5;
out_motion = currUV - prevUV;   // gen_frag.tmpl / light_frag.tmpl
```

Consume:

```glsl
historyUV = sampleUV - motion;  // taa.frag / weapon_taa.frag
```

No pass may rescale stored motion by `renderScale`, `outputSize/inputSize`, or `4.0`.

## Root causes addressed

| Phase | Bug class | Fix |
|---|---|---|
| 1 | Mixed UV / NDC / pixel conventions | `vk_velocity_space.h` + producer/consumer comments |
| 2 | Missing `* 0.5` NDC→UV (2× error) | Audit + `r_temporalVelocityProbe` warns on 2×/4×/0.5×/0.25× |
| 3/4 | Render vs display / quarter-res mismatch | `r_temporalResolutionDebug` / `temporal_resolution_status` |
| 5 | Previous transform older than 1 temporal frame | `vk_prev_matrices_frame`, `VK_MOTION_INVALID_STALE_PREV`, age in `temporal_motion_status` |
| 5/8 | Multi-stage draws append duplicate motion records | Per-entity dedupe in `vk_motion_resolve_entity` |
| 6 | TAA / upscale running more than once | Counters + GPU markers `TemporalResolveWorld` / `Weapon` / `FinalComposite` |
| 7 | Jitter delta embedded in motion; Halton advanced per camera | Rebase prev projection onto current jitter; once-per-`tr.frameCount` advance |
| 9 | No direct visual of scale error | `r_temporalDebug` **28–35** |

## Debug controls

```bash
# Extents + velocity-space convention (auto-prints on change when 1)
seta r_temporalResolutionDebug 1
temporal_resolution_status

# CPU probe: warn on scale anomalies / stale prev matrices
seta r_temporalVelocityProbe 1   # 2 = also print every ~60 frames

# Reprojection debugger (world TAA)
seta r_temporalDebug 28   # raw stored velocity
seta r_temporalDebug 29   # velocity as UV
seta r_temporalDebug 30   # velocity as pixels (abs/64 → 1.0 at 64 px)
seta r_temporalDebug 31   # history UV displacement
seta r_temporalDebug 32   # error ratio vs matrix reprojection (green=1× yellow=2× red=4×)
seta r_temporalDebug 33   # previous-matrix age (green=1 red>1)
seta r_temporalDebug 34   # temporal resolves last frame (green=1 red>1)
seta r_temporalDebug 35   # correspondence: cyan=current, magenta=history lookup

temporal_motion_status    # per-entity age (1=green, >1=red, invalid=yellow)
```

Modes **16–27** remain on the weapon resolve path; **28–35** are world-only.

## Controlled validation

```bash
./scripts/temporal_reprojection_check.sh          # static gate
exec demo_temporal_reprojection.cfg               # in-engine
```

Suggested live cases (checkerboard wall + one rigid mesh):

| Test | Expectation |
|---|---|
| A Object translate 64 px/frame, fixed camera | Mode 30 ≈ white (64/64); mode 32 green; no multi-silhouette |
| B Camera yaw only, static object | Same; prev matrix age = 1 |
| C Camera + object opposite | Error ratio still ~1× |
| D `r_renderScale` 50% / 75% / native | UV velocity meaning unchanged; extents report matches |
| E First frame / teleport / `vid_restart` / TAA toggle | History rejected (NaN MV or reset), no sticky echoes |

## Acceptance

- Reprojection error ≲ 0.5 px for rigid geometry.
- 64 px motion does **not** reproject as 128 / 256 px.
- Velocity meaning identical across native and dynamic resolution.
- Previous transforms exactly one temporal frame old.
- World TAA (and temporal upscale) execute once per rendered frame.
- Static geometry remains stable; fix does not rely on lowering history weight.
