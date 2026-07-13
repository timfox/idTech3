/*
===========================================================================
Open OSCAR gateway protocol helpers.

The engine talks to a local gateway with application-level JSON messages.
It never parses raw OSCAR/TOC packets directly.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "net_oscar_protocol.h"

#include <string.h>

static qboolean OSCAR_JsonEscape( const char *in, char *out, int outSize )
{
	int used = 0;

	if ( !in || !out || outSize <= 0 ) {
		return qfalse;
	}

	out[0] = '\0';
	while ( *in ) {
		unsigned char ch = (unsigned char)*in++;
		const char *rep = NULL;
		char tmp[8];

		switch ( ch ) {
		case '\\': rep = "\\\\"; break;
		case '"': rep = "\\\""; break;
		case '\b': rep = "\\b"; break;
		case '\f': rep = "\\f"; break;
		case '\n': rep = "\\n"; break;
		case '\r': rep = "\\r"; break;
		case '\t': rep = "\\t"; break;
		default:
			if ( ch < 0x20 ) {
				Com_sprintf( tmp, sizeof( tmp ), "\\u%04x", ch );
				rep = tmp;
			}
			break;
		}

		if ( rep ) {
			int n = (int)strlen( rep );
			if ( used + n >= outSize ) {
				return qfalse;
			}
			Com_Memcpy( out + used, rep, n );
			used += n;
			out[used] = '\0';
		} else {
			if ( used + 1 >= outSize ) {
				return qfalse;
			}
			out[used++] = (char)ch;
			out[used] = '\0';
		}
	}

	return qtrue;
}

static qboolean OSCAR_JsonGetString( const char *json, const char *key, char *out, int outSize )
{
	char needle[64];
	const char *p;
	int used = 0;

	if ( !json || !key || !out || outSize <= 0 ) {
		return qfalse;
	}

	out[0] = '\0';
	Com_sprintf( needle, sizeof( needle ), "\"%s\"", key );
	p = strstr( json, needle );
	if ( !p ) {
		return qfalse;
	}
	p = strchr( p + strlen( needle ), ':' );
	if ( !p ) {
		return qfalse;
	}
	p++;
	while ( *p == ' ' || *p == '\t' ) {
		p++;
	}
	if ( *p != '"' ) {
		return qfalse;
	}
	p++;
	while ( *p && *p != '"' ) {
		if ( *p == '\\' && p[1] ) {
			p++;
			switch ( *p ) {
			case 'n': out[used++] = '\n'; break;
			case 'r': out[used++] = '\r'; break;
			case 't': out[used++] = '\t'; break;
			default: out[used++] = *p; break;
			}
		} else {
			out[used++] = *p;
		}
		if ( used + 1 >= outSize ) {
			out[used] = '\0';
			return qtrue;
		}
		p++;
	}
	out[used] = '\0';
	return qtrue;
}

static int OSCAR_JsonGetInt( const char *json, const char *key, int fallback )
{
	char needle[64];
	const char *p;

	Com_sprintf( needle, sizeof( needle ), "\"%s\"", key );
	p = strstr( json, needle );
	if ( !p ) {
		return fallback;
	}
	p = strchr( p + strlen( needle ), ':' );
	if ( !p ) {
		return fallback;
	}
	return atoi( p + 1 );
}

static qboolean OSCAR_JsonGetBool( const char *json, const char *key, qboolean fallback )
{
	char needle[64];
	const char *p;

	Com_sprintf( needle, sizeof( needle ), "\"%s\"", key );
	p = strstr( json, needle );
	if ( !p ) {
		return fallback;
	}
	p = strchr( p + strlen( needle ), ':' );
	if ( !p ) {
		return fallback;
	}
	p++;
	while ( *p == ' ' || *p == '\t' ) {
		p++;
	}
	return (qboolean)( !Q_stricmpn( p, "true", 4 ) ? qtrue : qfalse );
}

static qboolean OSCAR_Build2( char *out, int outSize, const char *type, int requestId,
                              const char *k1, const char *v1, const char *k2, const char *v2 )
{
	char e1[MAX_STRING_CHARS];
	char e2[MAX_STRING_CHARS];

	if ( !OSCAR_JsonEscape( v1 ? v1 : "", e1, sizeof( e1 ) ) ||
	     !OSCAR_JsonEscape( v2 ? v2 : "", e2, sizeof( e2 ) ) ) {
		return qfalse;
	}
	Com_sprintf( out, outSize,
		"{\"type\":\"%s\",\"request_id\":%d,\"%s\":\"%s\",\"%s\":\"%s\"}",
		type, requestId, k1, e1, k2, e2 );
	return qtrue;
}

qboolean OSCAR_ProtocolBuildAuth( char *out, int outSize, int requestId, const char *account, const char *token )
{
	return OSCAR_Build2( out, outSize, "authenticate", requestId, "account", account, "token", token );
}

qboolean OSCAR_ProtocolBuildJoinRoom( char *out, int outSize, int requestId, const char *room )
{
	char eroom[MAX_QPATH * 2];

	if ( !OSCAR_JsonEscape( room ? room : "", eroom, sizeof( eroom ) ) ) {
		return qfalse;
	}
	Com_sprintf( out, outSize, "{\"type\":\"join_room\",\"request_id\":%d,\"room\":\"%s\"}", requestId, eroom );
	return qtrue;
}

qboolean OSCAR_ProtocolBuildLeaveRoom( char *out, int outSize, int requestId, const char *room )
{
	char eroom[MAX_QPATH * 2];

	if ( !OSCAR_JsonEscape( room ? room : "", eroom, sizeof( eroom ) ) ) {
		return qfalse;
	}
	Com_sprintf( out, outSize, "{\"type\":\"leave_room\",\"request_id\":%d,\"room\":\"%s\"}", requestId, eroom );
	return qtrue;
}

qboolean OSCAR_ProtocolBuildRoomMessage( char *out, int outSize, int requestId, const char *room, const char *sender, const char *text )
{
	char eroom[MAX_QPATH * 2];
	char esender[MAX_NAME_LENGTH * 2];
	char etext[MAX_STRING_CHARS];

	if ( !OSCAR_JsonEscape( room ? room : "", eroom, sizeof( eroom ) ) ||
	     !OSCAR_JsonEscape( sender ? sender : "", esender, sizeof( esender ) ) ||
	     !OSCAR_JsonEscape( text ? text : "", etext, sizeof( etext ) ) ) {
		return qfalse;
	}
	Com_sprintf( out, outSize,
		"{\"type\":\"send_room_message\",\"request_id\":%d,\"room\":\"%s\",\"sender\":\"%s\",\"text\":\"%s\"}",
		requestId, eroom, esender, etext );
	return qtrue;
}

qboolean OSCAR_ProtocolBuildIM( char *out, int outSize, int requestId, const char *screenName, const char *text )
{
	return OSCAR_Build2( out, outSize, "send_im", requestId, "screen_name", screenName, "text", text );
}

qboolean OSCAR_ProtocolBuildPresence( char *out, int outSize, int requestId, const char *status, const char *message )
{
	return OSCAR_Build2( out, outSize, "set_presence", requestId, "status", status, "message", message );
}

qboolean OSCAR_ProtocolParseEvent( const char *json, oscarEvent_t *eventOut )
{
	char type[64];

	if ( !json || !eventOut || strlen( json ) >= OSCAR_MAX_JSON_FRAME ) {
		return qfalse;
	}

	Com_Memset( eventOut, 0, sizeof( *eventOut ) );
	eventOut->type = OSCAR_EVENT_NONE;
	eventOut->requestId = OSCAR_JsonGetInt( json, "request_id", 0 );
	eventOut->ok = OSCAR_JsonGetBool( json, "ok", qfalse );

	if ( !OSCAR_JsonGetString( json, "type", type, sizeof( type ) ) ) {
		return qfalse;
	}

	if ( !Q_stricmp( type, "connected" ) ) {
		eventOut->type = OSCAR_EVENT_CONNECTED;
		return qtrue;
	}
	if ( !Q_stricmp( type, "disconnected" ) ) {
		eventOut->type = OSCAR_EVENT_DISCONNECTED;
		OSCAR_JsonGetString( json, "reason", eventOut->text, sizeof( eventOut->text ) );
		return qtrue;
	}
	if ( !Q_stricmp( type, "request_complete" ) ) {
		eventOut->type = OSCAR_EVENT_REQUEST_COMPLETE;
		OSCAR_JsonGetString( json, "error", eventOut->text, sizeof( eventOut->text ) );
		return qtrue;
	}
	if ( !Q_stricmp( type, "instant_message" ) ) {
		eventOut->type = OSCAR_EVENT_INSTANT_MESSAGE;
		OSCAR_JsonGetString( json, "screen_name", eventOut->screenName, sizeof( eventOut->screenName ) );
		OSCAR_JsonGetString( json, "text", eventOut->text, sizeof( eventOut->text ) );
		return qtrue;
	}
	if ( !Q_stricmp( type, "room_message" ) ) {
		eventOut->type = OSCAR_EVENT_ROOM_MESSAGE;
		OSCAR_JsonGetString( json, "room", eventOut->room, sizeof( eventOut->room ) );
		OSCAR_JsonGetString( json, "screen_name", eventOut->screenName, sizeof( eventOut->screenName ) );
		OSCAR_JsonGetString( json, "text", eventOut->text, sizeof( eventOut->text ) );
		return qtrue;
	}
	if ( !Q_stricmp( type, "presence_changed" ) ) {
		eventOut->type = OSCAR_EVENT_PRESENCE_CHANGED;
		OSCAR_JsonGetString( json, "screen_name", eventOut->screenName, sizeof( eventOut->screenName ) );
		OSCAR_JsonGetString( json, "status", eventOut->status, sizeof( eventOut->status ) );
		OSCAR_JsonGetString( json, "away_message", eventOut->text, sizeof( eventOut->text ) );
		return qtrue;
	}
	if ( !Q_stricmp( type, "error" ) ) {
		eventOut->type = OSCAR_EVENT_ERROR;
		OSCAR_JsonGetString( json, "error", eventOut->text, sizeof( eventOut->text ) );
		return qtrue;
	}

	return qfalse;
}
