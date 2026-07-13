/*
===========================================================================
Open OSCAR raw FLAP/SNAC helpers.

This file implements the narrow binary client subset used by the engine:
classic FLAP login, BOS cookie signon, online notification, presence, and
channel-1 IM.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "net_oscar_raw.h"

#include <string.h>

#define OSCAR_TLV_SCREEN_NAME 0x0001
#define OSCAR_TLV_ROASTED_PASSWORD 0x0002
#define OSCAR_TLV_CLIENT_ID 0x0003
#define OSCAR_TLV_RECONNECT_HERE 0x0005
#define OSCAR_TLV_AUTH_COOKIE 0x0006
#define OSCAR_TLV_ERROR_SUBCODE 0x0008
#define OSCAR_TLV_GROUP_ID 0x000d
#define OSCAR_TLV_SSL_STATE 0x008e
#define OSCAR_TLV_MULTI_CONN_FLAGS 0x004a

#define OSCAR_FAMILY_OSERVICE 0x0001
#define OSCAR_FAMILY_BUDDY 0x0003
#define OSCAR_FAMILY_ICBM 0x0004
#define OSCAR_FAMILY_CHAT_NAV 0x000d
#define OSCAR_FAMILY_CHAT 0x000e
#define OSCAR_OSERVICE_CLIENT_ONLINE 0x0002
#define OSCAR_OSERVICE_ERR 0x0001
#define OSCAR_OSERVICE_SERVICE_REQUEST 0x0004
#define OSCAR_OSERVICE_SERVICE_RESPONSE 0x0005
#define OSCAR_OSERVICE_USER_INFO_UPDATE 0x000f
#define OSCAR_OSERVICE_SET_USER_INFO_FIELDS 0x001e
#define OSCAR_OSERVICE_USER_INFO_USER_FLAGS 0x0001
#define OSCAR_OSERVICE_USER_INFO_STATUS 0x0006
#define OSCAR_BUDDY_ARRIVED 0x000b
#define OSCAR_BUDDY_DEPARTED 0x000c
#define OSCAR_BUDDY_ADD_TEMP 0x000f
#define OSCAR_BUDDY_DEL_TEMP 0x0010
#define OSCAR_ICBM_CHANNEL_MSG_TO_HOST 0x0006
#define OSCAR_ICBM_CHANNEL_MSG_TO_CLIENT 0x0007
#define OSCAR_ICBM_TLV_AOL_IM_DATA 0x0002
#define OSCAR_ICBM_CHANNEL_IM 0x0001
#define OSCAR_ICBM_CHANNEL_MIME 0x0003
#define OSCAR_CHAT_CHANNEL_MSG_TO_HOST 0x0005
#define OSCAR_CHAT_CHANNEL_MSG_TO_CLIENT 0x0006
#define OSCAR_CHAT_TLV_SENDER_INFORMATION 0x0003
#define OSCAR_CHAT_TLV_MESSAGE_INFO 0x0005
#define OSCAR_CHAT_TLV_ENABLE_REFLECTION 0x0006
#define OSCAR_CHAT_MSG_TLV_TEXT 0x0001
#define OSCAR_CHAT_MSG_TLV_ENCODING 0x0002
#define OSCAR_CHAT_MSG_TLV_LANG 0x0003

#define OSCAR_STATUS_AVAILABLE 0x00000000u
#define OSCAR_STATUS_AWAY 0x00000001u
#define OSCAR_STATUS_DND 0x00000002u
#define OSCAR_STATUS_OUT 0x00000004u
#define OSCAR_STATUS_BUSY 0x00000010u
#define OSCAR_STATUS_CHAT 0x00000020u
#define OSCAR_STATUS_INVISIBLE 0x00000100u
#define OSCAR_USER_FLAG_UNAVAILABLE 0x0020u

static void OSCAR_WriteU16( byte *p, unsigned int v )
{
	p[0] = (byte)( ( v >> 8 ) & 0xff );
	p[1] = (byte)( v & 0xff );
}

static void OSCAR_WriteU32( byte *p, unsigned int v )
{
	p[0] = (byte)( ( v >> 24 ) & 0xff );
	p[1] = (byte)( ( v >> 16 ) & 0xff );
	p[2] = (byte)( ( v >> 8 ) & 0xff );
	p[3] = (byte)( v & 0xff );
}

static void OSCAR_WriteU64( byte *p, unsigned int hi, unsigned int lo )
{
	OSCAR_WriteU32( p, hi );
	OSCAR_WriteU32( p + 4, lo );
}

static unsigned int OSCAR_ReadU16( const byte *p )
{
	return ( (unsigned int)p[0] << 8 ) | p[1];
}

static unsigned int OSCAR_ReadU32( const byte *p )
{
	return ( (unsigned int)p[0] << 24 ) | ( (unsigned int)p[1] << 16 ) | ( (unsigned int)p[2] << 8 ) | p[3];
}

static qboolean OSCAR_WriteBytes( byte *out, int outSize, int *used, const void *data, int len )
{
	if ( !out || !used || !data || len < 0 || *used < 0 || *used + len > outSize ) {
		return qfalse;
	}
	Com_Memcpy( out + *used, data, len );
	*used += len;
	return qtrue;
}

static qboolean OSCAR_WriteTLV( byte *out, int outSize, int *used, unsigned int tag, const void *data, int len )
{
	if ( !out || !used || len < 0 || *used + 4 + len > outSize ) {
		return qfalse;
	}
	OSCAR_WriteU16( out + *used, tag );
	OSCAR_WriteU16( out + *used + 2, (unsigned int)len );
	*used += 4;
	if ( len > 0 && data ) {
		Com_Memcpy( out + *used, data, len );
	}
	*used += len;
	return qtrue;
}

static qboolean OSCAR_TLVFind( const byte *payload, int payloadLen, int offset, unsigned int tag, const byte **value, int *valueLen )
{
	int pos = offset;

	if ( !payload || payloadLen < offset ) {
		return qfalse;
	}
	while ( pos + 4 <= payloadLen ) {
		unsigned int curTag = OSCAR_ReadU16( payload + pos );
		int len = (int)OSCAR_ReadU16( payload + pos + 2 );
		pos += 4;
		if ( len < 0 || pos + len > payloadLen ) {
			return qfalse;
		}
		if ( curTag == tag ) {
			if ( value ) {
				*value = payload + pos;
			}
			if ( valueLen ) {
				*valueLen = len;
			}
			return qtrue;
		}
		pos += len;
	}
	return qfalse;
}

static void OSCAR_CopyHostPort( const byte *value, int valueLen, char *hostOut, int hostOutSize, int *portOut )
{
	char hostPort[160];
	const char *colon;
	int copyLen;
	int hostLen;

	if ( !value || valueLen <= 0 || !hostOut || hostOutSize <= 0 || !portOut ) {
		return;
	}
	copyLen = valueLen < (int)sizeof( hostPort ) - 1 ? valueLen : (int)sizeof( hostPort ) - 1;
	Com_Memcpy( hostPort, value, copyLen );
	hostPort[copyLen] = '\0';
	colon = strrchr( hostPort, ':' );
	if ( colon ) {
		*portOut = atoi( colon + 1 );
		hostLen = (int)( colon - hostPort );
		if ( hostLen > 0 && hostLen < hostOutSize ) {
			Com_Memcpy( hostOut, hostPort, hostLen );
			hostOut[hostLen] = '\0';
		}
	} else {
		Q_strncpyz( hostOut, hostPort, hostOutSize );
	}
}

int OSCAR_RawBuildFlap( byte channel, unsigned short sequence, const byte *payload, int payloadLen, byte *out, int outSize )
{
	if ( !out || payloadLen < 0 || payloadLen > 0xffff || outSize < payloadLen + 6 ) {
		return 0;
	}
	out[0] = 0x2a;
	out[1] = channel;
	OSCAR_WriteU16( out + 2, sequence );
	OSCAR_WriteU16( out + 4, (unsigned int)payloadLen );
	if ( payloadLen > 0 && payload ) {
		Com_Memcpy( out + 6, payload, payloadLen );
	}
	return payloadLen + 6;
}

int OSCAR_RawBuildLoginSignon( unsigned short sequence, const char *screenName, const char *password, byte *out, int outSize )
{
	static const byte roastTable[16] = {
		0xf3, 0x26, 0x81, 0xc4, 0x39, 0x86, 0xdb, 0x92,
		0x71, 0xa3, 0xb9, 0xe6, 0x53, 0x7a, 0x95, 0x7c
	};
	static const char clientId[] = "idtech3 raw OSCAR client";
	byte payload[1024];
	byte roasted[256];
	byte multiConn = 0x01;
	int used = 0;
	int passLen;
	int i;

	if ( !screenName || !screenName[0] || !password || !out ) {
		return 0;
	}

	passLen = (int)strlen( password );
	if ( passLen <= 0 || passLen > (int)sizeof( roasted ) ) {
		return 0;
	}
	for ( i = 0; i < passLen; i++ ) {
		roasted[i] = (byte)password[i] ^ roastTable[i & 15];
	}

	OSCAR_WriteU32( payload, 1 );
	used = 4;
	if ( !OSCAR_WriteTLV( payload, sizeof( payload ), &used, OSCAR_TLV_SCREEN_NAME, screenName, (int)strlen( screenName ) ) ||
	     !OSCAR_WriteTLV( payload, sizeof( payload ), &used, OSCAR_TLV_ROASTED_PASSWORD, roasted, passLen ) ||
	     !OSCAR_WriteTLV( payload, sizeof( payload ), &used, OSCAR_TLV_CLIENT_ID, clientId, (int)strlen( clientId ) ) ||
	     !OSCAR_WriteTLV( payload, sizeof( payload ), &used, OSCAR_TLV_MULTI_CONN_FLAGS, &multiConn, 1 ) ) {
		return 0;
	}

	return OSCAR_RawBuildFlap( OSCAR_RAW_FLAP_SIGNON, sequence, payload, used, out, outSize );
}

int OSCAR_RawBuildCookieSignon( unsigned short sequence, const byte *cookie, int cookieLen, byte *out, int outSize )
{
	byte payload[OSCAR_RAW_MAX_COOKIE + 16];
	int used = 0;

	if ( !cookie || cookieLen <= 0 || cookieLen > OSCAR_RAW_MAX_COOKIE ) {
		return 0;
	}
	OSCAR_WriteU32( payload, 1 );
	used = 4;
	if ( !OSCAR_WriteTLV( payload, sizeof( payload ), &used, OSCAR_TLV_AUTH_COOKIE, cookie, cookieLen ) ) {
		return 0;
	}
	return OSCAR_RawBuildFlap( OSCAR_RAW_FLAP_SIGNON, sequence, payload, used, out, outSize );
}

int OSCAR_RawBuildSnac( unsigned short sequence, unsigned short family, unsigned short subtype, unsigned int requestId,
                        const byte *body, int bodyLen, byte *out, int outSize )
{
	byte payload[OSCAR_RAW_MAX_FRAME];
	int used = 0;

	if ( bodyLen < 0 || bodyLen + 10 > (int)sizeof( payload ) ) {
		return 0;
	}
	OSCAR_WriteU16( payload, family );
	OSCAR_WriteU16( payload + 2, subtype );
	OSCAR_WriteU16( payload + 4, 0 );
	OSCAR_WriteU32( payload + 6, requestId );
	used = 10;
	if ( bodyLen > 0 && !OSCAR_WriteBytes( payload, sizeof( payload ), &used, body, bodyLen ) ) {
		return 0;
	}
	return OSCAR_RawBuildFlap( OSCAR_RAW_FLAP_DATA, sequence, payload, used, out, outSize );
}

int OSCAR_RawBuildClientOnline( unsigned short sequence, unsigned int requestId, byte *out, int outSize )
{
	return OSCAR_RawBuildSnac( sequence, OSCAR_FAMILY_OSERVICE, OSCAR_OSERVICE_CLIENT_ONLINE, requestId, NULL, 0, out, outSize );
}

int OSCAR_RawBuildIM( unsigned short sequence, unsigned int requestId, const char *screenName, const char *text, byte *out, int outSize )
{
	byte body[OSCAR_RAW_MAX_FRAME];
	byte tlvData[MAX_STRING_CHARS + 64];
	byte msg[4 + MAX_STRING_CHARS];
	int bodyUsed = 0;
	int tlvUsed = 0;
	int msgLen;
	int nameLen;

	if ( !screenName || !screenName[0] || !text || !text[0] ) {
		return 0;
	}
	nameLen = (int)strlen( screenName );
	msgLen = (int)strlen( text );
	if ( nameLen > 255 || msgLen > MAX_STRING_CHARS - 1 ) {
		return 0;
	}

	OSCAR_WriteU64( body, 0, requestId );
	bodyUsed = 8;
	OSCAR_WriteU16( body + bodyUsed, OSCAR_ICBM_CHANNEL_IM );
	bodyUsed += 2;
	body[bodyUsed++] = (byte)nameLen;
	if ( !OSCAR_WriteBytes( body, sizeof( body ), &bodyUsed, screenName, nameLen ) ) {
		return 0;
	}

	tlvData[tlvUsed++] = 5;
	tlvData[tlvUsed++] = 1;
	OSCAR_WriteU16( tlvData + tlvUsed, 3 );
	tlvUsed += 2;
	tlvData[tlvUsed++] = 1;
	tlvData[tlvUsed++] = 1;
	tlvData[tlvUsed++] = 2;

	OSCAR_WriteU16( msg, 0 );
	OSCAR_WriteU16( msg + 2, 0 );
	Com_Memcpy( msg + 4, text, msgLen );
	tlvData[tlvUsed++] = 1;
	tlvData[tlvUsed++] = 1;
	OSCAR_WriteU16( tlvData + tlvUsed, (unsigned int)( msgLen + 4 ) );
	tlvUsed += 2;
	if ( !OSCAR_WriteBytes( tlvData, sizeof( tlvData ), &tlvUsed, msg, msgLen + 4 ) ) {
		return 0;
	}

	if ( !OSCAR_WriteTLV( body, sizeof( body ), &bodyUsed, OSCAR_ICBM_TLV_AOL_IM_DATA, tlvData, tlvUsed ) ) {
		return 0;
	}
	return OSCAR_RawBuildSnac( sequence, OSCAR_FAMILY_ICBM, OSCAR_ICBM_CHANNEL_MSG_TO_HOST, requestId, body, bodyUsed, out, outSize );
}

static qboolean OSCAR_RawStatusMaskForName( const char *status, unsigned int *maskOut )
{
	if ( !maskOut ) {
		return qfalse;
	}
	if ( !status || !status[0] || !Q_stricmp( status, "available" ) || !Q_stricmp( status, "online" ) ) {
		*maskOut = OSCAR_STATUS_AVAILABLE;
		return qtrue;
	}
	if ( !Q_stricmp( status, "away" ) ) {
		*maskOut = OSCAR_STATUS_AWAY;
		return qtrue;
	}
	if ( !Q_stricmp( status, "dnd" ) || !Q_stricmp( status, "donotdisturb" ) ) {
		*maskOut = OSCAR_STATUS_DND;
		return qtrue;
	}
	if ( !Q_stricmp( status, "out" ) || !Q_stricmp( status, "na" ) || !Q_stricmp( status, "notavailable" ) ) {
		*maskOut = OSCAR_STATUS_OUT;
		return qtrue;
	}
	if ( !Q_stricmp( status, "busy" ) || !Q_stricmp( status, "occupied" ) ) {
		*maskOut = OSCAR_STATUS_BUSY;
		return qtrue;
	}
	if ( !Q_stricmp( status, "chat" ) || !Q_stricmp( status, "freeforchat" ) ) {
		*maskOut = OSCAR_STATUS_CHAT;
		return qtrue;
	}
	if ( !Q_stricmp( status, "invisible" ) ) {
		*maskOut = OSCAR_STATUS_INVISIBLE;
		return qtrue;
	}
	return qfalse;
}

int OSCAR_RawBuildPresence( unsigned short sequence, unsigned int requestId, const char *status, byte *out, int outSize )
{
	byte body[16];
	byte statusBytes[4];
	int used = 0;
	unsigned int mask;

	if ( !OSCAR_RawStatusMaskForName( status, &mask ) ) {
		return 0;
	}

	OSCAR_WriteU32( statusBytes, mask );
	if ( !OSCAR_WriteTLV( body, sizeof( body ), &used, OSCAR_OSERVICE_USER_INFO_STATUS, statusBytes, sizeof( statusBytes ) ) ) {
		return 0;
	}
	return OSCAR_RawBuildSnac( sequence, OSCAR_FAMILY_OSERVICE, OSCAR_OSERVICE_SET_USER_INFO_FIELDS,
		requestId, body, used, out, outSize );
}

static int OSCAR_RawBuildBuddyNameList( unsigned short sequence, unsigned int requestId, unsigned short subtype,
                                        const char *screenName, byte *out, int outSize )
{
	byte body[MAX_NAME_LENGTH + 1];
	int nameLen;

	if ( !screenName || !screenName[0] ) {
		return 0;
	}
	nameLen = (int)strlen( screenName );
	if ( nameLen <= 0 || nameLen > 255 || nameLen >= (int)sizeof( body ) ) {
		return 0;
	}
	body[0] = (byte)nameLen;
	Com_Memcpy( body + 1, screenName, nameLen );
	return OSCAR_RawBuildSnac( sequence, OSCAR_FAMILY_BUDDY, subtype, requestId, body, nameLen + 1, out, outSize );
}

int OSCAR_RawBuildBuddyAddTemp( unsigned short sequence, unsigned int requestId, const char *screenName, byte *out, int outSize )
{
	return OSCAR_RawBuildBuddyNameList( sequence, requestId, OSCAR_BUDDY_ADD_TEMP, screenName, out, outSize );
}

int OSCAR_RawBuildBuddyDelTemp( unsigned short sequence, unsigned int requestId, const char *screenName, byte *out, int outSize )
{
	return OSCAR_RawBuildBuddyNameList( sequence, requestId, OSCAR_BUDDY_DEL_TEMP, screenName, out, outSize );
}

qboolean OSCAR_RawParseFlap( const byte *data, int dataLen, oscarRawFlapFrame_t *frame, int *consumed )
{
	int payloadLen;

	if ( !data || !frame || !consumed || dataLen < 6 ) {
		return qfalse;
	}
	if ( data[0] != 0x2a ) {
		*consumed = -1;
		return qfalse;
	}
	payloadLen = (int)OSCAR_ReadU16( data + 4 );
	if ( payloadLen > OSCAR_RAW_MAX_FRAME - 6 ) {
		*consumed = -1;
		return qfalse;
	}
	if ( dataLen < payloadLen + 6 ) {
		*consumed = 0;
		return qfalse;
	}
	frame->channel = data[1];
	frame->sequence = (unsigned short)OSCAR_ReadU16( data + 2 );
	frame->payloadLen = (unsigned short)payloadLen;
	frame->payload = data + 6;
	*consumed = payloadLen + 6;
	return qtrue;
}

qboolean OSCAR_RawParseSnac( const byte *payload, int payloadLen, oscarRawSnac_t *snac )
{
	if ( !payload || !snac || payloadLen < 10 ) {
		return qfalse;
	}
	Com_Memset( snac, 0, sizeof( *snac ) );
	snac->family = (unsigned short)OSCAR_ReadU16( payload );
	snac->subtype = (unsigned short)OSCAR_ReadU16( payload + 2 );
	snac->flags = (unsigned short)OSCAR_ReadU16( payload + 4 );
	snac->requestId = OSCAR_ReadU32( payload + 6 );
	snac->body = payload + 10;
	snac->bodyLen = payloadLen - 10;
	return qtrue;
}

qboolean OSCAR_RawParseAuthReply( const byte *payload, int payloadLen, oscarRawAuthReply_t *reply )
{
	const byte *value;
	int valueLen;

	if ( !payload || !reply || payloadLen < 4 ) {
		return qfalse;
	}
	Com_Memset( reply, 0, sizeof( *reply ) );
	reply->bosPort = 5190;

	if ( OSCAR_TLVFind( payload, payloadLen, 4, OSCAR_TLV_ERROR_SUBCODE, &value, &valueLen ) && valueLen >= 1 ) {
		reply->errorCode = valueLen >= 2 ? (unsigned short)OSCAR_ReadU16( value ) : value[0];
	}
	if ( OSCAR_TLVFind( payload, payloadLen, 4, OSCAR_TLV_RECONNECT_HERE, &value, &valueLen ) && valueLen > 0 ) {
		OSCAR_CopyHostPort( value, valueLen, reply->bosHost, sizeof( reply->bosHost ), &reply->bosPort );
	}
	if ( OSCAR_TLVFind( payload, payloadLen, 4, OSCAR_TLV_AUTH_COOKIE, &value, &valueLen ) && valueLen > 0 ) {
		if ( valueLen > OSCAR_RAW_MAX_COOKIE ) {
			return qfalse;
		}
		Com_Memcpy( reply->cookie, value, valueLen );
		reply->cookieLen = valueLen;
	}
	return (qboolean)( reply->errorCode || ( reply->bosHost[0] && reply->cookieLen > 0 ) );
}

qboolean OSCAR_RawParseServiceReply( const oscarRawSnac_t *snac, oscarRawServiceReply_t *reply )
{
	const byte *value;
	int valueLen;

	if ( !snac || !reply ) {
		return qfalse;
	}
	if ( snac->family != OSCAR_FAMILY_OSERVICE ||
	     ( snac->subtype != OSCAR_OSERVICE_SERVICE_RESPONSE && snac->subtype != OSCAR_OSERVICE_ERR ) ) {
		return qfalse;
	}

	Com_Memset( reply, 0, sizeof( *reply ) );
	reply->port = 5190;
	if ( snac->subtype == OSCAR_OSERVICE_ERR && snac->bodyLen >= 2 ) {
		reply->errorCode = (unsigned short)OSCAR_ReadU16( snac->body );
		return qtrue;
	}
	if ( OSCAR_TLVFind( snac->body, snac->bodyLen, 0, OSCAR_TLV_ERROR_SUBCODE, &value, &valueLen ) && valueLen >= 1 ) {
		reply->errorCode = valueLen >= 2 ? (unsigned short)OSCAR_ReadU16( value ) : value[0];
	}
	if ( OSCAR_TLVFind( snac->body, snac->bodyLen, 0, OSCAR_TLV_GROUP_ID, &value, &valueLen ) && valueLen >= 2 ) {
		reply->service = (unsigned short)OSCAR_ReadU16( value );
	}
	if ( OSCAR_TLVFind( snac->body, snac->bodyLen, 0, OSCAR_TLV_RECONNECT_HERE, &value, &valueLen ) && valueLen > 0 ) {
		OSCAR_CopyHostPort( value, valueLen, reply->host, sizeof( reply->host ), &reply->port );
	}
	if ( OSCAR_TLVFind( snac->body, snac->bodyLen, 0, OSCAR_TLV_AUTH_COOKIE, &value, &valueLen ) && valueLen > 0 ) {
		if ( valueLen > OSCAR_RAW_MAX_COOKIE ) {
			return qfalse;
		}
		Com_Memcpy( reply->cookie, value, valueLen );
		reply->cookieLen = valueLen;
	}
	return (qboolean)( reply->errorCode || ( reply->service && reply->host[0] && reply->cookieLen > 0 ) );
}

qboolean OSCAR_RawParseIncomingIM( const oscarRawSnac_t *snac, oscarEvent_t *eventOut )
{
	const byte *body;
	const byte *tlv;
	int bodyLen;
	int pos = 0;
	int nameLen;
	int originalNameLen;
	int tlvLen;
	int fragPos = 0;

	if ( !snac || !eventOut || snac->family != OSCAR_FAMILY_ICBM || snac->subtype != OSCAR_ICBM_CHANNEL_MSG_TO_CLIENT ) {
		return qfalse;
	}
	body = snac->body;
	bodyLen = snac->bodyLen;
	if ( bodyLen < 11 ) {
		return qfalse;
	}
	pos += 8;
	if ( OSCAR_ReadU16( body + pos ) != OSCAR_ICBM_CHANNEL_IM ) {
		return qfalse;
	}
	pos += 2;
	nameLen = body[pos++];
	originalNameLen = nameLen;
	if ( originalNameLen <= 0 || pos + originalNameLen > bodyLen ) {
		return qfalse;
	}

	Com_Memset( eventOut, 0, sizeof( *eventOut ) );
	eventOut->type = OSCAR_EVENT_INSTANT_MESSAGE;
	if ( nameLen >= (int)sizeof( eventOut->screenName ) ) {
		nameLen = (int)sizeof( eventOut->screenName ) - 1;
	}
	Com_Memcpy( eventOut->screenName, body + pos, nameLen );
	eventOut->screenName[nameLen] = '\0';
	pos += originalNameLen;

	while ( pos + 4 <= bodyLen ) {
		unsigned int tag = OSCAR_ReadU16( body + pos );
		tlvLen = (int)OSCAR_ReadU16( body + pos + 2 );
		pos += 4;
		if ( tlvLen < 0 || pos + tlvLen > bodyLen ) {
			return qfalse;
		}
		if ( tag == OSCAR_ICBM_TLV_AOL_IM_DATA ) {
			tlv = body + pos;
			while ( fragPos + 4 <= tlvLen ) {
				int fragPayloadLen = (int)OSCAR_ReadU16( tlv + fragPos + 2 );
				const byte *fragPayload;
				fragPos += 4;
				if ( fragPayloadLen < 0 || fragPos + fragPayloadLen > tlvLen ) {
					return qfalse;
				}
				fragPayload = tlv + fragPos;
				if ( tlv[fragPos - 4] == 1 && fragPayloadLen >= 4 ) {
					int textLen = fragPayloadLen - 4;
					if ( textLen >= (int)sizeof( eventOut->text ) ) {
						textLen = (int)sizeof( eventOut->text ) - 1;
					}
					Com_Memcpy( eventOut->text, fragPayload + 4, textLen );
					eventOut->text[textLen] = '\0';
					return qtrue;
				}
				fragPos += fragPayloadLen;
			}
		}
		pos += tlvLen;
	}

	return qfalse;
}

static const char *OSCAR_RawStatusName( unsigned int flags, unsigned int mask, qboolean departed )
{
	if ( departed ) {
		return "offline";
	}
	if ( mask & OSCAR_STATUS_INVISIBLE ) {
		return "invisible";
	}
	if ( mask & OSCAR_STATUS_DND ) {
		return "dnd";
	}
	if ( mask & OSCAR_STATUS_BUSY ) {
		return "busy";
	}
	if ( mask & OSCAR_STATUS_CHAT ) {
		return "chat";
	}
	if ( mask & OSCAR_STATUS_OUT ) {
		return "out";
	}
	if ( ( flags & OSCAR_USER_FLAG_UNAVAILABLE ) || ( mask & OSCAR_STATUS_AWAY ) ) {
		return "away";
	}
	return "available";
}

static qboolean OSCAR_RawParseTLVUserInfoPresence( const byte *body, int bodyLen, qboolean departed, oscarEvent_t *eventOut )
{
	unsigned int flags = 0;
	unsigned int mask = OSCAR_STATUS_AVAILABLE;
	int pos = 0;
	int nameLen;
	int tlvCount;
	int i;

	if ( !body || !eventOut || bodyLen < 4 ) {
		return qfalse;
	}
	nameLen = body[pos++];
	if ( nameLen <= 0 || pos + nameLen + 4 > bodyLen ) {
		return qfalse;
	}

	Com_Memset( eventOut, 0, sizeof( *eventOut ) );
	eventOut->type = OSCAR_EVENT_PRESENCE_CHANGED;
	if ( nameLen >= (int)sizeof( eventOut->screenName ) ) {
		nameLen = (int)sizeof( eventOut->screenName ) - 1;
	}
	Com_Memcpy( eventOut->screenName, body + pos, nameLen );
	eventOut->screenName[nameLen] = '\0';
	pos += (int)body[0];

	pos += 2; /* warning level */
	tlvCount = (int)OSCAR_ReadU16( body + pos );
	pos += 2;
	if ( tlvCount < 0 ) {
		return qfalse;
	}

	for ( i = 0; i < tlvCount && pos + 4 <= bodyLen; i++ ) {
		unsigned int tag = OSCAR_ReadU16( body + pos );
		int len = (int)OSCAR_ReadU16( body + pos + 2 );
		const byte *value = body + pos + 4;

		pos += 4;
		if ( len < 0 || pos + len > bodyLen ) {
			return qfalse;
		}
		if ( tag == OSCAR_OSERVICE_USER_INFO_USER_FLAGS && len >= 2 ) {
			flags = OSCAR_ReadU16( value );
		} else if ( tag == OSCAR_OSERVICE_USER_INFO_STATUS && len >= 4 ) {
			mask = OSCAR_ReadU32( value );
		}
		pos += len;
	}

	Q_strncpyz( eventOut->status, OSCAR_RawStatusName( flags, mask, departed ), sizeof( eventOut->status ) );
	return qtrue;
}

qboolean OSCAR_RawParsePresence( const oscarRawSnac_t *snac, oscarEvent_t *eventOut )
{
	qboolean departed;

	if ( !snac || !eventOut ) {
		return qfalse;
	}
	departed = (qboolean)( snac->family == OSCAR_FAMILY_BUDDY && snac->subtype == OSCAR_BUDDY_DEPARTED );
	if ( snac->family == OSCAR_FAMILY_OSERVICE && snac->subtype == OSCAR_OSERVICE_USER_INFO_UPDATE ) {
		return OSCAR_RawParseTLVUserInfoPresence( snac->body, snac->bodyLen, qfalse, eventOut );
	}
	if ( snac->family == OSCAR_FAMILY_BUDDY &&
	     ( snac->subtype == OSCAR_BUDDY_ARRIVED || snac->subtype == OSCAR_BUDDY_DEPARTED ) ) {
		return OSCAR_RawParseTLVUserInfoPresence( snac->body, snac->bodyLen, departed, eventOut );
	}
	return qfalse;
}

int OSCAR_RawBuildServiceRequest( unsigned short sequence, unsigned int requestId, unsigned short service, byte *out, int outSize )
{
	byte body[2];

	OSCAR_WriteU16( body, service );
	return OSCAR_RawBuildSnac( sequence, OSCAR_FAMILY_OSERVICE, OSCAR_OSERVICE_SERVICE_REQUEST, requestId, body, sizeof( body ), out, outSize );
}

int OSCAR_RawBuildChatServiceRequest( unsigned short sequence, unsigned int requestId, unsigned short exchange,
                                      const char *roomCookie, unsigned short instance, byte *out, int outSize )
{
	byte body[512];
	byte roomInfo[256];
	int bodyUsed = 0;
	int roomUsed = 0;
	int cookieLen;

	if ( !roomCookie || !roomCookie[0] ) {
		return 0;
	}
	cookieLen = (int)strlen( roomCookie );
	if ( cookieLen > 255 || cookieLen + 6 > (int)sizeof( roomInfo ) ) {
		return 0;
	}

	OSCAR_WriteU16( body + bodyUsed, OSCAR_FAMILY_CHAT );
	bodyUsed += 2;
	OSCAR_WriteU16( roomInfo + roomUsed, exchange );
	roomUsed += 2;
	roomInfo[roomUsed++] = (byte)cookieLen;
	Com_Memcpy( roomInfo + roomUsed, roomCookie, cookieLen );
	roomUsed += cookieLen;
	OSCAR_WriteU16( roomInfo + roomUsed, instance );
	roomUsed += 2;
	if ( !OSCAR_WriteTLV( body, sizeof( body ), &bodyUsed, 0x0001, roomInfo, roomUsed ) ) {
		return 0;
	}
	return OSCAR_RawBuildSnac( sequence, OSCAR_FAMILY_OSERVICE, OSCAR_OSERVICE_SERVICE_REQUEST, requestId, body, bodyUsed, out, outSize );
}

int OSCAR_RawBuildChatMessage( unsigned short sequence, unsigned int requestId, const char *text, byte *out, int outSize )
{
	byte body[OSCAR_RAW_MAX_FRAME];
	byte msgInfo[MAX_STRING_CHARS + 64];
	int bodyUsed = 0;
	int msgUsed = 0;
	int textLen;
	static const char enc[] = "us-ascii";
	static const char lang[] = "en";

	if ( !text || !text[0] ) {
		return 0;
	}
	textLen = (int)strlen( text );
	if ( textLen > MAX_STRING_CHARS - 1 ) {
		return 0;
	}

	OSCAR_WriteU64( body + bodyUsed, 0, requestId );
	bodyUsed += 8;
	OSCAR_WriteU16( body + bodyUsed, OSCAR_ICBM_CHANNEL_MIME );
	bodyUsed += 2;

	if ( !OSCAR_WriteTLV( msgInfo, sizeof( msgInfo ), &msgUsed, OSCAR_CHAT_MSG_TLV_TEXT, text, textLen ) ||
	     !OSCAR_WriteTLV( msgInfo, sizeof( msgInfo ), &msgUsed, OSCAR_CHAT_MSG_TLV_ENCODING, enc, (int)strlen( enc ) ) ||
	     !OSCAR_WriteTLV( msgInfo, sizeof( msgInfo ), &msgUsed, OSCAR_CHAT_MSG_TLV_LANG, lang, (int)strlen( lang ) ) ) {
		return 0;
	}

	if ( !OSCAR_WriteTLV( body, sizeof( body ), &bodyUsed, OSCAR_CHAT_TLV_MESSAGE_INFO, msgInfo, msgUsed ) ||
	     !OSCAR_WriteTLV( body, sizeof( body ), &bodyUsed, OSCAR_CHAT_TLV_ENABLE_REFLECTION, "", 0 ) ) {
		return 0;
	}

	return OSCAR_RawBuildSnac( sequence, OSCAR_FAMILY_CHAT, OSCAR_CHAT_CHANNEL_MSG_TO_HOST, requestId, body, bodyUsed, out, outSize );
}

qboolean OSCAR_RawParseChatMessage( const oscarRawSnac_t *snac, const char *roomName, oscarEvent_t *eventOut )
{
	const byte *body;
	const byte *msgInfo = NULL;
	const byte *senderInfo = NULL;
	int bodyLen;
	int pos = 0;
	int msgInfoLen = 0;
	int senderInfoLen = 0;

	if ( !snac || !eventOut || snac->family != OSCAR_FAMILY_CHAT || snac->subtype != OSCAR_CHAT_CHANNEL_MSG_TO_CLIENT ) {
		return qfalse;
	}
	body = snac->body;
	bodyLen = snac->bodyLen;
	if ( bodyLen < 10 ) {
		return qfalse;
	}
	pos = 10; /* cookie + channel */
	while ( pos + 4 <= bodyLen ) {
		unsigned int tag = OSCAR_ReadU16( body + pos );
		int len = (int)OSCAR_ReadU16( body + pos + 2 );
		pos += 4;
		if ( len < 0 || pos + len > bodyLen ) {
			return qfalse;
		}
		if ( tag == OSCAR_CHAT_TLV_SENDER_INFORMATION ) {
			senderInfo = body + pos;
			senderInfoLen = len;
		} else if ( tag == OSCAR_CHAT_TLV_MESSAGE_INFO ) {
			msgInfo = body + pos;
			msgInfoLen = len;
		}
		pos += len;
	}
	if ( !msgInfo || msgInfoLen <= 0 ) {
		return qfalse;
	}

	Com_Memset( eventOut, 0, sizeof( *eventOut ) );
	eventOut->type = OSCAR_EVENT_ROOM_MESSAGE;
	Q_strncpyz( eventOut->room, roomName ? roomName : "", sizeof( eventOut->room ) );

	if ( senderInfo && senderInfoLen > 0 ) {
		int nameLen = senderInfo[0];
		if ( nameLen > 0 && 1 + nameLen <= senderInfoLen ) {
			if ( nameLen >= (int)sizeof( eventOut->screenName ) ) {
				nameLen = (int)sizeof( eventOut->screenName ) - 1;
			}
			Com_Memcpy( eventOut->screenName, senderInfo + 1, nameLen );
			eventOut->screenName[nameLen] = '\0';
		}
	}

	pos = 0;
	while ( pos + 4 <= msgInfoLen ) {
		unsigned int tag = OSCAR_ReadU16( msgInfo + pos );
		int len = (int)OSCAR_ReadU16( msgInfo + pos + 2 );
		pos += 4;
		if ( len < 0 || pos + len > msgInfoLen ) {
			return qfalse;
		}
		if ( tag == OSCAR_CHAT_MSG_TLV_TEXT ) {
			if ( len >= (int)sizeof( eventOut->text ) ) {
				len = (int)sizeof( eventOut->text ) - 1;
			}
			Com_Memcpy( eventOut->text, msgInfo + pos, len );
			eventOut->text[len] = '\0';
			return qtrue;
		}
		pos += len;
	}
	return qfalse;
}
