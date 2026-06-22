/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

WebSocket client for engine-to-service communication.
Uses raw TCP sockets with RFC 6455 WebSocket framing.
Supports text and binary messages, ping/pong, and clean close.
===========================================================================
*/

#ifndef CL_WEBSOCKET_H
#define CL_WEBSOCKET_H

#include "../../qcommon/q_shared.h"

#define WS_MAX_CONNECTIONS  4
#define WS_MAX_FRAME_SIZE   65536
#define WS_INVALID_HANDLE   (-1)

typedef int wsHandle_t;

typedef enum {
	WS_STATE_DISCONNECTED,
	WS_STATE_CONNECTING,
	WS_STATE_OPEN,
	WS_STATE_CLOSING,
	WS_STATE_CLOSED
} wsState_t;

typedef enum {
	WS_TEXT   = 0x1,
	WS_BINARY = 0x2,
	WS_CLOSE  = 0x8,
	WS_PING   = 0x9,
	WS_PONG   = 0xA
} wsOpcode_t;

typedef void (*wsOnMessage_t)( wsHandle_t h, wsOpcode_t opcode, const byte *data, int len );
typedef void (*wsOnOpen_t)( wsHandle_t h );
typedef void (*wsOnClose_t)( wsHandle_t h, int code, const char *reason );
typedef void (*wsOnError_t)( wsHandle_t h, const char *error );

void        WS_Init( void );
void        WS_Shutdown( void );
void        WS_Frame( void );

wsHandle_t  WS_Connect( const char *url, wsOnMessage_t onMsg, wsOnOpen_t onOpen, wsOnClose_t onClose, wsOnError_t onError );
void        WS_Disconnect( wsHandle_t h );
qboolean    WS_Send( wsHandle_t h, wsOpcode_t opcode, const byte *data, int len );
qboolean    WS_SendText( wsHandle_t h, const char *text );
wsState_t   WS_GetState( wsHandle_t h );

#endif /* CL_WEBSOCKET_H */
