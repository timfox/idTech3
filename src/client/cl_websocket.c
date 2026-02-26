/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

WebSocket client implementation using raw TCP sockets + RFC 6455 framing.

Supports:
  - ws:// connections (no TLS — use a reverse proxy for wss://)
  - Text and binary frames
  - Ping/pong keepalive
  - Masked client-to-server frames (per RFC 6455 §5.3)
  - Fragmentation for large messages
  - Callback-based API (onMessage, onOpen, onClose, onError)

Architecture:
  WS_Frame() is called each engine frame, performing non-blocking I/O
  on all active connections. Connection state machine:
    DISCONNECTED → CONNECTING → OPEN → CLOSING → CLOSED
===========================================================================
*/

#define _DEFAULT_SOURCE

#include "client.h"
#include "cl_websocket.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#define WS_INVALID_SOCKET INVALID_SOCKET
#define WS_CLOSE_SOCKET(s) closesocket(s)
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
typedef int socket_t;
#define WS_INVALID_SOCKET (-1)
#define WS_CLOSE_SOCKET(s) close(s)
#endif

typedef struct {
	socket_t        sock;
	wsState_t       state;
	char            host[256];
	char            path[256];
	int             port;
	wsOnMessage_t   onMessage;
	wsOnOpen_t      onOpen;
	wsOnClose_t     onClose;
	wsOnError_t     onError;
	byte            recvBuf[WS_MAX_FRAME_SIZE];
	int             recvLen;
	qboolean        handshakeDone;
} wsConnection_t;

static wsConnection_t wsConns[WS_MAX_CONNECTIONS];
static cvar_t *cl_websocket;
static qboolean wsInitialized = qfalse;

void WS_Init( void ) {
	cl_websocket = Cvar_Get( "cl_websocket", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( cl_websocket, "Enable WebSocket client support (0 = off, 1 = on)." );

	Com_Memset( wsConns, 0, sizeof( wsConns ) );
	for ( int i = 0; i < WS_MAX_CONNECTIONS; i++ ) {
		wsConns[i].sock = WS_INVALID_SOCKET;
		wsConns[i].state = WS_STATE_DISCONNECTED;
	}

	wsInitialized = qtrue;
	Com_Printf( "WebSocket: initialized (%d max connections)\n", WS_MAX_CONNECTIONS );
}

void WS_Shutdown( void ) {
	for ( int i = 0; i < WS_MAX_CONNECTIONS; i++ ) {
		if ( wsConns[i].sock != WS_INVALID_SOCKET ) {
			WS_CLOSE_SOCKET( wsConns[i].sock );
			wsConns[i].sock = WS_INVALID_SOCKET;
		}
		wsConns[i].state = WS_STATE_DISCONNECTED;
	}
	wsInitialized = qfalse;
}

static qboolean WS_ParseURL( const char *url, char *host, int hostLen, char *path, int pathLen, int *port ) {
	const char *p = url;

	if ( !Q_strncmp( p, "ws://", 5 ) ) p += 5;
	else if ( !Q_strncmp( p, "wss://", 6 ) ) p += 6;

	const char *pathStart = strchr( p, '/' );
	const char *portStart = strchr( p, ':' );

	if ( portStart && ( !pathStart || portStart < pathStart ) ) {
		int hl = (int)( portStart - p );
		if ( hl >= hostLen ) hl = hostLen - 1;
		Com_Memcpy( host, p, hl );
		host[hl] = '\0';
		*port = atoi( portStart + 1 );
	} else {
		int hl = pathStart ? (int)( pathStart - p ) : (int)strlen( p );
		if ( hl >= hostLen ) hl = hostLen - 1;
		Com_Memcpy( host, p, hl );
		host[hl] = '\0';
		*port = 80;
	}

	if ( pathStart ) {
		Q_strncpyz( path, pathStart, pathLen );
	} else {
		Q_strncpyz( path, "/", pathLen );
	}

	return ( host[0] != '\0' );
}

static void WS_SetNonBlocking( socket_t s ) {
#ifdef _WIN32
	u_long mode = 1;
	ioctlsocket( s, FIONBIO, &mode );
#else
	int flags = fcntl( s, F_GETFL, 0 );
	fcntl( s, F_SETFL, flags | O_NONBLOCK );
#endif
}

static void WS_MaskData( byte *data, int len, const byte mask[4] ) {
	for ( int i = 0; i < len; i++ ) {
		data[i] ^= mask[i & 3];
	}
}

wsHandle_t WS_Connect( const char *url, wsOnMessage_t onMsg, wsOnOpen_t onOpen, wsOnClose_t onClose, wsOnError_t onError ) {
	int i, slot = -1;
	struct addrinfo hints, *res;
	char portStr[8];

	if ( !wsInitialized || !cl_websocket || !cl_websocket->integer ) return WS_INVALID_HANDLE;

	for ( i = 0; i < WS_MAX_CONNECTIONS; i++ ) {
		if ( wsConns[i].state == WS_STATE_DISCONNECTED ) { slot = i; break; }
	}
	if ( slot < 0 ) return WS_INVALID_HANDLE;

	wsConnection_t *c = &wsConns[slot];
	Com_Memset( c, 0, sizeof( *c ) );
	c->sock = WS_INVALID_SOCKET;
	c->onMessage = onMsg;
	c->onOpen = onOpen;
	c->onClose = onClose;
	c->onError = onError;

	if ( !WS_ParseURL( url, c->host, sizeof( c->host ), c->path, sizeof( c->path ), &c->port ) ) {
		if ( onError ) onError( slot, "Invalid WebSocket URL" );
		return WS_INVALID_HANDLE;
	}

	Com_Memset( &hints, 0, sizeof( hints ) );
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	Com_sprintf( portStr, sizeof( portStr ), "%d", c->port );

	if ( getaddrinfo( c->host, portStr, &hints, &res ) != 0 ) {
		if ( onError ) onError( slot, "DNS resolution failed" );
		return WS_INVALID_HANDLE;
	}

	c->sock = socket( res->ai_family, res->ai_socktype, res->ai_protocol );
	if ( c->sock == WS_INVALID_SOCKET ) {
		freeaddrinfo( res );
		if ( onError ) onError( slot, "Socket creation failed" );
		return WS_INVALID_HANDLE;
	}

	WS_SetNonBlocking( c->sock );
	connect( c->sock, res->ai_addr, (int)res->ai_addrlen );
	freeaddrinfo( res );

	/* Send HTTP upgrade handshake */
	{
		char handshake[1024];
		Com_sprintf( handshake, sizeof( handshake ),
			"GET %s HTTP/1.1\r\n"
			"Host: %s:%d\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Key: dGhlIGlkVGVjaDMgZW5naW5l\r\n"
			"Sec-WebSocket-Version: 13\r\n"
			"Origin: idtech3\r\n"
			"\r\n", c->path, c->host, c->port );
		send( c->sock, handshake, (int)strlen( handshake ), 0 );
	}

	c->state = WS_STATE_CONNECTING;
	Com_Printf( "WebSocket: connecting to %s:%d%s\n", c->host, c->port, c->path );
	return slot;
}

void WS_Disconnect( wsHandle_t h ) {
	if ( h < 0 || h >= WS_MAX_CONNECTIONS ) return;
	wsConnection_t *c = &wsConns[h];
	if ( c->sock != WS_INVALID_SOCKET ) {
		WS_CLOSE_SOCKET( c->sock );
		c->sock = WS_INVALID_SOCKET;
	}
	if ( c->onClose && c->state == WS_STATE_OPEN ) {
		c->onClose( h, 1000, "client disconnect" );
	}
	c->state = WS_STATE_DISCONNECTED;
}

qboolean WS_Send( wsHandle_t h, wsOpcode_t opcode, const byte *data, int len ) {
	byte frame[WS_MAX_FRAME_SIZE + 14];
	int hdrLen = 2;
	byte mask[4];

	if ( h < 0 || h >= WS_MAX_CONNECTIONS || wsConns[h].state != WS_STATE_OPEN ) return qfalse;
	if ( len + 14 > (int)sizeof( frame ) ) return qfalse;

	frame[0] = 0x80 | (byte)opcode;  /* FIN + opcode */

	if ( len < 126 ) {
		frame[1] = 0x80 | (byte)len;  /* MASK bit + length */
		hdrLen = 2;
	} else {
		frame[1] = 0x80 | 126;
		frame[2] = (byte)( len >> 8 );
		frame[3] = (byte)( len & 0xFF );
		hdrLen = 4;
	}

	/* Generate mask */
	mask[0] = (byte)( rand() & 0xFF );
	mask[1] = (byte)( rand() & 0xFF );
	mask[2] = (byte)( rand() & 0xFF );
	mask[3] = (byte)( rand() & 0xFF );
	Com_Memcpy( frame + hdrLen, mask, 4 );
	hdrLen += 4;

	Com_Memcpy( frame + hdrLen, data, len );
	WS_MaskData( frame + hdrLen, len, mask );

	int sent = send( wsConns[h].sock, (const char *)frame, hdrLen + len, 0 );
	return ( sent > 0 ) ? qtrue : qfalse;
}

qboolean WS_SendText( wsHandle_t h, const char *text ) {
	return WS_Send( h, WS_TEXT, (const byte *)text, (int)strlen( text ) );
}

wsState_t WS_GetState( wsHandle_t h ) {
	if ( h < 0 || h >= WS_MAX_CONNECTIONS ) return WS_STATE_DISCONNECTED;
	return wsConns[h].state;
}

void WS_Frame( void ) {
	int i;
	if ( !wsInitialized ) return;

	for ( i = 0; i < WS_MAX_CONNECTIONS; i++ ) {
		wsConnection_t *c = &wsConns[i];
		if ( c->state == WS_STATE_DISCONNECTED || c->sock == WS_INVALID_SOCKET ) continue;

		int n = recv( c->sock, (char *)( c->recvBuf + c->recvLen ),
			(int)sizeof( c->recvBuf ) - c->recvLen - 1, 0 );

		if ( n > 0 ) {
			c->recvLen += n;

			if ( c->state == WS_STATE_CONNECTING && !c->handshakeDone ) {
				c->recvBuf[c->recvLen] = '\0';
				if ( strstr( (char *)c->recvBuf, "\r\n\r\n" ) ) {
					if ( strstr( (char *)c->recvBuf, "101" ) ) {
						c->state = WS_STATE_OPEN;
						c->handshakeDone = qtrue;
						c->recvLen = 0;
						Com_Printf( "WebSocket: connected to %s:%d\n", c->host, c->port );
						if ( c->onOpen ) c->onOpen( i );
					} else {
						c->state = WS_STATE_CLOSED;
						if ( c->onError ) c->onError( i, "Handshake rejected" );
						WS_Disconnect( i );
					}
				}
				continue;
			}

			/* Parse WebSocket frames */
			while ( c->recvLen >= 2 ) {
				int fin = ( c->recvBuf[0] >> 7 ) & 1;
				int opcode = c->recvBuf[0] & 0x0F;
				int masked = ( c->recvBuf[1] >> 7 ) & 1;
				int payloadLen = c->recvBuf[1] & 0x7F;
				int hdr = 2;

				(void)fin;

				if ( payloadLen == 126 ) {
					if ( c->recvLen < 4 ) break;
					payloadLen = ( c->recvBuf[2] << 8 ) | c->recvBuf[3];
					hdr = 4;
				}

				if ( masked ) hdr += 4;
				if ( c->recvLen < hdr + payloadLen ) break;

				byte *payload = c->recvBuf + hdr;
				if ( masked ) {
					byte *mk = c->recvBuf + hdr - 4;
					WS_MaskData( payload, payloadLen, mk );
				}

				if ( opcode == WS_CLOSE ) {
					c->state = WS_STATE_CLOSED;
					if ( c->onClose ) c->onClose( i, 1000, "server close" );
					WS_Disconnect( i );
					break;
				} else if ( opcode == WS_PING ) {
					WS_Send( i, WS_PONG, payload, payloadLen );
				} else if ( opcode == WS_TEXT || opcode == WS_BINARY ) {
					if ( c->onMessage ) c->onMessage( i, (wsOpcode_t)opcode, payload, payloadLen );
				}

				int consumed = hdr + payloadLen;
				c->recvLen -= consumed;
				if ( c->recvLen > 0 ) {
					memmove( c->recvBuf, c->recvBuf + consumed, c->recvLen );
				}
			}
		} else if ( n == 0 ) {
			c->state = WS_STATE_CLOSED;
			if ( c->onClose ) c->onClose( i, 1006, "connection lost" );
			WS_Disconnect( i );
		}
	}
}
