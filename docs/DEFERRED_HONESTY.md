# Deferred Honesty Milestone

**Status:** Milestone 2 shipping (mixed material deferred + lightmap ownership)  
**Related:** [GBUFFER_2.md](GBUFFER_2.md) · [RENDERER_PATH_OWNERSHIP.md](RENDERER_PATH_OWNERSHIP.md) · [SHARED_BRDF.md](SHARED_BRDF.md)

---

## Architecture (`r_deferredArchitecture`, latched)

| Value | Name | Role |
|------:|------|------|
| **0** | `HYBRID_ADDITIVE_DEFERRED` | Forward+/legacy writes **SceneBaseLit**; deferred adds clustered dynamics. Compatibility / reference. |
| **1** | `MIXED_MATERIAL_DEFERRED` | Eligible surfaces export **unlit** G-buffer; deferred owns **lightmap static + dynamics**. Ineligible stay Forward+. **Production target.** |
| **2** | `STRICT_DEFERRED_VALIDATION` | Same mixed path; non-translatable / invalid surfaces are **UNSUPPORTED** (no silent Forward+). |
| **3** | `DEFERRED_COMPARISON` | Mixed deferred vs Forward+ (split via hybridCompare). |

### `HYBRID_ADDITIVE_DEFERRED` (0)

| Owner | Role |
|-------|------|
| Forward+ / legacy fragment | **SceneBaseLit** — lightmaps, vertex primary, IBL-in-fragment where applicable |
| Deferred compute | **DeferredDynamicLighting** — clustered point/spot (+ area) on top |
| Composite | `CombinedSceneHDR ≈ SceneBaseLit + DeferredDynamic` |

### `MIXED_MATERIAL_DEFERRED` (1)

| Owner | Role |
|-------|------|
| Eligible opaque fragment | **GBufferBaseColor** (unlit), normal, metal/rough, **lightmap irradiance** packed into G-buffer; `out_color` = unlit (albedo copy) |
| Deferred compute | `static = albedo × (1−metal) × lightmap` + clustered dynamics; **owner mask** in lighting `.a` |
| Ineligible opaque | Full Forward+ **SceneBaseLit**; deferred writes zero |
| Composite | Owned pixels **replace** with deferred HDR; others keep scene base |

Packing (owned pixels): `material = (metal, rough, lm.r, lm.g)`, `normal.a = lm.b + 1024` (owner bias).

---

## Eligibility

Authoritative API: `R_GetDeferredEligibility()` in `vk_deferred_honesty.c`.

| Result | Tint (`r_deferredEligibilityDebug 1`) | Path |
|--------|----------------------------------------|------|
| `ELIGIBLE_FULL` | green | Deferred opaque (native PBR) |
| `ELIGIBLE_APPROXIMATE` | yellow | Certified translated classic |
| `FORWARD_FALLBACK` | blue | Forward+ opaque |
| `UNSUPPORTED` | magenta | Sky / weapon / transparent / portal / refractive (strict: also failed classic) |
| `DEBUG_FORCED` | red | `r_deferredForceEligibility` |

Non-translatable classic multi-stage / env / animated shaders **do not** enter deferred by accident.

**Mixed export gate:** `MIXED_MATERIAL_DEFERRED` only takes deferred ownership when the draw can use the gbuf/PBR fragment path (`hasPBR` or stage `vk_pbr_flags`). Certified classics without that path remain Forward+ (no black holes). Hybrid arch 0 can still light them via SceneBaseLit.

---

## Classic translation (certified subset)

`R_TranslateClassicShaderToMaterial()` accepts:

- single diffuse stage
- optional lightmap stage
- optional alpha-test
- optional PBR maps on that stage (normal / physical / emissive)

Rejects (→ Forward+ or UNSUPPORTED in strict): multi-diffuse stages, complex tcMods, env maps, animation/video, deforms, portals, blends, transmission/refraction.

Console: `material_translate_status <shader-name>`

---

## Cvars / commands

| Name | Role |
|------|------|
| `r_deferredArchitecture` | 0 hybrid · **1 mixed** · 2 strict · 3 compare (latched) |
| `r_deferredCompositeMode` | 0 additive · 1 full replace · 2 side-by-side · 3 material validate |
| `r_deferredEligibilityDebug` | Eligibility false-color |
| `r_gbufferInvalidPolicy` | 0 magenta · **1 Forward+** · 2 unlit diag |
| `r_gbufferCompact` | Storage layout; mixed owned pixels override compact packing |
| `deferred_status` | Honesty counters + architecture label |
| `material_translate_status` | Per-shader translation dump |

---

## Missing (later phases)

- Full sun BRDF in compute (today: CSM visibility modulate)
- Shared lobed parity (IBL / sheen / SSS) in deferred
- Dedicated lightmap MRT (today: packed into material/normal)
- Compact decode parity tests
- OA material validation map captures
- Double-shade removal is done for **eligible** mixed pixels; hybrid arch 0 still additive by design

---

## Regression

`tests/scripts/test_deferred_honesty.sh` · `tests/scripts/test_deferred_eligibility.sh` · `tests/scripts/test_deferred_mixed_material.sh`
