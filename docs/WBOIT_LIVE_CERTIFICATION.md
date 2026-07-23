# WBOIT Live Certification (Phase 2.6)

**Status:** Controller + laboratory + specialized route scaffolds landed.  
**Production level:** granted only after live GPU stages + soak — **not** by static grep gates alone.

## Commands

| Command | Role |
|---------|------|
| `oit_certify_wboit` | Legacy B0–B7 operator matrix |
| `oit_certification_status` | Legacy levels + Phase 2.6 production level |
| `wboit_production_status` | Stage matrix + `WBOIT_*_CERTIFIED` levels |
| `oit_cert_stage pass\|fail\|skip <STAGE>` | Record live GPU stage outcomes |
| `oit_certification_capture [name]` | Arm capture root |
| `oit_certification_abort` | Reset pending stages; re-run CPU contract gates |
| `oit_soak_wboit` | Long soak; clean completion marks `WBOIT_CERT_SOAK` |

## Stages

```text
WBOIT_CERT_CONTRACT
WBOIT_CERT_RESOURCES
WBOIT_CERT_EMPTY_PIXEL
WBOIT_CERT_SINGLE_LAYER
WBOIT_CERT_REVEALAGE
WBOIT_CERT_ORDER_STABILITY
WBOIT_CERT_ALPHA_ENCODING
WBOIT_CERT_DEPTH
WBOIT_CERT_FOG
WBOIT_CERT_ADDITIVE
WBOIT_CERT_HDR_RESOLVE
WBOIT_CERT_EXPOSURE
WBOIT_CERT_LIFECYCLE
WBOIT_CERT_SOAK
```

Each stage reports: result, observed, threshold, failing material/region, contract hashes, resource generations, capture path.

## Levels

| Level | Requirement |
|-------|-------------|
| `WBOIT_STATIC_CERTIFIED` | Module registered; foundation contracts validate |
| `WBOIT_GPU_CORE_CERTIFIED` | Contract + resources + empty + single + revealage + alpha + order PASS |
| `WBOIT_FOG_HDR_CERTIFIED` | Core + depth + fog + additive + HDR resolve + exposure PASS |
| `WBOIT_LIFECYCLE_CERTIFIED` | Fog/HDR + lifecycle PASS |
| `WBOIT_PRODUCTION_CERTIFIED` | Lifecycle + soak PASS **and** legacy `LIVE_FULL` |

## Frozen equations

Do **not** retune WBOIT weight coefficients during live certification. Coefficient changes require contract version bump, hash update, shader rebuild, static + live recert.

## Debug

- `r_oitCertificationDebug` 1–4 — empty mask / fog_scene / resolved / difference  
- `r_oitRevealageDebug` 1–4 — GPU / expected / difference / additive contamination  

## Laboratory

See `r_transparencyReference`, `r_transparencyFreeze`, `transparency_lab_status`, [TRANSPARENCY_ROUTING.md](TRANSPARENCY_ROUTING.md).

## Thresholds

See [WBOIT_CERTIFICATION_THRESHOLDS.md](WBOIT_CERTIFICATION_THRESHOLDS.md).
