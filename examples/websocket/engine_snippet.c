/*
 * TEMPLATE — not part of the CMake build.
 * Copy into a client module, register a command in CL_Init (or similar), and call
 * after WS_Init() (see cl_websocket.h / cl_websocket.c).
 */
#include "client.h"
#include "cl_websocket.h"

static wsHandle_t g_exampleWs = WS_INVALID_HANDLE;

static void ExampleWS_OnMessage( wsHandle_t h, wsOpcode_t op, const byte *data, int len ) {
	(void)h;
	(void)op;
	Com_Printf( "WebSocket example: received %d byte(s)\n", len );
}

static void ExampleWS_OnOpen( wsHandle_t h ) {
	Com_Printf( "WebSocket example: connected (handle %d)\n", h );
	(void)WS_SendText( h, "hello from idtech3" );
}

static void ExampleWS_OnClose( wsHandle_t h, int code, const char *reason ) {
	Com_Printf( "WebSocket example: closed code=%d %s\n", code, reason ? reason : "" );
	if ( h == g_exampleWs ) {
		g_exampleWs = WS_INVALID_HANDLE;
	}
}

static void ExampleWS_OnError( wsHandle_t h, const char *err ) {
	Com_Printf( S_COLOR_YELLOW "WebSocket example: %s (handle %d)\n", err ? err : "error", h );
}

static void ExampleWS_Connect_f( void ) {
	if ( g_exampleWs != WS_INVALID_HANDLE ) {
		WS_Disconnect( g_exampleWs );
	}
	/* Point at your local echo server (see examples/websocket/server/node) */
	g_exampleWs = WS_Connect( "ws://127.0.0.1:8765/", ExampleWS_OnMessage, ExampleWS_OnOpen, ExampleWS_OnClose, ExampleWS_OnError );
}

static void ExampleWS_Disconnect_f( void ) {
	if ( g_exampleWs != WS_INVALID_HANDLE ) {
		WS_Disconnect( g_exampleWs );
		g_exampleWs = WS_INVALID_HANDLE;
	}
}

/* In your init:  Cmd_AddCommand( "ws_example", ExampleWS_Connect_f ); */
/*                 Cmd_AddCommand( "ws_example_close", ExampleWS_Disconnect_f ); */
/* I/O: WS_Frame() is already called each client frame from cl_main.c. */
