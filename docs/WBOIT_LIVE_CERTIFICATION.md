# WBOIT Live Certification (Phase 2.6 / 2.6A)

**Status:** Evidence-backed controller + GPU readback + deterministic `oit_lab` cases.  
**Level today:** `WBOIT_STATIC_CERTIFIED` until GPU lab cases produce matching evidence.

## Evidence policy (2.6A)

Every stage result carries `wboitCertEvidence_t`:

| Evidence | May grant PRODUCTION? |
|----------|------------------------|
| `STATIC` | No (contract/resources only) |
| `CPU_REFERENCE` | No |
| `GPU_READBACK` / `GPU_REDUCTION` | Yes (when stage requires it) |
| `LIFECYCLE` / `SOAK` | Yes for those stages |
| `MANUAL_OVERRIDE` | **No** by default (`r_oitAllowManualCertification 0`) |

`oit_cert_stage pass …` always stamps **`MANUAL_OVERRIDE`** and cannot grant `WBOIT_PRODUCTION_CERTIFIED`.

Required evidence per stage is enforced in `vk_wboit_cert_required_evidence()`.

## Commands

| Command | Role |
|---------|------|
| `oit_lab_list` / `oit_lab_run` / `oit_lab_run_group` | Deterministic lab |
| `oit_lab_status` / `oit_lab_reset` | Lab session |
| `cert_readback_capture` / `flush` / `status` | GPU readback |
| `wboit_production_status` | Stage + evidence matrix |
| `oit_certification_export` | Write `render_cert/wboit_certification.json` |
| `oit_certification_invalidate` | Invalidate prior evidence |
| `oit_certification_import` | Display-only (never certifies this device) |
| `oit_soak_wboit` | Soak → `WBOIT_EVIDENCE_SOAK` on clean completion |

## Cvars

- `r_oitAllowManualCertification` (default **0**)
- `r_requireWboitCertification` (default **1** = warn)
- `r_oitLabFreeze` (default **1**)
- `r_certReadbackBlocking` (default **1**)

## Lab groups

`core` `alpha` `weight` `order` `fog` `additive` `resolve` `lifecycle` `soak` `specialized` `mboit` `all`

## Persistence

Evidence is exported to `render_cert/wboit_certification.json` and must be invalidated when any OIT/alpha/depth/weight/resolve contract, accum/resolve shader, blend, format, pass order, fog ownership, or color-space convention changes.

## Startup

Prints real level, e.g. `WBOIT: WBOIT_STATIC_CERTIFIED` — never claims production from manual flags.

See also [WBOIT_CERTIFICATION_THRESHOLDS.md](WBOIT_CERTIFICATION_THRESHOLDS.md), [TRANSPARENCY_ROUTING.md](TRANSPARENCY_ROUTING.md).
