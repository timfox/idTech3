# C# scripting (Mono)

Optional **in-engine C#** scripting via embedded **Mono**, alongside **Lua** and **JavaScript (Duktape)**.

## Build

```bash
# Debian/Ubuntu deps
sudo apt-get install libmono-2.0-dev mono-devel

./scripts/compile_engine.sh vulkan csharp
# or
cmake -S . -B build-vk-Release -DUSE_CSHARP=ON -DUSE_VULKAN=ON ...
```

**Default:** `USE_CSHARP=OFF` (CI and minimal builds stay unchanged).

Startup log when enabled: `C# scripting: USE_CSHARP enabled (cs_reload, scripts/csharp/)`

## Console commands

| Command | Description |
|---------|-------------|
| `cs_reload [path.cs ...]` | Compile `.cs` with `mcs` and load assembly (see `cs_compiler` cvar) |
| `cs_list` | Runtime status and tracked scripts |
| `cs_dump` | Same as `cs_list` |

## Script layout

- API: `src/qcommon/csharp/IdTech3.Engine.cs` (compiled with your script)
- Entry: `namespace Game { public static class Script { public static void Init(); public static void Frame(int msec, int realMsec); } }`
- Events: `IdTech3.Engine.On("event", (s0,s1,i0,i1) => { ... });`
- Allowed paths: `scripts/csharp/`, `gameplay/`, `client/`, `ui/`

Compiled DLL cache: `<fs_home>/vm/csharp_cache/`

## Cvars

| Cvar | Default | Purpose |
|------|---------|---------|
| `cs_autoInit` | `0` | Open Mono at startup (no scripts until `cs_reload`) |
| `cs_allowEvents` | `1` | `Engine.On` / `DispatchEvent` |
| `cs_frameCallbackBudgetMs` | `2` | Reserved for future budget enforcement |
| `cs_compiler` | `mcs` | Compiler executable |
| `cs_compatTarget` | `mono-4.7-api` | Read-only API profile label |

## Events (shared with JavaScript)

Emitted via `Com_ScriptEmitEvent`: `frame`, `map_load`, `menu_changed`, `entity_spawn`, `entity_death`, `weapon_fire`, `input_key`, `mouse_move`, `ui_open`, `ui_close`, `console_open`, `client_connect`, and cgame `trap_EmitJSEvent`.

## Demo mod

```bash
exec demo_csharp.cfg   # in idtech3_demo.pk3 when USE_CSHARP=ON
```

See `examples/demo_game/mod/scripts/csharp/demo_hooks.cs`.

## License

Mono is LGPL; engine code is GPL-2.0. Game scripts you write are separate works; do not link proprietary runtimes into `idtech3` without complying with Mono and engine licenses.
