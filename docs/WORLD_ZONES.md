# Engine spatial zones

Spatial zones are an engine-original residency and visibility hint layer. They
divide the world into bounded regions with load/unload hysteresis, a fixed
resident budget, and per-layer ownership masks. Zones are independent of legacy
BSP formats and can be populated from USDA districts, glTF, or a future world
compiler.

## Quick start

With districts (typical path):

```text
set r_district 1
set r_freeusd 1
set r_worldZones 1
district_load world/playfield.usda
district_list
exec demo_districts.cfg
```

Optional virtual texture + virtual shadows (zone-gated when a district manifest
is loaded):

```text
set r_vt 1
set r_vtFeedback 1
set r_vshadow 1
vt_status          // zone-gated feedback frames=
vshadow_status     // zone gated shadow updates skipped
```

## Contract

Each zone contains:

| Field | Role |
|-------|------|
| `boundsMin` / `boundsMax` | World AABB |
| `loadRadius` / `unloadRadius` | Activation and release distance (world units); per-zone overrides or `r_worldZoneLoadRadius` / `r_worldZoneUnloadRadius` defaults |
| `priority` | Score bias when multiple zones compete under budget |
| `residencyMask` | Bitmask of resource owners (district, texture, shadow) |
| `districtIndex` | Links zone transitions to `WorldDistrict_*` payload residency |
| `neighbors[]` | Optional adjacency indices (reserved for portal/stream prediction) |
| `state` | `INACTIVE`, `RESIDENT`, `PENDING_LOAD`, `PENDING_UNLOAD` |

`WorldZone_UpdateView` scores every active zone from the view point, marks zones
inside the load radius (or still inside unload hysteresis) as candidates, then
promotes the highest-scoring candidates until `r_worldZoneBudget` is filled.
Resident zones stay alive until distance exceeds the unload radius.

Score (approximate): inside-bounds bonus + normalized distance within load
radius + `priority × 100`.

Load/unload callbacks let district, texture, collision, and GPU page systems
own their resources explicitly. `WorldZone_IsLayerResidentAtPoint` exposes the
shared texture/shadow residency decision to renderer consumers.

## Residency layers

Bitmask constants in `modules/world/world_zone.h`:

| Bit | Constant | Owner |
|-----|----------|-------|
| `1` | `WORLD_ZONE_RESIDENCY_DISTRICT` | District proxy/full load (`WorldDistrict_ZoneLoad` / `Unload`) |
| `2` | `WORLD_ZONE_RESIDENCY_TEXTURE` | Virtual texture feedback + page binds (`vk_vt.c`) |
| `4` | `WORLD_ZONE_RESIDENCY_SHADOW` | Virtual shadow clipmap updates (`vk_vshadow.c`) |
| `7` | `WORLD_ZONE_RESIDENCY_ALL` | Default when USDA `residencyMask` is unset |

Renderer snapshot uses `REF_WORLD_ZONE_RESIDENCY_TEXTURE` and
`REF_WORLD_ZONE_RESIDENCY_SHADOW` in `renderers/common/tr_public.h` (district
bit is handled before publish).

## Cvars

| Cvar | Default | Role |
|------|---------|------|
| `r_worldZones` | `1` | Master toggle for zone scoring and callbacks |
| `r_worldZoneBudget` | `16` | Max resident zones per view (clamped to 128) |
| `r_worldZoneLoadRadius` | `1024` | Default activation radius when a zone omits `loadRadius` |
| `r_worldZoneUnloadRadius` | `1536` | Default release radius (must be ≥ load radius) |

District import falls back to `r_districtLoadRadius` (default `8192`) for
`loadRadius` when USDA omits `zoneLoadRadius`; unload defaults to `1.25 × load`.

## Frame order

Client (`cl_gameframe.c`):

1. `CL_District_Frame` → `WorldDistrict_UpdateView` → `WorldZone_UpdateView` → `CL_District_PublishZoneResidency` (`re.SetWorldZoneResidency`)
2. `CL_OpenWorld_Frame` → `WorldOpen_UpdateView` → `WorldZone_UpdateView` again, then sector residency

When both `r_district` and `r_openWorld` are on, zones update twice per frame
with the same view origin (redundant but consistent). Zone updates always run
**before** open-world sector residency (`WorldResidency_UpdateView` or legacy
disk streaming).

## District + USDA authoring

`WorldDistrict_Import` builds one zone per district. Optional prim custom data
on district assemblies:

| Key | Type | Maps to |
|-----|------|---------|
| `zoneLoadRadius` | double | Per-district load radius |
| `zoneUnloadRadius` | double | Per-district unload radius |
| `zonePriority` | double | Score bias |
| `residencyMask` | int32 | Layer mask (`7` = all layers) |

Fixture: `tests/data/usd/world_playfield.usda` (`residencyMask = 7` on sample
districts). See [DISTRICTS.md](DISTRICTS.md) for manifest layout and console
commands.

District callbacks:

- **Load** with `WORLD_ZONE_RESIDENCY_DISTRICT` set → `WorldDistrict_LoadProxy` when district is `UNLOADED`
- **Unload** → `WorldDistrict_Unload` for the linked `districtIndex`

## Vulkan renderer integration

After each district frame, `CL_District_PublishZoneResidency` copies resident zone
bounds + masks into the renderer. `RE_SetWorldZoneResidency` fans out to:

| Module | Gating behavior |
|--------|-----------------|
| `vk_vt.c` | Skips feedback drain, GPU feedback dispatch, and page ensure when no resident zone has `TEXTURE` in mask (`VT_HasResidentTextureZone`). `vt_status` reports `zone-gated feedback frames=` |
| `vk_vshadow.c` | Skips clipmap page demand when no resident zone has `SHADOW` in mask (`VShadow_HasResidentShadowZone`). `vshadow_status` reports `zone gated` skipped updates |

**Legacy scenes** (no district manifest / zero published zones): both paths treat
the snapshot as absent and allow texture/shadow work (backward compatible).

Point queries inside the renderer can use `WorldZone_IsLayerResidentAtPoint` for
texture/shadow layers without re-publishing.

Further reading: [VIRTUAL_TEXTURE.md](VIRTUAL_TEXTURE.md), [RASTER_ULTRA_1.9.md](RASTER_ULTRA_1.9.md), [SHADOW_CONTRACT.md](SHADOW_CONTRACT.md).

## Open-world + value-aware residency

Zones are orthogonal to [WORLD_RESIDENCY.md](WORLD_RESIDENCY.md) sector
cardinality budgets. Zones gate **authored district payloads** and GPU page
shadow/texture demand; `r_openWorldResidency` gates **sector cells** (collision,
nav, scatter). Both can be active: zones first, then sector planner.

## Diagnostics

| Tool | What it shows |
|------|----------------|
| `district_list` / `district_status` | District state tied to zone `districtIndex` |
| `vt_status` | `zones=N`, `zone-gated feedback frames` |
| `vshadow_status` | `zone gated` shadow update skips |
| `WorldZone_Status()` | C API printf per zone (no console command yet) |

## Source layout

| Component | Path |
|-----------|------|
| Zone manager | `modules/world/world_zone.cpp` |
| District → zone import | `modules/world/world_district.cpp` |
| Open-world hook | `modules/world/world_open.cpp` |
| Client publish | `runtime/client/world/cl_district.cpp` |
| Renderer API | `renderers/common/tr_public.h` (`SetWorldZoneResidency`) |

## Limits

- Max **128** zones (`WORLD_ZONE_MAX`); max **8** neighbors per zone (unused in selection today)
- Neighbor graph is not consulted by `WorldZone_UpdateView` yet
- No MP replication of zone residency (district/visual path is client-driven)
- `r_districtAutoFull` does not run from zone load — use `district_load_full` or proxy-only residency

## Testing

```bash
./tests/scripts/test_districts.sh
ctest -R test_districts -V
```

Smoke script asserts zone symbols, USDA `residencyMask`, `SetWorldZoneResidency`,
`VT_HasResidentTextureZone`, `VShadow_HasResidentShadowZone`, and zone-gated
counter fields in `vk_vt.c` / `vk_vshadow.c`.

## Related docs

- [DISTRICTS.md](DISTRICTS.md) — USDA manifests and district payload ownership
- [OPEN_WORLD.md](OPEN_WORLD.md) — sector streaming after zones
- [WORLD_RESIDENCY.md](WORLD_RESIDENCY.md) — submodular sector cardinality
- [FREEUSD.md](FREEUSD.md) — manifest parse path
