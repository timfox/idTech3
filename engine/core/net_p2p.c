/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Optional peer-to-peer networking facade.
===========================================================================
*/

#include <inttypes.h>

#include "q_shared.h"
#include "qcommon.h"
#include "net_p2p.h"
#include "net_p2p_nat.h"
#include "net_p2p_ice.h"
#include "net_sdr.h"

#define P2P_PUNCH_MAX_PEERS 8

typedef struct p2p_punch_peer_s {
	qboolean active;
	qboolean acknowledged;
	netadr_t adr;
	char addressString[MAX_OSPATH];
	int nonce;
	int startTime;
	int lastSendTime;
	int lastResponseTime;
	int attempts;
} p2p_punch_peer_t;

static cvar_t *net_p2p;
static cvar_t *net_p2pBackend;
static cvar_t *net_p2pAdvertiseAddress;
static cvar_t *net_p2pPunch;
static cvar_t *net_p2pPunchInterval;
static cvar_t *net_p2pPunchAttempts;
static cvar_t *net_p2pPunchKeepalive;
static p2p_punch_peer_t net_p2pPunchPeers[P2P_PUNCH_MAX_PEERS];

static qboolean NET_P2P_ParseAddress( const char *address, netadr_t *adr )
{
	const char *parseAddress;

	if ( !address || !address[0] || !adr ) {
		return qfalse;
	}

	parseAddress = address;
	if ( !Q_stricmpn( parseAddress, "udp:", 4 ) ) {
		parseAddress += 4;
	}

	return (qboolean)( NET_StringToAdr( parseAddress, adr, NA_UNSPEC ) != 0 );
}

static p2p_punch_peer_t *NET_P2P_FindPunchPeerByAddress( const netadr_t *adr )
{
	int i;

	if ( !adr ) {
		return NULL;
	}

	for ( i = 0; i < P2P_PUNCH_MAX_PEERS; i++ ) {
		if ( net_p2pPunchPeers[i].active && NET_CompareAdr( &net_p2pPunchPeers[i].adr, adr ) ) {
			return &net_p2pPunchPeers[i];
		}
	}

	return NULL;
}

static p2p_punch_peer_t *NET_P2P_AllocPunchPeer( void )
{
	int i;
	p2p_punch_peer_t *oldest;

	oldest = &net_p2pPunchPeers[0];

	for ( i = 0; i < P2P_PUNCH_MAX_PEERS; i++ ) {
		if ( !net_p2pPunchPeers[i].active ) {
			return &net_p2pPunchPeers[i];
		}
		if ( net_p2pPunchPeers[i].startTime < oldest->startTime ) {
			oldest = &net_p2pPunchPeers[i];
		}
	}

	return oldest;
}

static void NET_P2P_SendPunchPacket( const netadr_t *adr, const char *cmd, int nonce )
{
	NET_OutOfBandPrint( NS_CLIENT, adr, "%s %d", cmd, nonce );
	NET_OutOfBandPrint( NS_SERVER, adr, "%s %d", cmd, nonce );
}

static void NET_P2P_RegisterCvars( void )
{
	if ( net_p2p && net_p2pBackend && net_p2pAdvertiseAddress &&
	     net_p2pPunch && net_p2pPunchInterval && net_p2pPunchAttempts && net_p2pPunchKeepalive ) {
		return;
	}

	net_p2p = Cvar_Get( "net_p2p", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	net_p2pBackend = Cvar_Get( "net_p2pBackend", "auto", CVAR_ARCHIVE_ND | CVAR_LATCH );
	net_p2pAdvertiseAddress = Cvar_Get( "net_p2pAdvertiseAddress", "", CVAR_ARCHIVE_ND );
	net_p2pPunch = Cvar_Get( "net_p2pPunch", "1", CVAR_ARCHIVE_ND );
	net_p2pPunchInterval = Cvar_Get( "net_p2pPunchInterval", "750", CVAR_ARCHIVE_ND );
	net_p2pPunchAttempts = Cvar_Get( "net_p2pPunchAttempts", "8", CVAR_ARCHIVE_ND );
	net_p2pPunchKeepalive = Cvar_Get( "net_p2pPunchKeepalive", "1", CVAR_ARCHIVE_ND );

	Cvar_SetDescription( net_p2p, "Enable optional peer-to-peer transport (0=off, 1=on)." );
	Cvar_SetDescription( net_p2pBackend, "Select P2P backend: auto, steam_sdr, or direct_udp." );
	Cvar_SetDescription( net_p2pAdvertiseAddress, "Optional externally reachable direct UDP address for P2P advertisement, e.g. udp:203.0.113.7:27960 or 203.0.113.7:27960." );
	Cvar_SetDescription( net_p2pPunch, "Enable direct-UDP punchthrough helper packets (0=off, 1=on)." );
	Cvar_SetDescription( net_p2pPunchInterval, "Milliseconds between direct-UDP punch packets." );
	Cvar_SetDescription( net_p2pPunchAttempts, "Maximum direct-UDP punch attempts before giving up." );
	Cvar_SetDescription( net_p2pPunchKeepalive, "Keep sending periodic direct-UDP keepalive packets after a punch succeeds (0=off, 1=on)." );
}

static qboolean NET_P2P_SteamSupported( void )
{
#if defined(USE_STEAM_NETWORKING) && defined(STEAMWORKS_AVAILABLE) && STEAMWORKS_AVAILABLE
	return qtrue;
#else
	return qfalse;
#endif
}

static qboolean NET_P2P_UsingDirectUdp( void )
{
	const char *backend;

	NET_P2P_RegisterCvars();
	backend = net_p2pBackend ? net_p2pBackend->string : "auto";

	if ( !Q_stricmp( backend, "direct_udp" ) ) {
		return qtrue;
	}

	if ( !Q_stricmp( backend, "steam_sdr" ) ) {
		return qfalse;
	}

	return (qboolean)!NET_P2P_SteamSupported();
}

qboolean NET_P2P_UsesSteamSdrBackend( void )
{
	const char *backend;

	NET_P2P_RegisterCvars();
	backend = net_p2pBackend ? net_p2pBackend->string : "auto";

	if ( !Q_stricmp( backend, "steam_sdr" ) ) {
		return NET_P2P_SteamSupported();
	}

	if ( !Q_stricmp( backend, "direct_udp" ) ) {
		return qfalse;
	}

	return NET_P2P_SteamSupported();
}

static void NET_P2P_EnsurePunchPeer( const netadr_t *adr, const char *addressString, int nonce, qboolean resetAttempts )
{
	p2p_punch_peer_t *peer;
	int now;

	if ( !adr || !addressString || !addressString[0] ) {
		return;
	}

	peer = NET_P2P_FindPunchPeerByAddress( adr );
	if ( !peer ) {
		peer = NET_P2P_AllocPunchPeer();
		Com_Memset( peer, 0, sizeof( *peer ) );
		peer->active = qtrue;
		peer->adr = *adr;
		peer->startTime = Sys_Milliseconds();
		Q_strncpyz( peer->addressString, addressString, sizeof( peer->addressString ) );
	}

	now = Sys_Milliseconds();
	if ( nonce ) {
		peer->nonce = nonce;
	}
	peer->lastResponseTime = now;
	if ( resetAttempts ) {
		peer->attempts = 0;
		peer->acknowledged = qfalse;
		peer->lastSendTime = 0;
	}
}

void NET_P2P_Init( void )
{
	NET_P2P_RegisterCvars();
	NET_P2P_NatInit();
	NET_P2P_IceInit();
}

void NET_P2P_Shutdown( void )
{
	NET_P2P_IceShutdown();
	NET_P2P_NatShutdown();
	Com_Memset( net_p2pPunchPeers, 0, sizeof( net_p2pPunchPeers ) );
}

void NET_P2P_Frame( void )
{
	int i;
	int now;

	NET_P2P_RegisterCvars();
	NET_P2P_NatFrame();
	NET_P2P_IceFrame();

	if ( !NET_P2P_IsEnabled() || !NET_P2P_UsingDirectUdp() || !net_p2pPunch || !net_p2pPunch->integer ) {
		return;
	}

	now = Sys_Milliseconds();

	for ( i = 0; i < P2P_PUNCH_MAX_PEERS; i++ ) {
		p2p_punch_peer_t *peer = &net_p2pPunchPeers[i];

		if ( !peer->active ) {
			continue;
		}

		if ( peer->acknowledged ) {
			if ( !net_p2pPunchKeepalive || !net_p2pPunchKeepalive->integer ) {
				continue;
			}
			if ( now - peer->lastSendTime < net_p2pPunchInterval->integer * 4 ) {
				continue;
			}
			NET_P2P_SendPunchPacket( &peer->adr, "p2pKeepalive", peer->nonce );
			peer->lastSendTime = now;
			continue;
		}

		if ( peer->attempts >= net_p2pPunchAttempts->integer ) {
			peer->active = qfalse;
			Com_Printf( "P2P direct_udp: punch expired for %s\n", peer->addressString );
			continue;
		}

		if ( peer->lastSendTime && now - peer->lastSendTime < net_p2pPunchInterval->integer ) {
			continue;
		}

		NET_P2P_SendPunchPacket( &peer->adr, "p2pPunch", peer->nonce );
		peer->lastSendTime = now;
		peer->attempts++;
	}
}

qboolean NET_P2P_IsSupported( void )
{
	return qtrue;
}

qboolean NET_P2P_IsEnabled( void )
{
	NET_P2P_RegisterCvars();
	return ( net_p2p && net_p2p->integer ) ? qtrue : qfalse;
}

qboolean NET_P2P_IsReady( void )
{
	if ( !NET_P2P_IsEnabled() ) {
		return qfalse;
	}

	if ( NET_P2P_UsesSteamSdrBackend() ) {
		return NET_SDR_IsReady();
	}

	if ( NET_P2P_UsingDirectUdp() ) {
		char address[MAX_STRING_CHARS];
		return NET_P2P_GetLocalAddressString( address, sizeof( address ) );
	}

	return qfalse;
}

const char *NET_P2P_BackendName( void )
{
	if ( !NET_P2P_IsEnabled() ) {
		return "disabled";
	}

	if ( NET_P2P_UsesSteamSdrBackend() ) {
		return "steam_sdr";
	}

	if ( NET_P2P_UsingDirectUdp() ) {
		return "direct_udp";
	}

	return "none";
}

qboolean NET_P2P_GetLocalAddressString( char *buffer, int bufferSize )
{
	uint64_t steamid;
	netadr_t adr;

	if ( !buffer || bufferSize <= 0 ) {
		return qfalse;
	}

	buffer[0] = '\0';

	NET_P2P_RegisterCvars();

	if ( NET_P2P_UsesSteamSdrBackend() ) {
		if ( !NET_SDR_GetLocalSteamID( &steamid ) ) {
			return qfalse;
		}

		Com_sprintf( buffer, bufferSize, "steam:%llu", (unsigned long long)steamid );
		return qtrue;
	}

	if ( !net_p2pAdvertiseAddress || !net_p2pAdvertiseAddress->string[0] ) {
		if ( NET_P2P_NatGetAdvertiseAddress( buffer, bufferSize ) ) {
			return qtrue;
		}
		return qfalse;
	}

	if ( !NET_P2P_NormalizeAddressString( net_p2pAdvertiseAddress->string, buffer, bufferSize ) ) {
		return qfalse;
	}

	if ( !Q_stricmpn( buffer, "udp:", 4 ) ) {
		if ( !NET_StringToAdr( buffer + 4, &adr, NA_UNSPEC ) ) {
			buffer[0] = '\0';
			return qfalse;
		}
	}

	return qtrue;
}

qboolean NET_P2P_IsAddressString( const char *address )
{
	return (qboolean)( address &&
		address[0] &&
		( !Q_stricmpn( address, "steam:", 6 ) || !Q_stricmpn( address, "udp:", 4 ) ) );
}

qboolean NET_P2P_NormalizeAddressString( const char *address, char *buffer, int bufferSize )
{
	uint64_t steamid;
	netadr_t adr;

	if ( !address || !address[0] || !buffer || bufferSize <= 0 ) {
		return qfalse;
	}

	buffer[0] = '\0';

	if ( !Q_stricmpn( address, "steam:", 6 ) ) {
		address += 6;
		if ( sscanf( address, "%" SCNu64, &steamid ) == 1 && steamid != 0 ) {
			Com_sprintf( buffer, bufferSize, "steam:%" PRIu64, steamid );
			return qtrue;
		}
		return qfalse;
	}

	if ( address[0] >= '0' && address[0] <= '9' ) {
		const char *p = address;
		int digits = 0;

		while ( *p >= '0' && *p <= '9' ) {
			digits++;
			p++;
		}

		/* Bare SteamID64 only; reject host:port fragments like "192.168...". */
		if ( digits >= 15 && *p == '\0' && sscanf( address, "%" SCNu64, &steamid ) == 1 && steamid != 0 ) {
			Com_sprintf( buffer, bufferSize, "steam:%" PRIu64, steamid );
			return qtrue;
		}
	}

	if ( !Q_stricmpn( address, "udp:", 4 ) ) {
		address += 4;
	}

	if ( NET_StringToAdr( address, &adr, NA_UNSPEC ) ) {
		Com_sprintf( buffer, bufferSize, "udp:%s", NET_AdrToStringwPort( &adr ) );
		return qtrue;
	}

	return qfalse;
}

void NET_P2P_BeginPunchForAddress( const char *address )
{
	char normalized[MAX_OSPATH];
	netadr_t adr;

	if ( !NET_P2P_IsEnabled() || !NET_P2P_UsingDirectUdp() ) {
		return;
	}

	NET_P2P_RegisterCvars();

	if ( !net_p2pPunch || !net_p2pPunch->integer ) {
		return;
	}

	if ( !NET_P2P_NormalizeAddressString( address, normalized, sizeof( normalized ) ) ) {
		return;
	}

	if ( Q_stricmpn( normalized, "udp:", 4 ) != 0 ) {
		return;
	}

	if ( !NET_P2P_ParseAddress( normalized, &adr ) ) {
		return;
	}

	NET_P2P_EnsurePunchPeer( &adr, normalized, rand() ^ Sys_Milliseconds(), qtrue );
	Com_Printf( "P2P direct_udp: punching %s\n", normalized );
}

void NET_P2P_PrintPunchStatus( void )
{
	int i;
	int active;

	active = 0;
	for ( i = 0; i < P2P_PUNCH_MAX_PEERS; i++ ) {
		p2p_punch_peer_t *peer = &net_p2pPunchPeers[i];

		if ( !peer->active ) {
			continue;
		}

		Com_Printf(
			"punch %d  %s  attempts:%d  ack:%s  lastResponse:%dms\n",
			i,
			peer->addressString,
			peer->attempts,
			peer->acknowledged ? "yes" : "no",
			peer->lastResponseTime ? ( Sys_Milliseconds() - peer->lastResponseTime ) : -1
		);
		active++;
	}

	if ( !active ) {
		Com_Printf( "P2P direct_udp: no active punch peers\n" );
	}
}

qboolean NET_P2P_HandleOobPacket( const netadr_t *from, const char *cmd )
{
	p2p_punch_peer_t *peer;
	int nonce;

	if ( !from || !cmd || !cmd[0] ) {
		return qfalse;
	}

	if ( Q_stricmp( cmd, "p2pPunch" ) != 0 &&
	     Q_stricmp( cmd, "p2pPong" ) != 0 &&
	     Q_stricmp( cmd, "p2pKeepalive" ) != 0 &&
	     Q_stricmp( cmd, "p2pCand" ) != 0 &&
	     Q_stricmp( cmd, "p2pCandRequest" ) != 0 &&
	     Q_stricmp( cmd, "p2pCheck" ) != 0 &&
	     Q_stricmp( cmd, "p2pCheckAck" ) != 0 &&
	     Q_stricmp( cmd, "p2pMigrate" ) != 0 &&
	     Q_stricmp( cmd, "p2pReconnect" ) != 0 ) {
		return qfalse;
	}

	if ( NET_P2P_IceHandleOobPacket( from, cmd ) ) {
		return qtrue;
	}

	if ( Q_stricmp( cmd, "p2pPunch" ) != 0 &&
	     Q_stricmp( cmd, "p2pPong" ) != 0 &&
	     Q_stricmp( cmd, "p2pKeepalive" ) != 0 ) {
		return qfalse;
	}

	nonce = atoi( Cmd_Argv( 1 ) );

	if ( !Q_stricmp( cmd, "p2pPunch" ) ) {
		char addressString[MAX_OSPATH];

		NET_P2P_SendPunchPacket( from, "p2pPong", nonce );
		Com_sprintf( addressString, sizeof( addressString ), "udp:%s", NET_AdrToStringwPort( from ) );
		NET_P2P_EnsurePunchPeer( from, addressString, nonce, qtrue );
		Com_Printf( "P2P direct_udp: inbound punch from %s, starting symmetric session\n", addressString );
		return qtrue;
	}

	peer = NET_P2P_FindPunchPeerByAddress( from );
	if ( !peer ) {
		return qtrue;
	}

	peer->lastResponseTime = Sys_Milliseconds();
	peer->acknowledged = qtrue;

	if ( !Q_stricmp( cmd, "p2pPong" ) ) {
		Com_Printf( "P2P direct_udp: punch acknowledged by %s\n", peer->addressString );
		return qtrue;
	}

	if ( !Q_stricmp( cmd, "p2pKeepalive" ) ) {
		NET_P2P_SendPunchPacket( from, "p2pPong", nonce );
		return qtrue;
	}

	return qfalse;
}

void NET_P2P_PrintIceCandidates( void )
{
	NET_P2P_NatPrintCandidates();
}

void NET_P2P_PrintPathStatus( void )
{
	NET_P2P_IcePrintStatus();
	NET_P2P_PrintPunchStatus();
}

void NET_P2P_BeginMasterList( const char *masterAddress )
{
	NET_P2P_NatBeginMasterList( masterAddress );
}

qboolean NET_P2P_TryHandleNatPacket( const netadr_t *from, const byte *data, int len )
{
	return NET_P2P_NatTryHandlePacket( from, data, len );
}

qboolean NET_P2P_TryHandleBrowseOob( const netadr_t *from, const char *cmd, msg_t *msg )
{
	return NET_P2P_NatTryHandleConnectionless( from, cmd, msg );
}

qboolean NET_P2P_BeginConnectPath( const char *peerAddress )
{
	if ( NET_P2P_UsesSteamSdrBackend() ) {
		Com_Printf( "P2P: using steam_sdr (ICE skipped)\n" );
		return qfalse;
	}

	return NET_P2P_IceBeginConnectPath( peerAddress );
}
