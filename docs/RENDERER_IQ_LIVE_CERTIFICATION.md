# Renderer IQ Live Certification (Phase 1.5)

**Hub:** `vk_renderer_p1_cert.c` · `vk_iq_lab.c` · `vk_iq_cert_geometry.c` · `vk_cert_readback` IQ snapshot  
**Docs:** [RENDERER_P1_CERTIFICATION.md](RENDERER_P1_CERTIFICATION.md)

## Honesty rule

Static/cvar checklist alone may reach at most **`RENDERER_P1_PROFILE_CERTIFIED`**.  
**`RENDERER_P1_IMAGE_QUALITY_CERTIFIED`** requires measured GPU evidence from `iq_certify_core` (manual override never grants final).

## Level ladder

| Level | Meaning |
|-------|---------|
| `UNCERTIFIED` | Static contracts failing |
| `STATIC_READY` | Bloom/MSAA/LOD/G-buffer static OK |
| `PROFILE_CERTIFIED` | `modern_raster_iq_reference` validates |
| `GPU_CORE_CERTIFIED` | Bloom source + firefly + G-buffer quant (GPU readback) |
| `TEMPORAL_CERTIFIED` | History registry + velocity |
| `EDGE_CERTIFIED` | Edge + SMAA metrics |
| `LIGHTING_PARITY_CERTIFIED` | Deferred/Forward+/cluster stages |
| `IMAGE_QUALITY_CERTIFIED` | All above + soak + export; no manual evidence |

## Operator path

```text
exec modern_raster_iq_reference.cfg
vid_restart
# load map
exec demo_iq_certify_core.cfg
renderer_p1_status
iq_certification_status
iq_certification_export render_cert/renderer_iq_p1.json
```

## Commands / cvars

| Command | Role |
|---------|------|
| `iq_certify_core` / `iq_lab_run core` | Arm full measured queue |
| `iq_lab_status` | Pending eval / queue position |
| `iq_certification_status` | Stage table + level |
| `iq_certification_export` | JSON under `render_cert/` |
| `renderer_p1_status` | Gate list with evidence types |

| Cvar | Default | Notes |
|------|---------|-------|
| `r_iqCertIsolate` | 1 | Isolate fixtures |
| `r_iqCertMaxRetries` | 3 | Snapshot retries |
| `r_iqCertContinueOnFail` | 1 | Continue queue on FAIL |
| `r_iqCertSoakMinutes` | 1 | Formal soak uses 30 |

## Evidence types

`STATIC` · `GPU_READBACK` · `SOAK` · `PENDING` · `MANUAL_OVERRIDE` (blocked for final)
