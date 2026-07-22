# Shared BRDF Core

**Status:** Foundation Consolidation — single PBR microfacet library for all shading paths.  
**Source:** `renderers/vulkan/shaders/glsl/pbr_brdf_core.glsl` · **Compare:** `vk_shading_compare.c`

---

## Ownership

| Consumer | Include | Notes |
|----------|---------|-------|
| Forward+ / OIT | `forward_plus_light_eval.glsl` | Tile/cluster lights + WBOIT |
| Deferred | `deferred_lighting_common.glsl` | Compute lighting |
| Generic opaque | `gen_frag.tmpl` | Classic Forward+ shade path |

**Do not fork** NDF / visibility / Fresnel formulas in path-specific shaders. Wrap with path-local aliases only.

---

## Data flow

```text
Material params (albedo, roughness, metal, N, V, L)
  → PbrDiffuseBurley / PbrD_GGX / PbrVisibilitySmithGGX / PbrFresnelSchlick
  → (optional) PbrSpecularAARoughness (Toksvig)
  → path-specific composite (direct + indirect + emissive)
r_shadingCompare 1–6 → visualize deferred vs Forward+ / diff / BRDF terms
```

---

## Buffer formats

BRDF evaluation is **analytic** — no dedicated BRDF buffer. Inputs come from G-buffer textures, uniform push constants, or varyings. Compare modes may write false-color to SceneHDR overlay (cheat cvar).

---

## Lifecycle

- Shader compile: `pbr_brdf_core.glsl` included at SPIR-V generation via `compile_shaders.sh`.
- Runtime toggles: `r_shadingCompare`, `r_pbr`, classic material fallback bypasses PBR core.
- Parity tests: `tests/scripts/test_pbr_brdf_core.sh`, `tests/scripts/test_brdf_parity.sh`.

---

## Fallback behavior

- Classic lighting materials skip PBR core (legacy Lambert / specular).
- `r_shadingCompare 1` — deferred only; `2` — Forward+ only; `3` — split; `4` — diff heatmap.
- Diff tolerance target: **≤2% luminance RMSE** on material spheres lab; **≤0.01** N·L response delta on grazing angles (manual / lab capture).

---

## Debug commands

| Cvar / command | Role |
|----------------|------|
| `r_shadingCompare` | 0 off, 1 deferred, 2 Forward+, 3 split, 4 diff, 5 BRDF terms, 6 material path |
| `shading_compare_status` | Active modes, deferred state, gbuffer gen |
| `r_pbr` | Master PBR enable |

---

## Performance cost

Shared include — **zero** extra runtime cost vs duplicated formulas. Compare modes add one full-screen or split pass when cheat enabled (~0.1–0.3 ms).

---

## Known limitations

- Clearcoat / sheen / anisotropy extensions not in core — Forward+ specialized paths.
- Energy compensation simplified (Kulla/Conty-style) for metals only.
- Compare mode 6 (material path) requires both deferred and Forward+ active same frame.

---

## Next milestone hooks

- Automated pixel diff in reference lab BRDF scene.
- Wire `r_shadingCompare 4` threshold into CI soak.
- Extend core for thin-film / cloth lobes behind feature flags.

Regression: `tests/scripts/test_brdf_parity.sh` · `tests/scripts/test_pbr_brdf_core.sh`
