# WBOIT Weight Contract (Phase 2.5.1)

**Status:** Frozen production weight function for WBOIT accum (`r_oit 1`).  
**Code:** `vk_oit_weight_contract.h` / `.c` · GLSL `oit_weight.glsl`  
**Commands:** `oit_weight_status`, `oit_weight_validate`  
**Version:** `OIT_WEIGHT_CONTRACT_VERSION` **1** + `contractHash`

Parent: [COLOR_PIPELINE.md](COLOR_PIPELINE.md) · [WBOIT_CONTRACT.md](WBOIT_CONTRACT.md) · Depth: [DEPTH_CONTRACT.md](DEPTH_CONTRACT.md)

**Do not** change coefficients without bumping the version and re-running resolve certification.

---

## Modes

| Mode | Role |
|------|------|
| `OIT_WEIGHT_ALPHA_REFERENCE` | Laboratory — weight from opacity only |
| `OIT_WEIGHT_LEGACY_DEPTH` | Diagnostic alias of pre-named McGuire form |
| `OIT_WEIGHT_BOUNDED_PRODUCTION` | **Production default** |
| `OIT_WEIGHT_MATERIAL_RESEARCH` | Reserved — not Spine production |

---

## Frozen `oitWeightContract_t` (BOUNDED_PRODUCTION)

| Field | Value |
|-------|-------|
| `minWeight` | `0.01` |
| `maxWeight` | `3000` |
| `alphaExponent` | `3` |
| `depthExponent` | `3` |
| `depthScale` | `0.9` |
| `minimumOpacityContribution` | `0.01` |
| `nearClamp` | `8` |
| `farClamp` | `8192` |
| `usesPositiveViewDepth` | **1** |

Implicit frozen scales (documented, matched in GLSL): `alphaGain=10`, `lumaScale=1e3`.

---

## Equation

```text
zTrad   = saturate( (viewDepth - near) / (far - near) )   # certified +view depth
aFactor = pow( min(1, alpha * 10) + 0.01, 3 )
zFactor = pow( 1 - zTrad * 0.9, 3 )
w       = clamp( aFactor * 1e3 * zFactor, 0.01, 3000 )
accum   = (lit * alpha * w, alpha * w)
```

Shader: `OitWeight_BoundedProduction` in `oit_weight.glsl` (included by `oit_accum.frag`).

---

## Related Phase 2.5 policies

- **Additive separation:** `r_oitClassify 1` (production default) — ONE/ONE materials use additive accum pipeline with reveal write-mask off.
- **Excluded from ordinary WBOIT:** modulate/filter, refractive/screenMap, UI, decals (`transparency_route_status`).
- **Resolve:** [HDR_RESOLVE_INTEGRITY.md](HDR_RESOLVE_INTEGRITY.md) — fog_scene opaque base, empty → opaque.

---

## Print / validate

```text
oit_weight_status
oit_weight_validate
```

Static gate: `tests/scripts/test_oit_weight_contract.sh`. Unit: `unit_oit_weight`.
