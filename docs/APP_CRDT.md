# App CRDT (distributed Lua updates)

Inspired by Wyns et al., *App CRDT: Dynamic Software Updates for Distributed Applications* ([arXiv:2606.10920](https://arxiv.org/abs/2606.10920)).

Opt-in layer for **semver-ordered, server-pushed Lua mod updates** with versioned script events. Does not change vanilla snapshots, `usercmd_t`, or native renderer modules.

## Goals (paper §2.1–2.4)

| Goal | Implementation |
|------|----------------|
| Dynamic code update without coordinated shutdown | `app_crdt_publish` → clients run `teardown → reload → init(previous)` |
| Eventually consistent app version | LWW semver merge (`AppCrdt_MergeLWW`) + `com_app_crdt_version` in systeminfo |
| Versioned message delivery | Algorithm 2 on `appcrdt event` channel only |
| Safe degradation | `com_app_crdt 0` default; unsigned bundles accepted when `com_app_crdt_sign 0` |

## Topology

**Star (server authoritative)** — not P2P. The dedicated server holds the authoritative semver; clients merge on push. Rolling multi-server deploy: run `app_crdt_publish` with the same semver on each instance (manual sync in v1).

## Cvars

| Cvar | Default | Description |
|------|---------|-------------|
| `com_app_crdt` | `0` | Master toggle |
| `com_app_crdt_sign` | `0` | Signed bundle requirement (stub) |
| `com_app_crdt_version` | `0.0.0` | Authoritative semver (systeminfo) |
| `com_app_crdt_queue_max` | `64` | Event buffer size |
| `com_app_crdt_auto` | `1` | Auto-publish idtech3backend manifest on map load |
| `com_app_crdt_backend` | `1` | Resolve scripts/manifest from idtech3backend submodule |
| `com_app_crdt_backend_root` | `""` | Override backend root directory |

## idtech3backend

With the [idtech3backend](https://github.com/timfox/idtech3backend) submodule initialized (`./scripts/init_optional_submodules.sh --backend`) and `app_crdt/manifest.json` present:

```bash
# dedicated server — auto-publishes backend on first map load
./release/idtech3_server +set dedicated 1 +set com_app_crdt 1 +map mymap

# client
./release/idtech3 +connect localhost +set com_app_crdt 1
```

Server Lua (`server/lua/backend_app.lua`) receives **`Engine.AppCrdt.publish` / `emit`**. Clients use **`Engine.AppCrdt.emit`** (relay via server). See [IDTECH3_BACKEND.md](IDTECH3_BACKEND.md).

## Console

| Command | Where | Purpose |
|---------|-------|---------|
| `app_crdt_status` | anywhere | Print local vs authoritative version |
| `app_crdt_publish <semver> [manifest.json]` | server | Merge LWW, reload Lua, broadcast to clients |
| `app_crdt_emit <payload>` | server | Broadcast versioned script event |
| `app_crdt_flush` | client | Flush buffered events (debug) |

## Wire format (layercake)

Uses existing reliable commands — **no new `svc_*` opcodes**:

- Server → client: `appcrdt publish 1.0.0 examples/app_crdt/manifest.json`
- Server → client: `appcrdt event 1 {"ping":1}`
- Client → server: `appcrdt event 1 {"pong":1}` (relayed with server major)

## Lua lifecycle

Implement in your app script:

```lua
function on_hotload_destroy() return state end
function on_hotload_create(previous) state = previous or {} end
function on_app_crdt_message(fromMajor, payload) -- updateMessage adapter end
```

Client API:

```lua
AppCrdt.emit('{"hello":1}')  -- sends to server, rebroadcast to all
```

## Manifest

JSON at mod path:

```json
{
  "version": "1.0.0",
  "scripts": ["scripts/lua/app_spec.lua"]
}
```

See [examples/app_crdt/manifest.json](../examples/app_crdt/manifest.json).

## Manual test

Script paths in the manifest must use allowed prefixes (`scripts/lua/`, etc.). Point `fs_basepath` at the example mod root so FS resolves `scripts/lua/app_spec.lua`.

```bash
# terminal 1
./release/idtech3_server +set dedicated 1 +set com_app_crdt 1 +set fs_basepath examples/app_crdt

# terminal 2
./release/idtech3 +connect localhost +set com_app_crdt 1 +set fs_basepath examples/app_crdt

# server console
app_crdt_publish 1.0.0 manifest.json
app_crdt_emit {"ping":1}
```

## Non-goals

- Live `idtech3_vulkan.so` / native `.so` game module hot-swap
- P2P CRDT mesh between clients
- Changes to Q3 network snapshots or entity state
- Automatic gameplay changes (publish is operator-driven)

## References

- [MOD_SDK.md](MOD_SDK.md) — cvar table
- [API_STABILITY.md](API_STABILITY.md) — layercake extension policy
