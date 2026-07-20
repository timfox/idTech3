# Raster Ultra 1.1 — Cinematic Raster Lighting

Continuation of [RASTER_ULTRA_1.0.md](RASTER_ULTRA_1.0.md). **RT remains locked off.**

**Certification:** experimental / quality opt-in. Boot default stays `modern_vulkan.cfg`.

## Enable

```
exec modern_raster_ultra.cfg
vid_restart
```

Requires latched shadow recreate when changing `r_sunShadowCascades`.

## Lighting ownership (1.1)

| Signal | Owner |
|--------|--------|
| Directional shadow | Raster CSM atlas (`r_sunShadowCascades` 1–4) |
| Local shadow | Atlas scaffold (volumetric; lighting sample pending) |
| Contact shadow | Not shipping yet |
| Direct opaque | Deferred compute |
| Direct transparent | Forward+ |
| Indirect diffuse | Lightmaps + SH/IBL |
| Indirect specular | Forward+ env/probe; deferred terminal pending |
| Ambient visibility | GTAO |
| RT | Locked (`r_rasterUltra`) |

## Cascaded sun shadows

| CVar | Default | Role |
|------|---------|------|
| `r_sunShadowCascades` | `1` | Latched; Ultra sets `4` |
| `r_sunShadowDistance` | `0` | Max distance (0 → fog max / zFar) |
| `r_sunShadowSplitLambda` | `0.75` | PSSM mix linear↔log |
| `r_sunShadowCascadeBlend` | `0.08` | Overlap blend |
| `r_sunShadowStable` | `1` | Texel snap |
| `r_sunShadowDebug` | `0` | `2` = log splits |

Implementation: 2×2 depth atlas when cascades > 1; per-cascade viewport render; UBO packs 4 matrices + splits; Forward+ samples with blend.

## Materials (1.1)

| Lobe | Forward+ | Deferred |
|------|----------|----------|
| Substrate MR | yes | yes |
| Specular AA | yes | yes |
| Clearcoat | yes | packed in G-buffer `material.a` + GGX coat |
| Anisotropy | yes | not yet |
| Sheen / iridescence / flakes | Forward+ only | pending |

## Explicit gaps (next vertical)

1. ~~Contact-shadow supplement~~ *(still open — see Ultra lighting)*  
2. Soft-particle overdraw cost under heavy `gpu_particles_burst` *(1.4 follow-up)*  
3. LTC rectangular area lights  
4. Deferred IBL / probe terminal specular  
5. GPU soak for WBOIT × particles × distortion  
6. ~~Diffuse probe GI + SSGI~~ → [RASTER_ULTRA_1.3.md](RASTER_ULTRA_1.3.md)  
7. ~~WBOIT / particles / decals / distortion~~ → [RASTER_ULTRA_1.4.md](RASTER_ULTRA_1.4.md)

## Static gate

```
./scripts/raster_ultra_1_0_check.sh
./scripts/raster_ultra_1_1_check.sh
```
