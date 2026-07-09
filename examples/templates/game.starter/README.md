# game.starter template

Opinionated starter shell for teams who want a recognizable default project instead of a blank folder.

## What this gives you

- branded `gameinfo.txt` + `autoexec.cfg`
- Lua hot-reload entrypoint in `scripts/lua/main.lua`
- starter localization in `loc/en.loc`
- asset pipeline config for predictable staging and packaging
- simple dev launch + package scripts
- `game.idproj` metadata for IDE / Radiant tooling

## Create a project

```bash
./scripts/create_starter_game.sh mygame ./release/mygame
```

Or manually:

```bash
./scripts/new_mod_from_template.sh game.starter mygame ./release/mygame
./scripts/generate_mod_workspace.sh ./release/mygame
```

## First workflow

```bash
./run_dev.sh
./pack_game.sh
```

Then in the console:

```text
starter_status
lua_run starter_boot()
lua_run starter_checkpoint()
```

## Project shape

- `autoexec.cfg` sets the default startup flow
- `scripts/lua/main.lua` is the main gameplay shell
- `loc/en.loc` is the starter string table
- `asset_pipeline.conf` tells `scripts/asset_pipeline.sh` how to package the mod
- `run_dev.sh` launches the engine with `fs_game`, `com_scriptWatch 1`, and your startup map
- `pack_game.sh` produces a staged build and shipping `.pk3`
