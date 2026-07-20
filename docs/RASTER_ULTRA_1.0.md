# Raster Ultra 1.0

High-end **raster-only** rendering for HavenRP. Ray tracing remains optional and
is **forced off** while `r_rasterUltra 1`.

**Certification:** foundation sprint — **experimental / quality opt-in**. Not the
boot default. Certified stable remains `modern_vulkan.cfg` → `modern_vulkan_stable.cfg`
(mode 2 Forward+).

## Enable

```
exec modern_raster_ultra.cfg
vid_restart
```

Recovery: `exec modern_vulkan.cfg` then `vid_restart`.

## Foundation sprint (this milestone)

| Signal | Raster owner |
|--------|----------------|
| Opaque lighting | Deferred compute (mode 3) |
| Transparent / weapon lighting | Forward+ clustered |
| Light assignment | Depth-partitioned clusters (`r_forwardPlusZSlices 8`, log slices) |
| Ambient visibility | GTAO (`r_ambientVisibilityMode 2`) |
| Diffuse GI | Lightmaps + SH/IBL (baseline) |
| Specular reflections | Off in foundation; SSR/probe later |
| Sun shadows | Raster `r_pbrSunShadow` (single cascade) |
| Local shadows | Atlas / cubemap scaffold (not fully budgeted) |
| AA | SMAA (`r_aaMode 2`); TAA off |
| Specular AA | Forward+ `ApplySpecularAA` + deferred screen-space variance |
| RT | Locked off (`r_rasterUltra` enforce) |

## CVars

| CVar | Role |
|------|------|
| `r_rasterUltra` | Latched contract; forces RT masters to 0 |
| `r_havenrpProfile` | `raster_ultra` / `low_latency` / `raster_reference` / … |
| `r_forwardPlusDebug` | `0` off; `0.08–1` occupancy heatmap (Z-aware); `2` Z-slice colors |
| `r_pbr_specularAA` | Normal-variance roughness inflate |

## Profiles

| File | Purpose |
|------|---------|
| `modern_vulkan.cfg` | Certified stable (unchanged) |
| `modern_clustered.cfg` | Mode 3 + Z clusters; RT reasserted off |
| `modern_raster_ultra.cfg` | Raster Ultra foundation |
| `modern_low_latency.cfg` | Mode 2 + SMAA, minimal extras |
| `modern_raster_reference.cfg` | Material/lighting reference (AO/bloom/AA off) |
| `modern_experimental.cfg` | Alias of kitchen-sink experimental |

## Diagnostics

```
havenrp_renderer_status
```

Look for:

```
rasterUltra: active=1 completeness=complete rtReq=0 rtEff=0
RT: … requested=0 effective=0
```

`completeness=rt_leak` means an RT master or selective hybrid owner is still on.

## Explicit non-goals (foundation)

- Cascaded shadow maps / contact shadows
- Probe-grid / screen-space GI certification
- WBOIT / water / planar reflections certification
- GPU-driven cull / meshlets / present-time adaptive recon
- Frame generation
- Any RT path

## Static gate

```
./scripts/raster_ultra_1_0_check.sh
```

## Promotion policy

Promote subsystems into `modern_raster_ultra.cfg` only after Spine reliability
gates (validation, resize, vid_restart, soak). Keep unfinished work in
`modern_experimental.cfg`.
