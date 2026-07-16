# idTech3Radiant (hybrid editor)

Maps and BSP remain authored in **idTech3Radiant** (or compatible GtkRadiant fork). In-engine **Studio** tools (`r_studio_tools`) handle session, console, entity preview/export, material paint, and a thin animation strip—not full BSP editing. See [IN_ENGINE_STUDIO_TOOLS.md](IN_ENGINE_STUDIO_TOOLS.md).

Patterns backported from [s&box / Source-2](https://github.com/timfox/Source-2): staged bootstrap, `game.idproj` manifest, `Editor/` Python scripts, VS Code workspace generation, and a file-bridge for Studio entity export.

## Quick start

```bash
./scripts/bootstrap.sh all                    # engine + shaders + demo + radiant gamepack
./scripts/new_mod_from_template.sh game.minimal mygame ./release/mygame
./scripts/install_radiant_gamepack.sh ./release/mygame
./scripts/generate_radiant_workspace.sh ./release/mygame
./scripts/clone_radiant.sh                    # optional: clone editor beside engine
```

## Install idTech3Radiant

1. Clone [idTech3Radiant](https://github.com/ArtmeScienceLab/idTech3Radiant) beside the engine, or run `./scripts/clone_radiant.sh`.
2. Build per upstream [COMPILING](https://github.com/ArtmeScienceLab/idTech3Radiant).
3. Install the engine gamepack fragment into your mod:

   ```bash
   ./scripts/install_radiant_gamepack.sh ./release/mygame
   ```

4. Copy `mygame/idtech3.game` into Radiant `gamepacks/` **or** set **Preferences → Game → Engine path** to `release/`.
5. Point Radiant **fs_game** at your mod ident (`mygame`).

Entity defs ship in `scripts/entities_idtech3_bridge.def` (keys from [EDITOR_BRIDGE.md](EDITOR_BRIDGE.md)).

## Project manifest (`game.idproj`)

Same file drives engine IDE launch **and** Radiant Editor scripts (s&box `.sbproj` analogue):

| Field | Radiant use |
|-------|-------------|
| `Ident` | `fs_game` |
| `Metadata.StartupMap` | Build menu “Test in engine” / VS Code launch |
| `EditorScripts` | Python scripts under mod `Editor/` |
| `Radiant.EntityDef` | Bridge entity definition path |
| `Radiant.MapSrcDir` / `MapsDir` | Recommended map layout |

See [MOD_MANIFEST.md](MOD_MANIFEST.md).

## Studio ↔ Radiant bridge (file IPC)

No native IPC yet—uses shared files (s&box-style separation without replacing Radiant):

```
idTech3Radiant (.map)  →  q3map2  →  .bsp  →  engine
        ↑                                        ↓
   paste entity block                    Studio / Entities
        ↑                                        ↓
   Editor/bridge_tools.py              studio_exportents.cfg
```

1. In-engine: `r_studio_tools 1` → **Studio / Entities** → export selection → `studio_exportents.cfg`
2. In Radiant: **Tools → Python Script Editor** → run `Editor/bridge_tools.py` → paste block into map
3. Optional: `Editor/watch_studio_export.py` polls the cfg file while both tools are open

## Build menu

`default_build_menu.xml` (installed into mod root) adds:

- **Draft:** fast BSP / light
- **Test:** launch `idtech3` with `r_studio_tools 1` and current map
- **Entities:** `-onlyents` / `-exportents`

Customize via Radiant **Build → Customize**.

## Sync with engine

Shared parser: [engine_sprite_map.c](../engine/core/engine_sprite_map.c), [engine_decal_map.c](../engine/core/engine_decal_map.c).
Studio **Entities** panel lists parsed props and writes `studio_exportents.cfg`.

Run `./scripts/sync_editor_bridge.sh` after editing [EDITOR_BRIDGE.md](EDITOR_BRIDGE.md) entity tables.

## Editor contract checklist

Single source of truth for engine-native map props (verify before shipping a conversion mod):

| Step | Radiant entityDef | Engine parser | Studio export |
|------|-------------------|---------------|---------------|
| 1 | Keys match [EDITOR_BRIDGE.md](EDITOR_BRIDGE.md) `misc_*` table | `EngineSpriteMap_Parse` / `EngineDecalMap_Parse` | Entities panel lists same class names |
| 2 | `shader` path uses `scripts/...` qpath | CS catalog `CS_ENGINE_*_SHADERS` | Snippet uses identical shader string |
| 3 | `radius`, `angles` / `pitch`/`yaw` units | `entityState` fields in [MOD_SDK.md](MOD_SDK.md) | Preview radius matches in-engine |
| 4 | Rebuild pk3 after entityDef change | `vid_restart` or reconnect | `studio_exportents.cfg` diff reviewed |

**Classes:** `misc_billboard`, `misc_flipbook`, `misc_imposter`, `misc_decal` — see [MOD_SDK.md](MOD_SDK.md) replication table.

## What we did not port from Source-2

| s&box feature | idTech3 approach |
|---------------|------------------|
| Hammer in-process | Keep idTech3Radiant for BSP |
| Roslyn hotload | Lua `com_scriptWatch` + Radiant Python |
| `.scene` prefabs | `.map` + entity defs |
| Quake mount shaders | Native pk3 / q3map2 pipeline |
