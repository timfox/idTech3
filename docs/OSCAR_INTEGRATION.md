# Open OSCAR Integration

`idtech3` can connect to Open OSCAR Server either as a direct raw FLAP/BOS
client (with optional Chat service socket for rooms) or through the optional
JSON gateway bridge.

```text
idtech3 gameplay / Lua / console / ImGui AIM panel
  -> engine/core/net_oscar.c
  -> direct FLAP/BOS on Open OSCAR port 5190
     (+ second Chat FLAP socket for rooms)
  -> Open OSCAR Server
```

## Hybrid AIM model

One OSCAR session exists **per process** (not per local player):

| Process | Typical `oscar_account` | Role |
| --- | --- | --- |
| Dedicated server | Service screen name (e.g. `legacy-server-42`) | Match invites, lobby IMs, presence |
| Game client | Shared local screen name | Buddy list, IMs, rooms, ImGui AIM panel |
| Listen server | Shared local account | Same session for host + server Lua |

Full hybrid deployments run a **dedicated** process for the service identity and
a separate client for the player AIM UI. Per-player OSCAR logins are deferred.

## Runtime Model

Direct mode is the default. It performs classic FLAP signon, sends a roasted
password, follows the BOS reconnect cookie, marks the BOS session online, sends
basic channel-1 IMs, maintains an in-memory buddy roster, can open a **Chat
service** socket for rooms, and queues incoming IMs/rooms/presence as events.

Gateway mode remains available for higher-level room translation, presence
caches, and policy mediation. In that mode, the engine exchanges bounded JSON
messages over
`ws://<oscar_gateway>:<oscar_gatewayPort>/engine`.

`oscar_gateway` accepts `localhost` or a numeric IPv4/IPv6 address. The engine
deliberately avoids DNS resolution from the network frame path.

## Cvars

| Cvar | Default | Purpose |
| --- | --- | --- |
| `oscar_enable` | `0` | Enable Open OSCAR integration. |
| `oscar_mode` | `direct` | `direct` for raw FLAP/BOS, `gateway` for JSON bridge. |
| `oscar_gateway` | `127.0.0.1` | Open OSCAR or gateway address: `localhost` or numeric private IP. |
| `oscar_gatewayPort` | `5190` | Open OSCAR port in direct mode; use `5191` for the gateway bridge. |
| `oscar_account` | empty | OSCAR screen name or gateway service account name. |
| `oscar_password` | empty | Protected raw OSCAR password. Prefer `IDTECH3_OSCAR_PASSWORD`. |
| `oscar_token` | empty | Protected gateway token. Prefer `IDTECH3_OSCAR_TOKEN`. |
| `oscar_defaultRoom` | empty | Room joined after authentication (direct Chat or gateway). |
| `oscar_reconnect` | `1` | Reconnect after disconnects. |
| `oscar_reconnectMaxDelay` | `60` | Maximum reconnect delay in seconds. |
| `oscar_presence` | `1` | Print/queue presence events. |
| `oscar_notify` | `1` | Print IM/room messages to console/notify. |
| `oscar_debug` | `0` | Print gateway JSON or raw FLAP diagnostics. |
| `oscar_rosterSnapshot` | ROM | `name:status;...` mirror for ImGui. |
| `oscar_rosterGen` | ROM | Roster dirty generation for UI. |
| `cl_oscarNotify` | `1` | Client shell notify bridge enable. |
| `cl_oscarChat` | `1` | Client AIM-style chat coloring preference. |
| `cl_oscarUi` | `1` | Show OSCAR buddy panel when `r_imgui` is on. |

Do not store OSCAR account passwords in archived cvars. Use
`IDTECH3_OSCAR_PASSWORD` or protected server configuration outside downloadable
game data.

## Dedicated Commands

| Command | Purpose |
| --- | --- |
| `oscar_status` | Print OSCAR state, room, buddy count, and last error. |
| `oscar_buddies` | Dump the in-memory buddy roster. |
| `oscar_connect` | Connect and authenticate. |
| `oscar_disconnect` | Disconnect. |
| `oscar_join <room>` | Join a room (direct Chat socket or gateway). |
| `oscar_leave [room]` | Leave a room. |
| `oscar_announce <message>` | Send a message to the current/default room. |
| `oscar_im <screenName> <message>` | Send a direct message. |
| `oscar_presence <status> [message]` | Set presence. Direct mode supports `available`, `away`, `dnd`, `out`, `busy`, `chat`, and `invisible`. |
| `oscar_buddy_add <screenName>` | Direct mode: subscribe + roster entry for this session. |
| `oscar_buddy_del <screenName>` | Direct mode: remove temporary buddy subscription. |

These register from qcommon (`OSCAR_RegisterCommands`) so **client and dedicated**
both see them. They remain operator/console commands — player chat must not be
forwarded into account or gateway administration.

## Client AIM UI

With `r_imgui 1` and `cl_oscarUi 1`, open **Window → OSCAR / AIM** for connect,
buddy add/remove, IM, and room join/announce. The roster reads
`oscar_rosterSnapshot`.

Demo: `exec demo_oscar_aim.cfg` (under `examples/demo_game/mod/`). That loads
`scripts/lua/demo_oscar_aim.lua` with `demo_oscar_status()` /
`demo_oscar_poll()` helpers for the Lua VM.

## Lua

Server and client Lua both expose `Engine.Oscar`:

```lua
Engine.Oscar.IsAvailable()
Engine.Oscar.GetState()
Engine.Oscar.GetStatus()  -- includes buddyCount, rosterGeneration
Engine.Oscar.Connect()
Engine.Oscar.Disconnect(reason)
Engine.Oscar.SendIM(screenName, text)
Engine.Oscar.JoinRoom(roomName)
Engine.Oscar.LeaveRoom(roomName)
Engine.Oscar.SendRoomMessage(roomName, text)
Engine.Oscar.SetPresence(status, message)
Engine.Oscar.AddBuddy(screenName)
Engine.Oscar.RemoveBuddy(screenName)
Engine.Oscar.BuddyCount()
Engine.Oscar.GetBuddy(index)  -- { screenName, status, awayMessage, online }
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
oscar_buddy_add SomeBuddy
oscar_im SomeBuddy "Need one more player for CTF."
oscar_join my-lobby
oscar_announce "Server is up — join the room."
```

Direct mode supports login, BOS cookie reconnect, online notification, basic
outgoing IMs, incoming IM events, temporary buddy subscriptions, in-memory
roster, buddy arrival/departure/status, presence publishing, **Chat service
room join/leave/message** (second FLAP socket via service redirect), disconnect,
and reconnect. Gateway mode remains useful for sidecar policy and richer room
caching. Feedbag/SSI persistence is not implemented in this slice.

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
translation to a sidecar. Gateway buddy subscribe remains unimplemented in the
engine bridge (use direct mode for roster).

## Validation

`unit_oscar_protocol` verifies the bounded JSON gateway contract, including
escaping, event parsing, malformed input, and oversized frame rejection.
`unit_oscar_raw` verifies raw FLAP/TLV/SNAC helpers for login, BOS cookie signon,
client-online, IM receive parsing, presence publishing, temporary buddy
subscription, buddy presence parsing, service responses, and chat message
encoding/parsing. `test_oscar_bridge` verifies that both direct and gateway code
paths stay wired (roster APIs, Chat service path, client shell, docs) without
shelling out to external tools.
