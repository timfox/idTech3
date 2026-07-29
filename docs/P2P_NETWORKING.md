# P2P Networking

`idtech3` can expose an optional peer-to-peer transport for client-hosted games.

## Current backend

`net_p2p` is now a backend-neutral feature flag with two transport paths:

- `steam_sdr`: Steam Datagram Relay through Steam Networking Sockets
- `direct_udp`: manual/direct UDP for offline, non-Steam, LAN, or port-forwarded hosting

- Build with `-DUSE_STEAM_NETWORKING=ON`
- Point `STEAMWORKS_SDK` at a Steamworks SDK checkout
- Enable at runtime with `net_p2p 1`
- Choose backend with `net_p2pBackend auto|steam_sdr|direct_udp`
- For `direct_udp`, set `net_p2pAdvertiseAddress` to an externally reachable `udp:host:port` or `host:port`, or enable STUN auto-advertise (below)

`net_p2p` is the user-facing cvar. `net_sdr` remains as a compatibility alias for the Steam-backed path.

## Client commands

- `p2p_status`
- `p2p_address`
- `p2p_connect <steamid|steam:steamid|host:port|udp:host:port>`
- `p2p_punch <host:port|udp:host:port>`
- `p2p_punch_status`
- `p2p_candidates`
- `p2p_list [local|global|favorites|master]`
- `p2p_sessioninfo <local|global|favorites> <index>`
- `p2p_connect_browser <local|global|favorites> <index>`

`p2p_address` prints the current shareable P2P address: `steam:STEAMID64` for Steam SDR or `udp:host:port` for direct UDP when `net_p2pAdvertiseAddress` is configured or ICE auto-advertise resolves a candidate.
`p2p_status` also reports a compact `P2P summary` line, current ICE path state, and direct-UDP punch session state, which helps debug peer ownership, fallback, and keepalive behavior.
`p2p_connect` normalizes the input and forwards to `connect` with either a `steam:` or `udp:` address. For `udp:` peers it starts ICE-lite (when enabled) and, with `net_p2pIceDeferConnect 1`, waits for nomination/timeout before `connect`; otherwise it punches and connects immediately.
`p2p_punch` explicitly starts a direct-UDP punchthrough helper session to a peer.
`p2p_punch_status` prints active punch peers, attempts, and whether a peer has acknowledged the punchthrough flow.
`p2p_candidates` prints gathered ICE candidates (`host`, `srflx`, `relay`).
`p2p_list` prints cached browser entries that advertise P2P support, including their browser source, index, Steam address, and current UDP endpoint. Use `p2p_list master` to query the configured master server directly.
`p2p_sessioninfo` prints the advertised session contract for one cached browser entry: session id, protocol, reconnect window, host migration support, anti-cheat posture, and failover policy.
`p2p_connect_browser` uses the browser cache and prefers an advertised `p2paddr` when one is available.
When `net_p2p 1` is enabled, legacy UI/browser joins also prefer the advertised `p2paddr` automatically.

## Dedicated server commands

The dedicated server exposes the same P2P tooling where applicable:

- `p2p_status`, `p2p_address`, `p2p_punch`, `p2p_punch_status`, `p2p_candidates`
- `p2p_connect <address>` — starts ICE/punch toward a peer and prints this server's shareable P2P address for inbound clients
- `p2p_list [local|master]` — `local` prints server P2P status plus connected client UDP endpoints; `master` queries `sv_master1` for P2P-capable listings
- `p2p_grace_prime <host:port>` — test/dev helper that seeds a reconnect grace slot for the given endpoint (used by `test_p2p_reconnect` live smoke)

## ICE / STUN / TURN (direct_udp)

The `direct_udp` backend now includes ICE-lite candidate gathering:

| Candidate | Source | Cvars |
|-----------|--------|-------|
| `host` | bound `net_ip` + `net_port` | `net_p2pHostAdvertise 1` |
| `srflx` | STUN Binding response | `net_p2pStun 1`, `net_p2pStunServer`, `net_p2pStunAutoAdvertise 1` |
| `relay` | TURN Allocate (OpenSSL builds) | `net_p2pTurn 1`, `net_p2pTurnServer`, `net_p2pTurnUser`, `net_p2pTurnPass` |

Advertise priority when `net_p2pAdvertiseAddress` is unset:

1. Manual `net_p2pAdvertiseAddress`
2. TURN relay candidate
3. STUN server-reflexive candidate
4. Host candidate (optional)

Default STUN server: `stun.l.google.com:19302`. Refresh interval: `net_p2pStunInterval` (default 30000 ms).

TURN relay allocation requires a build with OpenSSL (`USE_DTLS=ON`) and long-term credentials on the TURN server.

## Direct UDP punchthrough helpers

The `direct_udp` backend includes a symmetric connectionless helper path built on top of the engine's existing OOB packets:

- `p2pPunch` / `p2pPong` packets help establish and keep NAT mappings warm for manually coordinated peers.
- Inbound `p2pPunch` now registers a symmetric local punch session and continues outbound keepalive/punch toward the peer.
- `p2pKeepalive` packets can continue after the initial acknowledgement when `net_p2pPunchKeepalive 1` is enabled.
- Tuning cvars:
  - `net_p2pPunch 1`
  - `net_p2pPunchInterval 750`
  - `net_p2pPunchAttempts 8`
  - `net_p2pPunchKeepalive 1`

This is ICE-lite rather than a full interactive ICE agent, but it covers host + STUN reflexive + optional TURN relay discovery and active symmetric UDP punching.

Code that needs status without parsing console text can call `NET_P2P_GetPathStatus`.
It fills `p2p_path_status_t` with backend, local address, ICE activity/result,
deferred-connect state, candidate/check counts, and active/acknowledged punch peer counts.

### ICE connectivity checks (`direct_udp`)

When `net_p2pIceChecks 1` (default) and backend is `direct_udp`, `p2p_connect` runs ICE-lite checks **before** issuing `connect`:

| OOB packet | Purpose |
|------------|---------|
| `p2pCand` / `p2pCandRequest` | Exchange gathered candidate lists |
| `p2pCheck` / `p2pCheckAck` | Nominate first working peer path |

Cvars: `net_p2pIceChecks`, `net_p2pIceTimeout` (default 3000 ms), `net_p2pIceDeferConnect` (default 1).

With `net_p2pIceDeferConnect 1` (clients only), game `connect` is queued until ICE nominates a path or the timeout falls back to direct punch. Set `0` to restore the old race (immediate `connect` while ICE still runs). Dedicated `p2p_connect` starts the ICE/punch path only and never issues a game connect.

With `net_p2pBackend auto` and Steam SDR ready, ICE is skipped (`P2P: using steam_sdr (ICE skipped)`).

### TURN hardening

| Feature | Cvar |
|---------|------|
| Allocation refresh | `net_p2pTurnRefresh` (seconds before expiry) |
| CreatePermission for peer | automatic before relay ICE checks |
| ChannelBind scaffold | `net_p2pTurnChannels` (default 0) |

TURN auth uses OpenSSL when available (`net_p2p_turn_auth.c`), not tied to game DTLS.

## Listen server flow

1. Build with `USE_STEAM_NETWORKING=ON`.
2. Start the client with Steam running.
3. Set `net_p2p 1`.
4. Start or host a local game from the client.
5. Run `p2p_address` and share the printed address.
6. The remote player runs `net_p2p 1` and `p2p_connect <that-steamid>`.

For non-Steam or offline/manual hosting:

1. Set `net_p2p 1`.
2. Set `net_p2pBackend direct_udp`.
3. Either set `net_p2pAdvertiseAddress <public-or-lan-host>:<port>` **or** leave it empty and enable `net_p2pStun 1` + `net_p2pStunAutoAdvertise 1`.
4. Start or host the game.
5. Run `p2p_address` or `p2p_candidates` and share the printed `udp:` address.
6. The remote player runs `net_p2p 1` and `p2p_connect <that-udp-address>`.

Quickstart overlay/demo: `exec vulkan_overlay_p2p_direct_udp.cfg` or `exec demo_p2p_direct_udp.cfg`.

## Discovery

Servers now advertise `p2p=1` and `p2paddr=<backend-address>` in their info responses when the P2P backend is ready.
They also advertise a compact session contract:

- `p2psession` — stable session identifier for browser caching and reconnect UX
- `p2preconn` — reconnect recovery window in seconds
- `p2pmigrate` — whether the host advertises migration support
- `p2psecure` — anti-cheat posture (`none`, `pure`, `pure_signed`)
- `p2pfail` — failure recovery policy (`reconnect`, `migrate`, or `none`)
- `p2pproto` — protocol compatibility identifier

That metadata is stored in the client browser cache and exposed through `LAN_GetServerInfo`, so UI scripts or terminal users can connect with `p2p_connect_browser`.
Browser entries also match on `p2paddr`, so if a server's UDP endpoint changes but its advertised P2P identity stays the same, the cached entry is refreshed instead of treated as a different server.
Favorites preserve the same identity too: adding or removing `steam:...` or `udp:...` entries works directly, and favorites copied from browser listings retain the advertised `p2paddr` for later joins.

## Supported session model

The engine's current supported multiplayer model is:

- **Transport**: `steam_sdr` or `direct_udp`
- **NAT traversal**: host/STUN/TURN candidate gathering, ICE-lite connectivity checks, symmetric UDP punch helpers
- **Reconnect**: `cl_p2pAutoReconnect 1` replays `connect` within advertised `p2preconn` window; server accepts `p2pReconnect <session>` fast-path
- **Recovery policy controls**: `cl_p2pReconnectMaxAttempts` caps reconnect storms, `cl_p2pReconnectJitterMs` staggers retries, and `Engine.P2P.getSession()` now exposes `currentTarget` plus `recoveryStopReason`
- **Versioning**: browser-visible protocol plus game/mod identity
- **Anti-cheat posture**: browser-visible `sv_pure` / `sv_pureSigned` summary
- **Failure recovery**: `p2pfail=reconnect` auto-rejoin; `p2pfail=migrate` promotes `cl_p2pBackupHost 1` client and broadcasts `p2pMigrate`

Listen-host migration v1 re-hosts via `listen` + fresh map load (no full gamestate handoff). See `docs/P2P_NAT_TESTING.md` for CI validation tiers.

## Dedicated server status

The dedicated server binary now initializes the shared Steam runtime when built with `USE_STEAM_NETWORKING=ON`.
That means `idtech3_server` can report `p2p_status` and `p2p_address` and accept Steam P2P traffic when Steam API initialization succeeds.

This is still not the Steam game-server API path.
In practice, that makes it suitable for user-run or locally supervised dedicated servers where Steam is available, while the default UDP flow remains unchanged.
