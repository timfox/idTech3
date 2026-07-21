# Temporal Resource Ownership (Architecture B)

Authoritative ownership map for every temporal history and per-frame temporal
input in the Vulkan renderer. Companion documents:

- [TEMPORAL_WEAPON_VALIDATION.md](TEMPORAL_WEAPON_VALIDATION.md) — validation
  matrix, debug modes, failure criteria.
- [RENDERER_TEMPORAL_GHOSTING.md](RENDERER_TEMPORAL_GHOSTING.md) — root cause,
  pass ordering, bisect tooling.

All images below are allocated in `vk_create_attachments()` and destroyed in
`vk_destroy_attachments()` (`renderers/vulkan/vk_attachments.c`). Validity
bits, frame IDs, and reset reasons live in `vk.temporal`
(`renderers/vulkan/vk.h`) and are managed by `renderers/vulkan/vk_temporal.c`.
`r_dumpTemporalState` prints the live values of every field in this document
(validity line, frame-ID line, resources line, resets line).

## Shared reset reasons

Reset reasons are the `VK_TEMPORAL_RESET_*` bitmask in
`renderers/vulkan/vk_temporal.h`. The scopes referenced below:

- **Shared world reset**: `RENDERER_INIT`, `SWAPCHAIN_CHANGE`,
  `RENDER_SIZE_CHANGE`, `WORLD_CHANGE`, `CAMERA_CUT`, `MISSING_PREV_DATA`,
  `CLIENT_STATE_CHANGE`, `EXPLICIT_DEBUG`. Applied through
  `vk_temporal_apply_resets` and the sticky-reset queue
  (`vk_temporal_request_sticky_reset`).
- **Weapon reset**: everything in the shared world reset **plus**
  `vk_reset_weapon_history()` calls on weapon switch, first-person FOV
  discontinuity, TAA toggle, and `r_weaponTemporalMode` toggle
  (`tr_backend.c`, `vk_frame_end.c`, `vk_frame_submit.c`). Weapon history can
  reset without touching world history; the reverse is never true.

## Resource table

| Resource | Image(s) | Format | Producer pass | Consumer pass | Validity bit | Frame ID field | Reset scope |
|---|---|---|---|---|---|---|---|
| World color history (current/previous) | `vk.taa_history_image[2]` | `vk.color_format` (HDR scene format, typically RGBA16F) | World TAA resolve (`taa.frag` via `vk_end_frame_record_taa_pass`, `vk_frame_end.c`); committed only after a valid world resolve | Next-frame `taa.frag` history sample | `vk.temporal.hasValidTAAHistory` / `prevColorValid` | `taaHistoryFrameId[2]` | Shared world reset |
| Weapon color history (current/previous) | `vk.weapon_history_image[2]` (`WeaponTemporalHistory[i]`) | `vk.color_format`; RGB = resolved weapon color, A = current weapon coverage | Independent weapon resolve (`weapon_taa.frag`), mode 2 only | Next-frame `weapon_taa.frag`; composite via `weapon_taa_composite.frag` | `vk.temporal.weaponHistoryValid` | `weaponHistoryFrameId[2]` | Weapon reset |
| World previous depth (current/previous) | `vk.temporal_prev_depth_image[2]` (`TemporalPrevDepthR32F[i]`) | `VK_FORMAT_R32_SFLOAT`, reversed-Z single-sample | Depth-history copy (`temporal_depth_history_copy_pipeline`) after the world resolve and **before** deferred weapon depth is written | `taa.frag` set 7 `previousDepthTex` | `vk.temporal.prevDepthValid` (`prevDepthIndex` selects previous) | `prevDepthFrameId[2]` | Shared world reset |
| Weapon previous depth (current/previous) | `vk.weapon_prev_depth_image[2]` | `VK_FORMAT_R32_SFLOAT`, reversed-Z | Weapon depth-history copy after the isolated weapon draw | `weapon_taa.frag` depth rejection (debug modes 29–33) | `vk.temporal.weaponHistoryValid` (weapon state is invalidated as a unit) | `weaponDepthFrameId[2]` | Weapon reset |
| Temporal class (current/previous) | `vk.temporal_class_image[2]` | `VK_FORMAT_R8_UNORM` (WORLD/WEAPON) | Class stamp (`temporal_class_stamp.frag` via `vk_temporal_class.c`; weapon stamped by the deferred flush in `tr_backend.c`) | `taa.frag` set 6 class-mismatch rejection | `vk.temporal.prevClassValid` (`classHistoryIndex` / `classHasPrev` ping-pong) | `classFrameId[2]` | Shared world reset |
| Reactive mask | `vk.reactive_mask_image` (`TemporalReactiveR8`) | `VK_FORMAT_R8_UNORM` | Reactive stamp (`reactive_stamp_weapon.frag` via `vk_reactive_mask.c`), depth-aware dilation | Current-frame `taa.frag` reactive weighting | none — current-frame input, never history; missing descriptor binds `TemporalReactiveFallbackR8` and forces history rejection | none | Recreated with attachments only |
| Velocity (motion vectors) | `vk.motion_vector_image` (+ MSAA variant) | `VK_FORMAT_R16G16_SFLOAT` | Main-pass MRT motion output; weapon MVP velocity merged after the deferred weapon flush (`vk.temporal.weaponPrev*` matrices) | Current-frame `taa.frag` / weapon resolve reprojection | `vk.temporal.prevVelocityValid` (previous-matrix availability, `weaponMatricesHavePrev` for the weapon MVP) | none (matrices carry provenance, not image history) | Shared world reset clears previous-matrix validity |

## Ownership rules

- **Owner module** for all image memory is `vk_attachments.c`; no other module
  creates or destroys these images. `vk_temporal.c` owns validity/frame-ID
  state; `vk_temporal_class.c` owns the class stamp; `vk_reactive_mask.c` owns
  the reactive stamp; `vk_frame_end.c` / `vk_frame_submit.c` own pass ordering.
- World TAA never samples `weapon_history_image`; the weapon resolve never
  samples world color history (mode 2 contract).
- Every history image records a temporal frame ID at commit. Debug builds
  assert a sampled history ID equals the current temporal frame ID minus one.
- Descriptor faults never substitute one resource for another: a missing class
  descriptor binds `TemporalUnclassifiedR8` (never the reactive or motion
  descriptor), prints the fault, and rejects all history for that frame
  (`vk_frame_end.c`).

## Destruction sites

`vk_destroy_attachments()` destroys `taa_history_image[]`,
`temporal_prev_depth_image[]`, `weapon_prev_depth_image[]`,
`weapon_history_image[]`, and clears their descriptors and layouts, then sets
`vk.temporal.prevDepthValid = qfalse` and
`vk.temporal.weaponHistoryValid = qfalse`. The class and reactive images are
destroyed on the same path. Any swapchain or render-size recreation therefore
implies full invalidation before the next frame samples history.
