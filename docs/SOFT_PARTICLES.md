# Soft Particles (Phase 2.6)

Shared certified positive view-depth metric (`depth_view.glsl` / reversed-Z linearize).

## Cvars

| Cvar | Meaning |
|------|---------|
| `r_softParticleRange` | Fade range in **world units** |
| `r_softParticleMinFade` | Minimum retained opacity |
| `r_softParticleQuality` | 0–2 quality tier |

Avoid raw device-depth fades, depth inversion, excessive distant thickness, and fading against sky / no-depth pixels.
