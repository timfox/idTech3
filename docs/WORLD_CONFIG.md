# World Config (Map-State Transitions)

Named **world configurations** swap alternate geometry, nav tiles, spawn layouts, and lighting keys for large world sections without a full `map` reload. Server-authoritative via `CS_ENGINE_WORLD_CONFIG`.

Requires `USE_OPEN_WORLD` (default on). Builds on [OPEN_WORLD.md](OPEN_WORLD.md) sector overlays.

## Quick start

```text
seta r_worldConfigEnable 1
seta sv_worldConfigEnable 1
world_config_load
world_config siege
world_config_validate all
```

Demo: `exec demo_world_config.cfg` / `exec vulkan_overlay_world_config.cfg`.

## Manifest (`world/<map>.wcfg`)

```text
config default
config siege
config night_raid

siege.geometrySuffix _siege
siege.navSuffix _siege
siege.spawnLayout siege
siege.ndgiTime 0.65
siege.bounds mins -1024 -1024 0 maxs 1024 1024 256
siege.sightline A 0 0 64 B 512 0 64 clear

night_raid.geometrySuffix _night
night_raid.spawnLayout night
night_raid.ndgiTime 0.9
```

| Asset | Prefer | Fallback |
|-------|--------|----------|
| Collision / visual BSP | `maps/sector_X_Y_siege.bsp` | `maps/sector_X_Y.bsp` |
| Nav | `nav/sector_X_Y_siege.nav` | `nav/sector_X_Y.nav` |
| Scatter / spawns | `sprites/layout_siege.ents` | `sprites/sector_X_Y.ents` |
| Lighting | `r_ndgi_time` ← `ndgiTime` | unchanged |

## Replication

| Configstring | Payload |
|--------------|---------|
| `CS_ENGINE_WORLD_CONFIG` | `"name generation"` e.g. `siege 3` |

Server: `sv_worldConfigEnable 1`, `world_config <name>`. Clients apply via `CL_WorldConfig_OnConfigstring` (reload resident sectors + NDGI time + temporal epoch bump).

## Spawn layouts

- Map entities: `info_director_spawn` with `layout`, `spawn_type`, `min_intensity`, `max_intensity`
- `world_config_spawnlayout <name>` switches layout without remeshing
- Lua: `Engine.WorldConfig.set/get/list/spawnLayout/validate`

## Validation

`world_config_validate [name|all]`:

- Bounds AABB floor probes (CM)
- Sightlines (`clear` / `blocked`)
- Spawn walkability traces for the config’s layout

## Cvars

| Cvar | Default | Role |
|------|---------|------|
| `r_worldConfigEnable` | 0 | Client enable |
| `sv_worldConfigEnable` | 0 | Server publish / authority |
| `r_worldConfig` / `sv_worldConfig` | default | Active name |
| `r_worldConfigEpoch` | 0 | ROM; temporal sticky reset |
| `r_worldConfigGeoSuffix` / `NavSuffix` | | ROM; published for renderer |

## Console

| Command | Purpose |
|---------|---------|
| `world_config <name>` | Activate config |
| `world_config_list` | List configs |
| `world_config_load [path]` | Load `world/<map>.wcfg` |
| `world_config_spawnlayout <name>` | Layout-only switch |
| `world_config_validate [name\|all]` | Sightlines / bounds / spawns |

## Related

- [OPEN_WORLD.md](OPEN_WORLD.md), [DISTRICTS.md](DISTRICTS.md), [NEURAL_DYNAMIC_GI.md](NEURAL_DYNAMIC_GI.md), [EDITOR_BRIDGE.md](EDITOR_BRIDGE.md)
