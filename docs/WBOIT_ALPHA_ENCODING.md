# WBOIT Alpha Encoding (Phase 2.2)

**Status:** Source encoding ≠ internal accumulation form.  
**Code:** `vk_oit_alpha.{h,c}`, `oit_source_normalize.glsl`, `oit_accum.frag`  
**Commands:** `oit_alpha_status`, `oit_alpha_validate`, `material_alpha_status`, `classic_alpha_translate_status`

Parent: [COLOR_PIPELINE.md](COLOR_PIPELINE.md) · Contract: [WBOIT_CONTRACT.md](WBOIT_CONTRACT.md) · Normalization: [WBOIT_SOURCE_NORMALIZATION.md](WBOIT_SOURCE_NORMALIZATION.md) · Authoring: [TRANSPARENT_TEXTURE_AUTHORING.md](TRANSPARENT_TEXTURE_AUTHORING.md) · Classic: [CLASSIC_SHADER_ALPHA_TRANSLATION.md](CLASSIC_SHADER_ALPHA_TRANSLATION.md)

---

## Source encodings

| Enum | WBOIT? | Meaning |
|------|--------|---------|
| `OIT_SOURCE_ALPHA_STRAIGHT` | yes | RGB unassociated; α = opacity |
| `OIT_SOURCE_ALPHA_PREMULTIPLIED` | yes | RGB already × α; divide for unassociated |
| `OIT_SOURCE_ALPHA_OPAQUE` | n/a | α = 1 |
| `OIT_SOURCE_ALPHA_ADDITIVE` | no | additive bucket / reveal write-mask off |
| `OIT_SOURCE_ALPHA_MASKED` | no | alpha-tested path |
| `OIT_SOURCE_ALPHA_MULTIPLICATIVE` | no | filter / modulate path |
| `OIT_SOURCE_ALPHA_UNKNOWN` | warn | treated as straight + warning |

## Internal representation (production WBOIT)

```text
unassociatedRadiance  = scene-linear lit radiance (before × opacity)
opacity               = alpha in [0,1]
associatedRadiance    = unassociatedRadiance * opacity

accum.rgb += unassociatedRadiance * opacity * weight
accum.a   += opacity * weight
reveal    *= (1 - opacity)
```

Do **not** call the internal value “premultiplied color” when referring to pre-accum radiance. The frozen contract’s `premultipliedRadiance` flag means the **accum buffer** already stores associated weighted color.

## Material declaration

`material_alpha_status <shader>` prints encoding, reason, path, eligibility.

Reasons: `EXPLICIT_STRAIGHT`, `EXPLICIT_PREMULTIPLIED`, `CLASSIC_COMPATIBILITY`, `INFERRED_UNSAFE`, `UNKNOWN`, `NOT_WBOIT_COMPATIBLE`.

Default: classic `SRC_ALPHA / ONE_MINUS_SRC_ALPHA` → straight. Name hint `premul` → premultiplied. `r_oitSourceAlphaDefault 1` forces premul (unsafe inference).

## Debug / policy

| Cvar | Role |
|------|------|
| `r_alphaDebug` 5–12 | edge / associated / unassociated / opacity / emissive views |
| `r_transparentEdgePolicy` 0–3 | preserve / zero RGB at α=0 / edge-safe / diagnostic |
| `r_alphaFilterDebug` | filter/mip sampling diagnostics |
| `r_alphaEncodingCompare` | straight vs premul compare |
| `r_oitSingleLayerCompare` | source-over vs WBOIT |
| `r_oitFault*` | association fault injection |

## Certification levels

`OIT_ALPHA_DECLARED` → `NORMALIZED` → `ACCUMULATION_VALID` → `OIT_ALPHA_EDGE_CERTIFIED`

`oit_alpha_validate` reaches **EDGE_CERTIFIED** when the frozen contract validates and no association faults are active.
