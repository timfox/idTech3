# Deferred Honesty Milestone

**Status:** Phase 1–4 shipping (eligibility + hybrid naming + classic translation gate)  
**Related:** [GBUFFER_2.md](GBUFFER_2.md) · [RENDERER_PATH_OWNERSHIP.md](RENDERER_PATH_OWNERSHIP.md) · [SHARED_BRDF.md](SHARED_BRDF.md)

---

## Architecture name (current)

**`HYBRID_ADDITIVE_DEFERRED`** (`r_deferredArchitecture 0`, default)

| Owner | Role |
|-------|------|
| Forward+ / legacy fragment | **SceneBaseLit** — lightmaps, vertex primary, IBL-in-fragment where applicable |
| Deferred compute | **DeferredDynamicLighting** — clustered point/spot (+ area) on top |
| Composite | `CombinedSceneHDR ≈ SceneBaseLit + DeferredDynamic` when `r_deferredCompositeMode 0` |

This is **not** a complete deferred renderer. Do not describe it as “standard deferred opaque world.”

Resources (logical names):

| Name | Meaning today |
|------|----------------|
| SceneBaseLit | Opaque HDR after fragment shade (often lightmapped) |
| GBuffer* | Scaffold attachments; albedo channel is frequently a **copy of SceneBaseLit**, not unlit base color |
| DeferredLightingHDR | Compute dynamic contribution |
| CombinedSceneHDR | Post-composite scene |

---

## Eligibility

Authoritative API: `R_GetDeferredEligibility()` in `vk_deferred_honesty.c`.

| Result | Tint (`r_deferredEligibilityDebug 1`) | Path |
|--------|----------------------------------------|------|
| `ELIGIBLE_FULL` | green | Deferred opaque (native PBR) |
| `ELIGIBLE_APPROXIMATE` | yellow | Deferred if classic translation succeeds (hybrid) |
| `FORWARD_FALLBACK` | blue | Forward+ opaque |
| `UNSUPPORTED` | magenta | Sky / weapon / transparent / portal / refractive |
| `DEBUG_FORCED` | red | `r_deferredForceEligibility` |

Non-translatable classic multi-stage / env / animated shaders **do not** enter deferred by accident.

---

## Classic translation (certified subset)

`R_TranslateClassicShaderToMaterial()` accepts:

- single diffuse stage
- optional lightmap stage
- optional alpha-test
- optional PBR maps on that stage (normal / physical / emissive)

Rejects (→ Forward+): multi-diffuse stages, complex tcMods, env maps, animation/video, deforms, portals, blends, transmission/refraction.

Console: `material_translate_status <shader-name>`

---

## Cvars / commands

| Name | Role |
|------|------|
| `r_deferredArchitecture` | 0 hybrid · 1 mixed · 2 strict · 3 compare (latched) |
| `r_deferredCompositeMode` | 0 additive · 1 full replace · 2 side-by-side · 3 material validate |
| `r_deferredEligibilityDebug` | Eligibility false-color |
| `r_gbufferInvalidPolicy` | 0 magenta · **1 Forward+** · 2 unlit diag |
| `r_gbufferCompact` | Storage layout only (not material correctness) |
| `deferred_status` | Honesty counters + architecture label |
| `material_translate_status` | Per-shader translation dump |

---

## Missing (later phases)

- True **GBufferBaseColor** MRT (unlit) separate from SceneBaseLit
- Lightmap as deferred static term (not baked into base color)
- Full sun BRDF in compute (today: CSM visibility modulate)
- Shared lobed parity (IBL / sheen / SSS)
- Compact decode parity tests
- OA material validation map captures

---

## Regression

`tests/scripts/test_deferred_honesty.sh` · `tests/scripts/test_deferred_eligibility.sh`
