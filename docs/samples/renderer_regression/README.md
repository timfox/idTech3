# Renderer visual regression pack

Small, **purpose-built** scenes for catching renderer regressions (tangents, PBR, emissive, fog, postFX, Vulkan baseline). This repo ships **specifications** only; BSP/map sources live in your game project. Build maps from the scene docs (or equivalent layouts), pack them into a `.pk3`, and install under `base/` next to the engine.

## Scenes (build 4–6 maps or one multi-room map)

| ID | Doc | Focus |
|----|-----|--------|
| 01 | [scenes/01_tangent_normals.md](scenes/01_tangent_normals.md) | MikkTSpace / normal-map seams, mirrored UVs |
| 02 | [scenes/02_pbr_metallic_roughness.md](scenes/02_pbr_metallic_roughness.md) | Metal vs rough response, dielectric baseline |
| 03 | [scenes/03_emissive.md](scenes/03_emissive.md) | Emissive-only and emissive + base color |
| 04 | [scenes/04_volumetric_fog.md](scenes/04_volumetric_fog.md) | Froxel fog density, light shafts / sun |
| 05 | [scenes/05_postfx.md](scenes/05_postfx.md) | MSAA, SMAA, SSAO, bloom toggles |
| 06 | [scenes/06_vulkan_opengl_parity.md](scenes/06_vulkan_opengl_parity.md) | Vulkan reference baseline (restart / resolution) |
| 07 | [scenes/07_gltf_gpu_tangent_topo.md](scenes/07_gltf_gpu_tangent_topo.md) | glTF GPU tangent mode 2 (topology) vs mode 1 |

## Suggested packaged names

Use consistent names so headless checks can optional-verify them (see `OPTIONAL_GAME_ASSETS.txt`):

| Map name | Scene |
|----------|--------|
| `rtest_tangent` | 01 |
| `rtest_pbr` | 02 |
| `rtest_emissive` | 03 |
| `rtest_volumetric` | 04 |
| `rtest_postfx` | 05 |
| `rtest_parity` | 06 |

BSP path: `maps/<name>.bsp` inside the pk3.

## Content bring-up and enforcement

Until **`z_renderer_regression.pk3`** (with the six BSPs and minimal assets) lives in a real **`base/`** next to normal game data, the engine repo can only prove scripts, manifests, and GLSL syntax-not regression content or load paths.

**1. Ship the pack** - Build the six maps, pack **`z_renderer_regression.pk3`**, install into **`base/`**. Keep the pk3 minimal and version it in your **game/content** repo.

**2. File contract** - Uncomment the `maps/rtest_*.bsp` lines in [`OPTIONAL_GAME_ASSETS.txt`](OPTIONAL_GAME_ASSETS.txt), then:

```bash
GAME_BASE=/abs/path/to/base ./scripts/renderer_regression_check.sh
```

**3. Runtime contract** - After the file contract passes:

```bash
GAME_BASE=/abs/path/to/base ./scripts/renderer_regression_maps.sh
```

**4. Manual truth** - With both green, run the first full pass from [RENDERER_CONFIDENCE.md](../../RENDERER_CONFIDENCE.md): baseline on `rtest_parity`, tangents on `rtest_tangent`, PBR on `rtest_pbr`, emissive on `rtest_emissive`, fog on `rtest_volumetric`, postFX on `rtest_postfx`. Goal: surface regressions and fix or document them—not day-one perfection.

**5. Merge law (renderer-adjacent changes)** - After content is online:

```bash
./scripts/compile_engine.sh vulkan
cd build-vk-Release && ctest --output-on-failure
GAME_BASE=/abs/path/to/base ./scripts/renderer_regression_check.sh
GAME_BASE=/abs/path/to/base ./scripts/renderer_regression_maps.sh
```

Add the manual checklist when the change is visually meaningful.

**6. Stronger visual automation** - Screenshot or framebuffer capture (e.g. anchor on `rtest_parity` and `rtest_pbr`) only after the pack, both scripts, and the manual pass are stable.

## Workflow (authoring)

1. Implement geometry and materials per scene doc (GtkRadiant, NetRadiant, Blender → export, etc.).
2. Bake lighting as you normally would for your game; keep revision control on **map sources**, not necessarily on BSP in the engine repo.
3. Pack `maps/*.bsp` (+ needed textures/shaders) into `z_renderer_regression.pk3` and drop into `base/`.

## Related

- [RENDERER_CONFIDENCE.md](../../RENDERER_CONFIDENCE.md) - automated vs manual gate
- [PBR_TEXTURES.md](../../PBR_TEXTURES.md) - texture conventions
- [samples/flowmap/](../flowmap/README.md) - optional water/flow regression add-on
