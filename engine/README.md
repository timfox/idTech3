# Engine (vanilla core)

2026 layout root for the id Tech 3 foundation layer.

| Path | Role |
|------|------|
| `engine/core/` | qcommon — fs, cvar, cmd, vm, jobs, net |
| `engine/platform/` | SDL, unix, win32, android |

CMake: `IDTECH3_DIR_ENGINE_CORE`, `IDTECH3_DIR_ENGINE_PLATFORM` in [`cmake/IdTech3Layout.cmake`](../cmake/IdTech3Layout.cmake).

**Phase 5c:** sources are physical here. Legacy `#include` and CMake globs may still use `src/qcommon` → `engine/core` forwarding shims until Phase 5d.
