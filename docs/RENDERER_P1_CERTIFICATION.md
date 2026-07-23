# Renderer P1 Image-Quality Certification

**Hub:** `vk_renderer_iq_p1.c` · `vk_bloom_source_contract.c`  
**Commands:** `renderer_p1_status` · `renderer_p1_certify` · `renderer_iq_profile_*` · `temporal_history_status` · `ghosting_isolation_status` · `bloom_*` · `gbuffer_quality_status` · `msaa_policy_status`

## Gates

| Gate | Meaning |
|------|---------|
| BLOOM_SOURCE_CERTIFIED | Scene-linear pre-exposure extract noted |
| BLOOM_FIREFLY_CONTROL_CERTIFIED | `r_bloomFireflyClamp 1` |
| NO_UNOWNED_TEMPORAL_HISTORY | Registry API only |
| VELOCITY / SPECULAR / PARITY / EDGE / … | Scaffold: pass when IQ profile validates |
| GBUFFER_FULL_FIDELITY_CERTIFIED | Quality 2 + compact 0 |
| MSAA_POLICY_CERTIFIED | No MSAA×OIT |
| SMAA_CERTIFIED | SMAA enabled for reference |
| TEXTURE_LOD_CERTIFIED | No negative global LOD bias |

Success level: `RENDERER_P1_IMAGE_QUALITY_CERTIFIED` when all gates PASS.

## Docs index

- [RENDERER_IQ_REFERENCE_PROFILE.md](RENDERER_IQ_REFERENCE_PROFILE.md)
- [BLOOM_SOURCE_INTEGRITY.md](BLOOM_SOURCE_INTEGRITY.md)
- [BLOOM_FIREFLY_CONTROL.md](BLOOM_FIREFLY_CONTROL.md)
- [DEFERRED_HONESTY.md](DEFERRED_HONESTY.md) (lighting ownership)
- [SHARED_BRDF.md](SHARED_BRDF.md) (specular AA / BRDF)
- [COLOR_PIPELINE.md](COLOR_PIPELINE.md) (P0 ownership)

## P2 after P1

Exposure → tonemap/gamut → artistic bloom → reflections → volumetrics → shadows → final profile.
