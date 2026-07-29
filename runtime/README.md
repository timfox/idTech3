# Runtime

`runtime/` owns executable-facing and VM-facing runtime code. These are
canonical physical source roots, not `src/*` forwarding shims.

| Path | Contents |
|------|----------|
| `runtime/client/` | client runtime split into `core/`, `world/`, `media/`, `platform/`, and `shell/` |
| `runtime/server/` | server runtime split into `core/`, `client/`, `gameplay/`, `net/`, `services/`, and `world/` |
| `runtime/game/` | native game/runtime glue split into `middleware/`, `systems/`, `scripting/`, and `ecs/`; C ABI headers stay at the root |
| `runtime/cgame/` | cgame VM-facing public interface |
| `runtime/ui/` | UI VM-facing public interface |

CMake layout variables live in
[`cmake/IdTech3Layout.cmake`](../cmake/IdTech3Layout.cmake):

- `IDTECH3_DIR_RUNTIME_CLIENT`
- `IDTECH3_DIR_RUNTIME_SERVER`
- `IDTECH3_DIR_RUNTIME_GAME`
- `IDTECH3_DIR_RUNTIME_CGAME`
- `IDTECH3_DIR_RUNTIME_UI`

Domain source manifests:

- [`cmake/client/ClientSources.cmake`](../cmake/client/ClientSources.cmake)
- [`cmake/server/ServerSources.cmake`](../cmake/server/ServerSources.cmake)
- [`cmake/modules/ClientGameAiSources.cmake`](../cmake/modules/ClientGameAiSources.cmake)
