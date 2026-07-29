# Deferred Honesty Milestone

**Status:** Milestone 3 — lighting parity + dedicated SurfaceData (MIXED_MATERIAL_DEFERRED)  
**Related:** [GBUFFER_2.md](GBUFFER_2.md) · [RENDERER_PATH_OWNERSHIP.md](RENDERER_PATH_OWNERSHIP.md) · [SHARED_BRDF.md](SHARED_BRDF.md)

---

## Architecture (`r_deferredArchitecture`, latched)

| Value | Name | Role |
|------:|------|------|
| **0** | `HYBRID_ADDITIVE_DEFERRED` | Forward+/legacy writes **SceneBaseLit**; deferred adds clustered dynamics. Compatibility / reference — **not** full deferred. |
| **1** | `MIXED_MATERIAL_DEFERRED` | Eligible surfaces export **unlit** G-buffer; deferred owns **lightmap static + dynamics**. Ineligible stay Forward+. **Production target.** |
| **2** | `STRICT_DEFERRED_VALIDATION` | Mixed path; non-translatable / invalid → **UNSUPPORTED** (no silent Forward+). |
| **3** | `DEFERRED_COMPARISON` | Mixed deferred vs Forward+ (split). |

Opt-in config: `exec modern_deferred_mixed.cfg` (then `vid_restart`). Default remains arch **0** until OA parity captures pass.

### Logical resources

| Name | Meaning |
|------|---------|
| **GBufferBaseColor** | Unlit diffuse × rgbGen modulation only (albedo attachment / color copy for owned) |
| **GBufferNormal** | World-space shading normal; compact: `.a` = AO |
| **GBufferMaterial** | Expanded: `(metal, rough, AO, clearcoat)`; compact: `(metal, rough, oct.xy)` |
| **GBufferSurfaceData** | Dedicated MRT: `.rgb` = lightmap / static irradiance, `.a` = ownership `1` / `0` |
| **GBufferEmissive** | Material emissive (fragment path; not mixed into base) |
| **GBufferValidity** | CPU/GPU flags: `GBUFFER_VALID_*`, `APPROXIMATED`, `TRANSLATED_CLASSIC`, `PBR_NATIVE`, `USING_LIT_SCENE_AS_BASE` |
| **GBufferOwnership** | `PIXEL_OWNER_*` + lighting `.a` owner mask in mixed mode |
| **SceneBaseLit** | Full Forward+/legacy lit opaque (arch 0 base; Forward+ fallback in arch 1) |
| **DeferredLightingHDR** | Compute output (dynamics-only in arch 0; static+dynamic for owned in arch 1) |
| **ForwardFallbackHDR** | Scene color for ineligible opaques |
| **CombinedSceneHDR** | Ownership composite result |

### Mixed packing (owned pixels) — SurfaceData

Lightmap and ownership live in **GBufferSurfaceData** (`R16G16B16A16_SFLOAT` MRT location 4):

- `surface.rgb` = static lightmap / irradiance export
- `surface.a` = `1` owned, `0` unowned (clears are unowned)

Material and normal keep documented semantics (no `material.ba` LM pack; no `normal.a + 1024` owner bias).

### Lightmap decode

Shared helper: `shaders/glsl/lightmap_decode.glsl`. Decoded BSP lightmaps are a **scene-linear irradiance / baked diffuse radiance proxy** after `lightmap_scale` (overbright), optional sRGB decode. Not exposure, fog, or tonemap. Static term:

`staticDiffuse = baseColor × (1 − metal) × lightmapIrradiance × AO`

---

## Eligibility

`R_GetDeferredEligibility()` — tint via `r_deferredEligibilityDebug`.

| Result | Path |
|--------|------|
| `ELIGIBLE_FULL` | Native PBR deferred |
| `ELIGIBLE_APPROXIMATE` | Certified translated classic (gbuf path required in mixed) |
| `FORWARD_FALLBACK` | Forward+ |
| `UNSUPPORTED` | Sky / weapon / transparent / portal / refractive (strict: failed classic) |

**Mixed export gate:** needs gbuf/PBR fragment path (`hasPBR` or stage `vk_pbr_flags`).

---

## Classic translation

Certified: one diffuse ± lightmap ± alpha-test ± optional PBR maps.

**rgbGen audit:** `identity`, `identityLighting`, `vertex`, `exactVertex`, `const`, `entity` only. Others → `BASE_COLOR_EXPORT_UNREPRESENTABLE` → Forward+.

Audit bits: `BASE_COLOR_STAGE_VALID`, `BASE_COLOR_VERTEX_MODULATION_VALID`, `BASE_COLOR_CONSTANT_MODULATION_VALID`, `LIGHTMAP_STAGE_VALID`.

Legacy material defaults (visible approximation): `r_legacyDeferredRoughness` (0.72), `r_legacyDeferredSpecular` (0.04 F0), flagged `GBUFFER_APPROXIMATED`.

`material_translate_status <shader>` prints stage, rgbGen, tcGen, LM, logical base color, eligibility, owner.

---

## Cvars / commands

| Name | Role |
|------|------|
| `r_deferredArchitecture` | 0–3 latched |
| `r_gbufferInvalidPolicy` | 0 magenta · **1 Forward+** · 2 unlit |
| `r_legacyDeferredRoughness` / `r_legacyDeferredSpecular` | Classic defaults |
| `r_deferredLightmapMode` | 0 irradiance · 1 deluxe (when available) · 2 compare |
| `r_deferredLightmapDebug` | 1–5 LM debug |
| `r_deferredOwnershipDebug` | 1–3 ownership / double / unowned |
| `r_deferredCompositeDebug` | 1–4 composite inputs |
| `r_deferredArchitectureCompare` | arch0 vs arch1 split helper |
| `deferred_status` | Full M2/M3 counters |
| `material_translate_status` | Per-shader translation |

---

## `deferred_status` counters

architecture · eligible · true-G-buffer · additive-hybrid · Forward+ fallback · SceneBaseLit pixels · GBufferBaseColor pixels · deferred LM · Forward+ LM · **doubleShaded** · **unowned** · **invalidGBuffer**

In `MIXED_MATERIAL_DEFERRED`, double-shaded and unowned should stay **0**.

---

## Milestone 3 — lighting parity + SurfaceData

### Shipping

- Full sun BRDF in compute: Burley + GGX + multiscatter + clearcoat lobe; CSM on primary only
- Sky IBL: BRDF LUT + prefilter + irradiance (`r_deferredIbl`, bindings 12–14); LM owns diffuse ⇒ skip sky diffuse
- **GBufferSurfaceData** MRT + lighting binding 15 — replaces overloaded `material.ba` / `normal.a+1024` pack
- Compact octahedral decode works with mixed (oct in `material.ba`, AO in `normal.a`)
- Expanded mixed clearcoat from `material.a` (compact clearcoat still Forward+)
- Shared local lights: Forward+ tile SSBOs + Burley/GGX/`clearcoat_lobe`
- Deluxe mode: `r_deferredLightmapMode 1/2` uses a conservative directional approximation from lightmap energy + dominant sun direction until a true deluxe-vector channel ships

### Remaining / deferred

- Local reflection / irradiance probe pick (sky cubemap only for now)
- Compact clearcoat channel (extension buffer) — coat materials **Forward+** when compact
- True packed deluxe-vector decode in compute (current mode is approximate)
- GPU material-export / lightmap parity live stats
- Interactive OA capture matrix
- Promote arch 1 to default only after live OA validation

### Cvars (M3)

- `r_deferredSunBrdf 1` (default): evaluate directional sun in mixed deferred
- `r_deferredIbl 1` / `r_deferredIblStrength`: sky split-sum IBL for owned deferred pixels
- Lightmap owns static diffuse (`sunFlags` bit1): sun/IBL add specular without double-baking diffuse
- CSM modulates `staticTerm + sunTerm` only — clustered locals stay undarkened (Forward+ primary parity)
- `r_deferredLightingParity` reserved for difference views

### Local-light / lobe parity

| Term | Deferred mixed | Notes |
|------|----------------|-------|
| Point / spot / attenuation | Shared tile lists | Same `attenPointLight` / spot packing |
| Area (LTC) | Shared when LTC bound | VRCS path skips area |
| Sun CSM | Primary only | Locals not multiplied by sunVis |
| Clearcoat | Expanded (incl. mixed) | Compact → Forward+ |
| Sheen / anisotropy | Forward+ | Explicit eligibility gate |

---

## Recommended next after M3 parity

Promote arch 1 to `modern_clustered.cfg` only after live OA validation, then **GPU-driven scene + meshlets** (stop expanding deferred).

---

## Configs / tests

- `config/modern_deferred_mixed.cfg`
- `config/demo_deferred_material_export.cfg`
- `config/demo_deferred_lightmap_parity.cfg`
- `tests/scripts/test_deferred_*.sh` / `test_gbuffer_*.sh` / `test_lightmap_parity.sh` / `test_material_export_parity.sh`
