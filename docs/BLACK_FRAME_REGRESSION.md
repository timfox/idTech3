# Black Frame / SceneHDR Composition Regression

## Symptom

All-black **3D** scene while **UI / console / overlays** still present. Swapchain is alive; SceneHDR composition is not.

Disabling deferred lighting alone does **not** fix it — do not assume the deferred compute shader is the sole cause.

## Root cause (2026-07)

On the **non-split** path (`r_oit` on, deferred opaque/transparent split **off** — typical Forward+ / mode 2):

1. Opaque Forward+ (or classic) writes SceneHDR.
2. `vk_oit_pass` ends MAIN, resolves into HDR, opens `post_bloom`.
3. A later sidecar still called `vk_deferred_gbuffer_capture_after_geometry()` (and visbuf/AV).
4. That capture **ends `post_bloom`**, transitions `color_image` for TRANSFER, and **does not restore** the OIT-resolved HDR → black 3D.

Mode 1/3 split path was fine (capture runs **before** OIT; the post-block is skipped when split is on).

### Fix

- Non-split + OIT: run G-buffer / visbuf / AV (and optional deferred lighting) **before** `vk_oit_pass`.
- Post-geometry sidecar: skip when `oitFrameState` is `RESOLVED` or `ACCUMULATED`.
- WBOIT resolve: empty / invalid accum **preserves opaque** (never `vec4(0)`).
- OIT resources-not-ready after `vk_end_render_pass`: open `post_bloom` (do not orphan SceneHDR).

Regression: `tests/scripts/test_black_frame_oit_gbuffer_order.sh`

## SceneHDR writer chain (healthy non-split + WBOIT)

```text
ForwardOpaque
  → GBufferCapture / VisBufCapture   (before OIT only)
  → OITOpaqueCopy
  → WBOITResolve
  → PreBloom → Bloom → ToneMap → UI
```

Unhealthy (pre-fix):

```text
ForwardOpaque → WBOITResolve → GBufferCapture   ← destroys resolved HDR
```

## Diagnostics

| Control | Purpose |
|---------|---------|
| `renderer_validate_frame` | Milestone 1 frame-production checklist |
| `renderer_resource_status` | SceneHDR / G-buffer / OIT / debug cvars + bandwidth |
| `renderer_capture_black_frame` | Force dump + validate |
| `renderer_draw_status` | Draw counters + path status |
| `r_oit 0` | If scene returns → inspect WBOIT / post-OIT order |
| `frame_output_status` / `r_frameOutputDebug` | SceneHDR meta + writer chain |
| `render_path_status verbose` | Ownership counts (deferred vs Forward+) |
| `r_oitDebug 17/18/19` | Empty-pixel / opaque input / coverage |
| `r_forceMinimalScene 1` | Skip OIT + pre-OIT G-buffer sidecars |
| `r_captureBlackFrame 1` | Log `BLACK FRAME DETECTED` + chain |
| `r_exposureDebug 1` / `r_exposureManual 1` | Exposure collapse |
| `r_hybridCompare 0` `r_renderPathDebug 0` | Clear debug discards |

See also [RENDERER_IDTECH7_SPRINT.md](RENDERER_IDTECH7_SPRINT.md).

## Push constants

OIT accum / moments / MBOIT: **224 bytes** (`vkOitPushConstants_t`). Resolve: **16 bytes**. Startup logs device `maxPushConstantsSize`. Compile-time `_Static_assert` on CPU struct size.

## Acceptance

- Deferred lighting off → opaque world still visible via Forward+ / legacy.
- Every opaque surface has an active color owner (`render_path_status verbose`).
- SceneHDR has a writer each world frame; chain never shows G-buffer capture after WBOIT resolve.
- WBOIT empty pixels preserve opaque.
- `release/idtech3_vulkan.so` rebuilt after fix.
