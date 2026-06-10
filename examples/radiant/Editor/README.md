# idTech3Radiant bridge (s&box Editor/ folder)

Editor-only Python scripts run from **idTech3Radiant → Tools → Python Script Editor**.
They read `game.idproj` from the mod root and bridge to the in-engine Studio panel.

| Script | Purpose |
|--------|---------|
| `bridge_tools.py` | Print manifest paths; launch engine; show `studio_exportents.cfg` |
| `watch_studio_export.py` | Poll `studio_exportents.cfg` when Studio exports entities |

Install into your mod with `./scripts/install_radiant_gamepack.sh <mod_dir>`.
