# Raster Ultra 1.3 — Dynamic Probe GI + Screen-Space Indirect Diffuse

Continuation of [RASTER_ULTRA_1.1.md](RASTER_ULTRA_1.1.md). **RT remains locked off** (`r_rasterUltra`).

**Certification:** experimental / quality opt-in. Boot default stays `modern_vulkan.cfg` (stable). Lightmaps remain the baked baseline; this milestone adds **dynamic delta** probes and optional **near-field SSGI**.

## Enable

```
exec modern_raster_ultra.cfg
vid_restart
```

Latched: `r_probeGi`, `r_ssgi`, `r_probeGiSpacing`, `r_probeGiMax`.

## GI ownership (`INDIRECT_DIFFUSE`)

| Surface class | Primary | Dynamic supplement | Near-field | Fallback gaps |
|---------------|---------|--------------------|------------|---------------|
| Lightmapped world | Baked lightmap (in scene color) | Probe **dynamic delta** only (`r_probeGiStaticScale` 0) | SSGI confidence lerp | SH/IBL already in forward/base |
| Dynamic objects | Probe sample (entity lighting blend) | — | SSGI on deferred pixels | Lightgrid / SH |
| Unlightmapped world | Probe ambient (raise `r_probeGiStaticScale` or mode 1) | Dynamic lights into probes | SSGI | SH/IBL |

**Do not** add lightmap + full probe L0 + SH + SSGI at unrestricted strength. Resolve uses:

```
probeIndirect = probeIrr * albedo * AO * strength * lightmapDeltaScale
ssgiIndirect  = ssgiRad * ssgiStrength          // AO not reapplied
finalIndirect = lerp(probeIndirect, ssgiIndirect, ssgiWeight)
sceneColor   += finalIndirect
```

GTAO modulates **probe** approximate ambient only. Valid SSGI skips AO (occlusion already in hit).

## Energy model

- **Static baseline + dynamic delta** (default `r_probeGiMode 0`): CPU stores lightgrid ambient in `staticL0` (muted by `r_probeGiStaticScale`, Ultra default **0**) and dlight/sky soft contribution in `L0` / L1.
- **One-bounce approximation:** SSGI only; **no** probe→probe recursion.
- **Emissive:** screen-visible bounce via SSGI; unrestricted emissive→probe flooding is not enabled.

## Probe system

| Topic | Behavior |
|-------|----------|
| Placement | Auto grid from world bounds; spacing `r_probeGiSpacing`; cap `r_probeGiMax` |
| Representation | L0 RGB + L1 SH (xyz) + skyVis + interior + age + relocate |
| Visibility | Trilinear 8-tap + normal facing + distance falloff + `r_probeGiMinVis` floor |
| Relocation | Axis offsets out of solid BSP leaves |
| Update | Budgeted (`r_probeGiBudget`); priority = distance + age + dirty |
| Cache | `maps/<map>.rgi` when `r_probeGiCache 1` |
| Commands | `probe_gi_generate`, `probe_gi_save`, `probe_gi_invalidate`, `probe_gi_inspect`, `probe_gi_status` |

## SSGI

| Topic | Behavior |
|-------|----------|
| Traversal | Current-frame cosine-hemisphere march in view space |
| Inputs | Depth, normals, scene color, albedo |
| Outputs | Radiance + confidence; meta = hit UV / distance |
| Confidence | Facing × edge × thickness × distance; edge miss rejects |
| History | **None** in 1.3 (correctness first; TAA off in Ultra) |
| Limits | Near-field / on-screen only; off-screen occluders miss |

## Debug (`r_rasterGiDebug`)

0 off · 1 probe irr · 2 SSGI · 3 confidences · 4 probe contrib · 5 SSGI contrib · 6 final indirect · 7 AO ownership · 8 leak-risk

## Pass / resource registry

Pass `raster_gi`. Resources: `probe_grid`, `probe_irradiance`, `ssgi_radiance`, `ssgi_confidence`, `indirect_diffuse`.

## Validation notes (static / engineering)

- GPU soak / 30-min GI soak: **not measured in this check-in** (headless CI).
- Expected first in-game failure to fix next: **thin-wall probe leak** or **SSGI screen-edge smear** under moving camera — use debug 8 / 3.
- Contact shadows (1.1 gap) remain open.

## Static gate

```
./scripts/raster_ultra_1_0_check.sh
./scripts/raster_ultra_1_1_check.sh
./scripts/raster_ultra_1_3_check.sh
```

## Promotion

| Feature | Class |
|---------|--------|
| Probe GI | **experimental** (quality opt-in via Ultra) |
| SSGI | **experimental** |
| Boot default | unchanged (`modern_vulkan.cfg`) |

## Related

- [RASTER_ULTRA_1.13.md](RASTER_ULTRA_1.13.md) — clipmapped radiance cache + emissive transport (experimental)
