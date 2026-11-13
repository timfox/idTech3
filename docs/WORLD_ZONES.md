# Engine spatial zones

Spatial zones are an engine-original residency and visibility hint layer. They
divide authored world bounds into scored, budgeted regions with hysteresis, and
feed district proxy loads plus renderer texture/shadow gating. They do not
consume or reproduce any legacy map compiler, SDK, BSP extension, or
third-party source implementation.

See also [DISTRICTS.md](DISTRICTS.md) (USD manifest → one zone per district),
[OPEN_WORLD.md](OPEN_WORLD.md) and [WORLD_RESIDENCY.md](WORLD_RESIDENCY.md)
(sector streaming below zones), [VIRTUAL_TEXTURE.md](VIRTUAL_TEXTURE.md), and
[RASTER_ULTRA_1.9.md](RASTER_ULTRA_1.9.md) (GPU gating).

## Contract

Each zone contains:

- authored AABB bounds;
- load and unload radii with hysteresis;
- a priority score;
- a residency mask (district / texture / shadow owners);
- optional adjacency indices for future portal/stream prediction;
- a resident / inactive state machine.

`WorldZone_UpdateView` scores every active zone from the view origin, keeps the
top `r_worldZoneBudget` candidates inside load radius (or still inside unload
radius when already resident), then runs load/unload callbacks. Resident zones
survive until the view leaves the **unload** radius (≥ load radius), which
reduces boundary thrash.

## Residency mask bits

| Bit | `world_zone.h` | `tr_public.h` (renderer snapshot) | Effect |
|-----|----------------|-----------------------------------|--------|
| `1 << 0` | `WORLD_ZONE_RESIDENCY_DISTRICT` | — (not published to GPU) | `WorldDistrict_ZoneLoad` / `ZoneUnload` drive proxy residency |
| `1 << 1` | `WORLD_ZONE_RESIDENCY_TEXTURE` | `REF_WORLD_ZONE_RESIDENCY_TEXTURE` | Virtual texture feedback + page residency |
| `1 << 2` | `WORLD_ZONE_RESIDENCY_SHADOW` | `REF_WORLD_ZONE_RESIDENCY_SHADOW` | Virtual shadow clipmap update pass |

Default mask when unset: **all three** (`7`). Fixture manifest:
`tests/data/usd/world_playfield.usda` sets `residencyMask = 7` on each district.

## Cvars

| Cvar | Default | Role |
|------|---------|------|
| `r_worldZones` | `1` | Master toggle for zone selection (`WorldZone_UpdateView` no-ops when `0`) |
| `r_worldZoneBudget` | `16` | Max resident zones per view (clamped `1…128`) |
| `r_worldZoneLoadRadius` | `1024` | Default activation radius (world units) when USDA omits `zoneLoadRadius` |
| `r_worldZoneUnloadRadius` | `1536` | Default release radius; clamped to ≥ per-zone load radius |

Per-district USDA overrides replace the defaults; see [DISTRICTS.md](DISTRICTS.md#usd-zone-metadata).

## Frame order

Zones sit **above** sector residency and **below** the client render publish:

```
CL_District_Frame (view origin)
    └── WorldDistrict_UpdateView
            ├── WorldZone_UpdateView          ← budgeted zone load/unload
            └── legacy per-district radius    ← proxy/full promotion (r_districtLoadRadius)
    └── CL_District_PublishZoneResidency
            └── re.SetWorldZoneResidency → R_VT_SetWorldZoneResidency + vk_vshadow_set_world_zone_residency
```

`WorldZone_Init` runs from `WorldOpen_Init` so open-world and district paths
share the same manager (`modules/world/world_zone.cpp`).

District import builds one zone per district (`WorldDistrict_Import` →
`WorldZone_Import`). Zone load callbacks call `WorldDistrict_LoadProxy` when the
district mask includes `WORLD_ZONE_RESIDENCY_DISTRICT` and the payload is still
`WD_STATE_UNLOADED`.

## Renderer gating

The client publishes a **snapshot** (`worldZoneResidency_t`: bounds, mask,
`resident` flag) each district frame. When **no** zones are registered
(`count == 0`), VT and virtual shadows behave as before (gating passes through).

When zones exist:

- **Virtual texture** (`vk_vt.c`): `VT_HasResidentTextureZone` requires at
  least one **resident** zone with the texture bit. Otherwise feedback dispatch,
  `R_VT_Feedback_RequestPage` / `RequestUV`, and related paths increment
  `zone-gated feedback frames` (see `vt_status`).
- **Virtual shadows** (`vk_vshadow.c`): `VShadow_HasResidentShadowZone` gates
  the clipmap update pass; skips increment `zoneGatedUpdates` (see
  `vshadow_status`).

`WorldZone_IsLayerResidentAtPoint` answers point queries on the CPU for future
collision/texture consumers; renderer gating today uses the published snapshot,
not live point tests.

## USD authoring

Parsed in `runtime/client/world/cl_district.cpp` from district assembly prims:

| `customData` key | Type | Maps to |
|------------------|------|---------|
| `zoneLoadRadius` | float | Zone load radius |
| `zoneUnloadRadius` | float | Zone unload radius (defaults to `load × 1.25` at import if `0`) |
| `zonePriority` | float | Score bias (higher wins ties) |
| `residencyMask` | int | Bit mask (`7` = district + texture + shadow) |

If manifest bounds are empty, an authored `fullMesh` payload may be opened to
derive bounds before zones are built (Sponza benchmark path; see
[renderer_validation/SPONZA_USDA_BENCHMARK.md](renderer_validation/SPONZA_USDA_BENCHMARK.md)).

## Source layout

| Component | Path |
|-----------|------|
| Zone manager | `modules/world/world_zone.cpp`, `world_zone.h` |
| District ↔ zone glue | `modules/world/world_district.cpp` |
| USDA parse + GPU publish | `runtime/client/world/cl_district.cpp` |
| VT gating | `renderers/vulkan/vk_vt.c` (`R_VT_SetWorldZoneResidency`) |
| Shadow gating | `renderers/vulkan/vk_vshadow.c` |
| Renderer export | `renderers/common/tr_public.h` (`SetWorldZoneResidency`) |

## Diagnostics

| Tool | Output |
|------|--------|
| `vt_status` | `zones=N`, `zone-gated feedback frames` |
| `vshadow_status` | `world zones`, `zone gated` shadow update skips |
| `WorldZone_Status()` | Com_Printf per-zone bounds/state (C API; no console alias yet) |
| `district_list` / `district_status` | District state after zone-driven proxy load |

## Testing

```bash
bash tests/scripts/test_districts.sh
ctest -R unit_world_zone -V    # when built with tests enabled
```

## Limits

- Adjacency indices are stored but not yet used for prefetch.
- GPU snapshot omits the district bit; district residency is owned by
  `WorldDistrict_*` callbacks, not the renderer.
- Classic BSP-only maps never import zones (`count == 0`); gating stays open.
