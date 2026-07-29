# Game Runtime

The game runtime keeps ABI-facing headers at `runtime/game/` and groups
implementation files by domain.

| Path | Contents |
|------|----------|
| `middleware/` | Director, GOAP, behavior tree, horde, response, choreography, facial/FACS, animgraph, dismemberment, AIML/EDA |
| `systems/` | engine save/quest/telemetry systems and entity/world bridge |
| `scripting/` | Lua bindings and generated include snippets |
| `ecs/` | EnTT-backed native ECS prototype |

Headers such as `g_public.h`, `bg_public.h`, and `g_*.h` remain in the
root to preserve the C-facing game/module include surface.

## Boundary Rules

- Keep trap/public ABI headers at `runtime/game/`.
- Put implementation `.c` / `.cpp` files in a domain folder.
- Prefer flat includes (`g_director.h`, `phys_bullet.h`, `world_config.h`)
  resolved by CMake include directories over bridge-relative includes.
- Keep Lua binding snippets in `scripting/`; generated or manually maintained
  `.inc` files should travel with `g_lua_bindings.c`.
- Keep middleware optional through
  [`cmake/modules/ClientGameAiSources.cmake`](../../cmake/modules/ClientGameAiSources.cmake).

## Validation

Focused checks after moving or adding game runtime files:

```bash
cmake -S . -B build-vk-Release
cmake --build build-vk-Release --target client -j2
./tests/scripts/test_module_include_canonical.sh
./tests/scripts/test_facs_facial.sh
./tests/scripts/test_engine_save.sh
./tests/scripts/test_physics.sh
```
