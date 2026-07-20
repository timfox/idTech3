# Raster Ultra 1.8 — Material Authoring 2.0 + Procedural Layering + Surface Evolution

Continuation of [RASTER_ULTRA_1.7.md](RASTER_ULTRA_1.7.md). **RT remains completely disabled.**

**Certification:** experimental / opt-in. Boot stays `modern_vulkan.cfg`. Ultra base does **not** force material IR / surface evolution — use the overlay. Classic Q3 `.shader` stages remain compatible by default.

## Enable

```
exec modern_raster_ultra.cfg
exec vulkan_overlay_raster_ultra_1_8_materials.cfg
vid_restart
```

Commands: `material_ir_status`, `material_graph_status`, `material_instance_status`, `material_cache_status`, `surface_evolution_status`

## Signal ownership

| Signal | Owner |
|--------|-------|
| Classic Q3 stage evaluation | `shaderStage_t` / `tr_shader.c` (unchanged default) |
| Material IR bookkeeping | `vk_material_ir` when `r_materialIR 1` |
| Controlled graph → IR | `vk_material_graph` (bounded nodes, topo-ordered) |
| Parameter overrides | `vk_material_instance` (no pipeline clones) |
| Processed cache | `vk_material_cache` (versioned; runtime compile count must stay 0) |
| Height blend / layers | Existing `materialBlend` + set-19 + `gen_frag.tmpl` |
| Surface wetness/snow/dust/rust | `vk_surface_evolution` → `pbrSurfaceEvolution` UBO |
| Weather wetness request | Ultra 1.7 `vk_weather` → evolution consumer |
| Reactive stamp | `pbrMaterialBlend.z/w` (unchanged; not evolution) |
| Shader permutations | Spec constants + feature groups ([MATERIAL_PERMUTATIONS.md](MATERIAL_PERMUTATIONS.md)) |

**No unrestricted node graph.** Graph node classes are fixed; inputs must reference lower indices only.

## Material IR

Domains: opaque, alpha-tested, transparent, water, glass, particle, decal, sky, volumetric, UI, terrain.

Static features (permutation groups): alpha test, transmission, clearcoat, anisotropy, skinning, POM, triplanar, terrain, water, decals, height blend, sheen, flowmap, evolution.

Dynamic features (instance/weather, no new SPIR-V): wetness, snow, dust, rust, soot, moss, damage.

## Controlled graph

Initial nodes: texture, constant, param, add/mul/lerp/clamp/remap/power, normal/height blend, UV transform, triplanar, world/normal masks, vertex color, layer blend.

Compiles into IR feature flags + constants. Rejects cycles and excess nodes (`VK_MAT_GRAPH_MAX_NODES`).

## Layer stack / height blend

Physically ordered stack is represented in IR layers; runtime height blend continues to use existing `materialBlend` / `layerAlbedo` / height from `normalHeightMap` alpha (see [MATERIAL_BLEND.md](MATERIAL_BLEND.md)). Ultra 1.8 does not replace that path with linear alpha for layered surfaces.

## Triplanar

`VK_MAT_FEAT_TRIPLANAR` is reserved in IR/graph. Full triplanar normal-correct sampling in `gen_frag.tmpl` remains scaffolded — do not linearly average unrelated tangent normals.

## Surface evolution

When `r_surfaceEvolution 1`:

- Wetness from weather (`vk_weather_wetness_rate`) or `r_surfaceWetness`
- Snow / dust from weather presets or overrides
- Rust / soot / moss / damage from cvars
- Indoor suppress via `vk_weather_is_outdoor_view`

Fragment policy (energy-safe):

- Wet: darken + roughness down, soft clearcoat seed — **not** perfect mirrors
- Snow: upward-biased rough diffuse
- Dust/soot: darken + roughness up
- Rust/moss/damage: color shift, **metallic reduced** (oxide is not metal)

Packed UBO `pbrSurfaceEvolution` appended after sun-shadow meta (layout-compatible append). Applied in Forward+ and deferred G-buffer export for mode-3 parity on wet/rough/metal/clearcoat.

## Permutations

Surface evolution uses **UBO only** — no new `#ifdef` FS variants. Material blend remains specialization-constant based. See [MATERIAL_PERMUTATIONS.md](MATERIAL_PERMUTATIONS.md).

## Cache

Versioned: source hash × permutation × graph version × IR cache version × compiler version. Invalidate on version mismatch. `material_cache_status` reports hit rate and `runtimeCompiles` (must stay 0).

## Classic compatibility

- Default: all Ultra 1.8 cvars **0** (except cache may latch 1 under overlay only)
- Q3 shaders, `.mtr`, `.paint`, PBR maps unchanged
- Boot / Ultra base unchanged

## Validation

```
./scripts/raster_ultra_1_8_check.sh
```

Manual: wet concrete (`r_surfaceWetness 0.7`), snowy rock + weather snow, rusty metal (`r_surfaceRust 0.6`), classic map without overlay, deferred mode 3 vs Forward+, `vid_restart`, material cache hit after reload.

## Promotion decision

| Item | Status |
|------|--------|
| Material IR | **yes** |
| Controlled graph compile | **yes** (bounded) |
| Instances | **yes** |
| Height blend (existing) | **yes** |
| Surface evolution → shader | **yes** |
| Triplanar sampling | **scaffolded** (IR flag only) |
| Offline full graph bake tools | **partial** (cache + status) |
| Unrestricted node editor | **explicitly out of scope** |
| Promote to Ultra default | **no** — overlay only |
| Boot unchanged | **yes** |

## Highest-impact fix

**Weather wetness was exported but never consumed by surfaces.** Ultra 1.8 wires `vk_weather` → `vk_surface_evolution` → `pbrSurfaceEvolution` → `gen_frag.tmpl` (and deferred export), with exclusive packing that does not collide with reactive-mask `materialBlend.z/w`.
