# Scene 06 - Vulkan reference baseline

## Goal

Same **map, textures, cvars** across restarts and resolution changes: catch **init**, **postFX**, and **volumetric** regressions on the Vulkan path.

## Layout

- **Single compact map** combining: water or reflective surface (if applicable), fog volume, lit mesh, sky.
- Prefer assets that exercise the full Vulkan feature set you ship (PBR, fog, postFX).

## Pass criteria

- No **missing** surfaces or black frames after `vid_restart` and resolution toggles.
- Rough **brightness** stable across sessions (major hue/gamma shifts are failures unless documented).

## Procedure

1. Load `rtest_parity` with default Vulkan renderer.
2. Note `r_*` snapshot; capture reference screenshots if your team uses visual diff tooling.
3. `vid_restart`, change `r_mode` or windowed/fullscreen, reload the map — compare to reference.
4. File issues for intentional deltas only (document in [RENDERERS.md](../../../RENDERERS.md) if permanent).
