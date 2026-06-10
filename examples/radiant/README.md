# idTech3Radiant gamepack fragment

Ships entity definitions and build-menu defaults aligned with [docs/EDITOR_BRIDGE.md](../../docs/EDITOR_BRIDGE.md).
Inspired by [s&box project templates](https://github.com/timfox/Source-2/tree/master/game/templates).

## Install into a mod

```bash
./scripts/new_mod_from_template.sh game.minimal mygame ./release/mygame
./scripts/install_radiant_gamepack.sh ./release/mygame
./scripts/generate_radiant_workspace.sh ./release/mygame
```

## Point Radiant at the mod

1. Copy or symlink `idtech3.game` into your Radiant `gamepacks/` folder (or set **Preferences → Game → Engine path** to `release/`).
2. Set **fs_game** to your mod ident (`mygame`).
3. Entity defs load from `scripts/entities_idtech3_bridge.def` in the mod tree.

## Editor scripts (s&box `Editor/` analogue)

After install, mod contains `Editor/bridge_tools.py`. Open in Radiant Python Script Editor.

## Studio ↔ Radiant sync

1. In-engine: `r_studio_tools 1` → **Studio / Entities** → **Export selection** → `studio_exportents.cfg`
2. In Radiant: run `Editor/bridge_tools.py` or `watch_studio_export.py` → paste entity block into map

See [docs/RADIANT.md](../../docs/RADIANT.md).
