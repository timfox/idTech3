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
