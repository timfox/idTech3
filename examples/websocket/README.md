# WebSocket example pack (templated)

Self-contained **templates** you can copy: a small **Node** echo/JSON server and a **browser** client. The idTech3 **client** implements `ws://` in `src/client/cl_websocket.c` (RFC 6455 over raw TCP, no TLS). Use a reverse proxy (Caddy, nginx) for `wss://` in production.

## Layout

| Path | Role |
|------|------|
| `server/node/` | Node.js server using the `ws` package |
| `client/browser/echo_client.html` | Single-file browser demo; default URL in the input field |
| `engine_snippet.c` | Copy into `src/client/`: C sketch (not built by CMake) for `WS_*` API |

## Server (Node)

```bash
cd server/node
npm install
node echo_server.mjs
```

Default listen: `0.0.0.0:8765` (override with `PORT=9000`).

## Browser client

1. After the server is running, open `client/browser/echo_client.html` in a browser **or** serve the directory over HTTP (some browsers block `file://` WebSockets depending on security settings).
2. Set the **Server URL** field in the page (default `ws://127.0.0.1:8765/`) to match your host and port.

## Engine client (`idtech3`)

At runtime, `cl_websocket` must be `1` (default). The API is declared in `cl_websocket.h`: `WS_Init` / `WS_Frame` / `WS_Connect` / `WS_SendText` / `WS_Disconnect`. Connections are **non-blocking**; drive I/O with `WS_Frame` each client frame (already called from `cl_main.c`).

**Sketch** (not built by CMake—copy into a module that registers a `Cmd_AddCommand` and stores `wsHandle_t`):

```c
#include "cl_websocket.h"

static wsHandle_t g_ws = WS_INVALID_HANDLE;

static void Demo_OnMessage( wsHandle_t h, wsOpcode_t op, const byte *data, int len ) {
	(void)h; (void)op;
	Com_Printf( "WebSocket: got %d bytes\n", len );
	/* data is not NUL-terminated; use len */
}

static void Demo_OnOpen( wsHandle_t h ) { Com_Printf( "WebSocket: open slot %d\n", h ); }
static void Demo_OnClose( wsHandle_t h, int code, const char *reason ) {
	(void)h; Com_Printf( "WebSocket: close %d %s\n", code, reason ? reason : "" );
	g_ws = WS_INVALID_HANDLE;
}
static void Demo_OnError( wsHandle_t h, const char *err ) {
	(void)h; Com_Printf( S_COLOR_YELLOW "WebSocket: %s\n", err ? err : "error" );
}

/* After WS_Init and while the client is running, each frame: WS_Frame() already runs globally. */
static void CL_WsConnectDemo_f( void ) {
	if ( g_ws != WS_INVALID_HANDLE ) WS_Disconnect( g_ws );
	g_ws = WS_Connect( "ws://127.0.0.1:8765/", Demo_OnMessage, Demo_OnOpen, Demo_OnClose, Demo_OnError );
}
```

`wss://` URLs are parsed, but the stack does not perform TLS—terminate TLS at a proxy and use `ws://` to localhost, or extend the client for `SSL_connect` (out of scope for this template).

## Protocol in this example

- **Text frames** are echoed back to the sender.
- If a text message parses as JSON with `"type":"ping"`, the server replies with `{"type":"pong","t":<server_ms>}`.

## Security note

Do not expose raw `ws://` to untrusted networks. Prefer VPN or TLS at the edge.
