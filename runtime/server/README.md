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

