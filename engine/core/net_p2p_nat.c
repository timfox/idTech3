/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

ICE-lite NAT traversal for direct_udp P2P: STUN binding, optional TURN relay,
and dedicated-server master browsing.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "net_p2p_nat.h"
#include "net_p2p_stun_codec.h"
#include "net_p2p_turn_auth.h"

#include <string.h>

#define P2P_BROWSE_MAX_SERVERS 256
#define P2P_BROWSE_INFO_TIMEOUT_MS 30000

typedef struct {
	qboolean valid;
	netadr_t adr;
	char text[MAX_STRING_CHARS];
} p2p_candidate_t;

typedef struct {
	qboolean active;
	qboolean pending;
	qboolean mappedValid;
	netadr_t serverAdr;
	netadr_t mappedAdr;
	byte transactionId[12];
	int lastRequestTime;
	int nextRequestTime;
	char realm[64];
	char nonce[128];
	int turnPhase;
	uint32_t allocationLifetime;
	int nextRefreshTime;
	qboolean permissionSent;
	netadr_t permissionPeer;
} p2p_stun_state_t;

typedef struct {
	qboolean active;
	netadr_t adr;
	char p2pAddr[MAX_STRING_CHARS];
	char sessionId[64];
	char antiCheat[32];
	char failover[32];
	char hostName[MAX_NAME_LENGTH];
	char mapName[MAX_NAME_LENGTH];
	int clients;
	int protocol;
	int reconnectWindow;
	qboolean hostMigration;
	qboolean queried;
	qboolean haveInfo;
} p2p_browse_entry_t;

static cvar_t *net_p2pStun;
static cvar_t *net_p2pStunServer;
static cvar_t *net_p2pStunAutoAdvertise;
static cvar_t *net_p2pStunInterval;
static cvar_t *net_p2pTurn;
static cvar_t *net_p2pTurnServer;
static cvar_t *net_p2pTurnUser;
static cvar_t *net_p2pTurnPass;
static cvar_t *net_p2pHostAdvertise;
static cvar_t *net_p2pTurnRefresh;
static cvar_t *net_p2pTurnChannels;

static p2p_candidate_t net_p2pCandidates[P2P_CAND_COUNT];
static p2p_stun_state_t net_p2pStunState;
static p2p_stun_state_t net_p2pTurnState;
static qboolean net_p2pNatInitialized;

static p2p_browse_entry_t net_p2pBrowseServers[P2P_BROWSE_MAX_SERVERS];
static int net_p2pBrowseCount;
static int net_p2pBrowseInfoPending;
static qboolean net_p2pBrowseActive;
static int net_p2pBrowseStartTime;
static netadr_t net_p2pBrowseMasterAdr;

static void NET_P2P_NatRegisterCvars( void )
{
	if ( net_p2pStun && net_p2pStunServer && net_p2pStunAutoAdvertise &&
	     net_p2pStunInterval && net_p2pTurn && net_p2pTurnServer &&
	     net_p2pTurnUser && net_p2pTurnPass && net_p2pHostAdvertise &&
	     net_p2pTurnRefresh && net_p2pTurnChannels ) {
		return;
	}

	net_p2pStun = Cvar_Get( "net_p2pStun", "1", CVAR_ARCHIVE_ND );
	net_p2pStunServer = Cvar_Get( "net_p2pStunServer", "stun.l.google.com:19302", CVAR_ARCHIVE_ND );
	net_p2pStunAutoAdvertise = Cvar_Get( "net_p2pStunAutoAdvertise", "1", CVAR_ARCHIVE_ND );
	net_p2pStunInterval = Cvar_Get( "net_p2pStunInterval", "30000", CVAR_ARCHIVE_ND );
	net_p2pTurn = Cvar_Get( "net_p2pTurn", "0", CVAR_ARCHIVE_ND );
	net_p2pTurnServer = Cvar_Get( "net_p2pTurnServer", "", CVAR_ARCHIVE_ND );
	net_p2pTurnUser = Cvar_Get( "net_p2pTurnUser", "", CVAR_ARCHIVE_ND );
	net_p2pTurnPass = Cvar_Get( "net_p2pTurnPass", "", CVAR_ARCHIVE_ND );
	net_p2pHostAdvertise = Cvar_Get( "net_p2pHostAdvertise", "0", CVAR_ARCHIVE_ND );
	net_p2pTurnRefresh = Cvar_Get( "net_p2pTurnRefresh", "60", CVAR_ARCHIVE_ND );
	net_p2pTurnChannels = Cvar_Get( "net_p2pTurnChannels", "0", CVAR_ARCHIVE_ND );

	Cvar_SetDescription( net_p2pStun, "Enable STUN binding for ICE server-reflexive candidates (0=off, 1=on)." );
	Cvar_SetDescription( net_p2pStunServer, "STUN server host:port for NAT discovery." );
	Cvar_SetDescription( net_p2pStunAutoAdvertise, "Advertise STUN server-reflexive address when net_p2pAdvertiseAddress is unset (0=off, 1=on)." );
	Cvar_SetDescription( net_p2pStunInterval, "Milliseconds between STUN binding refreshes." );
	Cvar_SetDescription( net_p2pTurn, "Enable TURN relay allocation for ICE relay candidates (0=off, 1=on)." );
	Cvar_SetDescription( net_p2pTurnServer, "TURN server host:port for relay allocation." );
	Cvar_SetDescription( net_p2pTurnUser, "TURN long-term credential username." );
	Cvar_SetDescription( net_p2pTurnPass, "TURN long-term credential password." );
	Cvar_SetDescription( net_p2pHostAdvertise, "Allow advertising local host candidate when no STUN/TURN candidate is available (0=off, 1=on)." );
	Cvar_SetDescription( net_p2pTurnRefresh, "Seconds before TURN allocation expiry to send Refresh." );
	Cvar_SetDescription( net_p2pTurnChannels, "Enable TURN ChannelBind data relay scaffold (0=off, 1=on)." );
}

static void NET_P2P_NatRandomBytes( byte *out, int len )
{
	int i;

	for ( i = 0; i < len; i++ ) {
		out[i] = (byte)( rand() & 0xff );
	}
}

static void NET_P2P_NatSetCandidate( p2p_candidate_type_t type, const netadr_t *adr )
{
	if ( !adr || type < 0 || type >= P2P_CAND_COUNT ) {
		return;
	}

	net_p2pCandidates[type].valid = qtrue;
	net_p2pCandidates[type].adr = *adr;
	Com_sprintf(
		net_p2pCandidates[type].text,
		sizeof( net_p2pCandidates[type].text ),
		"udp:%s",
		NET_AdrToStringwPort( adr )
	);
}

static void NET_P2P_NatRefreshHostCandidate( void )
{
	netadr_t adr;
	const char *bindIp;

	if ( !net_p2pHostAdvertise || !net_p2pHostAdvertise->integer ) {
		net_p2pCandidates[P2P_CAND_HOST].valid = qfalse;
		return;
	}

	bindIp = Cvar_VariableString( "net_ip" );
	if ( !bindIp[0] || !Q_stricmp( bindIp, "0.0.0.0" ) ) {
		net_p2pCandidates[P2P_CAND_HOST].valid = qfalse;
		return;
	}

	Com_Memset( &adr, 0, sizeof( adr ) );
	if ( !NET_StringToAdr( va( "%s:%d", bindIp, Cvar_VariableIntegerValue( "net_port" ) ), &adr, NA_IP ) ) {
		net_p2pCandidates[P2P_CAND_HOST].valid = qfalse;
		return;
	}

	NET_P2P_NatSetCandidate( P2P_CAND_HOST, &adr );
}

static int NET_P2P_NatBuildBindingRequest( byte *out, int outSize, const byte *transactionId )
{
	return NET_P2P_StunBuildBindingRequest( out, outSize, transactionId );
}

static void NET_P2P_NatSendStun( p2p_stun_state_t *state, uint16_t msgType, const byte *extra, int extraLen, qboolean withIntegrity )
{
	byte packet[512];
	int len;

	if ( !state || !state->active ) {
		return;
	}

	len = 20;
	NET_P2P_StunWrite16( packet + 0, msgType );
	NET_P2P_StunWrite32( packet + 4, P2P_STUN_MAGIC_COOKIE );
	Com_Memcpy( packet + 8, state->transactionId, 12 );

	if ( extra && extraLen > 0 ) {
		if ( len + extraLen > (int)sizeof( packet ) ) {
			return;
		}
		Com_Memcpy( packet + len, extra, extraLen );
		len += extraLen;
	}

	if ( withIntegrity && NET_P2P_TurnAuthAvailable() && net_p2pTurnUser && net_p2pTurnUser->string[0] &&
	     net_p2pTurnPass && net_p2pTurnPass->string[0] && state->realm[0] && state->nonce[0] ) {
		len = NET_P2P_TurnAppendMessageIntegrity(
			packet, len, (int)sizeof( packet ),
			net_p2pTurnUser->string, net_p2pTurnPass->string, state->realm, state->nonce );
		if ( len <= 0 ) {
			return;
		}
	} else {
		NET_P2P_StunWrite16( packet + 2, (uint16_t)( len - 20 ) );
	}
	NET_SendPacket( NS_SERVER, len, packet, &state->serverAdr );
	NET_SendPacket( NS_CLIENT, len, packet, &state->serverAdr );
}

static void NET_P2P_NatBeginStun( p2p_stun_state_t *state, const char *server, qboolean turn )
{
	if ( !state || !server || !server[0] ) {
		return;
	}

	Com_Memset( state, 0, sizeof( *state ) );
	if ( !NET_StringToAdr( server, &state->serverAdr, NA_UNSPEC ) ) {
		return;
	}

	state->active = qtrue;
	state->pending = qtrue;
	state->turnPhase = turn ? 1 : 0;
	NET_P2P_NatRandomBytes( state->transactionId, sizeof( state->transactionId ) );
	state->lastRequestTime = Sys_Milliseconds();
	state->nextRequestTime = state->lastRequestTime;
}

static qboolean NET_P2P_NatParseStunResponse( p2p_stun_state_t *state, const byte *data, int len, qboolean turn )
{
	p2p_stun_parse_result_t result;
	byte attrs[256];
	int attrLen;

	if ( !state || !state->pending || len < 20 ) {
		return qfalse;
	}

	if ( !NET_P2P_StunParseMessage( data, len, state->transactionId, &result ) ) {
		return qfalse;
	}

	if ( turn ) {
		if ( result.msgType == P2P_STUN_ERROR_RESPONSE ) {
			if ( result.errorCode >= 0 ) {
				Com_Printf( "P2P ICE: TURN error %d %s\n", result.errorCode,
					result.errorReason[0] ? result.errorReason : "" );
			}
		} else if ( result.msgType != P2P_STUN_ALLOCATE_SUCCESS &&
		            result.msgType != P2P_STUN_REFRESH_SUCCESS &&
		            result.msgType != P2P_STUN_CREATE_PERMISSION_SUCCESS &&
		            result.msgType != P2P_STUN_CHANNEL_BIND_SUCCESS ) {
			return qfalse;
		}
	} else if ( result.msgType != P2P_STUN_BINDING_RESPONSE ) {
		return qfalse;
	}

	if ( result.realm[0] ) {
		Q_strncpyz( state->realm, result.realm, sizeof( state->realm ) );
	}
	if ( result.nonce[0] ) {
		Q_strncpyz( state->nonce, result.nonce, sizeof( state->nonce ) );
	}

	if ( result.haveMapped ) {
		state->mappedAdr = result.mappedAdr;
		state->mappedValid = qtrue;
	}
	if ( turn && result.haveRelayed ) {
		state->mappedAdr = result.relayedAdr;
		state->mappedValid = qtrue;
	}
	if ( turn && result.lifetime > 0 ) {
		state->allocationLifetime = result.lifetime;
		state->nextRefreshTime = Sys_Milliseconds() +
			(int)( ( result.lifetime - (uint32_t)net_p2pTurnRefresh->integer ) * 1000 );
	}

	if ( turn && !state->mappedValid && state->realm[0] && state->nonce[0] && state->turnPhase == 1 ) {
		attrLen = NET_P2P_StunBuildAllocateAttrs(
			attrs, sizeof( attrs ),
			net_p2pTurnUser->string, state->realm, state->nonce );
		if ( attrLen <= 0 ) {
			return qfalse;
		}
		state->turnPhase = 2;
		NET_P2P_NatSendStun( state, P2P_STUN_ALLOCATE_REQUEST, attrs, attrLen, qtrue );
		return qfalse;
	}

	if ( state->mappedValid ) {
		state->pending = qfalse;
		if ( turn ) {
			NET_P2P_NatSetCandidate( P2P_CAND_RELAY, &state->mappedAdr );
			Com_Printf( "P2P ICE: TURN relay candidate %s\n", net_p2pCandidates[P2P_CAND_RELAY].text );
		} else {
			NET_P2P_NatSetCandidate( P2P_CAND_SRFLX, &state->mappedAdr );
			Com_Printf( "P2P ICE: STUN server-reflexive candidate %s\n", net_p2pCandidates[P2P_CAND_SRFLX].text );
		}
		return qtrue;
	}

	return qfalse;
}

static void NET_P2P_NatSendTurnPermission( p2p_stun_state_t *state, const netadr_t *peer )
{
	byte attrs[256];
	int attrLen;

	if ( !state || !peer || !NET_P2P_TurnAuthAvailable() ) {
		return;
	}

	attrLen = NET_P2P_StunBuildCreatePermissionAttrs(
		attrs, sizeof( attrs ), peer,
		net_p2pTurnUser->string, state->realm, state->nonce );
	if ( attrLen <= 0 ) {
		return;
	}

	state->permissionPeer = *peer;
	state->permissionSent = qtrue;
	NET_P2P_NatSendStun( state, P2P_STUN_CREATE_PERMISSION_REQUEST, attrs, attrLen, qtrue );
	Com_Printf( "P2P ICE: TURN CreatePermission for %s\n", NET_AdrToString( peer ) );
}

static void NET_P2P_NatMaybeRefreshTurn( p2p_stun_state_t *state )
{
	byte attrs[256];
	int attrLen;
	int now;

	if ( !state || !state->active || !state->allocationLifetime ) {
		return;
	}

	now = Sys_Milliseconds();
	if ( state->nextRefreshTime > now ) {
		return;
	}

	attrLen = NET_P2P_StunBuildRefreshAttrs(
		attrs, sizeof( attrs ), state->allocationLifetime,
		net_p2pTurnUser->string, state->realm, state->nonce );
	if ( attrLen <= 0 ) {
		return;
	}

	NET_P2P_NatSendStun( state, P2P_STUN_REFRESH_REQUEST, attrs, attrLen, qtrue );
	state->nextRefreshTime = now + ( net_p2pTurnRefresh->integer * 1000 );
	Com_Printf( "P2P ICE: TURN Refresh (lifetime %u)\n", state->allocationLifetime );
}

static void NET_P2P_NatScheduleStunRefresh( p2p_stun_state_t *state, int intervalMs )
{
	if ( !state ) {
		return;
	}

	state->nextRequestTime = Sys_Milliseconds() + intervalMs;
}

static void NET_P2P_NatTickStun( p2p_stun_state_t *state, const char *server, int intervalMs, qboolean turn )
{
	byte packet[32];
	int now;

	if ( !state || !server || !server[0] ) {
		return;
	}

	now = Sys_Milliseconds();
	if ( !state->active ) {
		NET_P2P_NatBeginStun( state, server, turn );
	}

	if ( now < state->nextRequestTime ) {
		return;
	}

	if ( turn && state->turnPhase == 0 ) {
		state->turnPhase = 1;
	}

	if ( turn && state->turnPhase >= 2 ) {
		NET_P2P_NatSendStun( state, P2P_STUN_ALLOCATE_REQUEST, NULL, 0, qtrue );
	} else if ( turn && state->turnPhase == 1 ) {
		NET_P2P_NatSendStun( state, P2P_STUN_ALLOCATE_REQUEST, NULL, 0, qfalse );
	} else {
		int len = NET_P2P_NatBuildBindingRequest( packet, sizeof( packet ), state->transactionId );
		if ( len > 0 ) {
			NET_SendPacket( NS_SERVER, len, packet, &state->serverAdr );
			NET_SendPacket( NS_CLIENT, len, packet, &state->serverAdr );
		}
	}

	state->lastRequestTime = now;
	state->pending = qtrue;
	NET_P2P_NatScheduleStunRefresh( state, intervalMs );
}

static p2p_browse_entry_t *NET_P2P_NatFindBrowseServer( const netadr_t *adr )
{
	int i;

	for ( i = 0; i < net_p2pBrowseCount; i++ ) {
		if ( NET_CompareAdr( adr, &net_p2pBrowseServers[i].adr ) ) {
			return &net_p2pBrowseServers[i];
		}
	}

	return NULL;
}

static void NET_P2P_NatFinishBrowse( void )
{
	int i;
	int matches;

	if ( !net_p2pBrowseActive ) {
		return;
	}

	matches = 0;
	Com_Printf( "p2p_list master: %d servers scanned\n", net_p2pBrowseCount );
	for ( i = 0; i < net_p2pBrowseCount; i++ ) {
		p2p_browse_entry_t *entry = &net_p2pBrowseServers[i];

		if ( !entry->haveInfo ) {
			continue;
		}
		if ( !entry->p2pAddr[0] && entry->clients <= 0 ) {
			continue;
		}

		Com_Printf(
			"%-9s %3d  clients:%d  map:%-16s  host:%s\n",
			"master",
			i,
			entry->clients,
			entry->mapName[0] ? entry->mapName : "<unknown>",
			entry->hostName[0] ? entry->hostName : "<unnamed>"
		);
		Com_Printf(
			"           p2p:%s  udp:%s\n",
			entry->p2pAddr[0] ? entry->p2pAddr : "<unavailable>",
			NET_AdrToStringwPort( &entry->adr )
		);
		Com_Printf(
			"           session:%s  proto:%d  reconnect:%ds  migrate:%s  secure:%s  failover:%s\n",
			entry->sessionId[0] ? entry->sessionId : "<auto>",
			entry->protocol,
			entry->reconnectWindow,
			entry->hostMigration ? "yes" : "no",
			entry->antiCheat[0] ? entry->antiCheat : "unknown",
			entry->failover[0] ? entry->failover : "unknown"
		);
		matches++;
	}

	if ( !matches ) {
		Com_Printf( "master    no P2P-capable servers in browse results\n" );
	}

	net_p2pBrowseActive = qfalse;
	net_p2pBrowseInfoPending = 0;
}

void NET_P2P_NatInit( void )
{
	NET_P2P_NatRegisterCvars();
	net_p2pNatInitialized = qtrue;

	if ( net_p2pStun && net_p2pStun->integer ) {
		Com_Printf( "P2P ICE: STUN enabled (%s)\n", net_p2pStunServer->string );
	}
	if ( net_p2pTurn && net_p2pTurn->integer ) {
		if ( !NET_P2P_TurnAuthAvailable() ) {
			Com_Printf( "P2P ICE: TURN requested but OpenSSL not available; relay disabled\n" );
		} else {
			Com_Printf( "P2P ICE: TURN enabled (%s)\n", net_p2pTurnServer->string );
		}
	}
}

void NET_P2P_NatShutdown( void )
{
	Com_Memset( net_p2pCandidates, 0, sizeof( net_p2pCandidates ) );
	Com_Memset( &net_p2pStunState, 0, sizeof( net_p2pStunState ) );
	Com_Memset( &net_p2pTurnState, 0, sizeof( net_p2pTurnState ) );
	Com_Memset( net_p2pBrowseServers, 0, sizeof( net_p2pBrowseServers ) );
	net_p2pBrowseCount = 0;
	net_p2pBrowseInfoPending = 0;
	net_p2pBrowseActive = qfalse;
	net_p2pNatInitialized = qfalse;
}

void NET_P2P_NatFrame( void )
{
	int i;
	int now;

	if ( !net_p2pNatInitialized ) {
		return;
	}

	NET_P2P_NatRegisterCvars();
	NET_P2P_NatRefreshHostCandidate();

	if ( net_p2pStun && net_p2pStun->integer && net_p2pStunServer && net_p2pStunServer->string[0] ) {
		NET_P2P_NatTickStun( &net_p2pStunState, net_p2pStunServer->string, net_p2pStunInterval->integer, qfalse );
	}

#ifdef USE_DTLS
	if ( net_p2pTurn && net_p2pTurn->integer && NET_P2P_TurnAuthAvailable() &&
	     net_p2pTurnServer && net_p2pTurnServer->string[0] &&
	     net_p2pTurnUser && net_p2pTurnUser->string[0] && net_p2pTurnPass && net_p2pTurnPass->string[0] ) {
		NET_P2P_NatTickStun( &net_p2pTurnState, net_p2pTurnServer->string, net_p2pStunInterval->integer, qtrue );
		NET_P2P_NatMaybeRefreshTurn( &net_p2pTurnState );
	}
#endif

	if ( !net_p2pBrowseActive ) {
		return;
	}

	now = Sys_Milliseconds();
	for ( i = 0; i < net_p2pBrowseCount; i++ ) {
		p2p_browse_entry_t *entry = &net_p2pBrowseServers[i];

		if ( entry->queried || entry->haveInfo ) {
			continue;
		}

		entry->queried = qtrue;
		NET_OutOfBandPrint( NS_SERVER, &entry->adr, "getinfo %i", com_protocol->integer );
	}

	if ( net_p2pBrowseInfoPending <= 0 || now - net_p2pBrowseStartTime > P2P_BROWSE_INFO_TIMEOUT_MS ) {
		NET_P2P_NatFinishBrowse();
	}
}

qboolean NET_P2P_NatTryHandlePacket( const netadr_t *from, const byte *data, int len )
{
	if ( !from || !data || len < 20 ) {
		return qfalse;
	}

	if ( NET_P2P_StunRead32( data + 4 ) != P2P_STUN_MAGIC_COOKIE ) {
		return qfalse;
	}

	if ( net_p2pStunState.active && net_p2pStunState.pending &&
	     NET_CompareAdr( from, &net_p2pStunState.serverAdr ) &&
	     NET_P2P_NatParseStunResponse( &net_p2pStunState, data, len, qfalse ) ) {
		return qtrue;
	}

#ifdef USE_DTLS
	if ( net_p2pTurnState.active &&
	     NET_CompareAdr( from, &net_p2pTurnState.serverAdr ) &&
	     NET_P2P_NatParseStunResponse( &net_p2pTurnState, data, len, qtrue ) ) {
		return qtrue;
	}
#endif

	return qfalse;
}

static void NET_P2P_NatParseServersResponse( msg_t *msg, qboolean extended )
{
	byte *buffptr;
	byte *buffend;
	int numservers;

	if ( !msg ) {
		return;
	}

	buffptr = msg->data;
	buffend = buffptr + msg->cursize;

	while ( buffptr < buffend && *buffptr != '\\'
#ifdef USE_IPV6
	        && *buffptr != '/'
#endif
	      ) {
		buffptr++;
	}
	numservers = 0;

	while ( buffptr + 1 < buffend && net_p2pBrowseCount < P2P_BROWSE_MAX_SERVERS ) {
		netadr_t adr;
		int i;

		Com_Memset( &adr, 0, sizeof( adr ) );

		if ( *buffptr == '\\' ) {
			buffptr++;
			if ( buffend - buffptr < 4 ) {
				break;
			}
			for ( i = 0; i < 4; i++ ) {
				adr.ipv._4[i] = (byte)*buffptr++;
			}
			adr.type = NA_IP;
		}
#ifdef USE_IPV6
		else if ( extended && *buffptr == '/' ) {
			buffptr++;
			if ( buffend - buffptr < (int)sizeof( adr.ipv._6 ) + 2 ) {
				break;
			}
			for ( i = 0; i < (int)sizeof( adr.ipv._6 ); i++ ) {
				adr.ipv._6[i] = (byte)*buffptr++;
			}
			adr.type = NA_IP6;
			adr.scope_id = net_p2pBrowseMasterAdr.scope_id;
		}
#endif
		else {
			break;
		}

		if ( buffend - buffptr < 2 ) {
			break;
		}

		adr.port = ( (byte)*buffptr++ ) << 8;
		adr.port += (byte)*buffptr++;
		adr.port = BigShort( adr.port );

		if ( *buffptr != '\\' && *buffptr != '/' ) {
			break;
		}

		if ( NET_P2P_NatFindBrowseServer( &adr ) ) {
			continue;
		}

		net_p2pBrowseServers[net_p2pBrowseCount].active = qtrue;
		net_p2pBrowseServers[net_p2pBrowseCount].adr = adr;
		net_p2pBrowseCount++;
		net_p2pBrowseInfoPending++;
		numservers++;
	}

	Com_Printf( "P2P master browse: parsed %d servers (pending info %d)\n", numservers, net_p2pBrowseInfoPending );
	if ( numservers == 0 && net_p2pBrowseCount == 0 ) {
		NET_P2P_NatFinishBrowse();
	}
}

qboolean NET_P2P_NatTryHandleConnectionless( const netadr_t *from, const char *cmd, msg_t *msg )
{
	p2p_browse_entry_t *entry;
	const char *infoString;
	int prot;

	if ( !net_p2pBrowseActive || !from || !cmd || !msg ) {
		return qfalse;
	}

	if ( !Q_strncmp( cmd, "getserversResponse", 18 ) ) {
		NET_P2P_NatParseServersResponse( msg, qfalse );
		return qtrue;
	}

	if ( !Q_strncmp( cmd, "getserversExtResponse", 21 ) ) {
		NET_P2P_NatParseServersResponse( msg, qtrue );
		return qtrue;
	}

	if ( Q_stricmp( cmd, "infoResponse" ) ) {
		return qfalse;
	}

	infoString = MSG_ReadString( msg );
	prot = atoi( Info_ValueForKey( infoString, "protocol" ) );
	if ( prot != OLD_PROTOCOL_VERSION && prot != NEW_PROTOCOL_VERSION && prot != com_protocol->integer ) {
		return qtrue;
	}

	entry = NET_P2P_NatFindBrowseServer( from );
	if ( !entry ) {
		return qtrue;
	}

	entry->haveInfo = qtrue;
	entry->protocol = prot;
	entry->clients = atoi( Info_ValueForKey( infoString, "clients" ) );
	Q_strncpyz( entry->hostName, Info_ValueForKey( infoString, "hostname" ), sizeof( entry->hostName ) );
	Q_strncpyz( entry->mapName, Info_ValueForKey( infoString, "mapname" ), sizeof( entry->mapName ) );
	Q_strncpyz( entry->p2pAddr, Info_ValueForKey( infoString, "p2paddr" ), sizeof( entry->p2pAddr ) );
	Q_strncpyz( entry->sessionId, Info_ValueForKey( infoString, "p2psession" ), sizeof( entry->sessionId ) );
	Q_strncpyz( entry->antiCheat, Info_ValueForKey( infoString, "p2psecure" ), sizeof( entry->antiCheat ) );
	Q_strncpyz( entry->failover, Info_ValueForKey( infoString, "p2pfail" ), sizeof( entry->failover ) );
	entry->reconnectWindow = atoi( Info_ValueForKey( infoString, "p2preconn" ) );
	entry->hostMigration = atoi( Info_ValueForKey( infoString, "p2pmigrate" ) ) ? qtrue : qfalse;
	if ( net_p2pBrowseInfoPending > 0 ) {
		net_p2pBrowseInfoPending--;
	}
	return qtrue;
}

void NET_P2P_NatBeginMasterList( const char *masterAddress )
{
	const char *master;
	char command[128];

	const char *game;

	NET_P2P_NatRegisterCvars();
	Com_Memset( net_p2pBrowseServers, 0, sizeof( net_p2pBrowseServers ) );
	net_p2pBrowseCount = 0;
	net_p2pBrowseInfoPending = 0;
	net_p2pBrowseActive = qtrue;
	net_p2pBrowseStartTime = Sys_Milliseconds();

	master = ( masterAddress && masterAddress[0] ) ? masterAddress : Cvar_VariableString( "sv_master1" );
	if ( !master[0] ) {
		master = Cvar_VariableString( "sv_master2" );
	}
	if ( !master[0] ) {
		Com_Printf( "P2P master browse: no sv_master cvar configured\n" );
		net_p2pBrowseActive = qfalse;
		return;
	}

	if ( !NET_StringToAdr( master, &net_p2pBrowseMasterAdr, NA_UNSPEC ) ) {
		Com_Printf( "P2P master browse: failed to resolve %s\n", master );
		net_p2pBrowseActive = qfalse;
		return;
	}

	if ( !net_p2pBrowseMasterAdr.port ) {
		net_p2pBrowseMasterAdr.port = BigShort( 27950 );
	}

	game = Cvar_VariableString( "fs_game" );
	if ( !game[0] ) {
		game = "empty";
	}
	Com_sprintf( command, sizeof( command ), "getservers %s", game );

	Com_Printf( "P2P master browse: querying %s\n", master );
	NET_OutOfBandPrint( NS_SERVER, &net_p2pBrowseMasterAdr, "%s", command );
}

void NET_P2P_NatPrintMasterList( void )
{
	NET_P2P_NatFinishBrowse();
}

qboolean NET_P2P_NatGetAdvertiseAddress( char *buffer, int bufferSize )
{
	int i;

	if ( !buffer || bufferSize <= 0 ) {
		return qfalse;
	}

	buffer[0] = '\0';
	NET_P2P_NatRegisterCvars();

	if ( net_p2pTurn && net_p2pTurn->integer && net_p2pCandidates[P2P_CAND_RELAY].valid ) {
		Q_strncpyz( buffer, net_p2pCandidates[P2P_CAND_RELAY].text, bufferSize );
		return qtrue;
	}

	if ( net_p2pStunAutoAdvertise && net_p2pStunAutoAdvertise->integer &&
	     net_p2pCandidates[P2P_CAND_SRFLX].valid ) {
		Q_strncpyz( buffer, net_p2pCandidates[P2P_CAND_SRFLX].text, bufferSize );
		return qtrue;
	}

	if ( net_p2pHostAdvertise && net_p2pHostAdvertise->integer &&
	     net_p2pCandidates[P2P_CAND_HOST].valid ) {
		Q_strncpyz( buffer, net_p2pCandidates[P2P_CAND_HOST].text, bufferSize );
		return qtrue;
	}

	for ( i = 0; i < P2P_CAND_COUNT; i++ ) {
		if ( net_p2pCandidates[i].valid ) {
			Q_strncpyz( buffer, net_p2pCandidates[i].text, bufferSize );
			return qtrue;
		}
	}

	return qfalse;
}

qboolean NET_P2P_NatGetCandidateText( p2p_candidate_type_t type, char *buffer, int bufferSize )
{
	if ( type < 0 || type >= P2P_CAND_COUNT || !buffer || bufferSize <= 0 ) {
		return qfalse;
	}

	if ( !net_p2pCandidates[type].valid ) {
		return qfalse;
	}

	Q_strncpyz( buffer, net_p2pCandidates[type].text, bufferSize );
	return qtrue;
}

void NET_P2P_NatGrantTurnPermission( const netadr_t *peer )
{
	if ( !peer || !net_p2pTurn || !net_p2pTurn->integer ) {
		return;
	}

	if ( net_p2pTurnState.permissionSent && NET_CompareAdr( peer, &net_p2pTurnState.permissionPeer ) ) {
		return;
	}

	NET_P2P_NatSendTurnPermission( &net_p2pTurnState, peer );
}

void NET_P2P_NatPrintCandidates( void )
{
	static const char *labels[P2P_CAND_COUNT] = { "host", "srflx", "relay" };
	int i;

	for ( i = 0; i < P2P_CAND_COUNT; i++ ) {
		if ( net_p2pCandidates[i].valid ) {
			Com_Printf( "ICE %-5s  %s\n", labels[i], net_p2pCandidates[i].text );
		} else {
			Com_Printf( "ICE %-5s  <unavailable>\n", labels[i] );
		}
	}
}
