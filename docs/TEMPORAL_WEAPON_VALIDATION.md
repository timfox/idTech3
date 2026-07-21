# Temporal Weapon Resolve Validation

## Shipping contract

Surf explicitly enables `r_taa 1`, `r_weaponTemporalMode 1`,
`r_temporalReactiveMask 1`, weapon MVP velocity, class history, true previous
depth, and `r_weaponBloomMode 1`. `r_taa 0` remains a comparison path.

Modes:

- `0`: current-frame weapon only; weapon history is invalidated.
- `1`: classified shared world resolve plus responsive post-resolve weapon.
- `2`: physically separate weapon color/depth/coverage history. World TAA never
  samples this target, and the weapon resolve never samples world color history.

Mode 2 uses `r_weaponTemporalHistoryWeight`,
`r_weaponTemporalVarianceGamma`, `r_weaponTemporalDepthThreshold`, and
`r_weaponTemporalReactiveScale`. Weapon switch, FOV discontinuity, camera cut,
resize, map change, renderer restart, TAA toggle, or temporal-mode toggle
invalidates it independently.

## Resource ownership

- World color: `taa_history_image[2]`; committed only after a valid world TAA pass.
- World depth: `temporal_prev_depth_image[2]`, R32F reversed-Z, copied after the
  world resolve and before deferred weapon depth is written.
- Class: `temporal_class_image[2]`, R8 WORLD/WEAPON ping-pong.
- Weapon color: `weapon_history_image[2]`; RGB is resolved weapon color and
  alpha is current weapon coverage.
- Weapon depth: `weapon_prev_depth_image[2]`, R32F reversed-Z, copied after the
  isolated weapon draw.
- Reactive mask and motion vectors are current-frame inputs and are not history.

Every history image records a temporal frame ID. Debug builds require a sampled
history ID to equal the current temporal frame ID minus one. `r_dumpTemporalState`
prints IDs, validity, extents, formats, descriptor state, reset reasons, and the
latest mode-2 GPU timestamp.

## Previous-depth validation

TAA set 7 is always the persistent previous-depth descriptor. The shader
reprojects the current surface into the previous clip space, samples actual
previous R32F depth, linearizes finite reversed-Z depth, and rejects on relative
error. Current depth is never rebound as previous depth.

MSAA depth is first normalized into the single-sample R32F resolve. Dynamic
resolution and resize recreate both history images and clear validity.

## Descriptor failure policy

Class and reactive descriptors have distinct allocation, ownership, labels, and
shadow-bound image views. Missing class data binds `TemporalUnclassifiedR8`,
clears history validity for that frame, and increments:

- missing class descriptor frames
- missing reactive descriptor frames
- fallback texture frames
- forced history-rejection frames

`r_temporalDropClassDescriptor 1` deliberately exercises this path. It must
print the exact fault, reject history, remain deterministic, and never bind the
reactive or motion descriptor as class data.

## Debug modes

`r_temporalDebugVectorScale` controls signed velocity display scale. Velocity
uses neutral gray for zero, red/green signed axes, yellow for out-of-range, and
magenta for non-finite data. Class uses gray for WORLD and white for WEAPON.

- 14: pre-weapon merged velocity (explicitly not weapon MVP).
- 15: prior-class-gated pre-weapon velocity.
- 16: current class R8 after the deferred weapon class stamp.
- 17: previous class.
- 18: reprojected previous class.
- 19: class rejection.
- 20: world-gated velocity.
- 21: actual post-draw weapon MVP velocity.
- 22: final merged velocity.
- 23: raw reactive mask.
- 24: one-pixel dilated reactive mask.
- 25: weapon temporal confidence.
- 26: weapon-history validity.
- 27: weapon composition coverage.
- 28: current weapon depth.
- 29: previous weapon depth at current UV.
- 30: reprojected previous weapon depth.
- 31: absolute depth difference.
- 32: relative depth error.
- 33: final depth rejection.

Use `r_captureTemporalDebug <mode>` to queue a named screenshot after two
frames. `r_printViewmodelProjection` prints effective FOV, near/far, jitter,
reverse-Z, depth remap, and current/previous matrix provenance.

## Bloom and presentation ordering

`r_weaponBloomMode 1` is the Surf default:

1. world opaque lighting and world temporal resolve
2. isolated weapon draw / optional independent weapon resolve
3. weapon composite into scene-linear HDR
4. one global bloom extraction/composition
5. one exposure/tone-map/presentation path

Weapon depth remains excluded from world AO, SSR, SSGI, and volumetric history.
Mode 0 intentionally retains the no-weapon-bloom comparison. Mode 2 currently
uses the same single combined-HDR bloom ordering while retaining independent
weapon temporal ownership; it does not double-bloom or double-tone-map.

## Matrix and capture harness

The source of truth is `tests/data/temporal_weapon_validation.json`. It covers
TAA off, modes 0/1/2, native/75%/50%, resize, ultrawide, FOV 55/65/80, near 1/4,
ADS/FOV transitions, recoil, weapon switch, teleport, Surf motion, bloom,
emissive/muzzle-flash presentation, reactive disable, and descriptor faults.

Static gate:

```bash
./tests/scripts/test_temporal_weapon_validation.sh
```

Static-scene buffer capture:

```bash
./tests/scripts/test_temporal_weapon_validation.sh --capture
```

Lifecycle cases requiring input/window control remain explicit manifest events;
the harness does not falsely claim to synthesize alt-tab, weapon animation, or
teleport from a static console launch.

## Cost accounting

At width × height pixels:

- true world previous depth: `2 × R32F` = 8 bytes/pixel
- mode-2 weapon history: `2 × HDR color` = 2 × color-format bytes/pixel
- mode-2 weapon depth: `2 × R32F` = 8 bytes/pixel

For RGBA16F at 1920×1080 this is 15.8 MiB world-depth plus 47.5 MiB mode-2
weapon state, 63.3 MiB total. RGBA32F doubles only the weapon-color component.
`r_dumpTemporalState` reports the runtime color format and mode-2 GPU time.

## Measurable failure criteria

The feature fails validation if a weapon trail lasts more than two frames,
world pixels receive weapon history, weapon pixels receive world history,
history survives a class/reset transition, velocity becomes non-finite, a
descriptor substitution occurs, Surf fails to activate TAA, or combined bloom
is visibly double-applied. A single checkerboard screenshot is not completion
evidence; all required buffers and reset/fault logs must accompany a run.

## Known limitations

- Surf often packs `RF_FIRST_PERSON` surfaces into the main world draw command.
  Architecture B now splits those mixed commands: world draw skips first-person
  surfaces, then the deferred flush draws only the deferred first-person set.
- World TAA must not use a first-person-projection equality gate. Architecture B
  intentionally uses a different weapon projection, so that gate previously
  disabled world history on every viewmodel frame.
- Mode 2 still needs an isolated world HDR source (`post_fog_src != color`).
  Before the first successful world TAA write, the resolve falls back to
  current-frame weapon and rejects weapon history for that frame.
- SKY has a reserved debug color but is not stamped separately; unstamped sky
  follows WORLD ownership.
- Live buffer captures require
  `./tests/scripts/test_temporal_weapon_validation.sh --capture`. The default
  gate validates contracts and resources without claiming a full visual set.
