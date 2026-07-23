# Renderer IQ Reference Profile

**Profile name:** `modern_raster_iq_reference`  
**Config:** `config/modern_raster_iq_reference.cfg`  
**Commands:** `renderer_iq_profile_status` · `renderer_iq_profile_validate` · `renderer_iq_profile_apply`

## Purpose

Deterministic, native-resolution baseline for P1 image-quality comparisons:

- No temporal AA (`r_taa 0`)
- Full-fidelity G-buffer (`r_gbufferCompact 0`, `r_gbufferQuality 2`)
- Production WBOIT (`r_oit 1`, `r_oitFogMode 1`)
- Bloom on with firefly clamp (`r_bloom 1`, `r_bloomFireflyClamp 1`)
- SMAA only (`r_aaMode 2`)
- No sharpening, no MSAA×OIT, no dynamic resolution

Any remaining ghosting under this profile must be attributed to a named history owner (`temporal_history_status`), not “general temporal behavior.”

## Apply

```text
exec modern_raster_iq_reference.cfg
vid_restart
renderer_iq_profile_validate
renderer_p1_status
```

See [RENDERER_P1_CERTIFICATION.md](RENDERER_P1_CERTIFICATION.md).
