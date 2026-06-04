# idTech3Radiant (hybrid editor)

Maps and BSP remain authored in **idTech3Radiant** (or compatible GtkRadiant fork). In-engine **Studio** tools (`r_studio_tools`) handle session, entity preview, and Radiant snippet export—not full BSP editing.

## Install

1. Clone [idTech3Radiant](https://github.com/ArtmeScienceLab/idTech3Radiant) beside the engine, or use your distro package.
2. Point Radiant **gamepack** entity defs at keys in [EDITOR_BRIDGE.md](EDITOR_BRIDGE.md):
   - `misc_billboard`, `misc_flipbook`, `misc_imposter`, `misc_decal`
3. Optional submodule (when wired): `./scripts/init_optional_submodules.sh --radiant`

## Sync with engine

Shared parser: [engine_sprite_map.c](../src/qcommon/engine_sprite_map.c), [engine_decal_map.c](../src/qcommon/engine_decal_map.c).  
Studio **Entities** panel lists parsed props and writes `studio_exportents.cfg`.

## Editor contract checklist

Single source of truth for engine-native map props (verify before shipping a conversion mod):

| Step | Radiant entityDef | Engine parser | Studio export |
|------|-------------------|---------------|---------------|
| 1 | Keys match [EDITOR_BRIDGE.md](EDITOR_BRIDGE.md) `misc_*` table | `EngineSpriteMap_Parse` / `EngineDecalMap_Parse` | Entities panel lists same class names |
| 2 | `shader` path uses `scripts/...` qpath | CS catalog `CS_ENGINE_*_SHADERS` | Snippet uses identical shader string |
| 3 | `radius`, `angles` / `pitch`/`yaw` units | `entityState` fields in [MOD_SDK.md](MOD_SDK.md) | Preview radius matches in-engine |
| 4 | Rebuild pk3 after entityDef change | `vid_restart` or reconnect | `studio_exportents.cfg` diff reviewed |

**Classes:** `misc_billboard`, `misc_flipbook`, `misc_imposter`, `misc_decal` — see [MOD_SDK.md](MOD_SDK.md) replication table.
