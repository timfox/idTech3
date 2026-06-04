# Mod SDK Hub

Central index for **idTech3 engine mod development**. Publish this page on [idtech3.com](https://idtech3.com) alongside downloads and server ops docs.

See also [API_STABILITY.md](API_STABILITY.md) for layer cake and semver rules.

---

## Quick start

1. Build engine: `./scripts/compile_engine.sh vulkan`
2. Minimal mod shell: [MINIMAL_GAME_SHELL.md](MINIMAL_GAME_SHELL.md)
3. Example mod: [examples/demo_game/README.md](../examples/demo_game/README.md)
4. Run: `idtech3 +set fs_game yourmod +map yourmap`

---

## Engine-native props (template for all replicated props)

End-to-end: **map entity → server CS + spawn → snapshot → client bridge → renderer**.

| Stage | Sprites | Decals |
|-------|---------|--------|
| Map class | `misc_billboard`, `misc_flipbook`, `misc_imposter` | `misc_decal` |
| Shared parse | [engine_sprite_map.c](../src/qcommon/engine_sprite_map.c) | [engine_decal_map.c](../src/qcommon/engine_decal_map.c) |
| Server | `sv_engineSprites`, `sv_engineSpritesSpawn` | `sv_engineDecals`, `sv_engineDecalsSpawn` |
| CS catalog | `CS_ENGINE_SPRITE_SHADERS` | `CS_ENGINE_DECAL_SHADERS` |
| Meta CS | `CS_ENGINE_SPRITE_META` (spawn count) | `CS_ENGINE_DECAL_META` |
| Client | `cl_engineSprites` | `cl_engineDecals` |
| Renderer | `r_spriteProps`, `r_spritePropsMapParse` | `r_decalProps`, `r_decalPropsMapParse` |
| Flags | `EF_BILLBOARD`, `EF_FLIPBOOK`, `EF_IMPOSTER` | `EF_DECAL` |

**entityState contract (networked):**

| Field | Sprites | Decals |
|-------|---------|--------|
| `modelindex` | 1-based CS shader index | 1-based CS decal shader index |
| `modelindex2` | flipbook cols \| (rows<<8) | reserved |
| `generic1` | radius / 4 | radius / 4 |
| `eventParm` | flipbook fps | fade duration (ticks, 0=default) |
| `apos.trBase[0]` | rotation (degrees) | pitch (degrees) |
| `apos.trBase[1]` | — | yaw (degrees) |
| `eFlags` | `EF_*` sprite flags | `EF_DECAL` |

Console: `sprite_spawn`, `sv_sprite_spawn`, `decal_spawn`, `sv_decal_spawn`  
Lua: `Engine.Sprites.*`, `Engine.Decals.*`  
Cgame trap: `trap_EngineSpriteAddLocal`, `trap_EngineDecalAddLocal`

---

## Game module traps (qagame)

Discover via `trap_GetValue("trap_Name", buf, len)` when built with extension support.

| Trap | ID (append order) | Purpose |
|------|-------------------|---------|
| `G_ENGINE_SPRITE_SHADER_INDEX` | shader path → CS index | Register/list sprite shaders |
| `G_ENGINE_SPRITE_SPAWN` | type, shader, xyz, radius, rot, cols, rows, fps | Networked sprite ent |
| `G_ENGINE_DECAL_SHADER_INDEX` | shader path → CS index | Decal catalog |
| `G_ENGINE_DECAL_SPAWN` | shader, xyz, radius, pitch, yaw, fade | Networked decal ent |
| `G_PHYS_CHARACTER_CREATE` | capsule params | Kinematic character body |
| `G_PHYS_CHARACTER_MOVE` | handle, wishdir, jump | Character controller step |
| `G_LOC_LOOKUP` | string key → UTF-8 text | i18n ([com_loc.c](../src/qcommon/com_loc.c)) |

Full enum: [g_public.h](../src/game/g_public.h) (before `G_TRAP_GETVALUE` = 700).

---

## Cgame traps (cgame)

| Trap | Purpose |
|------|---------|
| `CG_ENGINE_SPRITE_ADD_LOCAL` | Local sprite preview (no snapshot) |
| `CG_ENGINE_DECAL_ADD_LOCAL` | Local decal preview |

---

## Cvars (engine-native features)

| Cvar | Default | Subsystem |
|------|---------|-----------|
| `r_spriteProps` | 1 | Map sprite BSP parse |
| `r_spritePropsMapParse` | 1 | Auto 0 when server spawns map sprites |
| `r_decalProps` | 1 | Map decal BSP parse |
| `cl_engineSprites` / `sv_engineSprites` | 1 | Snapshot / CS path |
| `r_studio_tools` | 0 | ImGui Studio panels |
| `r_upscale` | 0 | 0=off, 1=renderScale, 2=FSR2 (experimental) |
| `com_crashReportURL` | "" | Opt-in crash POST (no PII) |
| `sv_pureSigned` | 0 | Require valid `pk3.sig` sidecars |
| `sv_interestMaxDist` | 0 | 0=off; distance cull after PVS |
| `sv_sectorURL` | "" | Base URL for sector pk3 autodownload |
| `com_loc_language` | "en" | i18n table selection |

Grouped lists: renderer [RENDERERS.md](RENDERERS.md), Lua [LUA_API.md](LUA_API.md), physics [PHYSICS.md](PHYSICS.md).

---

## Lua `Engine.*` tables

| Table | Functions |
|-------|-----------|
| `Engine.Sprites` | `spawnLocal`, `spawnServer` |
| `Engine.Decals` | `spawnLocal`, `spawnServer` |
| `Engine.Character` | `create`, `move`, `destroy` |
| `Engine.AnimGraph` | `load`, `setState`, `update` |
| `Engine.Telemetry` | `record`, `get` |
| `Engine.VDB` | volumetric grids |

Reload: `script_reload` (requires `USE_LUA=ON`).

---

## Authoring

| Tool | Role |
|------|------|
| [idTech3Radiant](RADIANT.md) | BSP, entities, lighting (primary map editor) |
| In-engine Studio (`r_studio_tools`) | Session, console, **Entities** panel, **Animation** panel |
| [EDITOR_BRIDGE.md](EDITOR_BRIDGE.md) | Shared entity keys (Radiant ↔ engine) |
| [TILED.md](TILED.md) | Zone metadata (optional) |

---

## Animation

- **Playback:** glTF / IQM clips via `refEntity_t.frame` / `oldframe` / `backlerp`
- **Graph:** `animgraph/*.json` — [ANIMGRAPH.md](ANIMGRAPH.md)
- **Retarget / mocap:** `scripts/retarget_skel.py`, `tools/mocap_bvh_to_gltf.py`

---

## Security & shipping

| Feature | Doc |
|---------|-----|
| Signed pk3 | `pk3.sig` SHA-256 integrity sidecar (`sha256=<hex>`); `com_pk3Signed` / `sv_pureSigned` |
| Pure server | Classic `sv_pure` + CRC |
| Crash reports | `com_crashReportURL` — [CRASH_REPORTING.md](CRASH_REPORTING.md) |
| Anti-cheat hooks | [ANTICHEAT_INTEGRATION.md](ANTICHEAT_INTEGRATION.md) |

---

## Testing

```bash
cd build-vk-Release
ctest --output-on-failure
./scripts/renderer_regression_check.sh
./scripts/gpu_golden_capture.sh --compare  # optional GPU host
```

---

## Submodule / external repos

| Repo | Purpose |
|------|---------|
| [idTech3Radiant](RADIANT.md) | Map editor |
| [FreeUSD](../src/external/FreeUSD) | USDA mesh/scene |
| [Tiled](../tools/tiled) | 2D zones |

---

## Version history (network)

| Change | Version |
|--------|---------|
| `eFlags` wire width 19 → 24 bits | Engine 2.0+ (document in CHANGELOG) |
| `CS_ENGINE_*` catalogs | Engine 1.x+ |
