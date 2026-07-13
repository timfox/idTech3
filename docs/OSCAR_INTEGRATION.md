# Open OSCAR Integration

`idtech3` can connect to Open OSCAR Server either as a direct raw FLAP/BOS
client or through the optional JSON gateway bridge.

```text
idtech3 gameplay / Lua / console
  -> engine/core/net_oscar.c
  -> direct FLAP/BOS on Open OSCAR port 5190
  -> Open OSCAR Server
```

## Runtime Model

Direct mode is the default. It performs classic FLAP signon, sends a roasted
password, follows the BOS reconnect cookie, marks the BOS session online, sends
basic channel-1 IMs, and queues incoming IMs as `instant_message` events.

Gateway mode remains available for higher-level room translation, presence
caches, and policy mediation. In that mode, the engine exchanges bounded JSON
messages over
`ws://<oscar_gateway>:<oscar_gatewayPort>/engine`.

`oscar_gateway` accepts `localhost` or a numeric IPv4/IPv6 address. The engine
deliberately avoids DNS resolution from the network frame path.

## Cvars

| Cvar | Default | Purpose |
|------|---------|---------|
| `oscar_enable` | `0` | Enable Open OSCAR integration. |
| `oscar_mode` | `direct` | `direct` for raw FLAP/BOS, `gateway` for JSON bridge. |
| `oscar_gateway` | `127.0.0.1` | Open OSCAR or gateway address: `localhost` or numeric private IP. |
| `oscar_gatewayPort` | `5190` | Open OSCAR port in direct mode; use `5191` for the gateway bridge. |
| `oscar_account` | empty | OSCAR screen name or gateway service account name. |
| `oscar_password` | empty | Protected raw OSCAR password. Prefer `IDTECH3_OSCAR_PASSWORD`. |
| `oscar_token` | empty | Protected gateway token. Prefer `IDTECH3_OSCAR_TOKEN`. |
| `oscar_defaultRoom` | empty | Gateway-mode room joined after authentication. |
| `oscar_reconnect` | `1` | Reconnect after disconnects. |
| `oscar_reconnectMaxDelay` | `60` | Maximum reconnect delay in seconds. |
| `oscar_presence` | `1` | Print/queue presence events. |
| `oscar_debug` | `0` | Print gateway JSON or raw FLAP diagnostics. |

Do not store OSCAR account passwords in archived cvars. Use
`IDTECH3_OSCAR_PASSWORD` or protected server configuration outside downloadable
game data.

## Dedicated Commands

| Command | Purpose |
|---------|---------|
| `oscar_status` | Print OSCAR state, current room, and last error. |
| `oscar_connect` | Connect and authenticate. |
| `oscar_disconnect` | Disconnect. |
| `oscar_join <room>` | Join a room through the gateway bridge. |
| `oscar_leave [room]` | Leave a room. |
| `oscar_announce <message>` | Send a message to the current/default room. |
| `oscar_im <screenName> <message>` | Send a direct message. Works in direct and gateway mode. |
| `oscar_presence <status> [message]` | Set presence. Direct mode supports `available`, `away`, `dnd`, `out`, `busy`, `chat`, and `invisible`; gateway mode may use the optional message. |

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
`disconnected`. Lua never receives raw FLAP/SNAC packets.

## Direct Raw Client

Minimal direct setup:

```cfg
set oscar_enable 1
set oscar_mode direct
set oscar_gateway 127.0.0.1
set oscar_gatewayPort 5190
set oscar_account legacy-server-42
```

Start the process with `IDTECH3_OSCAR_PASSWORD` set, then run:

```text
oscar_connect
oscar_im SomeBuddy "Need one more player for CTF."
```

Direct mode currently supports login, BOS cookie reconnect, online notification,
basic outgoing IMs, incoming IM events, presence status publishing, disconnect,
and reconnect. Raw OSCAR room chat still requires additional ChatNav/service
negotiation; use gateway mode for rooms until that layer is implemented.

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

## Gateway-Only Features

The Open OSCAR management API is useful for provisioning users and rooms, but it
is not the runtime messaging API. Gateway mode is still useful when the engine
should delegate chat rooms, richer presence cache behavior, and policy
translation to a sidecar.

## Validation

`unit_oscar_protocol` verifies the bounded JSON gateway contract, including
escaping, event parsing, malformed input, and oversized frame rejection.
`unit_oscar_raw` verifies raw FLAP/TLV/SNAC helpers for login, BOS cookie signon,
client-online, IM receive parsing, and presence publishing. `test_oscar_bridge`
verifies that both direct and gateway code paths stay wired without shelling out
to external tools.
