# Anti-cheat integration points

Engine v1 provides **hooks**, not a full anti-cheat product.

## Server

| Mechanism | Cvar / API |
|-----------|------------|
| Pure server | `sv_pure` |
| Signed pk3 | `sv_pureSigned`, `com_pk3Signed`, `pk3.sig` sidecar |
| Interest cull | `sv_interestMaxDist`, `sv_interestPriority` |
| Auth token | `sv_authEnable`, `SV_AuthVerifyToken()` in [sv_auth.c](../runtime/server/sv_auth.c) |
| Snapshot sampling | Game mod: hash `entityState` batches (document in mod SDK) |

## Client

- Read-only integrity cvars (game mod sets; engine does not attest)
- No kernel drivers in-tree

## Third-party

Integrate EAC/BattlEye/etc. at **game DLL** boundary: validate before `GAME_CLIENT_CONNECT`, deny with `GAME_CLIENT_CONNECT` string.
