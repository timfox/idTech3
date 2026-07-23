# Renderer P1 Image-Quality Certification

**Hub:** `vk_renderer_iq_p1.c` · `vk_renderer_p1_cert.c` · `vk_bloom_source_contract.c`  
**Live GPU:** [RENDERER_IQ_LIVE_CERTIFICATION.md](RENDERER_IQ_LIVE_CERTIFICATION.md)  
**Commands:** `renderer_p1_status` · `renderer_p1_certify` · `iq_certify_core` · `iq_certification_status` · `renderer_iq_profile_*` · `temporal_history_status` · `ghosting_isolation_status` · `bloom_*` · `gbuffer_quality_status` · `msaa_policy_status`

## Honest levels (Phase 1.5)

| Level | How reached |
|-------|-------------|
| `RENDERER_P1_STATIC_READY` | Static contracts (bloom source, MSAA×OIT, LOD, G-buffer fidelity) |
| `RENDERER_P1_PROFILE_CERTIFIED` | `modern_raster_iq_reference` validates — **maximum from cvars alone** |
| `RENDERER_P1_GPU_CORE_CERTIFIED` | GPU readback: bloom source + firefly + G-buffer quant |
| `RENDERER_P1_TEMPORAL_CERTIFIED` | History registry ownership + velocity |
| `RENDERER_P1_EDGE_CERTIFIED` | Edge + SMAA metrics |
| `RENDERER_P1_LIGHTING_PARITY_CERTIFIED` | Lighting + cluster parity stages |
| `RENDERER_P1_IMAGE_QUALITY_CERTIFIED` | All prior + soak; **not** grantable by profile or manual override |

`STATIC ≠ IMAGE_QUALITY`. Scaffold gates that need GPU evidence print `PEND` / `PENDING` until `iq_certify_core` records results.

## Gates (checklist view)

| Gate | Evidence |
|------|----------|
| BLOOM_SOURCE / FIREFLY (cvar) | STATIC until lab measures extract |
| NO_UNOWNED_TEMPORAL_HISTORY | Live registry notes each frame |
| VELOCITY / EDGE / PARITY / … | PENDING → GPU_READBACK via iq_lab |
| GBUFFER_FULL_FIDELITY | STATIC policy; quant via GPU_CORE |
| MSAA_POLICY / SMAA / TEXTURE_LOD | STATIC |

## Docs index

- [RENDERER_IQ_LIVE_CERTIFICATION.md](RENDERER_IQ_LIVE_CERTIFICATION.md)
- [RENDERER_IQ_REFERENCE_PROFILE.md](RENDERER_IQ_REFERENCE_PROFILE.md)
- [BLOOM_SOURCE_INTEGRITY.md](BLOOM_SOURCE_INTEGRITY.md)
- [BLOOM_FIREFLY_CONTROL.md](BLOOM_FIREFLY_CONTROL.md)
- [DEFERRED_HONESTY.md](DEFERRED_HONESTY.md)
- [SHARED_BRDF.md](SHARED_BRDF.md)
- [COLOR_PIPELINE.md](COLOR_PIPELINE.md)

## P2 after P1

Exposure → tonemap/gamut → artistic bloom → reflections → volumetrics → shadows → final profile.  
**Blocked until `RENDERER_P1_IMAGE_QUALITY_CERTIFIED`.**
