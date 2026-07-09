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
- For `direct_udp`, set `net_p2pAdvertiseAddress` to an externally reachable `udp:host:port` or `host:port`

`net_p2p` is the user-facing cvar. `net_sdr` remains as a compatibility alias for the Steam-backed path.

## Client commands

- `p2p_status`
- `p2p_address`
- `p2p_connect <steamid|steam:steamid|host:port|udp:host:port>`
- `p2p_punch <host:port|udp:host:port>`
- `p2p_punch_status`
- `p2p_list [local|global|favorites]`
- `p2p_connect_browser <local|global|favorites> <index>`

`p2p_address` prints the current shareable P2P address: `steam:STEAMID64` for Steam SDR or `udp:host:port` for direct UDP when `net_p2pAdvertiseAddress` is configured.
`p2p_connect` normalizes the input and forwards to `connect` with either a `steam:` or `udp:` address. For `udp:` peers it also starts a direct-UDP punchthrough helper session first.
`p2p_punch` explicitly starts a direct-UDP punchthrough helper session to a peer.
`p2p_punch_status` prints active punch peers, attempts, and whether a peer has acknowledged the punchthrough flow.
`p2p_list` prints cached browser entries that advertise P2P support, including their browser source, index, Steam address, and current UDP endpoint.
`p2p_connect_browser` uses the browser cache and prefers an advertised `p2paddr` when one is available.
When `net_p2p 1` is enabled, legacy UI/browser joins also prefer the advertised `p2paddr` automatically.

## Direct UDP punchthrough helpers

The `direct_udp` backend now includes a lightweight connectionless helper path built on top of the engine's existing OOB packets:

- `p2pPunch` / `p2pPong` packets help establish and keep NAT mappings warm for manually coordinated peers.
- `p2pKeepalive` packets can continue after the initial acknowledgement when `net_p2pPunchKeepalive 1` is enabled.
- Tuning cvars:
  - `net_p2pPunch 1`
  - `net_p2pPunchInterval 750`
  - `net_p2pPunchAttempts 8`
  - `net_p2pPunchKeepalive 1`

This is not full ICE/STUN/TURN yet, but it does move `direct_udp` closer to the behavior players expect from modern P2P games by actively assisting a peer before and after a direct UDP connect attempt.

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
3. Set `net_p2pAdvertiseAddress <public-or-lan-host>:<port>` or `udp:<public-or-lan-host>:<port>`.
4. Start or host the game.
5. Run `p2p_address` and share the printed `udp:` address.
6. The remote player runs `net_p2p 1` and `p2p_connect <that-udp-address>`.

## Discovery

Servers now advertise `p2p=1` and `p2paddr=<backend-address>` in their info responses when the P2P backend is ready.
That metadata is stored in the client browser cache and exposed through `LAN_GetServerInfo`, so UI scripts or terminal users can connect with `p2p_connect_browser`.
Browser entries also match on `p2paddr`, so if a server's UDP endpoint changes but its advertised P2P identity stays the same, the cached entry is refreshed instead of treated as a different server.
Favorites preserve the same identity too: adding or removing `steam:...` or `udp:...` entries works directly, and favorites copied from browser listings retain the advertised `p2paddr` for later joins.

## Dedicated server status

The dedicated server binary now initializes the shared Steam runtime when built with `USE_STEAM_NETWORKING=ON`.
That means `idtech3_server` can report `p2p_status` and `p2p_address` and accept Steam P2P traffic when Steam API initialization succeeds.

This is still not the Steam game-server API path.
In practice, that makes it suitable for user-run or locally supervised dedicated servers where Steam is available, while the default UDP flow remains unchanged.
