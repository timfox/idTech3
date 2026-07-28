# Modules (chocolate layer)

Optional gameplay systems gated by `IDTECH3_PROFILE` / `USE_*` flags.

| Path | Shim | Typical gate |
|------|------|--------------|
| `modules/world/` | `src/world` | `USE_OPEN_WORLD`, `USE_ARC_BLANC`, … |
| `modules/navigation/` | `src/navigation` | `USE_RECAST_NAV` |
| `modules/physics/` | `src/physics` | `phys_enabled` |
| `modules/audio/` | `src/audio` | always (core) |
| `modules/botlib/` | `src/botlib` | always (dedicated server + `bot_enable`) |
| `modules/rts/` | `src/rts` | `USE_RTS_SIM` |

CMake: `IDTECH3_DIR_MODULE_*` in [`cmake/IdTech3Layout.cmake`](../cmake/IdTech3Layout.cmake).
