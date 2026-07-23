# WBOIT Live Certification (Phase 2.6 / 2.6A / 2.6B)

**Status:** Evidence-backed controller + GPU readback + **renderer-owned deterministic fixtures** (`vk_oit_cert_geometry`) + `oit_certify_core` orchestration.  
**Level today:** `WBOIT_STATIC_CERTIFIED` until live `r_oit 1` frames run the fixture queue and produce matching GPU evidence.

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

## Phase 2.6B — deterministic GPU fixtures

Core stages arm pane/particle scenarios (`cert/wboit_pane`, `cert/wboit_additive`) that draw through the **live** WBOIT accum pipelines during `vk_oit_pass`. After resolve, `vk_oit_lab_on_oit_resolved()` captures fog/accum/revealage/resolved HDR and records PASS/FAIL/PENDING with GPU evidence.

Frozen contracts (equations, formats, blends, revealage, weights) are **not** changed by fixtures.

### One-command core queue

```
+set r_oit 1 +set r_fbo 1
oit_certify_core
```

Advances each successful OIT frame: empty → single → revealage → alpha → weight → order (6 perms) → fog → additive → HDR → lifecycle, then `oit_certification_export`.

Requires an in-world camera (not menu-only). Check progress with `oit_lab_status` / `wboit_production_status` / `oit_cert_geometry_status`.

## Commands

| Command | Role |
|---------|------|
| `oit_certify_core` | Queue all core fixture-backed GPU stages |
| `oit_lab_list` / `oit_lab_run` / `oit_lab_run_group` | Deterministic lab |
| `oit_lab_status` / `oit_lab_reset` | Lab session |
| `oit_cert_geometry_status` | Armed fixture panes |
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

Specialized refraction/portal/weapon/bloom paths remain scaffolds until production cert.

See also [WBOIT_CERTIFICATION_THRESHOLDS.md](WBOIT_CERTIFICATION_THRESHOLDS.md), [TRANSPARENCY_ROUTING.md](TRANSPARENCY_ROUTING.md).
