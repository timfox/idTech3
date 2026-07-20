# Raster Ultra 1.4 — Production Transparency, Particles, Decals, Effects

Continuation of [RASTER_ULTRA_1.3.md](RASTER_ULTRA_1.3.md). **RT remains locked off.**

**Certification:** experimental / quality opt-in. Boot default stays `modern_vulkan.cfg`.

## Enable

```
exec modern_raster_ultra.cfg
vid_restart
```

## Transparency routing

| Class | Pass |
|-------|------|
| alpha tested | Opaque / stochastic clip |
| sorted alpha | Classic blend when `r_oit 0` |
| WBOIT | Compatible α-blend (`r_oit 1`) |
| additive | OIT classify bucket 2 (WBOIT, no moments) |
| modulate | Sorted / classic |
| refractive / water / glass | **Excluded from OIT**; sorted after resolve (`r_refractiveExcludeOit 1`) |
| particle | Additive classify or GPU soft splat |
| decal | Deferred G-buffer (not OIT) |
| UI / weapon | Never world OIT |

Debug: `transparency_route_status`, `r_transparencyDebug`, `r_oitDebug`.

## WBOIT (certified for Ultra)

| Item | Value |
|------|-------|
| Accum | `R16G16B16A16_SFLOAT`, clear 0 |
| Reveal | `R16_SFLOAT`, clear 1 |
| Math | `C_avg = rgb/max(a,ε)`; `out = C_avg*(1-R) + bg*R` |
| Depth | Test on, write off, reversed-Z `GREATER_OR_EQUAL` |
| Lighting | Forward+ tiles (`r_oitForwardPlus 1`) |
| Safety | NaN/Inf → magenta; luminance soft-cap |
| MBOIT | Optional (`r_oit 2`) — **not** Ultra-certified |

Refraction does **not** use WBOIT. screenMap/water/glass draw sorted after resolve and must not sample unresolved OIT, UI, weapon, or tonemap.

## GPU particles

- Compute update (lifetime, velocity, gravity, drag, wind)
- Soft depth-aware splat into HDR
- Simplified ambient lighting (not full opaque BRDF)
- Reactive stamp when mask allocated
- Commands: `gpu_particles_status`, `gpu_particles_burst`
- Cvars: `r_gpuParticles`, `r_gpuParticlesDemo`, `r_softParticles`

CPU `cl_particles` still works via polys; GPU path is the Ultra producer.

## Deferred decals

- Apply **after G-buffer capture, before deferred lighting**
- Modifies albedo / normal (RNM) / roughness / metallic
- Max 64 volumes; `deferred_decal_spawn` / `deferred_decal_status`
- Requires `R16G16B16A16` albedo with STORAGE usage
- Transparent surfaces excluded initially

Legacy `R_MarkFragments` / `misc_decal` preserved.

## Distortion

- RGBA16F offset+mask buffer
- Depth-aware UV reject; max pixel clamp (`r_distortionMaxPixels`)
- Seeds from heat particles / `distortion_pulse`
- Samples current HDR scene only (not UI / tonemap)

## Reactive mask

Contributors: OIT reveal stamp, Forward+/stochastic, GPU particles, distortion.

Ultra forces allocation via `r_reactiveMaskForce 1` even with TAA off so the buffer is ready for future reconstruction. Correctness does **not** require TAA.

## Pass / resources

Existing Spine OIT + reactive passes; FX compute runs after G-buffer / after GI as documented in backend order.

## Validation (static)

```
./scripts/raster_ultra_1_3_check.sh
./scripts/raster_ultra_1_4_check.sh
```

GPU soak (resize / vid_restart / effects) not measured in CI.

## Promotion

| Feature | Class |
|---------|--------|
| WBOIT + classify + refractive exclude | **quality opt-in** (Ultra) |
| MBOIT | experimental |
| GPU particles / soft splat | **experimental** |
| Deferred decals | **experimental** |
| Distortion | **experimental** |
| Boot default | unchanged |

## Highest-impact next failure

Deferred decals when color format ≠ `R16G16B16A16_SFLOAT`, or soft-particle overdraw cost on heavy bursts — profile `gpu_particles_burst` under Ultra before promoting particles.
