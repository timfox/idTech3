# Scene 06 - Vulkan vs OpenGL parity

## Goal

Same **map, textures, cvars**: catch **fallback-only** bugs (water, fog, post, decals).

## Layout

- **Single compact map** combining: water or reflective surface (if applicable), fog volume, lit mesh, sky.
- Prefer assets that exist on **both** backends.

## Pass criteria

- No **missing** surfaces on OpenGL that appear on Vulkan (or vice versa) except documented unsupported features.
- Rough **brightness** match (exact match optional; major hue/gamma shifts are not).

## Procedure

1. `+set cl_renderer vulkan` (or project equivalent) - capture reference.
2. `+set cl_renderer opengl` - same position, same `r_*` snapshot.
3. File issues for intentional deltas only (document in [RENDERERS.md](../../../RENDERERS.md) if permanent).
