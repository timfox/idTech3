# WBOIT Live Certification (Phase 2.6 / 2.6A / 2.6B / 2.6C)

**Status:** Live execution path with **deferred OIT GPU snapshots**, fixture isolation, failure triage, and auto soak chain.  
**Level today:** `WBOIT_STATIC_CERTIFIED` until a live `oit_certify_core` run completes GPU stages + soak on your device.

## Operator sequence (Phase 2.6C)

```text
+set r_oit 1 +set r_fbo 1
load a valid world
oit_certify_core
```

Or: `exec demo_wboit_certify_core.cfg` after map load.

### What happens

1. Core fixture stages arm → draw through live WBOIT accum (world transparents skipped when `r_oitCertIsolate 1`).
2. After resolve, fog/accum/revealage/resolved are **copied in the same command buffer**.
3. After the frame fence, snapshots are decoded and evaluated (never mid-frame unsubmitted images).
4. PASS advances; FAIL stops (triage); PENDING retries (`r_oitCertMaxRetries`).
5. On GPU queue complete → marks `LIVE_FULL` + exports JSON.
6. Chains `oit_soak_wboit N` (`r_oitCertSoakMinutes`, default **1**; formal shipping **30**).
7. Clean soak → `WBOIT_EVIDENCE_SOAK` → may grant **`WBOIT_PRODUCTION_CERTIFIED`**.

## Evidence policy (2.6A)

| Evidence | May grant PRODUCTION? |
|----------|------------------------|
| `STATIC` / `CPU_REFERENCE` | No |
| `GPU_READBACK` / `GPU_REDUCTION` | Yes (when required) |
| `LIFECYCLE` / `SOAK` | Yes for those stages |
| `MANUAL_OVERRIDE` | **No** by default |

## Commands

| Command | Role |
|---------|------|
| `oit_certify_core` | GPU fixture queue + LIVE_FULL + optional soak |
| `oit_lab_*` | Manual lab control |
| `oit_cert_geometry_status` | Armed fixtures |
| `cert_readback_*` | Manual readback |
| `wboit_production_status` | Stage matrix + level |
| `oit_soak_wboit [minutes]` | Soak evidence |

## Cvars

- `r_oitCertIsolate` (default **1**) — deterministic fixtures only
- `r_oitCertSoakMinutes` (default **1**; formal **30**)
- `r_oitCertContinueOnFail` (default **0**)
- `r_oitCertMaxRetries` (default **8**)
- `r_oitLabFreeze` / `r_certReadbackBlocking` / `r_oitAllowManualCertification`

## Persistence

`render_cert/wboit_certification.json` — invalidate when contracts/shaders/blends/formats change.

See [WBOIT_CERTIFICATION_THRESHOLDS.md](WBOIT_CERTIFICATION_THRESHOLDS.md), [COLOR_PIPELINE.md](COLOR_PIPELINE.md).
