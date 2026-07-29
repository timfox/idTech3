# Server Runtime

Server sources are grouped by runtime domain. Public server headers remain at
`runtime/server/` so legacy include sites can keep using flat names such as
`server.h`, `sv_auth.h`, and `sv_openworld.h`.

| Path | Contents |
|------|----------|
| `core/` | server init, frame/main loop, operator console commands |
| `client/` | client lifecycle, snapshots, bans/filtering |
| `gameplay/` | game VM bridge, bots, server-owned engine entities, physics hooks |
| `net/` | server netchan helpers |
| `services/` | auth, App CRDT, TV/stream services |
| `world/` | world linking, open-world hooks, world config |

CMake source ownership lives in
[`cmake/server/ServerSources.cmake`](../../cmake/server/ServerSources.cmake).

## Boundary Rules

- Keep externally included server headers at `runtime/server/`.
- Add new implementation files under the narrowest domain folder.
- Update `cmake/server/ServerSources.cmake` instead of relying on globs.
- If a feature is profile-gated, update
  [`cmake/server/ServerExtensionSources.cmake`](../../cmake/server/ServerExtensionSources.cmake)
  and its source guards together.

## Validation

Focused checks after moving or adding server files:

```bash
cmake -S . -B build-vk-Release
cmake --build build-vk-Release --target idtech3_server -j2
cmake --build build-vk-Release --target qcommon -j2
./tests/scripts/test_module_include_canonical.sh
./scripts/q3_openarena_compat_check.sh release
```
