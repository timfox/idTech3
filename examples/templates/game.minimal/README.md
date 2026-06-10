# game.minimal template

Minimal mod scaffold inspired by [s&box `game.minimal`](https://github.com/timfox/Source-2/tree/master/game/templates/game.minimal).

## Create a mod from this template

```bash
./scripts/new_mod_from_template.sh game.minimal mygame ./release/mygame
./scripts/generate_mod_workspace.sh ./release/mygame
```

## Manifest

`game.idproj` describes scripts, startup map, and template metadata. See [docs/MOD_MANIFEST.md](../../../docs/MOD_MANIFEST.md).

## Live coding

With a Lua-enabled engine build:

```
set com_scriptWatch 1
script_reload scripts/lua/main.lua
```

Edit `main.lua` on disk; the engine reloads tracked scripts when file mtimes change. Implement `on_hotload_destroy` / `on_hotload_create` to preserve state across reloads.
