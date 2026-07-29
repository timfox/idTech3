/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

ICE-lite connectivity checks for direct_udp P2P (p2pCand / p2pCheck / p2pCheckAck).
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "net_p2p.h"
#include "net_p2p_ice.h"
#include "net_p2p_nat.h"
#include "net_p2p_stun_codec.h"
#include "net_sdr.h"

#include <stdarg.h>
#include <string.h>

#define P2P_ICE_MAX_REMOTE_CANDS 8
#define P2P_ICE_MAX_CHECKS 16

typedef struct {
	qboolean valid;
	p2p_candidate_type_t type;
	char text[MAX_STRING_CHARS];
	netadr_t adr;
} p2p_ice_remote_cand_t;

typedef struct {
	qboolean active;
	char peerAddress[MAX_STRING_CHARS];
	netadr_t peerAdr;
	p2p_ice_remote_cand_t remote[P2P_ICE_MAX_REMOTE_CANDS];
	int remoteCount;
	int checkTxn;
	int checkIndex;
	int deadlineMs;
	int nominatedTxn;
	char nominatedAddress[MAX_STRING_CHARS];
	qboolean complete;
	qboolean success;
	qboolean deferConnect;
	qboolean connectReady;
} p2p_ice_connect_t;

static cvar_t *net_p2pIceChecks;
static cvar_t *net_p2pIceTimeout;
static cvar_t *net_p2pIceDeferConnect;

static p2p_ice_connect_t net_p2pIceConnect;

static void NET_P2P_IceMarkConnectReady( void )
{
	if ( !net_p2pIceConnect.deferConnect || net_p2pIceConnect.connectReady ) {
		return;
	}
	if ( !net_p2pIceConnect.peerAddress[0] ) {
		net_p2pIceConnect.deferConnect = qfalse;
		return;
	}
	net_p2pIceConnect.connectReady = qtrue;
	net_p2pIceConnect.deferConnect = qfalse;
	Com_Printf( "P2P ICE: connect ready for %s\n", net_p2pIceConnect.peerAddress );
}

static qboolean NET_P2P_IceIsActivePeer( const netadr_t *from )
{
	if ( !from || !net_p2pIceConnect.active ) {
		return qfalse;
	}

	return NET_CompareAdr( from, &net_p2pIceConnect.peerAdr );
}

static qboolean NET_P2P_IceUsingDirectUdp( void )
{
	if ( !NET_P2P_IsEnabled() ) {
		return qfalse;
	}
	return (qboolean)!NET_P2P_UsesSteamSdrBackend();
}

static void NET_P2P_IceRegisterCvars( void )
{
	if ( net_p2pIceChecks && net_p2pIceTimeout && net_p2pIceDeferConnect ) {
		return;
	}

	net_p2pIceChecks = Cvar_Get( "net_p2pIceChecks", "1", CVAR_ARCHIVE_ND );
	net_p2pIceTimeout = Cvar_Get( "net_p2pIceTimeout", "3000", CVAR_ARCHIVE_ND );
	net_p2pIceDeferConnect = Cvar_Get( "net_p2pIceDeferConnect", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( net_p2pIceChecks, "Run ICE-lite connectivity checks before direct_udp connect (0=off, 1=on)." );
	Cvar_SetDescription( net_p2pIceTimeout, "Milliseconds to wait for ICE candidate exchange and connectivity checks." );
	Cvar_SetDescription( net_p2pIceDeferConnect,
		"Defer client connect until ICE nominates a path or times out (0=connect immediately, 1=wait)." );
}

static void NET_P2P_IceSendOob( const netadr_t *adr, const char *fmt, ... )
{
	va_list ap;
	char msg[MAX_STRING_CHARS];

	if ( !adr ) {
		return;
	}

	va_start( ap, fmt );
#if defined( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
#endif
	Q_vsnprintf( msg, sizeof( msg ), fmt, ap );
#if defined( __clang__ )
#pragma clang diagnostic pop
#endif
	va_end( ap );

	NET_OutOfBandPrint( NS_CLIENT, adr, "%s", msg );
	NET_OutOfBandPrint( NS_SERVER, adr, "%s", msg );
}

static void NET_P2P_IceSendLocalCandidates( const netadr_t *peer )
{
	char line[MAX_STRING_CHARS];
	int i;
	static const char *labels[P2P_CAND_COUNT] = { "host", "srflx", "relay" };

	line[0] = '\0';
	for ( i = 0; i < P2P_CAND_COUNT; i++ ) {
		char cand[MAX_STRING_CHARS];

		if ( !NET_P2P_NatGetCandidateText( (p2p_candidate_type_t)i, cand, sizeof( cand ) ) ) {
			continue;
		}
		if ( line[0] ) {
			Q_strcat( line, sizeof( line ), ";" );
		}
		Q_strcat( line, sizeof( line ), labels[i] );
		Q_strcat( line, sizeof( line ), "=" );
		Q_strcat( line, sizeof( line ), cand );
	}

	if ( line[0] ) {
		NET_P2P_IceSendOob( peer, "p2pCand %s", line );
	}
}

static qboolean NET_P2P_IceParseRemoteCandidate( const char *token, p2p_ice_remote_cand_t *out )
{
	const char *eq;
	char typeName[16];
	char address[MAX_STRING_CHARS];
	netadr_t adr;
	int i;
	static const char *labels[P2P_CAND_COUNT] = { "host", "srflx", "relay" };

	if ( !token || !out || !token[0] ) {
		return qfalse;
	}

	eq = strchr( token, '=' );
	if ( !eq ) {
		return qfalse;
	}

	if ( eq - token >= (int)sizeof( typeName ) ) {
		return qfalse;
	}

	Com_Memset( typeName, 0, sizeof( typeName ) );
	Com_Memcpy( typeName, token, (size_t)( eq - token ) );
	Q_strncpyz( address, eq + 1, sizeof( address ) );

	if ( !NET_P2P_NormalizeAddressString( address, address, sizeof( address ) ) ) {
		return qfalse;
	}

	if ( Q_stricmpn( address, "udp:", 4 ) != 0 ) {
		return qfalse;
	}

	if ( !NET_StringToAdr( address + 4, &adr, NA_UNSPEC ) ) {
		return qfalse;
	}

	for ( i = 0; i < P2P_CAND_COUNT; i++ ) {
		if ( !Q_stricmp( typeName, labels[i] ) ) {
			out->valid = qtrue;
			out->type = (p2p_candidate_type_t)i;
			out->adr = adr;
			Q_strncpyz( out->text, address, sizeof( out->text ) );
			return qtrue;
		}
	}

	return qfalse;
}

static void NET_P2P_IceParseRemoteCandidates( const char *line )
{
	char buffer[MAX_STRING_CHARS];
	char *cursor;
	char *token;

	if ( !line || !net_p2pIceConnect.active ) {
		return;
	}

	Q_strncpyz( buffer, line, sizeof( buffer ) );
	net_p2pIceConnect.remoteCount = 0;

	cursor = buffer;
	while ( cursor && *cursor && net_p2pIceConnect.remoteCount < P2P_ICE_MAX_REMOTE_CANDS ) {
		token = cursor;
		cursor = strchr( cursor, ';' );
		if ( cursor ) {
			*cursor = '\0';
			cursor++;
		}
		if ( NET_P2P_IceParseRemoteCandidate( token, &net_p2pIceConnect.remote[net_p2pIceConnect.remoteCount] ) ) {
			net_p2pIceConnect.remoteCount++;
		}
	}
}

static void NET_P2P_IceBeginChecks( void )
{
	int i;
	int localIdx;
	int remoteIdx;
	int bestPriority = -1;
	int bestLocal = -1;
	int bestRemote = -1;

	for ( localIdx = 0; localIdx < P2P_CAND_COUNT; localIdx++ ) {
		char localText[MAX_STRING_CHARS];
		netadr_t localAdr;

		if ( !NET_P2P_NatGetCandidateText( (p2p_candidate_type_t)localIdx, localText, sizeof( localText ) ) ) {
			continue;
		}
		if ( !NET_StringToAdr( localText + 4, &localAdr, NA_UNSPEC ) ) {
			continue;
		}

		for ( remoteIdx = 0; remoteIdx < net_p2pIceConnect.remoteCount; remoteIdx++ ) {
			int priority;

			if ( !net_p2pIceConnect.remote[remoteIdx].valid ) {
				continue;
			}

			priority = NET_P2P_StunCandidatePriority(
				(p2p_candidate_type_t)localIdx,
				net_p2pIceConnect.remote[remoteIdx].type );
			if ( priority > bestPriority ) {
				bestPriority = priority;
				bestLocal = localIdx;
				bestRemote = remoteIdx;
			}
		}
	}

	if ( bestRemote < 0 ) {
		return;
	}

	net_p2pIceConnect.checkTxn = rand() ^ Sys_Milliseconds();
	net_p2pIceConnect.checkIndex = 0;
	net_p2pIceConnect.nominatedTxn = 0;

	for ( i = 0; i < P2P_ICE_MAX_CHECKS; i++ ) {
		int checkRemoteIdx = ( bestRemote + i ) % net_p2pIceConnect.remoteCount;
		if ( !net_p2pIceConnect.remote[checkRemoteIdx].valid ) {
			continue;
		}
		if ( net_p2pIceConnect.remote[checkRemoteIdx].type == P2P_CAND_RELAY ) {
			NET_P2P_NatGrantTurnPermission( &net_p2pIceConnect.peerAdr );
		}
		net_p2pIceConnect.checkTxn++;
		NET_P2P_IceSendOob(
			&net_p2pIceConnect.peerAdr,
			"p2pCheck %d %s",
			net_p2pIceConnect.checkTxn,
			net_p2pIceConnect.remote[checkRemoteIdx].text );
		net_p2pIceConnect.checkIndex++;
		if ( net_p2pIceConnect.checkIndex >= 3 ) {
			break;
		}
	}

	(void)bestLocal;
}

void NET_P2P_IceInit( void )
{
	NET_P2P_IceRegisterCvars();
	Com_Printf( "P2P ICE: connectivity checks %s (timeout %dms, deferConnect %s)\n",
		( net_p2pIceChecks && net_p2pIceChecks->integer ) ? "enabled" : "disabled",
		net_p2pIceTimeout ? net_p2pIceTimeout->integer : 3000,
		( net_p2pIceDeferConnect && net_p2pIceDeferConnect->integer ) ? "on" : "off" );
}

void NET_P2P_IceShutdown( void )
{
	Com_Memset( &net_p2pIceConnect, 0, sizeof( net_p2pIceConnect ) );
}

void NET_P2P_IceFrame( void )
{
	if ( !net_p2pIceConnect.active || net_p2pIceConnect.complete ) {
		return;
	}

	if ( Sys_Milliseconds() >= net_p2pIceConnect.deadlineMs ) {
		net_p2pIceConnect.complete = qtrue;
		if ( !net_p2pIceConnect.success ) {
			Com_Printf( "P2P ICE: connectivity checks timed out, using direct punch fallback\n" );
			NET_P2P_BeginPunchForAddress( net_p2pIceConnect.peerAddress );
		}
		net_p2pIceConnect.active = qfalse;
		NET_P2P_IceMarkConnectReady();
	}
}

qboolean NET_P2P_IceBeginConnectPath( const char *peerAddress )
{
	char normalized[MAX_STRING_CHARS];
	netadr_t adr;

	NET_P2P_IceRegisterCvars();

	if ( !NET_P2P_IceUsingDirectUdp() ) {
		return qfalse;
	}

	if ( NET_P2P_UsesSteamSdrBackend() ) {
		Com_Printf( "P2P: using steam_sdr (ICE skipped)\n" );
		return qfalse;
	}

	if ( !net_p2pIceChecks || !net_p2pIceChecks->integer ) {
		NET_P2P_BeginPunchForAddress( peerAddress );
		return qtrue;
	}

	if ( !NET_P2P_NormalizeAddressString( peerAddress, normalized, sizeof( normalized ) ) ) {
		return qfalse;
	}

	if ( Q_stricmpn( normalized, "udp:", 4 ) != 0 ) {
		return qfalse;
	}

	if ( !NET_StringToAdr( normalized + 4, &adr, NA_UNSPEC ) ) {
		return qfalse;
	}

	Com_Memset( &net_p2pIceConnect, 0, sizeof( net_p2pIceConnect ) );
	net_p2pIceConnect.active = qtrue;
	net_p2pIceConnect.peerAdr = adr;
	Q_strncpyz( net_p2pIceConnect.peerAddress, normalized, sizeof( net_p2pIceConnect.peerAddress ) );
	net_p2pIceConnect.deadlineMs = Sys_Milliseconds() + ( net_p2pIceTimeout ? net_p2pIceTimeout->integer : 3000 );
	/* Dedicated servers run ICE for punch/NAT only; clients defer game connect. */
	if ( net_p2pIceDeferConnect && net_p2pIceDeferConnect->integer &&
	     !( com_dedicated && com_dedicated->integer ) ) {
		net_p2pIceConnect.deferConnect = qtrue;
	}

	NET_P2P_IceSendLocalCandidates( &adr );
	NET_P2P_IceSendOob( &adr, "p2pCandRequest" );
	Com_Printf( "P2P ICE: started connectivity checks for %s%s\n", normalized,
		net_p2pIceConnect.deferConnect ? " (connect deferred)" : "" );
	return qtrue;
}

qboolean NET_P2P_IceHandleOobPacket( const netadr_t *from, const char *cmd )
{
	int txn;

	if ( !from || !cmd || !cmd[0] ) {
		return qfalse;
	}

	if ( !Q_stricmp( cmd, "p2pCandRequest" ) ) {
		NET_P2P_IceSendLocalCandidates( from );
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "p2pCand" ) ) {
		if ( net_p2pIceConnect.active && !NET_P2P_IceIsActivePeer( from ) ) {
			Com_Printf( "P2P ICE: ignoring candidates from unexpected peer %s\n",
				NET_AdrToString( from ) );
			return qtrue;
		}
		NET_P2P_IceParseRemoteCandidates( Cmd_Argv( 1 ) );
		if ( net_p2pIceConnect.active && !net_p2pIceConnect.complete ) {
			NET_P2P_IceBeginChecks();
		}
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "p2pCheck" ) ) {
		txn = atoi( Cmd_Argv( 1 ) );
		NET_P2P_IceSendOob( from, "p2pCheckAck %d", txn );
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "p2pCheckAck" ) ) {
		txn = atoi( Cmd_Argv( 1 ) );
		if ( net_p2pIceConnect.active && !NET_P2P_IceIsActivePeer( from ) ) {
			Com_Printf( "P2P ICE: ignoring check ack from unexpected peer %s\n",
				NET_AdrToString( from ) );
			return qtrue;
		}
		if ( net_p2pIceConnect.active && !net_p2pIceConnect.success ) {
			if ( !net_p2pIceConnect.nominatedTxn || txn == net_p2pIceConnect.checkTxn ) {
				net_p2pIceConnect.nominatedTxn = txn;
				net_p2pIceConnect.success = qtrue;
				net_p2pIceConnect.complete = qtrue;
				net_p2pIceConnect.active = qfalse;
				Q_strncpyz( net_p2pIceConnect.nominatedAddress, net_p2pIceConnect.peerAddress,
					sizeof( net_p2pIceConnect.nominatedAddress ) );
				Com_Printf( "P2P ICE: nominated path %s (txn %d)\n", net_p2pIceConnect.nominatedAddress, txn );
				NET_P2P_BeginPunchForAddress( net_p2pIceConnect.nominatedAddress );
				NET_P2P_IceMarkConnectReady();
			}
		}
		return qtrue;
	}

	return qfalse;
}

qboolean NET_P2P_IceConnectIsDeferred( void )
{
	return (qboolean)( net_p2pIceConnect.deferConnect && !net_p2pIceConnect.connectReady );
}

qboolean NET_P2P_IceConsumeDeferredConnect( char *buffer, int bufferSize )
{
	if ( !buffer || bufferSize <= 0 || !net_p2pIceConnect.connectReady ) {
		return qfalse;
	}
	if ( !net_p2pIceConnect.peerAddress[0] ) {
		net_p2pIceConnect.connectReady = qfalse;
		return qfalse;
	}

	Q_strncpyz( buffer, net_p2pIceConnect.peerAddress, bufferSize );
	net_p2pIceConnect.connectReady = qfalse;
	return qtrue;
}

void NET_P2P_IceGetStatus( p2p_path_status_t *status )
{
	if ( !status ) {
		return;
	}

	status->iceActive = net_p2pIceConnect.active;
	status->iceSuccess = net_p2pIceConnect.success;
	status->iceComplete = net_p2pIceConnect.complete;
	status->connectDeferred = net_p2pIceConnect.deferConnect;
	status->connectReady = net_p2pIceConnect.connectReady;
	status->iceRemoteCandidates = net_p2pIceConnect.remoteCount;
	status->iceChecksSent = net_p2pIceConnect.checkIndex;
	status->iceNominatedTxn = net_p2pIceConnect.nominatedTxn;

	if ( net_p2pIceConnect.active && net_p2pIceConnect.deadlineMs > 0 ) {
		status->iceTimeoutRemainingMs = net_p2pIceConnect.deadlineMs - Sys_Milliseconds();
		if ( status->iceTimeoutRemainingMs < 0 ) {
			status->iceTimeoutRemainingMs = 0;
		}
	}

	Q_strncpyz( status->icePeerAddress, net_p2pIceConnect.peerAddress, sizeof( status->icePeerAddress ) );
	Q_strncpyz( status->iceNominatedAddress, net_p2pIceConnect.nominatedAddress, sizeof( status->iceNominatedAddress ) );
}

void NET_P2P_IcePrintStatus( void )
{
	if ( net_p2pIceConnect.active ) {
		Com_Printf( "P2P ICE: active peer %s remoteCands:%d checks:%d timeout:%dms\n",
			net_p2pIceConnect.peerAddress,
			net_p2pIceConnect.remoteCount,
			net_p2pIceConnect.checkIndex,
			net_p2pIceTimeout ? ( net_p2pIceConnect.deadlineMs - Sys_Milliseconds() ) : 0 );
	} else if ( net_p2pIceConnect.success && net_p2pIceConnect.nominatedAddress[0] ) {
		Com_Printf( "P2P ICE: last result success path %s txn:%d\n",
			net_p2pIceConnect.nominatedAddress,
			net_p2pIceConnect.nominatedTxn );
	} else if ( net_p2pIceConnect.complete && net_p2pIceConnect.peerAddress[0] ) {
		Com_Printf( "P2P ICE: last result fallback peer %s\n",
			net_p2pIceConnect.peerAddress );
	} else if ( net_p2pIceConnect.nominatedAddress[0] ) {
		Com_Printf( "P2P ICE: last nominated %s\n", net_p2pIceConnect.nominatedAddress );
	} else {
		Com_Printf( "P2P ICE: idle\n" );
	}
}
