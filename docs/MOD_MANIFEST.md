# Mod manifest (`game.idproj`)

Inspired by [s&box `.sbproj`](https://github.com/timfox/Source-2) project manifests. One JSON file per mod describes scripts, startup map, and template metadata. Tooling reads it; the engine does not require it to boot.

## File location

Place **`game.idproj`** at the mod root (alongside `gameinfo.txt` and your `.pk3` files):

```text
mygame/
├── game.idproj
├── gameinfo.txt
├── scripts/lua/main.lua
└── z_mygame.pk3
```

## Schema (v1)

| Field | Purpose |
|-------|---------|
| `Schema` | Manifest version (currently `1`) |
| `Type` | `game` or `addon` |
| `Ident` | Mod id / `fs_game` name |
| `Title` | Human-readable title |
| `HasCode` | Mod ships script sources |
| `CodePath` | Default script root |
| `Scripts` | List of paths passed to `script_reload` on dev launch |
| `EditorScripts` | Radiant / studio Python paths (see [RADIANT.md](RADIANT.md)) |
| `Radiant` | Optional: `EngineRelease`, `EntityDef`, `MapSrcDir`, `MapsDir`, `BuildMenu` |
| `Metadata.StartupMap` | Suggested `+map` for IDE launch configs |
| `Metadata.ProjectTemplate` | Template gallery metadata |

Example: [examples/templates/game.minimal/game.idproj](../examples/templates/game.minimal/game.idproj).

## Tooling

| Script | Role |
|--------|------|
| `./scripts/new_mod_from_template.sh` | Scaffold from `examples/templates/*` |
| `./scripts/generate_mod_workspace.sh` | Emit `.vscode/launch.json` + tasks |
| `./scripts/install_radiant_gamepack.sh` | Deploy entity defs + Editor/ into mod |
| `./scripts/generate_radiant_workspace.sh` | VS Code for engine + Radiant |
| `./scripts/clone_radiant.sh` | Clone idTech3Radiant beside engine |

## Live coding (Lua)

1. Build with Lua: `./scripts/compile_engine.sh vulkan` (default `USE_LUA=ON`).
2. `script_reload scripts/lua/main.lua`
3. `com_scriptWatch 1` — reload tracked scripts when loose files change on disk.
4. Implement lifecycle hooks (s&box `IHotloadManaged` analogue):

```lua
function on_hotload_destroy()
  return my_state_table
end

function on_hotload_create(previous)
  if type(previous) == "table" then
    my_state_table = previous
  end
end
```

Hooks are optional. When present, `script_reload` calls `on_hotload_destroy` before tearing down the VM and `on_hotload_create` after reload.

## Bootstrap workflow

Mirrors s&box `Bootstrap.bat`:

```bash
./scripts/bootstrap.sh all          # engine + shaders + demo content
./scripts/bootstrap.sh engine       # compile only
./scripts/new_mod_from_template.sh game.minimal mygame ./release/mygame
./scripts/generate_mod_workspace.sh ./release/mygame
```

## Relation to `gameinfo.txt`

- **`gameinfo.txt`** — engine-native title / `requires_engine` (existing).
- **`game.idproj`** — mod tooling, IDE launch, script lists (new).

Both can coexist; tooling prefers `game.idproj` for dev workflows.
