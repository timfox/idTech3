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
