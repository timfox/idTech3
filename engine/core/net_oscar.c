/*
===========================================================================
Open OSCAR gateway bridge.

This is a WebSocket client for a local sidecar gateway. The gateway owns
OSCAR/TOC protocol semantics; the engine only exchanges validated JSON events.
===========================================================================
*/

#define _DEFAULT_SOURCE

#include "q_shared.h"
#include "qcommon.h"
#include "net_oscar.h"
#include "net_oscar_protocol.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET oscarSocket_t;
#define OSCAR_INVALID_SOCKET INVALID_SOCKET
#define OSCAR_CLOSE_SOCKET( s ) closesocket( s )
#define OSCAR_LAST_ERROR WSAGetLastError()
#define OSCAR_WOULD_BLOCK( e ) ( (e) == WSAEWOULDBLOCK || (e) == WSAEINPROGRESS || (e) == WSAEALREADY )
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
typedef int oscarSocket_t;
#define OSCAR_INVALID_SOCKET ( -1 )
#define OSCAR_CLOSE_SOCKET( s ) close( s )
#define OSCAR_LAST_ERROR errno
#define OSCAR_WOULD_BLOCK( e ) ( (e) == EWOULDBLOCK || (e) == EAGAIN || (e) == EINPROGRESS || (e) == EALREADY )
#endif

#define OSCAR_EVENT_QUEUE 16
#define OSCAR_RECV_BUFFER 8192

typedef struct {
	qboolean initialized;
	oscarState_t state;
	oscarSocket_t socket;
	qboolean handshakeSent;
	qboolean handshakeDone;
	int nextRequestId;
	int reconnectAttempt;
	int reconnectAt;
	char currentRoom[MAX_QPATH];
	char lastError[256];
	byte recvBuffer[OSCAR_RECV_BUFFER];
	int recvLen;
	oscarEvent_t events[OSCAR_EVENT_QUEUE];
	int eventHead;
	int eventTail;
} oscarClient_t;

static oscarClient_t oscar;

static cvar_t *oscar_enable;
static cvar_t *oscar_gateway;
static cvar_t *oscar_gatewayPort;
static cvar_t *oscar_account;
static cvar_t *oscar_token;
static cvar_t *oscar_defaultRoom;
static cvar_t *oscar_reconnect;
static cvar_t *oscar_reconnectMaxDelay;
static cvar_t *oscar_debug;
static cvar_t *oscar_presence;

static void OSCAR_RegisterCvars( void )
{
	const char *envToken;

	if ( oscar_enable ) {
		return;
	}

	oscar_enable = Cvar_Get( "oscar_enable", "0", CVAR_ARCHIVE_ND );
	oscar_gateway = Cvar_Get( "oscar_gateway", "127.0.0.1", CVAR_ARCHIVE_ND );
	oscar_gatewayPort = Cvar_Get( "oscar_gatewayPort", "5191", CVAR_ARCHIVE_ND );
	oscar_account = Cvar_Get( "oscar_account", "", CVAR_ARCHIVE_ND );
	oscar_token = Cvar_Get( "oscar_token", "", CVAR_PROTECTED | CVAR_NORESTART );
	oscar_defaultRoom = Cvar_Get( "oscar_defaultRoom", "", CVAR_ARCHIVE_ND );
	oscar_reconnect = Cvar_Get( "oscar_reconnect", "1", CVAR_ARCHIVE_ND );
	oscar_reconnectMaxDelay = Cvar_Get( "oscar_reconnectMaxDelay", "60", CVAR_ARCHIVE_ND );
	oscar_debug = Cvar_Get( "oscar_debug", "0", CVAR_ARCHIVE_ND );
	oscar_presence = Cvar_Get( "oscar_presence", "1", CVAR_ARCHIVE_ND );

	envToken = getenv( "IDTECH3_OSCAR_TOKEN" );
	if ( envToken && envToken[0] && !oscar_token->string[0] ) {
		Cvar_Set( "oscar_token", envToken );
	}

	Cvar_SetDescription( oscar_enable, "Enable Open OSCAR gateway integration (0=off, 1=on)." );
	Cvar_SetDescription( oscar_gateway, "Open OSCAR gateway address. Use localhost or a numeric private IP to avoid DNS stalls." );
	Cvar_SetDescription( oscar_gatewayPort, "Open OSCAR gateway WebSocket port." );
	Cvar_SetDescription( oscar_account, "Gateway service account name. Passwords stay outside archived cvars." );
	Cvar_SetDescription( oscar_token, "Short-lived gateway token; prefer IDTECH3_OSCAR_TOKEN." );
	Cvar_SetDescription( oscar_defaultRoom, "Default OSCAR room for server announcements." );
	Cvar_SetDescription( oscar_reconnect, "Reconnect to the OSCAR gateway after disconnects (0=off, 1=on)." );
	Cvar_SetDescription( oscar_reconnectMaxDelay, "Maximum OSCAR gateway reconnect delay in seconds." );
	Cvar_SetDescription( oscar_debug, "Print OSCAR gateway protocol diagnostics (0=off, 1=on)." );
	Cvar_SetDescription( oscar_presence, "Forward presence updates from the OSCAR gateway (0=off, 1=on)." );
}

static void OSCAR_SetError( const char *error )
{
	Q_strncpyz( oscar.lastError, error ? error : "", sizeof( oscar.lastError ) );
	if ( oscar.lastError[0] ) {
		Com_Printf( S_COLOR_YELLOW "OSCAR: %s\n", oscar.lastError );
	}
}

static void OSCAR_QueueEvent( const oscarEvent_t *ev )
{
	if ( !ev ) {
		return;
	}
	oscar.events[oscar.eventHead] = *ev;
	oscar.eventHead = ( oscar.eventHead + 1 ) % OSCAR_EVENT_QUEUE;
	if ( oscar.eventHead == oscar.eventTail ) {
		oscar.eventTail = ( oscar.eventTail + 1 ) % OSCAR_EVENT_QUEUE;
	}
}

static void OSCAR_CloseSocket( void )
{
	if ( oscar.socket != OSCAR_INVALID_SOCKET ) {
		OSCAR_CLOSE_SOCKET( oscar.socket );
		oscar.socket = OSCAR_INVALID_SOCKET;
	}
	oscar.handshakeSent = qfalse;
	oscar.handshakeDone = qfalse;
	oscar.recvLen = 0;
}

static void OSCAR_SetNonBlocking( oscarSocket_t s )
{
#ifdef _WIN32
	u_long mode = 1;
	ioctlsocket( s, FIONBIO, &mode );
#else
	int flags = fcntl( s, F_GETFL, 0 );
	if ( flags >= 0 ) {
		fcntl( s, F_SETFL, flags | O_NONBLOCK );
	}
#endif
}

static int OSCAR_BackoffMs( void )
{
	int delay = 1000;
	int maxDelay = oscar_reconnectMaxDelay ? oscar_reconnectMaxDelay->integer * 1000 : 60000;
	int i;

	for ( i = 0; i < oscar.reconnectAttempt && delay < maxDelay; i++ ) {
		delay <<= 1;
	}
	if ( delay > maxDelay ) {
		delay = maxDelay;
	}
	return delay;
}

static qboolean OSCAR_BuildGatewayAddress( struct sockaddr_storage *addr, int *addrLen, int *family )
{
	const char *host = ( oscar_gateway && oscar_gateway->string[0] ) ? oscar_gateway->string : "127.0.0.1";
	int port = oscar_gatewayPort ? oscar_gatewayPort->integer : 5191;
	struct sockaddr_in *addr4;
	struct sockaddr_in6 *addr6;

	if ( !addr || !addrLen || !family ) {
		return qfalse;
	}

	if ( !Q_stricmp( host, "localhost" ) ) {
		host = "127.0.0.1";
	}

	if ( port <= 0 || port > 65535 ) {
		OSCAR_SetError( "gateway port out of range" );
		return qfalse;
	}

	Com_Memset( addr, 0, sizeof( *addr ) );

	addr4 = (struct sockaddr_in *)addr;
	if ( inet_pton( AF_INET, host, &addr4->sin_addr ) == 1 ) {
		addr4->sin_family = AF_INET;
		addr4->sin_port = htons( (unsigned short)port );
		*addrLen = (int)sizeof( *addr4 );
		*family = AF_INET;
		return qtrue;
	}

	addr6 = (struct sockaddr_in6 *)addr;
	if ( inet_pton( AF_INET6, host, &addr6->sin6_addr ) == 1 ) {
		addr6->sin6_family = AF_INET6;
		addr6->sin6_port = htons( (unsigned short)port );
		*addrLen = (int)sizeof( *addr6 );
		*family = AF_INET6;
		return qtrue;
	}

	OSCAR_SetError( "gateway must be localhost or a numeric IP address" );
	return qfalse;
}

static qboolean OSCAR_OpenSocket( void )
{
	struct sockaddr_storage addr;
	int addrLen = 0;
	int family = AF_UNSPEC;
	int err;

	if ( !OSCAR_BuildGatewayAddress( &addr, &addrLen, &family ) ) {
		return qfalse;
	}

	oscar.socket = socket( family, SOCK_STREAM, 0 );
	if ( oscar.socket == OSCAR_INVALID_SOCKET ) {
		OSCAR_SetError( "gateway socket creation failed" );
		return qfalse;
	}

	OSCAR_SetNonBlocking( oscar.socket );
	err = connect( oscar.socket, (struct sockaddr *)&addr, addrLen );
	if ( err != 0 && !OSCAR_WOULD_BLOCK( OSCAR_LAST_ERROR ) ) {
		OSCAR_CloseSocket();
		OSCAR_SetError( "gateway connect failed" );
		return qfalse;
	}

	return qtrue;
}

static qboolean OSCAR_SendRaw( const void *data, int len )
{
	int sent;

	if ( oscar.socket == OSCAR_INVALID_SOCKET || !data || len <= 0 ) {
		return qfalse;
	}
	sent = send( oscar.socket, (const char *)data, len, 0 );
	if ( sent < 0 && OSCAR_WOULD_BLOCK( OSCAR_LAST_ERROR ) ) {
		return qfalse;
	}
	return (qboolean)( sent == len );
}

static qboolean OSCAR_SendHandshake( void )
{
	char request[1024];

	Com_sprintf( request, sizeof( request ),
		"GET /engine HTTP/1.1\r\n"
		"Host: %s:%d\r\n"
		"Upgrade: websocket\r\n"
		"Connection: Upgrade\r\n"
		"Sec-WebSocket-Key: aWR0ZWNoMy1vc2Nhci1icmlkZ2U=\r\n"
		"Sec-WebSocket-Version: 13\r\n"
		"Origin: idtech3\r\n"
		"\r\n",
		oscar_gateway->string,
		oscar_gatewayPort ? oscar_gatewayPort->integer : 5191 );
	oscar.handshakeSent = OSCAR_SendRaw( request, (int)strlen( request ) );
	return oscar.handshakeSent;
}

static qboolean OSCAR_SendTextFrame( const char *text )
{
	byte frame[OSCAR_MAX_JSON_FRAME + 14];
	byte mask[4];
	int len;
	int hdrLen = 2;

	if ( oscar.state != OSCAR_STATE_ONLINE && oscar.state != OSCAR_STATE_AUTHENTICATING ) {
		return qfalse;
	}
	if ( !text ) {
		return qfalse;
	}
	len = (int)strlen( text );
	if ( len <= 0 || len + 14 > (int)sizeof( frame ) ) {
		return qfalse;
	}

	frame[0] = 0x81;
	if ( len < 126 ) {
		frame[1] = 0x80 | (byte)len;
	} else {
		frame[1] = 0x80 | 126;
		frame[2] = (byte)( len >> 8 );
		frame[3] = (byte)( len & 0xff );
		hdrLen = 4;
	}

	mask[0] = (byte)( rand() & 0xff );
	mask[1] = (byte)( rand() & 0xff );
	mask[2] = (byte)( rand() & 0xff );
	mask[3] = (byte)( rand() & 0xff );
	Com_Memcpy( frame + hdrLen, mask, 4 );
	hdrLen += 4;
	Com_Memcpy( frame + hdrLen, text, len );
	for ( int i = 0; i < len; i++ ) {
		frame[hdrLen + i] ^= mask[i & 3];
	}

	if ( oscar_debug && oscar_debug->integer ) {
		Com_Printf( "OSCAR -> %s\n", text );
	}
	return OSCAR_SendRaw( frame, hdrLen + len );
}

static qboolean OSCAR_SendJson( const char *json )
{
	if ( !json || strlen( json ) >= OSCAR_MAX_JSON_FRAME ) {
		OSCAR_SetError( "gateway message too large" );
		return qfalse;
	}
	return OSCAR_SendTextFrame( json );
}

static qboolean OSCAR_Authenticate( void )
{
	char json[OSCAR_MAX_JSON_FRAME];

	if ( !oscar_account || !oscar_account->string[0] || !oscar_token || !oscar_token->string[0] ) {
		OSCAR_SetError( "oscar_account/oscar_token required for gateway auth" );
		return qfalse;
	}

	if ( !OSCAR_ProtocolBuildAuth( json, sizeof( json ), oscar.nextRequestId++, oscar_account->string, oscar_token->string ) ) {
		OSCAR_SetError( "failed to build auth message" );
		return qfalse;
	}
	oscar.state = OSCAR_STATE_AUTHENTICATING;
	return OSCAR_SendJson( json );
}

static void OSCAR_HandleEvent( const oscarEvent_t *ev )
{
	if ( !ev ) {
		return;
	}

	switch ( ev->type ) {
	case OSCAR_EVENT_CONNECTED:
		oscar.state = OSCAR_STATE_ONLINE;
		oscar.reconnectAttempt = 0;
		Com_Printf( "OSCAR: gateway authenticated\n" );
		if ( oscar_defaultRoom && oscar_defaultRoom->string[0] ) {
			OSCAR_JoinRoom( oscar_defaultRoom->string );
		}
		break;
	case OSCAR_EVENT_DISCONNECTED:
		OSCAR_SetError( ev->text[0] ? ev->text : "gateway disconnected" );
		OSCAR_Disconnect( ev->text );
		break;
	case OSCAR_EVENT_ROOM_MESSAGE:
		Com_Printf( "OSCAR room %s <%s>: %s\n", ev->room, ev->screenName, ev->text );
		break;
	case OSCAR_EVENT_INSTANT_MESSAGE:
		Com_Printf( "OSCAR IM <%s>: %s\n", ev->screenName, ev->text );
		break;
	case OSCAR_EVENT_PRESENCE_CHANGED:
		if ( oscar_presence && oscar_presence->integer ) {
			Com_Printf( "OSCAR presence %s: %s\n", ev->screenName, ev->status );
		}
		break;
	case OSCAR_EVENT_ERROR:
		OSCAR_SetError( ev->text[0] ? ev->text : "gateway error" );
		break;
	default:
		break;
	}
	OSCAR_QueueEvent( ev );
}

static void OSCAR_HandleText( const char *text )
{
	oscarEvent_t ev;

	if ( !text || strlen( text ) >= OSCAR_MAX_JSON_FRAME ) {
		OSCAR_SetError( "dropped oversized gateway event" );
		return;
	}
	if ( oscar_debug && oscar_debug->integer ) {
		Com_Printf( "OSCAR <- %s\n", text );
	}
	if ( OSCAR_ProtocolParseEvent( text, &ev ) ) {
		OSCAR_HandleEvent( &ev );
	}
}

static void OSCAR_ProcessFrames( void )
{
	int offset = 0;

	while ( oscar.recvLen - offset >= 2 ) {
		byte *buf = oscar.recvBuffer + offset;
		int payloadLen = buf[1] & 0x7f;
		int hdrLen = 2;
		char text[OSCAR_MAX_JSON_FRAME];

		if ( payloadLen == 126 ) {
			if ( oscar.recvLen - offset < 4 ) {
				break;
			}
			payloadLen = ( buf[2] << 8 ) | buf[3];
			hdrLen = 4;
		} else if ( payloadLen == 127 ) {
			OSCAR_SetError( "gateway frame too large" );
			OSCAR_Disconnect( "oversized frame" );
			return;
		}

		if ( payloadLen >= OSCAR_MAX_JSON_FRAME ) {
			OSCAR_SetError( "gateway payload too large" );
			OSCAR_Disconnect( "oversized payload" );
			return;
		}
		if ( oscar.recvLen - offset < hdrLen + payloadLen ) {
			break;
		}

		if ( ( buf[0] & 0x0f ) == 0x1 ) {
			Com_Memcpy( text, buf + hdrLen, payloadLen );
			text[payloadLen] = '\0';
			OSCAR_HandleText( text );
		} else if ( ( buf[0] & 0x0f ) == 0x8 ) {
			OSCAR_Disconnect( "gateway close" );
			return;
		}

		offset += hdrLen + payloadLen;
	}

	if ( offset > 0 ) {
		oscar.recvLen -= offset;
		if ( oscar.recvLen > 0 ) {
			memmove( oscar.recvBuffer, oscar.recvBuffer + offset, (size_t)oscar.recvLen );
		}
	}
}

static void OSCAR_ReadSocket( void )
{
	int n;

	if ( oscar.socket == OSCAR_INVALID_SOCKET ) {
		return;
	}

	while ( oscar.recvLen < (int)sizeof( oscar.recvBuffer ) - 1 ) {
		n = recv( oscar.socket, (char *)oscar.recvBuffer + oscar.recvLen,
			(int)sizeof( oscar.recvBuffer ) - oscar.recvLen - 1, 0 );
		if ( n > 0 ) {
			oscar.recvLen += n;
			oscar.recvBuffer[oscar.recvLen] = '\0';
			continue;
		}
		if ( n == 0 ) {
			OSCAR_Disconnect( "gateway closed socket" );
			return;
		}
		if ( OSCAR_WOULD_BLOCK( OSCAR_LAST_ERROR ) ) {
			break;
		}
		OSCAR_Disconnect( "gateway socket read failed" );
		return;
	}

	if ( !oscar.handshakeDone ) {
		char *end = strstr( (char *)oscar.recvBuffer, "\r\n\r\n" );
		if ( end ) {
			if ( !strstr( (char *)oscar.recvBuffer, " 101 " ) ) {
				OSCAR_SetError( "gateway websocket upgrade rejected" );
				OSCAR_Disconnect( "handshake failed" );
				return;
			}
			oscar.handshakeDone = qtrue;
			oscar.recvLen -= (int)( end + 4 - (char *)oscar.recvBuffer );
			if ( oscar.recvLen > 0 ) {
				memmove( oscar.recvBuffer, end + 4, (size_t)oscar.recvLen );
			}
			OSCAR_Authenticate();
		}
		return;
	}

	OSCAR_ProcessFrames();
}

void OSCAR_Init( void )
{
	OSCAR_RegisterCvars();
	Com_Memset( &oscar, 0, sizeof( oscar ) );
	oscar.initialized = qtrue;
	oscar.socket = OSCAR_INVALID_SOCKET;
	oscar.state = OSCAR_STATE_DISABLED;
	oscar.nextRequestId = 1;
	Com_Printf( "OSCAR bridge: %s\n", oscar_enable->integer ? "enabled" : "disabled" );
}

void OSCAR_Shutdown( void )
{
	OSCAR_CloseSocket();
	Com_Memset( &oscar, 0, sizeof( oscar ) );
	oscar.socket = OSCAR_INVALID_SOCKET;
	oscar.state = OSCAR_STATE_DISABLED;
}

qboolean OSCAR_IsAvailable( void )
{
	OSCAR_RegisterCvars();
	return (qboolean)( oscar_enable && oscar_enable->integer );
}

qboolean OSCAR_Connect( void )
{
	OSCAR_RegisterCvars();

	if ( !oscar_enable || !oscar_enable->integer ) {
		oscar.state = OSCAR_STATE_DISABLED;
		return qfalse;
	}
	if ( oscar.state == OSCAR_STATE_CONNECTING || oscar.state == OSCAR_STATE_AUTHENTICATING ||
	     oscar.state == OSCAR_STATE_ONLINE ) {
		return qtrue;
	}

	OSCAR_CloseSocket();
	if ( !OSCAR_OpenSocket() ) {
		oscar.state = OSCAR_STATE_ERROR;
		return qfalse;
	}

	oscar.state = OSCAR_STATE_CONNECTING;
	oscar.lastError[0] = '\0';
	OSCAR_SendHandshake();
	Com_Printf( "OSCAR: connecting to gateway %s:%d\n",
		oscar_gateway->string, oscar_gatewayPort ? oscar_gatewayPort->integer : 5191 );
	return qtrue;
}

void OSCAR_Disconnect( const char *reason )
{
	oscarEvent_t ev;
	qboolean wasConnected = (qboolean)( oscar.state == OSCAR_STATE_ONLINE ||
		oscar.state == OSCAR_STATE_AUTHENTICATING || oscar.state == OSCAR_STATE_CONNECTING );

	OSCAR_CloseSocket();
	if ( reason && reason[0] ) {
		Q_strncpyz( oscar.lastError, reason, sizeof( oscar.lastError ) );
	}

	if ( oscar_enable && oscar_enable->integer && oscar_reconnect && oscar_reconnect->integer && wasConnected ) {
		oscar.state = OSCAR_STATE_RECONNECTING;
		oscar.reconnectAttempt++;
		oscar.reconnectAt = Sys_Milliseconds() + OSCAR_BackoffMs();
	} else {
		oscar.state = oscar_enable && oscar_enable->integer ? OSCAR_STATE_DISCONNECTED : OSCAR_STATE_DISABLED;
	}

	Com_Memset( &ev, 0, sizeof( ev ) );
	ev.type = OSCAR_EVENT_DISCONNECTED;
	Q_strncpyz( ev.text, reason ? reason : "", sizeof( ev.text ) );
	OSCAR_QueueEvent( &ev );
}

void OSCAR_Frame( int realtime )
{
	(void)realtime;

	if ( !oscar.initialized ) {
		return;
	}
	if ( !oscar_enable || !oscar_enable->integer ) {
		if ( oscar.state != OSCAR_STATE_DISABLED ) {
			OSCAR_Disconnect( "disabled" );
			oscar.state = OSCAR_STATE_DISABLED;
		}
		return;
	}

	if ( oscar.state == OSCAR_STATE_RECONNECTING && Sys_Milliseconds() >= oscar.reconnectAt ) {
		OSCAR_Connect();
	}
	if ( oscar.state == OSCAR_STATE_CONNECTING && !oscar.handshakeSent ) {
		OSCAR_SendHandshake();
	}
	if ( oscar.state == OSCAR_STATE_CONNECTING || oscar.state == OSCAR_STATE_AUTHENTICATING ||
	     oscar.state == OSCAR_STATE_ONLINE ) {
		OSCAR_ReadSocket();
	}
}

static qboolean OSCAR_SendBuiltJson( qboolean ok, const char *json )
{
	if ( !ok ) {
		OSCAR_SetError( "failed to build gateway request" );
		return qfalse;
	}
	if ( oscar.state != OSCAR_STATE_ONLINE ) {
		OSCAR_SetError( "gateway is not online" );
		return qfalse;
	}
	return OSCAR_SendJson( json );
}

qboolean OSCAR_SendIM( const char *screenName, const char *message )
{
	char json[OSCAR_MAX_JSON_FRAME];
	return OSCAR_SendBuiltJson(
		OSCAR_ProtocolBuildIM( json, sizeof( json ), oscar.nextRequestId++, screenName, message ), json );
}

qboolean OSCAR_JoinRoom( const char *room )
{
	char json[OSCAR_MAX_JSON_FRAME];
	qboolean ok;

	ok = OSCAR_ProtocolBuildJoinRoom( json, sizeof( json ), oscar.nextRequestId++, room );
	if ( ok && room ) {
		Q_strncpyz( oscar.currentRoom, room, sizeof( oscar.currentRoom ) );
	}
	return OSCAR_SendBuiltJson( ok, json );
}

qboolean OSCAR_LeaveRoom( const char *room )
{
	char json[OSCAR_MAX_JSON_FRAME];
	const char *target = ( room && room[0] ) ? room : oscar.currentRoom;
	qboolean ok = OSCAR_ProtocolBuildLeaveRoom( json, sizeof( json ), oscar.nextRequestId++, target );

	if ( ok && ( !room || !room[0] || !Q_stricmp( oscar.currentRoom, room ) ) ) {
		oscar.currentRoom[0] = '\0';
	}
	return OSCAR_SendBuiltJson( ok, json );
}

qboolean OSCAR_SendRoomMessage( const char *room, const char *message )
{
	char json[OSCAR_MAX_JSON_FRAME];
	const char *target = ( room && room[0] ) ? room : oscar.currentRoom;
	const char *sender = oscar_account && oscar_account->string[0] ? oscar_account->string : "idtech3";

	return OSCAR_SendBuiltJson(
		OSCAR_ProtocolBuildRoomMessage( json, sizeof( json ), oscar.nextRequestId++, target, sender, message ), json );
}

qboolean OSCAR_SetPresence( const char *status, const char *message )
{
	char json[OSCAR_MAX_JSON_FRAME];
	return OSCAR_SendBuiltJson(
		OSCAR_ProtocolBuildPresence( json, sizeof( json ), oscar.nextRequestId++, status, message ), json );
}

oscarState_t OSCAR_GetState( void )
{
	return oscar.state;
}

const char *OSCAR_GetStatusString( void )
{
	switch ( oscar.state ) {
	case OSCAR_STATE_DISABLED: return "disabled";
	case OSCAR_STATE_DISCONNECTED: return "disconnected";
	case OSCAR_STATE_CONNECTING: return "connecting";
	case OSCAR_STATE_AUTHENTICATING: return "authenticating";
	case OSCAR_STATE_ONLINE: return "online";
	case OSCAR_STATE_RECONNECTING: return "reconnecting";
	case OSCAR_STATE_ERROR: return "error";
	default: return "unknown";
	}
}

const char *OSCAR_GetLastError( void )
{
	return oscar.lastError;
}

const char *OSCAR_GetCurrentRoom( void )
{
	return oscar.currentRoom;
}

int OSCAR_GetReconnectAttempt( void )
{
	return oscar.reconnectAttempt;
}

qboolean OSCAR_PollEvent( oscarEvent_t *eventOut )
{
	if ( oscar.eventTail == oscar.eventHead || !eventOut ) {
		return qfalse;
	}
	*eventOut = oscar.events[oscar.eventTail];
	oscar.eventTail = ( oscar.eventTail + 1 ) % OSCAR_EVENT_QUEUE;
	return qtrue;
}
