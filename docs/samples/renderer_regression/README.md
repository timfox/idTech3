# Renderer visual regression pack

Small, **purpose-built** scenes for catching renderer regressions (tangents, PBR, emissive, fog, postFX, dual API). This repo ships **specifications** only; BSP/map sources live in your game project. Build maps from the scene docs (or equivalent layouts), pack them into a `.pk3`, and install under `base/` next to the engine.

## Scenes (build 4–6 maps or one multi-room map)

| ID | Doc | Focus |
|----|-----|--------|
| 01 | [scenes/01_tangent_normals.md](scenes/01_tangent_normals.md) | MikkTSpace / normal-map seams, mirrored UVs |
| 02 | [scenes/02_pbr_metallic_roughness.md](scenes/02_pbr_metallic_roughness.md) | Metal vs rough response, dielectric baseline |
| 03 | [scenes/03_emissive.md](scenes/03_emissive.md) | Emissive-only and emissive + base color |
| 04 | [scenes/04_volumetric_fog.md](scenes/04_volumetric_fog.md) | Froxel fog density, light shafts / sun |
| 05 | [scenes/05_postfx.md](scenes/05_postfx.md) | MSAA, SMAA, SSAO, bloom toggles |
| 06 | [scenes/06_vulkan_opengl_parity.md](scenes/06_vulkan_opengl_parity.md) | Same cvars, both renderers |

## Suggested packaged names

Use consistent names so headless checks can optional-verify them (see `OPTIONAL_GAME_ASSETS.txt`):

| Map name | Scene |
|----------|--------|
| `rtest_tangent` | 01 |
| `rtest_pbr` | 02 |
| `rtest_emissive` | 03 |
| `rtest_vfog` | 04 |
| `rtest_postfx` | 05 |
| `rtest_parity` | 06 |

BSP path: `maps/<name>.bsp` inside the pk3.

## Workflow

1. Implement geometry and materials per scene doc (GtkRadiant, NetRadiant, Blender → export, etc.).
2. Bake lighting as you normally would for your game; keep revision control on **map sources**, not necessarily on BSP in the engine repo.
3. Pack `maps/*.bsp` (+ needed textures/shaders) into `z_renderer_regression.pk3` and drop into `base/`.
4. Run through [RENDERER_CONFIDENCE.md](../../RENDERER_CONFIDENCE.md) manual list for each change that touches rendering.
5. Optional: set `GAME_BASE` and uncomment lines in `OPTIONAL_GAME_ASSETS.txt` so `./scripts/renderer_regression_check.sh` fails CI if a regression map is missing from your content tree.

## Related

- [RENDERER_CONFIDENCE.md](../../RENDERER_CONFIDENCE.md) — automated vs manual gate
- [PBR_TEXTURES.md](../../PBR_TEXTURES.md) — texture conventions
- [samples/flowmap/](../flowmap/README.md) — optional water/flow regression add-on
