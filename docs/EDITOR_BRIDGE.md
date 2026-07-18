# Engine-Editor Bridge

Entity definitions and key/value conventions shared between the
id Tech 3 engine and idTech3Radiant editor for feature parity.

For optional in-engine **Studio** panels (Session, Console, Entities, Paint, Animation) that surface this document at runtime, see **[IN_ENGINE_STUDIO_TOOLS.md](IN_ENGINE_STUDIO_TOOLS.md)** (`r_studio_tools`).

## Entity Key/Value Reference

### Volumetric Fog (`worldspawn` keys)

Already supported in the editor via volumetric_fog.cfg.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `vfog_density` | float | 0.02 | Fog density |
| `vfog_heightFalloff` | float | 0.04 | Height-based falloff |
| `vfog_heightOffset` | float | 0 | Fog floor height |
| `vfog_colorR/G/B` | float | 0.5/0.55/0.65 | Fog color |
| `vfog_scatter` | float | 1.0 | Scattering intensity |
| `vfog_anisotropy` | float | 0.6 | Phase function anisotropy |
| `vfog_noiseScale` | float | 0.003 | Turbulence noise scale |
| `vfog_windX/Y/Z` | float | 1/0/0.2 | Wind direction |

### Director Zones (`info_director_zone`)

| Key | Type | Description |
|-----|------|-------------|
| `targetname` | string | Zone name |
| `mins` | vec3 | Zone AABB minimum |
| `maxs` | vec3 | Zone AABB maximum |
| `threat` | int | 0=none, 1=low, 2=medium, 3=high, 4=extreme |
| `budget_mult` | float | Spawn budget multiplier |

### DMM Destructible Objects (`func_destructible`)

| Key | Type | Description |
|-----|------|-------------|
| `dmm_material` | int | 0=wood, 1=glass, 2=thin_metal, 3=thick_metal, 4=concrete, 5=stone, 6=ice, 7=plastic, 8=cloth, 9=rubber, 10=flesh |
| `dmm_health` | float | Hit points before fracture |
| `dmm_fracture_mode` | int | 0=voronoi, 1=radial, 2=splinter, 3=shatter, 4=slice, 5=crumble, 6=tear, 7=peel |
| `dmm_min_fragments` | int | Minimum fragment count |
| `dmm_max_fragments` | int | Maximum fragment count |
| `dmm_break_sound` | string | Sound on destruction |
| `dmm_stress_sound` | string | Sound on stress/damage |

### Projected Lights (`light_projected`)

| Key | Type | Description |
|-----|------|-------------|
| `origin` | vec3 | Light position |
| `angles` | vec3 | Light direction |
| `_color` | vec3 | Light color (RGB 0-1) |
| `light` | float | Intensity |
| `cone_angle` | float | Spotlight cone angle (degrees) |
| `range` | float | Light range |
| `cast_shadows` | bool | Enable shadow casting |
| `volumetric` | bool | Enable volumetric cone |
| `volumetric_density` | float | Fog density in cone |
| `cookie` | string | Projected texture shader |

### Cloth Simulation (`func_cloth`)

| Key | Type | Description |
|-----|------|-------------|
| `width` | int | Grid width (particles) |
| `height` | int | Grid height (particles) |
| `spacing` | float | Particle spacing |
| `pin_edge` | int | 0=top, 1=bottom, 2=left, 3=right |
| `wind_strength` | float | Wind force |
| `wind_dir` | vec3 | Wind direction |
| `material` | string | Cloth material shader |

### HDR Skybox (`worldspawn` keys)

| Key | Type | Description |
|-----|------|-------------|
| `skybox_hdr` | string | Path to EXR panorama |
| `skybox_hdr_exposure` | float | Exposure multiplier |
| `skybox_hdr_rotation` | float | Y-axis rotation degrees |
| `skybox_hdr_intensity` | float | IBL contribution multiplier |
| `skybox_hdr_projection` | int | 0=equirect, 1=cubemap, 2=vcross, 3=hcross, 4=spherical |

### Navigation Mesh (`info_navmesh_config`)

Already supported in the editor via navmesh overlay.

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `cell_size` | float | 0.3 | Rasterization cell size |
| `cell_height` | float | 0.2 | Rasterization cell height |
| `agent_height` | float | 2.0 | Agent height |
| `agent_radius` | float | 0.6 | Agent radius |
| `agent_max_climb` | float | 0.9 | Max step height |
| `agent_max_slope` | float | 45 | Max walkable slope |

### Background Map Camera (`info_bgmap_camera`)

| Key | Type | Description |
|-----|------|-------------|
| `origin` | vec3 | Camera position |
| `angles` | vec3 | Camera angles |
| `fov` | float | Field of view |
| `time` | float | Time in cycle (seconds) |
| `target` | string | Next camera point |

### Spawn Points (`info_director_spawn`)

| Key | Type | Description |
|-----|------|-------------|
| `origin` | vec3 | Spawn position |
| `angles` | vec3 | Spawn facing |
| `spawn_type` | string | Director spawn type name |
| `layout` | string | World-config spawn layout (`default`, `siege`, …) — see [WORLD_CONFIG.md](WORLD_CONFIG.md) |
| `min_intensity` | float | Min Director intensity to use |
| `max_intensity` | float | Max Director intensity to use |

### Choreography Marks (`info_choreo_mark`)

| Key | Type | Description |
|-----|------|-------------|
| `origin` | vec3 | Mark position |
| `targetname` | string | Reference name for scene script |
| `scene` | string | Scene name this mark belongs to |
| `actor` | string | Actor name at this position |

### Response Rule Triggers (`trigger_response`)

| Key | Type | Description |
|-----|------|-------------|
| `concept` | string | Response concept to trigger |
| `zone` | string | Zone name for context |

### Engine-native sprites (`misc_billboard`, `misc_flipbook`, `misc_imposter`)

| Key | Type | Description |
|-----|------|-------------|
| `shader` | string | Material qpath (registered into CS_ENGINE_SPRITE_SHADERS) |
| `radius` | float | Sprite size (world units) |
| `angles` | vec3 | Yaw on Z for billboard; flipbook uses cols/rows/fps keys per class |
| `cols` / `rows` / `fps` | int | Flipbook only |

Parser: [engine_sprite_map.c](../engine/core/engine_sprite_map.c). Network: [MOD_SDK.md](MOD_SDK.md).

### Engine-native decals (`misc_decal`)

| Key | Type | Description |
|-----|------|-------------|
| `shader` | string | Decal material qpath |
| `radius` | float | Decal radius |
| `pitch` / `yaw` | float | Orientation (degrees) |
| `fade` | int | Optional fade duration (ticks) |

Parser: [engine_decal_map.c](../engine/core/engine_decal_map.c).

### Soft Step props (`misc_phys_*`)

Map-placed Box3D Soft Step bodies (server spawn when `sv_physSpawn 1`).

| Class | Description |
|-------|-------------|
| `misc_phys_box` | Dynamic box |
| `misc_phys_sphere` | Dynamic sphere |
| `misc_phys_static` | Static box collider |
| `misc_phys_sensor` | Trigger volume (`isSensor`) |
| `misc_phys_slider` | Prismatic joint (static base + sliding box) |
| `misc_phys_ragdoll` | Euphoria-like Soft Step ragdoll (ProcAnim + motor) |
| `misc_phys_dmm` / `func_destructible` | DMM-like Soft Step fracture proxy |

| Key | Type | Description |
|-----|------|-------------|
| `origin` / `angles` | vec3 | Placement |
| `_size` / `size` | float | Box edge length or sphere diameter |
| `mass` | float | Dynamic mass (default 20) |
| `material` | int | `PHYS_MAT_*` id |
| `lip` / `height` | float | Slider upper travel (default 96) |
| `targetname` | string | Optional name |
| `model` / `rag` | string | Optional `.rag` / model for `misc_phys_ragdoll` |
| `dead` | int | `1` = death pose (`ProcAnim_Kill`); default live Euphoria |

Parser: [engine_phys_map.c](../engine/core/engine_phys_map.c). Server: `SV_Physics_SpawnMapEntities`. See [PHYSICS.md](PHYSICS.md).

## Material-blend vertex paint (`.paint` sidecar)

In-engine Studio / Paint (`r_materialPaint` + `r_studio_tools`) writes **`maps/<map>.paint`**.
That file is the **source of truth** for multi-material vertex weights. q3map2 vertex lighting
can overwrite BSP `drawVert` colors; keep the sidecar next to the map.

Radiant helpers in [`examples/radiant/Editor/bridge_tools.py`](../examples/radiant/Editor/bridge_tools.py):

```bash
python3 Editor/bridge_tools.py                  # status + paint path
python3 Editor/bridge_tools.py export-paint [out.paint]
python3 Editor/bridge_tools.py import-paint path/to/weights.paint
```

See [MATERIAL_BLEND.md](MATERIAL_BLEND.md).
