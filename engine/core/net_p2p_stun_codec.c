/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

STUN/TURN packet codec helpers for P2P ICE-lite (RFC 5389 / RFC 5766 subset).
===========================================================================
*/

#include "net_p2p_stun_codec.h"

#include <string.h>

uint16_t NET_P2P_StunRead16( const byte *p )
{
	return (uint16_t)( ( p[0] << 8 ) | p[1] );
}

uint32_t NET_P2P_StunRead32( const byte *p )
{
	return (uint32_t)( ( p[0] << 24 ) | ( p[1] << 16 ) | ( p[2] << 8 ) | p[3] );
}

void NET_P2P_StunWrite16( byte *p, uint16_t v )
{
	p[0] = (byte)( ( v >> 8 ) & 0xff );
	p[1] = (byte)( v & 0xff );
}

void NET_P2P_StunWrite32( byte *p, uint32_t v )
{
	p[0] = (byte)( ( v >> 24 ) & 0xff );
	p[1] = (byte)( ( v >> 16 ) & 0xff );
	p[2] = (byte)( ( v >> 8 ) & 0xff );
	p[3] = (byte)( v & 0xff );
}

int NET_P2P_StunPad4( int len )
{
	return ( len + 3 ) & ~3;
}

int NET_P2P_StunBuildBindingRequest( byte *out, int outSize, const byte *transactionId )
{
	if ( !out || outSize < 20 || !transactionId ) {
		return 0;
	}

	NET_P2P_StunWrite16( out + 0, P2P_STUN_BINDING_REQUEST );
	NET_P2P_StunWrite16( out + 2, 0 );
	NET_P2P_StunWrite32( out + 4, P2P_STUN_MAGIC_COOKIE );
	Com_Memcpy( out + 8, transactionId, 12 );
	return 20;
}

static int NET_P2P_StunAppendStringAttr( byte *out, int outSize, int pos, uint16_t attrType, const char *value )
{
	int ulen;

	if ( !value ) {
		return pos;
	}

	ulen = (int)strlen( value );
	if ( pos + 4 + NET_P2P_StunPad4( ulen ) > outSize ) {
		return -1;
	}

	NET_P2P_StunWrite16( out + pos + 0, attrType );
	NET_P2P_StunWrite16( out + pos + 2, (uint16_t)ulen );
	Com_Memcpy( out + pos + 4, value, ulen );
	return pos + 4 + NET_P2P_StunPad4( ulen );
}

static int NET_P2P_StunAppendTransportAttr( byte *out, int outSize, int pos )
{
	if ( pos + 8 > outSize ) {
		return -1;
	}

	NET_P2P_StunWrite16( out + pos + 0, P2P_STUN_ATTR_REQUESTED_TRANSPORT );
	NET_P2P_StunWrite16( out + pos + 2, 4 );
	out[pos + 4] = 0;
	out[pos + 5] = 0;
	out[pos + 6] = 0;
	out[pos + 7] = 17;
	return pos + 8;
}

static int NET_P2P_StunAppendLifetimeAttr( byte *out, int outSize, int pos, uint32_t lifetimeSec )
{
	if ( pos + 8 > outSize ) {
		return -1;
	}

	NET_P2P_StunWrite16( out + pos + 0, P2P_STUN_ATTR_LIFETIME );
	NET_P2P_StunWrite16( out + pos + 2, 4 );
	NET_P2P_StunWrite32( out + pos + 4, lifetimeSec );
	return pos + 8;
}

static int NET_P2P_StunAppendXorPeerAddress( byte *out, int outSize, int pos, const netadr_t *peer, const byte *transactionId )
{
	uint32_t ipv4;
	uint16_t port;

	if ( !peer || peer->type != NA_IP || pos + 12 > outSize ) {
		return -1;
	}

	port = peer->port ^ (uint16_t)( P2P_STUN_MAGIC_COOKIE >> 16 );
	ipv4 = ( (uint32_t)peer->ipv._4[0] << 24 ) |
	       ( (uint32_t)peer->ipv._4[1] << 16 ) |
	       ( (uint32_t)peer->ipv._4[2] << 8 ) |
	       (uint32_t)peer->ipv._4[3];
	ipv4 ^= P2P_STUN_MAGIC_COOKIE;
	ipv4 ^= NET_P2P_StunRead32( transactionId );

	NET_P2P_StunWrite16( out + pos + 0, P2P_STUN_ATTR_XOR_PEER_ADDRESS );
	NET_P2P_StunWrite16( out + pos + 2, 8 );
	out[pos + 4] = 0;
	out[pos + 5] = 0x01;
	NET_P2P_StunWrite16( out + pos + 6, port );
	NET_P2P_StunWrite32( out + pos + 8, ipv4 );
	return pos + 12;
}

int NET_P2P_StunBuildAllocateAttrs( byte *out, int outSize, const char *username, const char *realm, const char *nonce )
{
	int pos = 0;
	byte transactionId[12] = { 0 };

	if ( !out || outSize < 8 ) {
		return 0;
	}

	pos = NET_P2P_StunAppendTransportAttr( out, outSize, pos );
	if ( pos < 0 ) {
		return 0;
	}

	pos = NET_P2P_StunAppendLifetimeAttr( out, outSize, pos, 600 );
	if ( pos < 0 ) {
		return 0;
	}

	if ( username && username[0] ) {
		pos = NET_P2P_StunAppendStringAttr( out, outSize, pos, P2P_STUN_ATTR_USERNAME, username );
		if ( pos < 0 ) {
			return 0;
		}
	}
	if ( nonce && nonce[0] ) {
		pos = NET_P2P_StunAppendStringAttr( out, outSize, pos, P2P_STUN_ATTR_NONCE, nonce );
		if ( pos < 0 ) {
			return 0;
		}
	}
	if ( realm && realm[0] ) {
		pos = NET_P2P_StunAppendStringAttr( out, outSize, pos, P2P_STUN_ATTR_REALM, realm );
		if ( pos < 0 ) {
			return 0;
		}
	}

	(void)transactionId;
	return pos;
}

int NET_P2P_StunBuildCreatePermissionAttrs( byte *out, int outSize, const netadr_t *peer, const char *username, const char *realm, const char *nonce )
{
	int pos = 0;
	byte transactionId[12] = { 0 };

	if ( !out || !peer ) {
		return 0;
	}

	pos = NET_P2P_StunAppendXorPeerAddress( out, outSize, pos, peer, transactionId );
	if ( pos < 0 ) {
		return 0;
	}

	if ( username && username[0] ) {
		pos = NET_P2P_StunAppendStringAttr( out, outSize, pos, P2P_STUN_ATTR_USERNAME, username );
		if ( pos < 0 ) {
			return 0;
		}
	}
	if ( nonce && nonce[0] ) {
		pos = NET_P2P_StunAppendStringAttr( out, outSize, pos, P2P_STUN_ATTR_NONCE, nonce );
		if ( pos < 0 ) {
			return 0;
		}
	}
	if ( realm && realm[0] ) {
		pos = NET_P2P_StunAppendStringAttr( out, outSize, pos, P2P_STUN_ATTR_REALM, realm );
		if ( pos < 0 ) {
			return 0;
		}
	}

	return pos;
}

int NET_P2P_StunBuildRefreshAttrs( byte *out, int outSize, uint32_t lifetimeSec, const char *username, const char *realm, const char *nonce )
{
	int pos = 0;

	if ( !out ) {
		return 0;
	}

	pos = NET_P2P_StunAppendLifetimeAttr( out, outSize, pos, lifetimeSec );
	if ( pos < 0 ) {
		return 0;
	}

	if ( username && username[0] ) {
		pos = NET_P2P_StunAppendStringAttr( out, outSize, pos, P2P_STUN_ATTR_USERNAME, username );
		if ( pos < 0 ) {
			return 0;
		}
	}
	if ( nonce && nonce[0] ) {
		pos = NET_P2P_StunAppendStringAttr( out, outSize, pos, P2P_STUN_ATTR_NONCE, nonce );
		if ( pos < 0 ) {
			return 0;
		}
	}
	if ( realm && realm[0] ) {
		pos = NET_P2P_StunAppendStringAttr( out, outSize, pos, P2P_STUN_ATTR_REALM, realm );
		if ( pos < 0 ) {
			return 0;
		}
	}

	return pos;
}

static int NET_P2P_StunAppendChannelNumberAttr( byte *out, int outSize, int pos, uint16_t channelNumber )
{
	if ( pos + 8 > outSize ) {
		return -1;
	}

	NET_P2P_StunWrite16( out + pos + 0, P2P_STUN_ATTR_CHANNEL_NUMBER );
	NET_P2P_StunWrite16( out + pos + 2, 4 );
	NET_P2P_StunWrite16( out + pos + 4, 0 );
	NET_P2P_StunWrite16( out + pos + 6, channelNumber );
	return pos + 8;
}

int NET_P2P_StunBuildChannelBindAttrs( byte *out, int outSize, uint16_t channelNumber, const netadr_t *peer, const char *username, const char *realm, const char *nonce )
{
	int pos = 0;
	byte transactionId[12] = { 0 };

	if ( !out || !peer || channelNumber < 0x4000 ) {
		return 0;
	}

	pos = NET_P2P_StunAppendChannelNumberAttr( out, outSize, pos, channelNumber );
	if ( pos < 0 ) {
		return 0;
	}

	pos = NET_P2P_StunAppendXorPeerAddress( out, outSize, pos, peer, transactionId );
	if ( pos < 0 ) {
		return 0;
	}

	if ( username && username[0] ) {
		pos = NET_P2P_StunAppendStringAttr( out, outSize, pos, P2P_STUN_ATTR_USERNAME, username );
		if ( pos < 0 ) {
			return 0;
		}
	}
	if ( nonce && nonce[0] ) {
		pos = NET_P2P_StunAppendStringAttr( out, outSize, pos, P2P_STUN_ATTR_NONCE, nonce );
		if ( pos < 0 ) {
			return 0;
		}
	}
	if ( realm && realm[0] ) {
		pos = NET_P2P_StunAppendStringAttr( out, outSize, pos, P2P_STUN_ATTR_REALM, realm );
		if ( pos < 0 ) {
			return 0;
		}
	}

	return pos;
}

qboolean NET_P2P_StunParseMappedAddress( const byte *value, int valueLen, netadr_t *out, qboolean xored, const byte *transactionId )
{
	uint16_t port;
	uint32_t ipv4;
	byte ipBytes[4];

	if ( !out || valueLen < 8 || value[1] != 0x01 ) {
		return qfalse;
	}

	port = NET_P2P_StunRead16( value + 2 );
	ipv4 = NET_P2P_StunRead32( value + 4 );

	if ( xored ) {
		port ^= (uint16_t)( P2P_STUN_MAGIC_COOKIE >> 16 );
		ipv4 ^= P2P_STUN_MAGIC_COOKIE;
		if ( transactionId ) {
			ipv4 ^= NET_P2P_StunRead32( transactionId );
		}
	}

	ipBytes[0] = (byte)( ( ipv4 >> 24 ) & 0xff );
	ipBytes[1] = (byte)( ( ipv4 >> 16 ) & 0xff );
	ipBytes[2] = (byte)( ( ipv4 >> 8 ) & 0xff );
	ipBytes[3] = (byte)( ipv4 & 0xff );

	Com_Memset( out, 0, sizeof( *out ) );
	out->type = NA_IP;
	Com_Memcpy( out->ipv._4, ipBytes, 4 );
	out->port = port;
	return qtrue;
}

qboolean NET_P2P_StunParseMessage( const byte *data, int len, const byte *expectedTransactionId, p2p_stun_parse_result_t *result )
{
	const byte *p;
	const byte *end;
	netadr_t mapped;

	if ( !data || len < 20 || !result ) {
		return qfalse;
	}

	Com_Memset( result, 0, sizeof( *result ) );
	result->errorCode = -1;

	result->msgType = NET_P2P_StunRead16( data + 0 );
	result->msgLen = NET_P2P_StunRead16( data + 2 );
	if ( NET_P2P_StunRead32( data + 4 ) != P2P_STUN_MAGIC_COOKIE ) {
		return qfalse;
	}

	Com_Memcpy( result->transactionId, data + 8, 12 );
	if ( expectedTransactionId && memcmp( data + 8, expectedTransactionId, 12 ) != 0 ) {
		return qfalse;
	}

	if ( 20 + result->msgLen > len ) {
		return qfalse;
	}

	p = data + 20;
	end = p + result->msgLen;
	while ( p + 4 <= end ) {
		uint16_t attrType = NET_P2P_StunRead16( p + 0 );
		uint16_t attrLen = NET_P2P_StunRead16( p + 2 );
		const byte *value = p + 4;

		if ( p + 4 + attrLen > end ) {
			break;
		}

		if ( attrType == P2P_STUN_ATTR_REALM && attrLen < sizeof( result->realm ) ) {
			Com_Memcpy( result->realm, value, attrLen );
			result->realm[attrLen] = '\0';
		} else if ( attrType == P2P_STUN_ATTR_NONCE && attrLen < sizeof( result->nonce ) ) {
			Com_Memcpy( result->nonce, value, attrLen );
			result->nonce[attrLen] = '\0';
		} else if ( attrType == P2P_STUN_ATTR_XOR_MAPPED_ADDRESS || attrType == P2P_STUN_ATTR_MAPPED_ADDRESS ) {
			if ( NET_P2P_StunParseMappedAddress( value, attrLen, &mapped, (qboolean)( attrType == P2P_STUN_ATTR_XOR_MAPPED_ADDRESS ), data + 8 ) ) {
				result->mappedAdr = mapped;
				result->haveMapped = qtrue;
			}
		} else if ( attrType == P2P_STUN_ATTR_XOR_RELAYED_ADDRESS ) {
			if ( NET_P2P_StunParseMappedAddress( value, attrLen, &mapped, qtrue, data + 8 ) ) {
				result->relayedAdr = mapped;
				result->haveRelayed = qtrue;
			}
		} else if ( attrType == P2P_STUN_ATTR_LIFETIME && attrLen >= 4 ) {
			result->lifetime = NET_P2P_StunRead32( value );
		} else if ( attrType == P2P_STUN_ATTR_ERROR_CODE && attrLen >= 4 ) {
			result->errorCode = (int)NET_P2P_StunRead32( value + 0 ) & 0xffff;
			if ( attrLen > 4 ) {
				int reasonLen = attrLen - 4;
				if ( reasonLen >= (int)sizeof( result->errorReason ) ) {
					reasonLen = (int)sizeof( result->errorReason ) - 1;
				}
				Com_Memcpy( result->errorReason, value + 4, reasonLen );
				result->errorReason[reasonLen] = '\0';
			}
		}

		p += 4 + NET_P2P_StunPad4( attrLen );
	}

	return qtrue;
}

int NET_P2P_StunCandidatePriority( p2p_candidate_type_t local, p2p_candidate_type_t remote )
{
	static const int typeScore[P2P_CAND_COUNT] = { 100, 200, 300 };

	if ( local < 0 || local >= P2P_CAND_COUNT || remote < 0 || remote >= P2P_CAND_COUNT ) {
		return 0;
	}

	return typeScore[local] + typeScore[remote];
}
