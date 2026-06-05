# World districts and proxy meshes

Engine-original **world partition** layer: USD-authored districts with **proxy mesh residency** and optional **BSP sector streaming** via `cm_stream`. Metadata is parsed through FreeUSD `BuildEngineSceneSnapshot` (same hybrid path as id Tech 4–8 / Northlight-style open worlds, without copying proprietary formats).

## Concepts

| Term | Meaning |
|------|---------|
| **Manifest** | Root USDA (e.g. `world/playfield.usda`) listing district assemblies |
| **District** | `kind=assembly` (or `group`) prim named `District_<Name>` |
| **Proxy mesh** | Low-poly USDA with `purpose=proxy`; loaded first for distant residency |
| **Full payload** | `world/districts/<slug>.usda` — meshes + future gameplay payloads |
| **Sector grid** | Derived from district bounds ÷ `r_districtSectorSize`; fed to `CM_Stream_LoadSector` |

### Load states

```
UNLOADED → PROXY → STREAMING → LOADED
```

- **View-driven**: `CL_District_Frame` calls `WorldDistrict_UpdateView` with `cl.snap.ps.origin` and `r_districtLoadRadius`.
- Inside radius: load proxy (`r_districtProxy` 1); within half radius: promote to full load + optional sector stream.

## USD conventions

Manifest layout (see `tests/data/usd/world_playfield.usda`):

- `/World/Districts/District_North` — `kind = "assembly"`, world bounds from composed transform.
- `/World/ProxyLayer/District_North_Proxy` — `purpose = "proxy"` (associates proxy with district by name).

Auto-resolved paths (slug = lowercase name after `District_` prefix):

| Asset | Path |
|-------|------|
| Proxy | `world/proxies/<slug>_proxy.usda` |
| Full | `world/districts/<slug>.usda` |

## Build

Requires **`USE_FREEUSD=ON`** (default) for manifest parse. Core state machine lives in `src/world/world_district.c` (qcommon); FreeUSD parse and console commands in `src/client/cl_district.cpp`.

## Console

| Command | Purpose |
|---------|---------|
| `district_load <world/playfield.usda>` | Parse manifest via FreeUSD, import districts |
| `district_list` | List districts, paths, sector ranges, state |
| `district_status [index\|District_Name]` | Detail for one district |
| `district_proxy <index\|name>` | Force proxy load (`RegisterModel` on proxy USDA) |
| `district_load_full <index\|name>` | Stream sectors + register full USDA |
| `district_unload <index\|name>` | Drop residency |

### Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `r_district` | `1` | Master toggle for district system |
| `r_districtProxy` | `1` | Load proxy meshes before full payloads |
| `cm_districtStream` | `1` | Call `CM_Stream_LoadSector` for district sector grid on full load |
| `r_districtSectorSize` | `4096` | World units per sector cell when deriving grid from bounds |
| `r_districtLoadRadius` | `8192` | View residency radius (units) |

Startup log: `[world_district] districts + proxy mesh layer initialized`.

## Quick start (demo mod)

```text
set r_district 1
set r_freeusd 1
district_load world/playfield.usda
district_list
district_proxy District_North
```

Shipped in `idtech3_demo.pk3` when built with `demo`: `exec demo_districts.cfg` from `autoexec.cfg`.

## Test assets

| File | Purpose |
|------|---------|
| `tests/data/usd/world_playfield.usda` | Two-district manifest |
| `tests/data/usd/world_proxies/*_proxy.usda` | Proxy box meshes |
| `tests/data/usd/world_districts/*.usda` | Full payload placeholders |

`ctest -R test_districts` validates sources, symbols, and fixture presence.

## Open-world integration

When **`r_openWorld 1`**, `district_load_full` calls **`WorldOpen_LoadSector`** for each cell in the district grid (collision + nav + scatter per cvars). When **`r_openWorld 0`**, full district load uses legacy **`CM_Stream_LoadSector`** (collision prefetch only). See [OPEN_WORLD.md](OPEN_WORLD.md#limitations).

## Related docs

- [OPEN_WORLD.md](OPEN_WORLD.md) — view-driven sector streaming (BSP prefetch, per-chunk nav, billboard scatter)
- [FREEUSD.md](FREEUSD.md) — mesh import and `usd_*` tools
- [COMPATIBILITY.md](COMPATIBILITY.md) — retail mod loading (unchanged)
