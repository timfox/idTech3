# P2P Networking

`idtech3` can expose an optional peer-to-peer transport for client-hosted games.

## Current backend

The current P2P backend is Steam Datagram Relay (SDR) through Steam Networking Sockets.

- Build with `-DUSE_STEAM_NETWORKING=ON`
- Point `STEAMWORKS_SDK` at a Steamworks SDK checkout
- Enable at runtime with `net_p2p 1`

`net_p2p` is the user-facing cvar. `net_sdr` remains as a compatibility alias.

## Client commands

- `p2p_status`
- `p2p_address`
- `p2p_connect <steamid|steam:steamid>`
- `p2p_connect_browser <local|global|favorites> <index>`

`p2p_address` prints the local `steam:STEAMID64` address to share with another player.
`p2p_connect` normalizes the input and forwards to `connect steam:STEAMID64`.
`p2p_connect_browser` uses the browser cache and prefers an advertised `p2paddr` when one is available.
When `net_p2p 1` is enabled, legacy UI/browser joins also prefer the advertised `p2paddr` automatically.

## Listen server flow

1. Build with `USE_STEAM_NETWORKING=ON`.
2. Start the client with Steam running.
3. Set `net_p2p 1`.
4. Start or host a local game from the client.
5. Run `p2p_address` and share the printed address.
6. The remote player runs `net_p2p 1` and `p2p_connect <that-steamid>`.

## Discovery

Servers now advertise `p2p=1` and `p2paddr=steam:...` in their info responses when the P2P backend is ready.
That metadata is stored in the client browser cache and exposed through `LAN_GetServerInfo`, so UI scripts or terminal users can connect with `p2p_connect_browser`.
Browser entries also match on `p2paddr`, so if a server's UDP endpoint changes but its advertised P2P identity stays the same, the cached entry is refreshed instead of treated as a different server.
Favorites preserve the same identity too: adding or removing `steam:STEAMID64` entries works directly, and favorites copied from browser listings retain the advertised `p2paddr` for later joins.

## Dedicated server status

The dedicated server binary now initializes the shared Steam runtime when built with `USE_STEAM_NETWORKING=ON`.
That means `idtech3_server` can report `p2p_status` and `p2p_address` and accept Steam P2P traffic when Steam API initialization succeeds.

This is still not the Steam game-server API path.
In practice, that makes it suitable for user-run or locally supervised dedicated servers where Steam is available, while the default UDP flow remains unchanged.
