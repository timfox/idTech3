# Runtime (client / server / game)

2026 layout alias for executables and VM-facing game glue.

| Symlink | Canonical path |
|---------|----------------|
| `runtime/client/` | `src/client/` — domain folders: `core/`, `world/`, `media/`, `platform/` |
| `runtime/server/` | `src/server/` |
| `runtime/game/` | `src/game/` — traps, Lua bindings, AI middleware |

CMake: `IDTECH3_DIR_RUNTIME_*` in [`cmake/IdTech3Layout.cmake`](../cmake/IdTech3Layout.cmake).
