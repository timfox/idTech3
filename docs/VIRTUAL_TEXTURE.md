# Virtual texture + sparse residency

Chocolate **MegaTexture-class** path: CPU page table with either a dense physical atlas or a **sparse `VkImage`** virtual space and `vkQueueBindSparse` page binds. Not a full BSP UV rewrite.

## Enable

```
set r_vt 1
set r_vtSparse 1
set r_vtFeedback 1
vid_restart
exec demo_vt.cfg
set r_vtDebug 1     // PiP atlas overlay
set r_vtSample 1    // brush-top bsp_stream faces sample the atlas
vt_status
```

Demo pages: `examples/demo_game/bootstrap_media/vt/page0..2.png`.

## Modes

| Mode | When | Behavior |
|------|------|----------|
| **Sparse** | `r_vt 1`, `r_vtSparse 1`, GPU has `sparseBinding` + `sparseResidencyImage2D`, queue supports `SPARSE_BINDING`, granularity 128² | Virtual `N×N` page grid (`r_vtVirtualPages`); bind up to `r_vtPhysicalPages` via LRU |
| **Dense fallback** | Sparse unavailable or `r_vtSparse 0` | Fixed 8×8×128 atlas (unchanged scaffold) |

Startup logs `[VK] sparseBinding…` and `[VK][VT] sparse=1…` or `sparse=0 fallback=dense`.

## Cvars / commands

| | |
|--|--|
| `r_vt` | Latched master (default 0) |
| `r_vtSparse` | Prefer sparse path (default 1, latched) |
| `r_vtFeedback` | Drain feedback bitfield each frame (default 1) |
| `r_vtVirtualPages` | Virtual grid side 8–64 (default 32, latched) |
| `r_vtPhysicalPages` | Max bound pages 8–256 (default 64, latched) |
| `r_vtDebug` | Atlas PiP overlay |
| `r_vtSample` | Use atlas shader on `r_bspStream` brush-top faces |
| `vt_status` / `vt_load` / `vt_flush` | Status, decode PNG/TGA/JPG into a page, clear table (+ unbind sparse) |

## Feedback (GPU)

1. `r_vtSample` surfaces queue UV samples via `R_VT_Feedback_RequestUV` (up to 256/frame).
2. End of frame: compute shader `vt_feedback.comp` **`atomicOr`s** virtual page bits into a host-visible storage buffer.
3. CPU drains the bitfield and binds missing pages (procedural fill / LRU).

`vt_status` reports `gpu=1`, `dispatches`, and `gpuSamples` when the compute pipeline is ready. If the pipeline fails to create, UV samples fall back to a CPU bit stamp.

Shader source: [`renderers/vulkan/shaders/glsl/vt_feedback.comp`](../renderers/vulkan/shaders/glsl/vt_feedback.comp) (SPIR-V include `vk_vt_feedback_spirv.inc`).

## Relationship to NDGI “VT”

Neural Dynamic GI dirty-page lightmap decode is unrelated; this module is albedo/page residency for MegaTexture-style streaming.

## World-zone gating

When a district manifest is loaded, resident zones with the texture layer bit
(`REF_WORLD_ZONE_RESIDENCY_TEXTURE`) gate feedback and page work. If no zone
snapshot is published (legacy maps) or at least one resident zone includes the
texture bit, VT runs normally. Otherwise feedback drain, GPU dispatch, and page
ensure are skipped for that frame. `vt_status` reports `zones=N` and
`zone-gated feedback frames=`. See [WORLD_ZONES.md](WORLD_ZONES.md).

## Limits

- No rewriting all BSP diffuse UVs yet
- Sparse path requires 128×128 image granularity (else dense fallback)
- Sample consumer is brush-top overlay + PiP + feedback
