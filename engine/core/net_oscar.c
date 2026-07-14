/*
===========================================================================
Open OSCAR client: direct FLAP/BOS (+ optional Chat service) or JSON gateway.
===========================================================================
*/

#define _DEFAULT_SOURCE

#include "q_shared.h"
#include "qcommon.h"
#include "net_oscar.h"
#include "net_oscar_protocol.h"
#include "net_oscar_raw.h"

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
#define OSCAR_ROSTER_CVAR_MAX 2048
#define OSCAR_EVENT_LOG_CVAR_MAX 1024
#define OSCAR_CHAT_EXCHANGE 4

typedef enum {
	OSCAR_MODE_GATEWAY = 0,
	OSCAR_MODE_DIRECT
} oscarMode_t;

typedef enum {
	OSCAR_DIRECT_NONE = 0,
	OSCAR_DIRECT_AUTH_WAIT_GREETING,
	OSCAR_DIRECT_AUTH_WAIT_REPLY,
	OSCAR_DIRECT_BOS_WAIT_GREETING,
	OSCAR_DIRECT_BOS_WAIT_ONLINE,
	OSCAR_DIRECT_ONLINE
} oscarDirectPhase_t;

typedef enum {
	OSCAR_CHAT_NONE = 0,
	OSCAR_CHAT_WAIT_REDIRECT,
	OSCAR_CHAT_WAIT_GREETING,
	OSCAR_CHAT_WAIT_ONLINE,
	OSCAR_CHAT_ONLINE
} oscarChatPhase_t;

typedef struct {
	qboolean initialized;
	oscarState_t state;
	oscarMode_t mode;
	oscarDirectPhase_t directPhase;
	oscarChatPhase_t chatPhase;
	oscarSocket_t socket;
	oscarSocket_t chatSocket;
	qboolean handshakeSent;
	qboolean handshakeDone;
	unsigned short flapSequence;
	unsigned short chatFlapSequence;
	int nextRequestId;
	int reconnectAttempt;
	int reconnectAt;
	char bosHost[128];
	int bosPort;
	byte authCookie[OSCAR_RAW_MAX_COOKIE];
	int authCookieLen;
	byte chatCookie[OSCAR_RAW_MAX_COOKIE];
	int chatCookieLen;
	char chatHost[128];
	int chatPort;
	char pendingRoom[MAX_QPATH];
	char currentRoom[MAX_QPATH];
	char lastError[256];
	char eventLog[OSCAR_EVENT_LOG_CVAR_MAX];
	byte recvBuffer[OSCAR_RECV_BUFFER];
	int recvLen;
	byte chatRecvBuffer[OSCAR_RECV_BUFFER];
	int chatRecvLen;
	oscarEvent_t events[OSCAR_EVENT_QUEUE];
	int eventHead;
	int eventTail;
	oscarBuddy_t buddies[OSCAR_MAX_BUDDIES];
	int buddyCount;
	unsigned int rosterGeneration;
} oscarClient_t;

static oscarClient_t oscar;

static cvar_t *oscar_enable;
static cvar_t *oscar_mode;
static cvar_t *oscar_gateway;
static cvar_t *oscar_gatewayPort;
static cvar_t *oscar_account;
static cvar_t *oscar_token;
static cvar_t *oscar_password;
static cvar_t *oscar_defaultRoom;
static cvar_t *oscar_reconnect;
static cvar_t *oscar_reconnectMaxDelay;
static cvar_t *oscar_debug;
static cvar_t *oscar_presence;
static cvar_t *oscar_notify;
static cvar_t *oscar_rosterSnapshot;
static cvar_t *oscar_rosterGen;
static cvar_t *oscar_stateCvar;
static cvar_t *oscar_currentRoomCvar;
static cvar_t *oscar_lastErrorCvar;
static cvar_t *oscar_lastEventCvar;
static cvar_t *oscar_eventLogCvar;
static qboolean oscar_cmdsRegistered;

static void OSCAR_CloseChatSocket( void );
static void OSCAR_RefreshRosterSnapshot( void );
static void OSCAR_RefreshUiSnapshot( void );
static void OSCAR_SetActivity( const char *text );
static void OSCAR_AppendActivityLog( const char *text );
static void OSCAR_RosterApplyPresence( const oscarEvent_t *ev );
static qboolean OSCAR_SendOnSocket( oscarSocket_t sock, const void *data, int len );
static qboolean OSCAR_OpenSocketOnto( oscarSocket_t *sockOut, const char *host, int port );

static void OSCAR_RegisterCvars( void )
{
	const char *envToken;
	const char *envPassword;

	if ( oscar_enable ) {
		return;
	}

	oscar_enable = Cvar_Get( "oscar_enable", "0", CVAR_ARCHIVE_ND );
	oscar_mode = Cvar_Get( "oscar_mode", "direct", CVAR_ARCHIVE_ND );
	oscar_gateway = Cvar_Get( "oscar_gateway", "127.0.0.1", CVAR_ARCHIVE_ND );
	oscar_gatewayPort = Cvar_Get( "oscar_gatewayPort", "5190", CVAR_ARCHIVE_ND );
	oscar_account = Cvar_Get( "oscar_account", "", CVAR_ARCHIVE_ND );
	oscar_token = Cvar_Get( "oscar_token", "", CVAR_PROTECTED | CVAR_NORESTART );
	oscar_password = Cvar_Get( "oscar_password", "", CVAR_PROTECTED | CVAR_NORESTART );
	oscar_defaultRoom = Cvar_Get( "oscar_defaultRoom", "", CVAR_ARCHIVE_ND );
	oscar_reconnect = Cvar_Get( "oscar_reconnect", "1", CVAR_ARCHIVE_ND );
	oscar_reconnectMaxDelay = Cvar_Get( "oscar_reconnectMaxDelay", "60", CVAR_ARCHIVE_ND );
	oscar_debug = Cvar_Get( "oscar_debug", "0", CVAR_ARCHIVE_ND );
	oscar_presence = Cvar_Get( "oscar_presence", "1", CVAR_ARCHIVE_ND );
	oscar_notify = Cvar_Get( "oscar_notify", "1", CVAR_ARCHIVE_ND );
	oscar_rosterSnapshot = Cvar_Get( "oscar_rosterSnapshot", "", CVAR_ROM | CVAR_NORESTART );
	oscar_rosterGen = Cvar_Get( "oscar_rosterGen", "0", CVAR_ROM | CVAR_NORESTART );
	oscar_stateCvar = Cvar_Get( "oscar_state", "disabled", CVAR_ROM | CVAR_NORESTART );
	oscar_currentRoomCvar = Cvar_Get( "oscar_currentRoom", "", CVAR_ROM | CVAR_NORESTART );
	oscar_lastErrorCvar = Cvar_Get( "oscar_lastError", "", CVAR_ROM | CVAR_NORESTART );
	oscar_lastEventCvar = Cvar_Get( "oscar_lastEvent", "", CVAR_ROM | CVAR_NORESTART );
	oscar_eventLogCvar = Cvar_Get( "oscar_eventLog", "", CVAR_ROM | CVAR_NORESTART );

	envToken = getenv( "IDTECH3_OSCAR_TOKEN" );
	if ( envToken && envToken[0] && !oscar_token->string[0] ) {
		Cvar_Set( "oscar_token", envToken );
	}
	envPassword = getenv( "IDTECH3_OSCAR_PASSWORD" );
	if ( envPassword && envPassword[0] && !oscar_password->string[0] ) {
		Cvar_Set( "oscar_password", envPassword );
	}

	Cvar_SetDescription( oscar_enable, "Enable Open OSCAR integration (0=off, 1=on)." );
	Cvar_SetDescription( oscar_mode, "Open OSCAR transport mode: direct raw FLAP/BOS client or gateway WebSocket bridge." );
	Cvar_SetDescription( oscar_gateway, "Open OSCAR host. Use localhost or a numeric private IP to avoid DNS stalls." );
	Cvar_SetDescription( oscar_gatewayPort, "Open OSCAR port. Direct mode usually uses 5190; gateway mode usually uses 5191." );
	Cvar_SetDescription( oscar_account, "OSCAR screen name (hybrid: service SN on dedicated, local shared SN on client)." );
	Cvar_SetDescription( oscar_token, "Short-lived gateway token; prefer IDTECH3_OSCAR_TOKEN." );
	Cvar_SetDescription( oscar_password, "Raw OSCAR account password for direct mode; prefer IDTECH3_OSCAR_PASSWORD." );
	Cvar_SetDescription( oscar_defaultRoom, "Default OSCAR room for server announcements / auto-join." );
	Cvar_SetDescription( oscar_reconnect, "Reconnect to OSCAR after disconnects (0=off, 1=on)." );
	Cvar_SetDescription( oscar_reconnectMaxDelay, "Maximum OSCAR reconnect delay in seconds." );
	Cvar_SetDescription( oscar_debug, "Print OSCAR protocol diagnostics (0=off, 1=on)." );
	Cvar_SetDescription( oscar_presence, "Forward buddy presence updates (0=off, 1=on)." );
	Cvar_SetDescription( oscar_notify, "Print IM/room messages to console/notify (0=off, 1=on)." );
	Cvar_SetDescription( oscar_rosterSnapshot, "ROM snapshot of buddy roster for ImGui/UI (name:status;...)." );
	Cvar_SetDescription( oscar_rosterGen, "ROM roster generation counter for UI dirty checks." );
	Cvar_SetDescription( oscar_stateCvar, "ROM OSCAR connection state mirror for native UI." );
	Cvar_SetDescription( oscar_currentRoomCvar, "ROM OSCAR current room mirror for native UI." );
	Cvar_SetDescription( oscar_lastErrorCvar, "ROM OSCAR last error mirror for native UI." );
	Cvar_SetDescription( oscar_lastEventCvar, "ROM OSCAR last user-facing event mirror for native UI." );
	Cvar_SetDescription( oscar_eventLogCvar, "ROM OSCAR user-facing activity transcript for native UI." );
}

static void OSCAR_RefreshUiSnapshot( void )
{
	if ( !oscar_stateCvar ) {
		return;
	}
	Cvar_Set( "oscar_state", OSCAR_GetStatusString() );
	Cvar_Set( "oscar_currentRoom", oscar.currentRoom );
	Cvar_Set( "oscar_lastError", oscar.lastError );
	(void)oscar_currentRoomCvar;
	(void)oscar_lastErrorCvar;
}

static void OSCAR_SetActivity( const char *text )
{
	if ( !oscar_lastEventCvar ) {
		return;
	}
	Cvar_Set( "oscar_lastEvent", text ? text : "" );
	OSCAR_AppendActivityLog( text );
}

static void OSCAR_AppendActivityLog( const char *text )
{
	char clean[256];
	char next[OSCAR_EVENT_LOG_CVAR_MAX];
	int i;

	if ( !oscar_eventLogCvar || !text || !text[0] ) {
		return;
	}

	Q_strncpyz( clean, text, sizeof( clean ) );
	for ( i = 0; clean[i]; ++i ) {
		if ( clean[i] == '\n' || clean[i] == '\r' || clean[i] == '|' ) {
			clean[i] = ' ';
		}
	}

	Com_sprintf( next, sizeof( next ), "%s%s%s",
		oscar.eventLog,
		oscar.eventLog[0] ? "|" : "",
		clean );
	while ( (int)strlen( next ) >= (int)sizeof( oscar.eventLog ) - 1 ) {
		char *separator = strchr( next, '|' );
		if ( !separator ) {
			break;
		}
		memmove( next, separator + 1, strlen( separator + 1 ) + 1 );
	}

	Q_strncpyz( oscar.eventLog, next, sizeof( oscar.eventLog ) );
	Cvar_Set( "oscar_eventLog", oscar.eventLog );
}

static void OSCAR_SetError( const char *error )
{
	Q_strncpyz( oscar.lastError, error ? error : "", sizeof( oscar.lastError ) );
	OSCAR_RefreshUiSnapshot();
	if ( oscar.lastError[0] ) {
		OSCAR_SetActivity( va( "error: %s", oscar.lastError ) );
	}
	if ( oscar.lastError[0] ) {
		Com_Printf( S_COLOR_YELLOW "OSCAR: %s\n", oscar.lastError );
	}
}

static void OSCAR_QueueEvent( const oscarEvent_t *ev )
{
	if ( !ev ) {
		return;
	}
	switch ( ev->type ) {
	case OSCAR_EVENT_CONNECTED:
		OSCAR_SetActivity( "connected to OSCAR" );
		break;
	case OSCAR_EVENT_DISCONNECTED:
		OSCAR_SetActivity( ev->text[0] ? va( "disconnected: %s", ev->text ) : "disconnected" );
		break;
	case OSCAR_EVENT_INSTANT_MESSAGE:
		OSCAR_SetActivity( va( "IM <%s>: %s", ev->screenName[0] ? ev->screenName : "?", ev->text ) );
		break;
	case OSCAR_EVENT_ROOM_MESSAGE:
		OSCAR_SetActivity( va( "room %s <%s>: %s", ev->room[0] ? ev->room : "?", ev->screenName[0] ? ev->screenName : "?", ev->text ) );
		break;
	case OSCAR_EVENT_PRESENCE_CHANGED:
		OSCAR_SetActivity( va( "%s is %s", ev->screenName[0] ? ev->screenName : "buddy", ev->status[0] ? ev->status : "online" ) );
		break;
	case OSCAR_EVENT_REQUEST_COMPLETE:
		OSCAR_SetActivity( ev->room[0] ? va( "room ready: %s", ev->room ) : "request complete" );
		break;
	case OSCAR_EVENT_ERROR:
		OSCAR_SetActivity( ev->text[0] ? va( "error: %s", ev->text ) : "OSCAR error" );
		break;
	default:
		break;
	}
	oscar.events[oscar.eventHead] = *ev;
	oscar.eventHead = ( oscar.eventHead + 1 ) % OSCAR_EVENT_QUEUE;
	if ( oscar.eventHead == oscar.eventTail ) {
		oscar.eventTail = ( oscar.eventTail + 1 ) % OSCAR_EVENT_QUEUE;
	}
}

static void OSCAR_CloseChatSocket( void )
{
	if ( oscar.chatSocket != OSCAR_INVALID_SOCKET ) {
		OSCAR_CLOSE_SOCKET( oscar.chatSocket );
		oscar.chatSocket = OSCAR_INVALID_SOCKET;
	}
	oscar.chatRecvLen = 0;
	oscar.chatCookieLen = 0;
	oscar.chatHost[0] = '\0';
	oscar.chatPort = 0;
	oscar.chatPhase = OSCAR_CHAT_NONE;
	oscar.chatFlapSequence = 0;
}

static void OSCAR_CloseSocket( void )
{
	OSCAR_CloseChatSocket();
	if ( oscar.socket != OSCAR_INVALID_SOCKET ) {
		OSCAR_CLOSE_SOCKET( oscar.socket );
		oscar.socket = OSCAR_INVALID_SOCKET;
	}
	oscar.handshakeSent = qfalse;
	oscar.handshakeDone = qfalse;
	oscar.recvLen = 0;
	oscar.pendingRoom[0] = '\0';
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

static oscarMode_t OSCAR_ConfiguredMode( void )
{
	if ( oscar_mode && oscar_mode->string[0] && !Q_stricmp( oscar_mode->string, "gateway" ) ) {
		return OSCAR_MODE_GATEWAY;
	}
	return OSCAR_MODE_DIRECT;
}

static void OSCAR_RefreshRosterSnapshot( void )
{
	char buf[OSCAR_ROSTER_CVAR_MAX];
	int used = 0;
	int i;

	buf[0] = '\0';
	for ( i = 0; i < oscar.buddyCount; i++ ) {
		char piece[MAX_NAME_LENGTH + 40];
		int pieceLen;

		Com_sprintf( piece, sizeof( piece ), "%s:%s%s",
			oscar.buddies[i].screenName,
			oscar.buddies[i].status[0] ? oscar.buddies[i].status : ( oscar.buddies[i].online ? "online" : "offline" ),
			( i + 1 < oscar.buddyCount ) ? ";" : "" );
		pieceLen = (int)strlen( piece );
		if ( used + pieceLen >= (int)sizeof( buf ) - 1 ) {
			break;
		}
		Com_Memcpy( buf + used, piece, pieceLen + 1 );
		used += pieceLen;
	}
	Cvar_Set( "oscar_rosterSnapshot", buf );
	Cvar_Set( "oscar_rosterGen", va( "%u", oscar.rosterGeneration ) );
	OSCAR_RefreshUiSnapshot();
}

static int OSCAR_RosterFind( const char *screenName )
{
	int i;

	if ( !screenName || !screenName[0] ) {
		return -1;
	}
	for ( i = 0; i < oscar.buddyCount; i++ ) {
		if ( !Q_stricmp( oscar.buddies[i].screenName, screenName ) ) {
			return i;
		}
	}
	return -1;
}

static int OSCAR_RosterEnsure( const char *screenName )
{
	int idx;

	idx = OSCAR_RosterFind( screenName );
	if ( idx >= 0 ) {
		return idx;
	}
	if ( oscar.buddyCount >= OSCAR_MAX_BUDDIES ) {
		return -1;
	}
	idx = oscar.buddyCount++;
	Com_Memset( &oscar.buddies[idx], 0, sizeof( oscar.buddies[idx] ) );
	Q_strncpyz( oscar.buddies[idx].screenName, screenName, sizeof( oscar.buddies[idx].screenName ) );
	Q_strncpyz( oscar.buddies[idx].status, "offline", sizeof( oscar.buddies[idx].status ) );
	oscar.buddies[idx].online = qfalse;
	oscar.rosterGeneration++;
	OSCAR_RefreshRosterSnapshot();
	return idx;
}

static void OSCAR_RosterApplyPresence( const oscarEvent_t *ev )
{
	int idx;
	qboolean online;

	if ( !ev || !ev->screenName[0] ) {
		return;
	}
	idx = OSCAR_RosterEnsure( ev->screenName );
	if ( idx < 0 ) {
		return;
	}
	online = (qboolean)( Q_stricmp( ev->status, "offline" ) != 0 );
	Q_strncpyz( oscar.buddies[idx].status, ev->status[0] ? ev->status : ( online ? "available" : "offline" ),
		sizeof( oscar.buddies[idx].status ) );
	Q_strncpyz( oscar.buddies[idx].awayMessage, ev->text, sizeof( oscar.buddies[idx].awayMessage ) );
	oscar.buddies[idx].online = online;
	oscar.rosterGeneration++;
	OSCAR_RefreshRosterSnapshot();
}

static void OSCAR_RosterRemove( const char *screenName )
{
	int idx = OSCAR_RosterFind( screenName );
	int i;

	if ( idx < 0 ) {
		return;
	}
	for ( i = idx; i < oscar.buddyCount - 1; i++ ) {
		oscar.buddies[i] = oscar.buddies[i + 1];
	}
	oscar.buddyCount--;
	oscar.rosterGeneration++;
	OSCAR_RefreshRosterSnapshot();
}

int OSCAR_BuddyCount( void )
{
	return oscar.buddyCount;
}

qboolean OSCAR_BuddyGet( int index, oscarBuddy_t *out )
{
	if ( !out || index < 0 || index >= oscar.buddyCount ) {
		return qfalse;
	}
	*out = oscar.buddies[index];
	return qtrue;
}

void OSCAR_BuddyClear( void )
{
	oscar.buddyCount = 0;
	Com_Memset( oscar.buddies, 0, sizeof( oscar.buddies ) );
	oscar.rosterGeneration++;
	OSCAR_RefreshRosterSnapshot();
}

unsigned int OSCAR_GetRosterGeneration( void )
{
	return oscar.rosterGeneration;
}

static qboolean OSCAR_BuildAddress( const char *hostIn, int port, struct sockaddr_storage *addr, int *addrLen, int *family )
{
	const char *host = ( hostIn && hostIn[0] ) ? hostIn : "127.0.0.1";
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

static qboolean OSCAR_OpenSocketOnto( oscarSocket_t *sockOut, const char *host, int port )
{
	struct sockaddr_storage addr;
	int addrLen = 0;
	int family = AF_UNSPEC;
	int err;
	oscarSocket_t s;

	if ( !sockOut ) {
		return qfalse;
	}
	if ( !OSCAR_BuildAddress( host, port, &addr, &addrLen, &family ) ) {
		return qfalse;
	}

	s = socket( family, SOCK_STREAM, 0 );
	if ( s == OSCAR_INVALID_SOCKET ) {
		OSCAR_SetError( "OSCAR socket creation failed" );
		return qfalse;
	}

	OSCAR_SetNonBlocking( s );
	err = connect( s, (struct sockaddr *)&addr, addrLen );
	if ( err != 0 && !OSCAR_WOULD_BLOCK( OSCAR_LAST_ERROR ) ) {
		OSCAR_CLOSE_SOCKET( s );
		OSCAR_SetError( "OSCAR connect failed" );
		return qfalse;
	}

	*sockOut = s;
	return qtrue;
}

static qboolean OSCAR_OpenConfiguredSocket( void )
{
	return OSCAR_OpenSocketOnto( &oscar.socket, oscar_gateway ? oscar_gateway->string : "127.0.0.1",
		oscar_gatewayPort ? oscar_gatewayPort->integer : 5190 );
}

static qboolean OSCAR_SendOnSocket( oscarSocket_t sock, const void *data, int len )
{
	int sent;

	if ( sock == OSCAR_INVALID_SOCKET || !data || len <= 0 ) {
		return qfalse;
	}
	sent = send( sock, (const char *)data, len, 0 );
	if ( sent < 0 && OSCAR_WOULD_BLOCK( OSCAR_LAST_ERROR ) ) {
		return qfalse;
	}
	return (qboolean)( sent == len );
}

static qboolean OSCAR_SendRaw( const void *data, int len )
{
	return OSCAR_SendOnSocket( oscar.socket, data, len );
}

static qboolean OSCAR_SendRawFrameBuffer( const byte *frame, int frameLen )
{
	if ( !frame || frameLen <= 0 ) {
		OSCAR_SetError( "failed to build raw OSCAR frame" );
		return qfalse;
	}
	return OSCAR_SendRaw( frame, frameLen );
}

static qboolean OSCAR_SendChatFrameBuffer( const byte *frame, int frameLen )
{
	if ( !frame || frameLen <= 0 ) {
		OSCAR_SetError( "failed to build raw OSCAR chat frame" );
		return qfalse;
	}
	return OSCAR_SendOnSocket( oscar.chatSocket, frame, frameLen );
}

static qboolean OSCAR_DirectSendLogin( void )
{
	byte frame[OSCAR_RAW_MAX_FRAME];
	int len;

	if ( !oscar_account || !oscar_account->string[0] || !oscar_password || !oscar_password->string[0] ) {
		OSCAR_SetError( "oscar_account/oscar_password required for direct OSCAR auth" );
		return qfalse;
	}

	len = OSCAR_RawBuildLoginSignon( oscar.flapSequence++, oscar_account->string, oscar_password->string, frame, sizeof( frame ) );
	oscar.state = OSCAR_STATE_AUTHENTICATING;
	oscar.directPhase = OSCAR_DIRECT_AUTH_WAIT_REPLY;
	return OSCAR_SendRawFrameBuffer( frame, len );
}

static qboolean OSCAR_DirectSendCookie( void )
{
	byte frame[OSCAR_RAW_MAX_FRAME];
	int len;

	len = OSCAR_RawBuildCookieSignon( oscar.flapSequence++, oscar.authCookie, oscar.authCookieLen, frame, sizeof( frame ) );
	oscar.directPhase = OSCAR_DIRECT_BOS_WAIT_ONLINE;
	return OSCAR_SendRawFrameBuffer( frame, len );
}

static qboolean OSCAR_DirectSendClientOnline( void )
{
	byte frame[OSCAR_RAW_MAX_FRAME];
	int len = OSCAR_RawBuildClientOnline( oscar.flapSequence++, (unsigned int)oscar.nextRequestId++, frame, sizeof( frame ) );
	return OSCAR_SendRawFrameBuffer( frame, len );
}

static qboolean OSCAR_ChatSendCookie( void )
{
	byte frame[OSCAR_RAW_MAX_FRAME];
	int len;

	len = OSCAR_RawBuildCookieSignon( oscar.chatFlapSequence++, oscar.chatCookie, oscar.chatCookieLen, frame, sizeof( frame ) );
	oscar.chatPhase = OSCAR_CHAT_WAIT_ONLINE;
	return OSCAR_SendChatFrameBuffer( frame, len );
}

static qboolean OSCAR_ChatSendClientOnline( void )
{
	byte frame[OSCAR_RAW_MAX_FRAME];
	int len = OSCAR_RawBuildClientOnline( oscar.chatFlapSequence++, (unsigned int)oscar.nextRequestId++, frame, sizeof( frame ) );
	return OSCAR_SendChatFrameBuffer( frame, len );
}

static qboolean OSCAR_DirectReconnectBOS( void )
{
	char host[sizeof( oscar.bosHost )];
	int port = oscar.bosPort > 0 ? oscar.bosPort : 5190;

	Q_strncpyz( host, oscar.bosHost[0] ? oscar.bosHost : ( oscar_gateway ? oscar_gateway->string : "127.0.0.1" ), sizeof( host ) );
	OSCAR_CloseSocket();
	if ( !OSCAR_OpenSocketOnto( &oscar.socket, host, port ) ) {
		oscar.state = OSCAR_STATE_ERROR;
		return qfalse;
	}
	oscar.state = OSCAR_STATE_CONNECTING;
	oscar.directPhase = OSCAR_DIRECT_BOS_WAIT_GREETING;
	Com_Printf( "OSCAR: connecting raw BOS session to %s:%d\n", host, port );
	return qtrue;
}

static qboolean OSCAR_OpenChatRedirect( const oscarRawServiceReply_t *reply )
{
	if ( !reply || reply->errorCode || !reply->cookieLen ) {
		OSCAR_SetError( "chat service redirect failed" );
		oscar.chatPhase = OSCAR_CHAT_NONE;
		return qfalse;
	}

	OSCAR_CloseChatSocket();
	Q_strncpyz( oscar.chatHost, reply->host[0] ? reply->host : "127.0.0.1", sizeof( oscar.chatHost ) );
	oscar.chatPort = reply->port > 0 ? reply->port : 5190;
	Com_Memcpy( oscar.chatCookie, reply->cookie, reply->cookieLen );
	oscar.chatCookieLen = reply->cookieLen;

	if ( !OSCAR_OpenSocketOnto( &oscar.chatSocket, oscar.chatHost, oscar.chatPort ) ) {
		oscar.chatPhase = OSCAR_CHAT_NONE;
		return qfalse;
	}
	oscar.chatPhase = OSCAR_CHAT_WAIT_GREETING;
	Com_Printf( "OSCAR: connecting chat service to %s:%d for room %s\n",
		oscar.chatHost, oscar.chatPort, oscar.pendingRoom[0] ? oscar.pendingRoom : "?" );
	return qtrue;
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
		Com_Printf( "OSCAR: session authenticated\n" );
		if ( oscar_defaultRoom && oscar_defaultRoom->string[0] ) {
			OSCAR_JoinRoom( oscar_defaultRoom->string );
		}
		break;
	case OSCAR_EVENT_DISCONNECTED:
		OSCAR_SetError( ev->text[0] ? ev->text : "disconnected" );
		OSCAR_Disconnect( ev->text );
		break;
	case OSCAR_EVENT_ROOM_MESSAGE:
		if ( !oscar_notify || oscar_notify->integer ) {
			Com_Printf( S_COLOR_CYAN "OSCAR room %s <%s>: %s\n", ev->room, ev->screenName, ev->text );
		}
		break;
	case OSCAR_EVENT_INSTANT_MESSAGE:
		if ( !oscar_notify || oscar_notify->integer ) {
			Com_Printf( S_COLOR_CYAN "OSCAR IM <%s>: %s\n", ev->screenName, ev->text );
		}
		break;
	case OSCAR_EVENT_PRESENCE_CHANGED:
		OSCAR_RosterApplyPresence( ev );
		if ( oscar_presence && oscar_presence->integer ) {
			Com_Printf( "OSCAR presence %s: %s\n", ev->screenName, ev->status );
		}
		break;
	case OSCAR_EVENT_ERROR:
		OSCAR_SetError( ev->text[0] ? ev->text : "OSCAR error" );
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

static void OSCAR_ProcessWebSocketFrames( void )
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

static void OSCAR_HandleChatOnline( void )
{
	oscarEvent_t ev;

	oscar.chatPhase = OSCAR_CHAT_ONLINE;
	Q_strncpyz( oscar.currentRoom, oscar.pendingRoom, sizeof( oscar.currentRoom ) );
	Com_Printf( "OSCAR: chat room online (%s)\n", oscar.currentRoom[0] ? oscar.currentRoom : "?" );
	Com_Memset( &ev, 0, sizeof( ev ) );
	ev.type = OSCAR_EVENT_REQUEST_COMPLETE;
	ev.ok = qtrue;
	ev.requestId = oscar.nextRequestId;
	Q_strncpyz( ev.room, oscar.currentRoom, sizeof( ev.room ) );
	OSCAR_QueueEvent( &ev );
}

static void OSCAR_HandleChatSnac( const oscarRawSnac_t *snac )
{
	oscarEvent_t ev;

	if ( !snac ) {
		return;
	}
	if ( snac->family == 0x0001 && snac->subtype == 0x0003 ) {
		OSCAR_ChatSendClientOnline();
		OSCAR_HandleChatOnline();
		return;
	}
	if ( OSCAR_RawParseChatMessage( snac, oscar.currentRoom, &ev ) ) {
		OSCAR_HandleEvent( &ev );
	}
}

static void OSCAR_HandleChatFlap( const oscarRawFlapFrame_t *flap )
{
	oscarRawSnac_t snac;

	if ( !flap ) {
		return;
	}
	if ( oscar_debug && oscar_debug->integer ) {
		Com_Printf( "OSCAR chat <- channel=%d len=%d phase=%d\n", flap->channel, flap->payloadLen, oscar.chatPhase );
	}
	switch ( flap->channel ) {
	case OSCAR_RAW_FLAP_SIGNON:
		if ( oscar.chatPhase == OSCAR_CHAT_WAIT_GREETING ) {
			OSCAR_ChatSendCookie();
		}
		break;
	case OSCAR_RAW_FLAP_DATA:
		if ( OSCAR_RawParseSnac( flap->payload, flap->payloadLen, &snac ) ) {
			OSCAR_HandleChatSnac( &snac );
		}
		break;
	case OSCAR_RAW_FLAP_SIGNOFF:
	case OSCAR_RAW_FLAP_ERROR:
		OSCAR_CloseChatSocket();
		oscar.currentRoom[0] = '\0';
		OSCAR_SetError( "chat service disconnected" );
		break;
	default:
		break;
	}
}

static void OSCAR_HandleRawSnac( const oscarRawSnac_t *snac )
{
	oscarEvent_t ev;
	oscarRawServiceReply_t serviceReply;

	if ( !snac ) {
		return;
	}

	if ( snac->family == 0x0001 && snac->subtype == 0x0003 ) {
		oscar.state = OSCAR_STATE_ONLINE;
		oscar.directPhase = OSCAR_DIRECT_ONLINE;
		oscar.reconnectAttempt = 0;
		Com_Printf( "OSCAR: raw BOS session online\n" );
		OSCAR_DirectSendClientOnline();
		Com_Memset( &ev, 0, sizeof( ev ) );
		ev.type = OSCAR_EVENT_CONNECTED;
		ev.ok = qtrue;
		OSCAR_QueueEvent( &ev );
		if ( oscar_defaultRoom && oscar_defaultRoom->string[0] ) {
			OSCAR_JoinRoom( oscar_defaultRoom->string );
		}
		return;
	}

	if ( OSCAR_RawParseServiceReply( snac, &serviceReply ) ) {
		if ( oscar.chatPhase == OSCAR_CHAT_WAIT_REDIRECT &&
		     ( serviceReply.service == 0x000e || serviceReply.service == 0x000d ) ) {
			OSCAR_OpenChatRedirect( &serviceReply );
		}
		return;
	}

	if ( OSCAR_RawParseIncomingIM( snac, &ev ) ) {
		OSCAR_HandleEvent( &ev );
		return;
	}
	if ( OSCAR_RawParsePresence( snac, &ev ) ) {
		OSCAR_HandleEvent( &ev );
	}
}

static void OSCAR_HandleRawFlap( const oscarRawFlapFrame_t *flap )
{
	oscarRawAuthReply_t authReply;
	oscarRawSnac_t snac;
	char err[128];

	if ( !flap ) {
		return;
	}

	if ( oscar_debug && oscar_debug->integer ) {
		Com_Printf( "OSCAR raw <- channel=%d len=%d phase=%d\n", flap->channel, flap->payloadLen, oscar.directPhase );
	}

	switch ( flap->channel ) {
	case OSCAR_RAW_FLAP_SIGNON:
		if ( oscar.directPhase == OSCAR_DIRECT_AUTH_WAIT_GREETING ) {
			OSCAR_DirectSendLogin();
		} else if ( oscar.directPhase == OSCAR_DIRECT_BOS_WAIT_GREETING ) {
			OSCAR_DirectSendCookie();
		}
		break;
	case OSCAR_RAW_FLAP_SIGNOFF:
		if ( oscar.directPhase == OSCAR_DIRECT_AUTH_WAIT_REPLY &&
		     OSCAR_RawParseAuthReply( flap->payload, flap->payloadLen, &authReply ) ) {
			if ( authReply.errorCode ) {
				Com_sprintf( err, sizeof( err ), "raw OSCAR auth failed: 0x%04x", authReply.errorCode );
				OSCAR_SetError( err );
				OSCAR_Disconnect( err );
				return;
			}
			Q_strncpyz( oscar.bosHost, authReply.bosHost, sizeof( oscar.bosHost ) );
			oscar.bosPort = authReply.bosPort;
			Com_Memcpy( oscar.authCookie, authReply.cookie, authReply.cookieLen );
			oscar.authCookieLen = authReply.cookieLen;
			OSCAR_DirectReconnectBOS();
		} else {
			OSCAR_Disconnect( "raw OSCAR signoff" );
		}
		break;
	case OSCAR_RAW_FLAP_DATA:
		if ( OSCAR_RawParseSnac( flap->payload, flap->payloadLen, &snac ) ) {
			OSCAR_HandleRawSnac( &snac );
		}
		break;
	case OSCAR_RAW_FLAP_ERROR:
		OSCAR_Disconnect( "raw OSCAR FLAP error" );
		break;
	case OSCAR_RAW_FLAP_KEEPALIVE:
		break;
	default:
		break;
	}
}

static void OSCAR_ProcessRawFramesBuffer( byte *recvBuffer, int *recvLen, void ( *handleFlap )( const oscarRawFlapFrame_t * ) )
{
	int offset = 0;

	while ( *recvLen - offset >= 6 ) {
		oscarRawFlapFrame_t flap;
		int consumed = 0;

		if ( !OSCAR_RawParseFlap( recvBuffer + offset, *recvLen - offset, &flap, &consumed ) ) {
			if ( consumed < 0 ) {
				OSCAR_Disconnect( "invalid raw OSCAR frame" );
				return;
			}
			break;
		}

		handleFlap( &flap );
		if ( oscar.socket == OSCAR_INVALID_SOCKET && oscar.chatSocket == OSCAR_INVALID_SOCKET ) {
			return;
		}
		offset += consumed;
	}

	if ( offset > 0 ) {
		*recvLen -= offset;
		if ( *recvLen > 0 ) {
			memmove( recvBuffer, recvBuffer + offset, (size_t)*recvLen );
		}
	}
}

static void OSCAR_ProcessRawFrames( void )
{
	OSCAR_ProcessRawFramesBuffer( oscar.recvBuffer, &oscar.recvLen, OSCAR_HandleRawFlap );
}

static void OSCAR_ProcessChatFrames( void )
{
	OSCAR_ProcessRawFramesBuffer( oscar.chatRecvBuffer, &oscar.chatRecvLen, OSCAR_HandleChatFlap );
}

static void OSCAR_ReadOneSocket( oscarSocket_t sock, byte *recvBuffer, int *recvLen, int recvCap, qboolean isChat )
{
	int n;

	if ( sock == OSCAR_INVALID_SOCKET ) {
		return;
	}

	while ( *recvLen < recvCap - 1 ) {
		n = recv( sock, (char *)recvBuffer + *recvLen, recvCap - *recvLen - 1, 0 );
		if ( n > 0 ) {
			*recvLen += n;
			recvBuffer[*recvLen] = '\0';
			continue;
		}
		if ( n == 0 ) {
			if ( isChat ) {
				OSCAR_CloseChatSocket();
				oscar.currentRoom[0] = '\0';
				OSCAR_SetError( "chat socket closed" );
			} else {
				OSCAR_Disconnect( "OSCAR closed socket" );
			}
			return;
		}
		if ( OSCAR_WOULD_BLOCK( OSCAR_LAST_ERROR ) ) {
			break;
		}
		if ( isChat ) {
			OSCAR_CloseChatSocket();
			OSCAR_SetError( "chat socket read failed" );
		} else {
			OSCAR_Disconnect( "OSCAR socket read failed" );
		}
		return;
	}
}

static void OSCAR_ReadSocket( void )
{
	OSCAR_ReadOneSocket( oscar.socket, oscar.recvBuffer, &oscar.recvLen, (int)sizeof( oscar.recvBuffer ), qfalse );
	if ( oscar.socket == OSCAR_INVALID_SOCKET ) {
		return;
	}

	if ( !oscar.handshakeDone ) {
		if ( oscar.mode == OSCAR_MODE_DIRECT ) {
			OSCAR_ProcessRawFrames();
		} else {
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
	} else if ( oscar.mode == OSCAR_MODE_DIRECT ) {
		OSCAR_ProcessRawFrames();
	} else {
		OSCAR_ProcessWebSocketFrames();
	}

	if ( oscar.mode == OSCAR_MODE_DIRECT && oscar.chatSocket != OSCAR_INVALID_SOCKET ) {
		OSCAR_ReadOneSocket( oscar.chatSocket, oscar.chatRecvBuffer, &oscar.chatRecvLen,
			(int)sizeof( oscar.chatRecvBuffer ), qtrue );
		if ( oscar.chatSocket != OSCAR_INVALID_SOCKET ) {
			OSCAR_ProcessChatFrames();
		}
	}
}

void OSCAR_Init( void )
{
	OSCAR_RegisterCvars();
	Com_Memset( &oscar, 0, sizeof( oscar ) );
	oscar.initialized = qtrue;
	oscar.socket = OSCAR_INVALID_SOCKET;
	oscar.chatSocket = OSCAR_INVALID_SOCKET;
	oscar.state = OSCAR_STATE_DISABLED;
	oscar.mode = OSCAR_ConfiguredMode();
	oscar.nextRequestId = 1;
	Cvar_Set( "oscar_lastEvent", "" );
	Cvar_Set( "oscar_eventLog", "" );
	OSCAR_RefreshRosterSnapshot();
	OSCAR_RegisterCommands();
	Com_Printf( "OSCAR %s: %s (roster+direct rooms)\n",
		oscar.mode == OSCAR_MODE_DIRECT ? "direct client" : "gateway bridge",
		oscar_enable->integer ? "enabled" : "disabled" );
}

void OSCAR_Shutdown( void )
{
	OSCAR_CloseSocket();
	Com_Memset( &oscar, 0, sizeof( oscar ) );
	oscar.socket = OSCAR_INVALID_SOCKET;
	oscar.chatSocket = OSCAR_INVALID_SOCKET;
	oscar.state = OSCAR_STATE_DISABLED;
	Cvar_Set( "oscar_state", "disabled" );
	Cvar_Set( "oscar_currentRoom", "" );
	Cvar_Set( "oscar_lastError", "" );
	Cvar_Set( "oscar_lastEvent", "" );
	Cvar_Set( "oscar_eventLog", "" );
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
	oscar.mode = OSCAR_ConfiguredMode();
	oscar.directPhase = OSCAR_DIRECT_NONE;
	oscar.authCookieLen = 0;
	oscar.bosHost[0] = '\0';
	oscar.bosPort = 0;
	oscar.currentRoom[0] = '\0';
	if ( !OSCAR_OpenConfiguredSocket() ) {
		oscar.state = OSCAR_STATE_ERROR;
		return qfalse;
	}

	oscar.state = OSCAR_STATE_CONNECTING;
	oscar.lastError[0] = '\0';
	oscar.flapSequence = 0;
	if ( oscar.mode == OSCAR_MODE_DIRECT ) {
		oscar.directPhase = OSCAR_DIRECT_AUTH_WAIT_GREETING;
		oscar.handshakeDone = qtrue;
		Com_Printf( "OSCAR: connecting raw FLAP auth to %s:%d\n",
			oscar_gateway->string, oscar_gatewayPort ? oscar_gatewayPort->integer : 5190 );
	} else {
		OSCAR_SendHandshake();
		Com_Printf( "OSCAR: connecting to gateway %s:%d\n",
			oscar_gateway->string, oscar_gatewayPort ? oscar_gatewayPort->integer : 5191 );
	}
	return qtrue;
}

void OSCAR_Disconnect( const char *reason )
{
	oscarEvent_t ev;
	qboolean wasConnected = (qboolean)( oscar.state == OSCAR_STATE_ONLINE ||
		oscar.state == OSCAR_STATE_AUTHENTICATING || oscar.state == OSCAR_STATE_CONNECTING );

	OSCAR_CloseSocket();
	oscar.currentRoom[0] = '\0';
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
			OSCAR_RefreshUiSnapshot();
		}
		return;
	}

	if ( oscar.state == OSCAR_STATE_RECONNECTING && Sys_Milliseconds() >= oscar.reconnectAt ) {
		OSCAR_Connect();
	}
	if ( oscar.mode == OSCAR_MODE_GATEWAY && oscar.state == OSCAR_STATE_CONNECTING && !oscar.handshakeSent ) {
		OSCAR_SendHandshake();
	}
	if ( oscar.state == OSCAR_STATE_CONNECTING || oscar.state == OSCAR_STATE_AUTHENTICATING ||
	     oscar.state == OSCAR_STATE_ONLINE ) {
		OSCAR_ReadSocket();
	}
	OSCAR_RefreshUiSnapshot();
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

static qboolean OSCAR_SendDirectIM( const char *screenName, const char *message )
{
	byte frame[OSCAR_RAW_MAX_FRAME];
	int len;

	if ( oscar.state != OSCAR_STATE_ONLINE || oscar.mode != OSCAR_MODE_DIRECT ) {
		OSCAR_SetError( "raw OSCAR session is not online" );
		return qfalse;
	}
	len = OSCAR_RawBuildIM( oscar.flapSequence++, (unsigned int)oscar.nextRequestId++, screenName, message, frame, sizeof( frame ) );
	return OSCAR_SendRawFrameBuffer( frame, len );
}

static qboolean OSCAR_SendDirectPresence( const char *status )
{
	byte frame[OSCAR_RAW_MAX_FRAME];
	int len;

	if ( oscar.state != OSCAR_STATE_ONLINE || oscar.mode != OSCAR_MODE_DIRECT ) {
		OSCAR_SetError( "raw OSCAR session is not online" );
		return qfalse;
	}
	len = OSCAR_RawBuildPresence( oscar.flapSequence++, (unsigned int)oscar.nextRequestId++, status, frame, sizeof( frame ) );
	if ( len <= 0 ) {
		OSCAR_SetError( "unsupported raw OSCAR presence status" );
		return qfalse;
	}
	return OSCAR_SendRawFrameBuffer( frame, len );
}

static qboolean OSCAR_JoinRoomDirect( const char *room )
{
	byte frame[OSCAR_RAW_MAX_FRAME];
	int len;

	if ( oscar.state != OSCAR_STATE_ONLINE || oscar.mode != OSCAR_MODE_DIRECT ) {
		OSCAR_SetError( "raw OSCAR session is not online" );
		return qfalse;
	}
	if ( !room || !room[0] ) {
		OSCAR_SetError( "room name required" );
		return qfalse;
	}
	if ( oscar.chatPhase != OSCAR_CHAT_NONE && oscar.chatPhase != OSCAR_CHAT_ONLINE ) {
		OSCAR_SetError( "chat room join already in progress" );
		return qfalse;
	}

	OSCAR_CloseChatSocket();
	oscar.currentRoom[0] = '\0';
	Q_strncpyz( oscar.pendingRoom, room, sizeof( oscar.pendingRoom ) );
	len = OSCAR_RawBuildChatServiceRequest( oscar.flapSequence++, (unsigned int)oscar.nextRequestId++,
		OSCAR_CHAT_EXCHANGE, room, 0, frame, sizeof( frame ) );
	if ( len <= 0 ) {
		OSCAR_SetError( "failed to build chat service request" );
		return qfalse;
	}
	oscar.chatPhase = OSCAR_CHAT_WAIT_REDIRECT;
	Com_Printf( "OSCAR: requesting chat room %s\n", room );
	return OSCAR_SendRawFrameBuffer( frame, len );
}

qboolean OSCAR_SendIM( const char *screenName, const char *message )
{
	char json[OSCAR_MAX_JSON_FRAME];

	if ( oscar.mode == OSCAR_MODE_DIRECT ) {
		return OSCAR_SendDirectIM( screenName, message );
	}
	return OSCAR_SendBuiltJson(
		OSCAR_ProtocolBuildIM( json, sizeof( json ), oscar.nextRequestId++, screenName, message ), json );
}

qboolean OSCAR_JoinRoom( const char *room )
{
	char json[OSCAR_MAX_JSON_FRAME];
	qboolean ok;

	if ( oscar.mode == OSCAR_MODE_DIRECT ) {
		return OSCAR_JoinRoomDirect( room );
	}
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
	qboolean ok;

	if ( oscar.mode == OSCAR_MODE_DIRECT ) {
		if ( room && room[0] && oscar.currentRoom[0] && Q_stricmp( oscar.currentRoom, room ) ) {
			OSCAR_SetError( "not in that chat room" );
			return qfalse;
		}
		OSCAR_CloseChatSocket();
		oscar.currentRoom[0] = '\0';
		oscar.pendingRoom[0] = '\0';
		Com_Printf( "OSCAR: left chat room\n" );
		return qtrue;
	}
	ok = OSCAR_ProtocolBuildLeaveRoom( json, sizeof( json ), oscar.nextRequestId++, target );
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
	byte frame[OSCAR_RAW_MAX_FRAME];
	int len;

	if ( oscar.mode == OSCAR_MODE_DIRECT ) {
		if ( oscar.chatPhase != OSCAR_CHAT_ONLINE || oscar.chatSocket == OSCAR_INVALID_SOCKET ) {
			OSCAR_SetError( "chat room is not online" );
			return qfalse;
		}
		if ( room && room[0] && oscar.currentRoom[0] && Q_stricmp( oscar.currentRoom, room ) ) {
			OSCAR_SetError( "not in that chat room" );
			return qfalse;
		}
		(void)target;
		len = OSCAR_RawBuildChatMessage( oscar.chatFlapSequence++, (unsigned int)oscar.nextRequestId++,
			message, frame, sizeof( frame ) );
		return OSCAR_SendChatFrameBuffer( frame, len );
	}
	return OSCAR_SendBuiltJson(
		OSCAR_ProtocolBuildRoomMessage( json, sizeof( json ), oscar.nextRequestId++, target, sender, message ), json );
}

qboolean OSCAR_SetPresence( const char *status, const char *message )
{
	char json[OSCAR_MAX_JSON_FRAME];

	if ( oscar.mode == OSCAR_MODE_DIRECT ) {
		(void)message;
		return OSCAR_SendDirectPresence( status );
	}
	return OSCAR_SendBuiltJson(
		OSCAR_ProtocolBuildPresence( json, sizeof( json ), oscar.nextRequestId++, status, message ), json );
}

static qboolean OSCAR_SendDirectBuddySub( const char *screenName, qboolean add )
{
	byte frame[OSCAR_RAW_MAX_FRAME];
	int len;

	if ( oscar.state != OSCAR_STATE_ONLINE || oscar.mode != OSCAR_MODE_DIRECT ) {
		OSCAR_SetError( "raw OSCAR session is not online" );
		return qfalse;
	}
	len = add
		? OSCAR_RawBuildBuddyAddTemp( oscar.flapSequence++, (unsigned int)oscar.nextRequestId++, screenName, frame, sizeof( frame ) )
		: OSCAR_RawBuildBuddyDelTemp( oscar.flapSequence++, (unsigned int)oscar.nextRequestId++, screenName, frame, sizeof( frame ) );
	return OSCAR_SendRawFrameBuffer( frame, len );
}

qboolean OSCAR_AddBuddy( const char *screenName )
{
	if ( !screenName || !screenName[0] ) {
		OSCAR_SetError( "screen name required" );
		return qfalse;
	}
	if ( oscar.mode == OSCAR_MODE_DIRECT ) {
		if ( !OSCAR_SendDirectBuddySub( screenName, qtrue ) ) {
			return qfalse;
		}
		OSCAR_RosterEnsure( screenName );
		return qtrue;
	}
	OSCAR_SetError( "gateway buddy subscriptions are not implemented in engine bridge mode" );
	return qfalse;
}

qboolean OSCAR_RemoveBuddy( const char *screenName )
{
	if ( !screenName || !screenName[0] ) {
		OSCAR_SetError( "screen name required" );
		return qfalse;
	}
	if ( oscar.mode == OSCAR_MODE_DIRECT ) {
		if ( !OSCAR_SendDirectBuddySub( screenName, qfalse ) ) {
			return qfalse;
		}
		OSCAR_RosterRemove( screenName );
		return qtrue;
	}
	OSCAR_SetError( "gateway buddy subscriptions are not implemented in engine bridge mode" );
	return qfalse;
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

/*
===============
Shared console commands (client + dedicated)
===============
*/
static void OSCAR_Status_f( void )
{
	Com_Printf( "OSCAR available: %s\n", OSCAR_IsAvailable() ? "yes" : "no" );
	Com_Printf( "OSCAR state: %s\n", OSCAR_GetStatusString() );
	Com_Printf( "OSCAR room: %s\n", OSCAR_GetCurrentRoom()[0] ? OSCAR_GetCurrentRoom() : "<none>" );
	Com_Printf( "OSCAR buddies: %d (gen %u)\n", OSCAR_BuddyCount(), OSCAR_GetRosterGeneration() );
	Com_Printf( "OSCAR reconnect attempt: %d\n", OSCAR_GetReconnectAttempt() );
	if ( OSCAR_GetLastError()[0] ) {
		Com_Printf( "OSCAR last error: %s\n", OSCAR_GetLastError() );
	}
}

static void OSCAR_Buddies_f( void )
{
	int i;
	oscarBuddy_t buddy;

	Com_Printf( "OSCAR buddy roster (%d):\n", OSCAR_BuddyCount() );
	for ( i = 0; i < OSCAR_BuddyCount(); i++ ) {
		if ( !OSCAR_BuddyGet( i, &buddy ) ) {
			continue;
		}
		Com_Printf( "  %s  %s%s%s\n", buddy.screenName, buddy.status,
			buddy.awayMessage[0] ? " — " : "",
			buddy.awayMessage[0] ? buddy.awayMessage : "" );
	}
}

static void OSCAR_Connect_f( void )
{
	if ( !OSCAR_Connect() ) {
		Com_Printf( "OSCAR: connect request failed (%s)\n",
			OSCAR_GetLastError()[0] ? OSCAR_GetLastError() : OSCAR_GetStatusString() );
	}
}

static void OSCAR_Disconnect_f( void )
{
	OSCAR_Disconnect( "operator disconnect" );
}

static void OSCAR_Announce_f( void )
{
	const char *message = Cmd_ArgsFrom( 1 );

	if ( !message[0] ) {
		Com_Printf( "Usage: oscar_announce <message>\n" );
		return;
	}
	if ( !OSCAR_SendRoomMessage( NULL, message ) ) {
		Com_Printf( "OSCAR: announce failed (%s)\n",
			OSCAR_GetLastError()[0] ? OSCAR_GetLastError() : OSCAR_GetStatusString() );
	}
}

static void OSCAR_Join_f( void )
{
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: oscar_join <room>\n" );
		return;
	}
	if ( !OSCAR_JoinRoom( Cmd_Argv( 1 ) ) ) {
		Com_Printf( "OSCAR: join failed (%s)\n",
			OSCAR_GetLastError()[0] ? OSCAR_GetLastError() : OSCAR_GetStatusString() );
	}
}

static void OSCAR_Leave_f( void )
{
	if ( !OSCAR_LeaveRoom( Cmd_Argc() >= 2 ? Cmd_Argv( 1 ) : NULL ) ) {
		Com_Printf( "OSCAR: leave failed (%s)\n",
			OSCAR_GetLastError()[0] ? OSCAR_GetLastError() : OSCAR_GetStatusString() );
	}
}

static void OSCAR_IM_f( void )
{
	const char *message;

	if ( Cmd_Argc() < 3 ) {
		Com_Printf( "Usage: oscar_im <screenName> <message>\n" );
		return;
	}
	message = Cmd_ArgsFrom( 2 );
	if ( !OSCAR_SendIM( Cmd_Argv( 1 ), message ) ) {
		Com_Printf( "OSCAR: IM failed (%s)\n",
			OSCAR_GetLastError()[0] ? OSCAR_GetLastError() : OSCAR_GetStatusString() );
	}
}

static void OSCAR_Presence_f( void )
{
	const char *message;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: oscar_presence <status> [message]\n" );
		return;
	}
	message = Cmd_Argc() >= 3 ? Cmd_ArgsFrom( 2 ) : "";
	if ( !OSCAR_SetPresence( Cmd_Argv( 1 ), message ) ) {
		Com_Printf( "OSCAR: presence failed (%s)\n",
			OSCAR_GetLastError()[0] ? OSCAR_GetLastError() : OSCAR_GetStatusString() );
	}
}

static void OSCAR_BuddyAdd_f( void )
{
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: oscar_buddy_add <screenName>\n" );
		return;
	}
	if ( !OSCAR_AddBuddy( Cmd_Argv( 1 ) ) ) {
		Com_Printf( "OSCAR: buddy add failed (%s)\n",
			OSCAR_GetLastError()[0] ? OSCAR_GetLastError() : OSCAR_GetStatusString() );
	}
}

static void OSCAR_BuddyDel_f( void )
{
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: oscar_buddy_del <screenName>\n" );
		return;
	}
	if ( !OSCAR_RemoveBuddy( Cmd_Argv( 1 ) ) ) {
		Com_Printf( "OSCAR: buddy remove failed (%s)\n",
			OSCAR_GetLastError()[0] ? OSCAR_GetLastError() : OSCAR_GetStatusString() );
	}
}

void OSCAR_RegisterCommands( void )
{
	if ( oscar_cmdsRegistered ) {
		return;
	}
	Cmd_AddCommand( "oscar_status", OSCAR_Status_f );
	Cmd_AddCommand( "oscar_buddies", OSCAR_Buddies_f );
	Cmd_AddCommand( "oscar_connect", OSCAR_Connect_f );
	Cmd_AddCommand( "oscar_disconnect", OSCAR_Disconnect_f );
	Cmd_AddCommand( "oscar_announce", OSCAR_Announce_f );
	Cmd_AddCommand( "oscar_join", OSCAR_Join_f );
	Cmd_AddCommand( "oscar_leave", OSCAR_Leave_f );
	Cmd_AddCommand( "oscar_im", OSCAR_IM_f );
	Cmd_AddCommand( "oscar_presence", OSCAR_Presence_f );
	Cmd_AddCommand( "oscar_buddy_add", OSCAR_BuddyAdd_f );
	Cmd_AddCommand( "oscar_buddy_del", OSCAR_BuddyDel_f );
	oscar_cmdsRegistered = qtrue;
}
