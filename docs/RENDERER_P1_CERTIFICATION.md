# Renderer P1 Image-Quality Certification

**Hub:** `vk_renderer_iq_p1.c` · `vk_renderer_p1_cert.c` · `vk_renderer_p1_live.c`  
**Live GPU:** [RENDERER_IQ_LIVE_CERTIFICATION.md](RENDERER_IQ_LIVE_CERTIFICATION.md)  
**Evidence / thresholds:** [RENDERER_P1_EVIDENCE.md](RENDERER_P1_EVIDENCE.md) · [RENDERER_P1_THRESHOLDS.md](RENDERER_P1_THRESHOLDS.md)

## Honest levels (Phase 1.6)

| Level | How reached |
|-------|-------------|
| `STATIC_READY` | Static contracts |
| `PROFILE_CERTIFIED` | IQ profile — **maximum from cvars alone** |
| `GPU_CORE_CERTIFIED` | Bloom source + firefly + pyramid + G-buffer (GPU) |
| `TEMPORAL_CERTIFIED` | Velocity + history + reset + ghosting + specular |
| `EDGE_CERTIFIED` | Edge + SMAA + MSAA policy + texture LOD |
| `LIGHTING_PARITY_CERTIFIED` | Material decode + lighting + ownership + clusters |
| `IMAGE_QUALITY_CERTIFIED` | All prior + normal_mip + lifecycle + soak |

`STATIC ≠ IMAGE_QUALITY`. P2 blocked until final P1 level from **current** GPU evidence.

## Operator path

```text
exec modern_raster_iq_reference.cfg
vid_restart
# load map
exec demo_iq_certify_core.cfg
renderer_p1_certify full   # after core succeeds
```
