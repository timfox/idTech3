# Renderer IQ Live Certification (Phase 1.6)

**Hub:** `vk_renderer_p1_live.c` · `vk_renderer_p1_cert.c` · `vk_iq_lab.c` · thresholds / failure / evidence  
**Related:** [RENDERER_P1_CERTIFICATION.md](RENDERER_P1_CERTIFICATION.md) · [RENDERER_P1_THRESHOLDS.md](RENDERER_P1_THRESHOLDS.md) · [RENDERER_P1_EVIDENCE.md](RENDERER_P1_EVIDENCE.md)

## Honesty rule

Static/cvar checklist alone may reach at most **`RENDERER_P1_PROFILE_CERTIFIED`**.  
**`RENDERER_P1_IMAGE_QUALITY_CERTIFIED`** requires measured GPU evidence from the live state machine.  
`MANUAL_OVERRIDE` never grants final.

## Live state machine

```text
IDLE → PREFLIGHT → ARM_CASE → WARMUP → WAIT_FOR_CAPTURE_POINT
  → REQUEST_READBACK → WAIT_FOR_READBACK → VALIDATE_FRAME_IDENTITY
  → EVALUATE → RECORD_EVIDENCE → ADVANCE_CASE → … → COMPLETE|FAILED
```

Hard rules:

- Do not evaluate before the readback fence.
- Do not advance merely because a frame count elapsed.
- Do not reuse staging from another case.
- Do not start the next fixture while readback is unresolved.
- Do not pass metrics over zero valid pixels (`IQ_FIXTURE_REGION_EMPTY`).
- Frame/generation mismatch → `IQ_READBACK_FRAME_MISMATCH`.

## Operator path

```text
exec modern_raster_iq_reference.cfg
vid_restart
# load a world
exec demo_iq_certify_core.cfg
iq_certify_status
renderer_p1_status
```

Groups:

```text
renderer_p1_certify core
renderer_p1_certify temporal
renderer_p1_certify edges
renderer_p1_certify lighting
renderer_p1_certify full
renderer_p1_certify resume
renderer_p1_certify from <stage>
```

Also: `iq_certify_preflight` · `iq_certify_abort` · `iq_certify_retry` · `iq_certify_retry_stage` · `iq_certify_resume` · `iq_certify_from`

## Preflight

Validates world, IQ profile, OIT/FBO/G-buffer/SMAA/TAA/motionBlur/DOF/sharpen/renderScale/LOD, MSAA×OIT policy, BloomSourceHDR, SceneHDR, thresholds. Failures print the exact operator action (never hang as PENDING).

## Promotion mapping

| Level | Required live stages |
|-------|----------------------|
| PROFILE | static + profile |
| GPU_CORE | bloom source, firefly, pyramid, G-buffer quant |
| TEMPORAL | history, velocity, reset, ghosting, specular |
| EDGE | edge, SMAA, MSAA policy, texture LOD |
| LIGHTING_PARITY | material decode, lighting parity, ownership, clusters |
| IMAGE_QUALITY | all prior + normal_mip + lifecycle + soak |

## Evidence / failures

- JSON: `render_cert/renderer_iq_p1.json`
- Thresholds: `render_cert/thresholds.json` (`iq_thresholds_export`)
- Failures: `render_cert/failures/p1_<stage>_case<id>_<ts>/` (`renderer_p1_last_failure`)

## Cvars

| Cvar | Default | Notes |
|------|---------|-------|
| `r_iqCertIsolate` | 1 | Isolate fixtures |
| `r_iqCertWarmupFrames` | 0 | Global warmup floor (0=per-case) |
| `r_iqCertTimeoutFrames` | 120 | Readback timeout |
| `r_iqCertContinueOnFail` | 1 | Advance after FAIL |
| `r_iqCertSoakMinutes` | 1 | Soak (formal 30) |
