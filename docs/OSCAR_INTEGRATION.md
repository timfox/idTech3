# Open OSCAR Gateway Integration

`idtech3` integrates with Open OSCAR Server through a separate gateway process.
The engine does not embed Open OSCAR Server and does not parse the full OSCAR or
TOC protocol directly.

```text
idtech3 gameplay / Lua / console
  -> engine/core/net_oscar.c
  -> local WebSocket gateway
  -> Open OSCAR Server as a normal AIM/ICQ client session
```

## Runtime Model

The gateway owns OSCAR login, buddy state, presence, rooms, reconnect policy, and
protocol translation. The engine owns game semantics such as announcements,
server invites, and script callbacks.

The engine exchanges bounded JSON messages over `ws://<oscar_gateway>:<oscar_gatewayPort>/engine`.
The default gateway is `127.0.0.1:5191`. `oscar_gateway` accepts `localhost`
or a numeric IPv4/IPv6 address; the engine deliberately avoids DNS resolution
from the network frame path.

## Cvars

| Cvar | Default | Purpose |
|------|---------|---------|
| `oscar_enable` | `0` | Enable the OSCAR gateway bridge. |
| `oscar_gateway` | `127.0.0.1` | Gateway address: `localhost` or numeric private IP. |
| `oscar_gatewayPort` | `5191` | Gateway WebSocket port. |
| `oscar_account` | empty | Gateway service account name. |
| `oscar_token` | empty | Protected short-lived gateway token. Prefer `IDTECH3_OSCAR_TOKEN`. |
| `oscar_defaultRoom` | empty | Room joined after gateway authentication. |
| `oscar_reconnect` | `1` | Reconnect after disconnects. |
| `oscar_reconnectMaxDelay` | `60` | Maximum reconnect delay in seconds. |
| `oscar_presence` | `1` | Print/queue presence events. |
| `oscar_debug` | `0` | Print gateway JSON diagnostics. |

Do not store OSCAR account passwords in archived cvars. Use gateway-issued
tokens or protected server configuration outside downloadable game data.

## Dedicated Commands

| Command | Purpose |
|---------|---------|
| `oscar_status` | Print gateway state, current room, and last error. |
| `oscar_connect` | Connect and authenticate with the gateway. |
| `oscar_disconnect` | Disconnect from the gateway. |
| `oscar_join <room>` | Join a room through the gateway. |
| `oscar_leave [room]` | Leave a room. |
| `oscar_announce <message>` | Send a message to the current/default room. |
| `oscar_im <screenName> <message>` | Send a direct message. |
| `oscar_presence <status> [message]` | Set gateway presence. |

These are operator commands. Player commands must not be forwarded into account
or gateway administration.

## Server Lua

Server Lua receives an `Engine.Oscar` table:

```lua
Engine.Oscar.IsAvailable()
Engine.Oscar.GetState()
Engine.Oscar.GetStatus()
Engine.Oscar.Connect()
Engine.Oscar.Disconnect(reason)
Engine.Oscar.SendIM(screenName, text)
Engine.Oscar.JoinRoom(roomName)
Engine.Oscar.LeaveRoom(roomName)
Engine.Oscar.SendRoomMessage(roomName, text)
Engine.Oscar.SetPresence(status, message)
Engine.Oscar.PollEvent()
```

`PollEvent()` returns validated application-level events such as
`room_message`, `instant_message`, `presence_changed`, `request_complete`, and
`disconnected`. Lua never receives raw OSCAR packets.

## Gateway Message Contract

Engine to gateway examples:

```json
{"type":"authenticate","request_id":1,"account":"legacy-server-42","token":"..."}
{"type":"join_room","request_id":2,"room":"legacy-server-42"}
{"type":"send_room_message","request_id":3,"room":"legacy-server-42","sender":"legacy-server-42","text":"Need one more player for CTF."}
```

Gateway to engine examples:

```json
{"type":"connected","request_id":1,"ok":true}
{"type":"room_message","room":"legacy-server-42","screen_name":"ArenaPlayer","text":"Joining now."}
{"type":"presence_changed","screen_name":"ArenaPlayer","status":"online","away_message":""}
```

## What This Is Not

The Open OSCAR management API is useful for provisioning users and rooms, but it
is not the runtime messaging API. Regular chat, IMs, presence, and room events go
through an authenticated gateway session.

## Validation

`unit_oscar_protocol` verifies the bounded JSON gateway contract, including
escaping, event parsing, malformed input, and oversized frame rejection.
`test_oscar_bridge` verifies that the engine remains wired as a gateway bridge
instead of becoming a direct OSCAR protocol implementation.
