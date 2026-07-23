# WBOIT Contract (Phase 2.1 Freeze)

**Status:** Frozen production configuration for Weighted Blended OIT (`r_oit 1`).  
**Code:** `vk_oit_contract.h` / `vk_oit_contract.c`  
**Commands:** `oit_contract_status`, `oit_contract_validate`  
**Version:** `OIT_CONTRACT_VERSION` (currently **1**) + `contractHash`

Parent color order: [COLOR_PIPELINE.md](COLOR_PIPELINE.md). Fog ownership: [WBOIT_FOG_LAYERS.md](WBOIT_FOG_LAYERS.md). GPU soak: [WBOIT_GPU_CERTIFICATION.md](WBOIT_GPU_CERTIFICATION.md).

**Do not** change fields without bumping `OIT_CONTRACT_VERSION` and updating this document.

## Source vs internal (Phase 2.2)

| Concept | Meaning |
|---------|---------|
| Source encoding | How the **texture/material** stores RGB vs α (`oitSourceAlphaEncoding_t`) |
| Internal sample | `unassociatedRadiance` + `opacity` after `NormalizeOitSource` |
| Accum buffer | Already stores `(radiance×opacity×w, opacity×w)` — associated weighted |

See [WBOIT_ALPHA_ENCODING.md](WBOIT_ALPHA_ENCODING.md). The frozen blend/resolve equations below are unchanged.

---

## Frozen `oitContract_t`

| Field | Production value |
|-------|------------------|
| `sourceAlphaEncoding` | **straight** (material α before WBOIT weight) |
| `accumulationMode` | **weighted color** `out = (lit × α × w, α × w)` |
| `revealageMode` | **∏(1−α)** — shader writes `α`; blend implements product |
| `weightMode` | **BOUNDED_PRODUCTION** (`oitWeightContract_t`) |
| `sceneLinear` | **true** (`SCENE_LINEAR_HDR`) |
| `preExposed` | **false** (accum/resolve before exposure) |
| `premultipliedRadiance` | **true** (accum RGB already × α × w) |
| `fogAppliedPerFragment` | **true** when `r_oitFogMode ≥ 1` (default) |
| `accumFormat` | `R16G16B16A16_SFLOAT` |
| `revealageFormat` | `R16_SFLOAT` |
| `accumClear` | `(0, 0, 0, 0)` |
| `revealageClear` | `1.0` |
| accum blend | **ONE / ONE** (additive) |
| reveal blend | **ZERO / ONE_MINUS_SRC_COLOR** |
| depth | reversed-Z **GREATER_OR_EQUAL**, **no write** |
| empty pixel | preserve opaque (never black) |
| additive particles | reveal write-mask **off** (`r_oitClassify 1`) |

---

## Equations

```text
# Accum (oit_accum.frag)
lit'   = lit * Tfog          # Tfog = exp(-density * viewDepth) when fog mode ≥ 1
w      = OitWeight_BoundedProduction(alpha, viewDepth, zNear, zFar)
       # see docs/WBOIT_WEIGHT_CONTRACT.md — clamp [1e-2, 3e3], positive view-depth
accum  = (lit' * alpha * w, alpha * w)   # RT0 ONE/ONE
reveal = alpha                            # RT1 → product(1-alpha) via blend
# Additive ONE/ONE: reveal write-mask off (r_oitClassify 1)

# Resolve (oit_resolve.frag)
if empty(accum): C_out = C_opaque
C_avg  = accum.rgb / max(accum.a, eps)
C_out  = C_avg * (1 - reveal) + C_opaque * reveal
```

`C_opaque` is fogged SceneHDR (`fog_scene`) sampled at resolve — **no second full-screen fog** on the transparent result.

---

## Print / validate

```text
oit_contract_status
oit_contract_validate
```

`oit_status` also prints `contract: WBOIT vN hash=0x…`.

Static gate: `tests/scripts/test_oit_contract.sh` (foundation consolidation).
