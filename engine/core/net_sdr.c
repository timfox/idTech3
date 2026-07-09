/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Steam Datagram Relay (SDR) integration using Steamworks SDK
ISteamNetworkingSockets for NAT traversal, relay, and encryption.
===========================================================================
*/

#ifndef USE_STEAM_NETWORKING
/* This file is compiled only when USE_STEAM_NETWORKING is defined. */
#else

#include "q_shared.h"
#include "qcommon.h"
#include "net_sdr.h"

#if STEAMWORKS_AVAILABLE
#include <steam/steam_api.h>
#include <steam/isteamnetworkingsockets.h>
#include <steam/isteamnetworkingutils.h>
#endif

#define SDR_RECV_QUEUE_MAX 64

#if !STEAMWORKS_AVAILABLE
/* Stub when Steamworks SDK not found: SDR is no-op, falls through to UDP */
static cvar_t *net_sdr_stub;
static cvar_t *net_p2p_stub;
void NET_SDR_OnSteamReady( void ) {}
void NET_SDR_Init( void ) {
	net_sdr_stub = Cvar_Get( "net_sdr", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	net_p2p_stub = Cvar_Get( "net_p2p", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( net_sdr_stub, "Use Steam Datagram Relay (0=off, 1=on). Alias: net_p2p. Requires Steamworks SDK at build time." );
	Cvar_SetDescription( net_p2p_stub, "Enable optional peer-to-peer transport (0=off, 1=on). Currently backed by Steam SDR when available." );
	Com_Printf( "Steam SDR: stub (Steamworks SDK not found at build; set STEAMWORKS_SDK)\n" );
}
void NET_SDR_Shutdown( void ) {}
void NET_SDR_Frame( void ) {}
qboolean NET_SDR_ReceivePacket( netadr_t *net_from, msg_t *net_message ) { (void)net_from; (void)net_message; return qfalse; }
qboolean NET_SDR_HasPacket( void ) { return qfalse; }
qboolean NET_SDR_SendPacket( netsrc_t sock, int length, const void *data, const netadr_t *to ) { (void)sock; (void)length; (void)data; (void)to; return qfalse; }
qboolean NET_SDR_IsActive( void ) { return qfalse; }
qboolean NET_SDR_UseForAddress( const netadr_t *adr ) { (void)adr; return qfalse; }
qboolean NET_SDR_IsReady( void ) { return qfalse; }
qboolean NET_SDR_GetLocalSteamID( uint64_t *steamid ) { (void)steamid; return qfalse; }
#else

#define SDR_CONN_MAP_MAX 64

typedef struct {
	netadr_t from;
	byte data[MAX_PACKETLEN];
	int len;
} sdr_packet_t;

typedef struct {
	uint64_t steamid;
	HSteamNetConnection conn;
	qboolean in_use;
} sdr_conn_map_t;

static cvar_t *net_sdr;
static cvar_t *net_p2p;
static qboolean sdr_initialized = qfalse;
static qboolean steam_ready = qfalse;
static HSteamListenSocket sdr_listen_socket = k_HSteamListenSocket_Invalid;
static HSteamNetPollGroup sdr_poll_group = k_HSteamNetPollGroup_Invalid;

static sdr_packet_t sdr_recv_queue[SDR_RECV_QUEUE_MAX];
static int sdr_recv_head;
static int sdr_recv_tail;
static int sdr_recv_count;

static sdr_conn_map_t sdr_conn_map[SDR_CONN_MAP_MAX];

static HSteamNetConnection sdr_pending_accept[SDR_CONN_MAP_MAX];
static int sdr_pending_count;
static int net_sdr_modification_count;
static int net_p2p_modification_count;

static void NET_SDR_SyncCvars( void )
{
	if ( !net_sdr || !net_p2p ) {
		return;
	}

	if ( net_p2p->modificationCount != net_p2p_modification_count ) {
		if ( strcmp( net_sdr->string, net_p2p->string ) != 0 ) {
			Cvar_Set( "net_sdr", net_p2p->string );
		}
	} else if ( net_sdr->modificationCount != net_sdr_modification_count ) {
		if ( strcmp( net_p2p->string, net_sdr->string ) != 0 ) {
			Cvar_Set( "net_p2p", net_sdr->string );
		}
	}

	net_sdr_modification_count = net_sdr->modificationCount;
	net_p2p_modification_count = net_p2p->modificationCount;
}

static void STEAM_CALLBACK sdr_connection_status_changed( SteamNetConnectionStatusChangedCallback_t *pInfo )
{
	/* Queue pending connections for AcceptConnection in Frame */
	if ( pInfo->m_eOldState == k_ESteamNetworkingConnectionState_None &&
	     pInfo->m_info.m_eState == k_ESteamNetworkingConnectionState_Connecting ) {
		if ( sdr_pending_count < SDR_CONN_MAP_MAX )
			sdr_pending_accept[sdr_pending_count++] = pInfo->m_hConn;
		else
			SteamNetworkingSockets()->CloseConnection( pInfo->m_hConn, 0, NULL, qfalse );
	}
}

static ISteamNetworkingSockets *SteamNetSockets( void )
{
	if ( !steam_ready || !SteamAPI_IsSteamRunning() )
		return NULL;
	return SteamNetworkingSockets();
}

static void sdr_conn_map_add( uint64_t steamid, HSteamNetConnection conn )
{
	int i;
	for ( i = 0; i < SDR_CONN_MAP_MAX; i++ ) {
		if ( !sdr_conn_map[i].in_use ) {
			sdr_conn_map[i].steamid = steamid;
			sdr_conn_map[i].conn = conn;
			sdr_conn_map[i].in_use = qtrue;
			return;
		}
	}
	Com_Printf( "SDR: connection map full, dropping mapping for steamid %llu\n", (unsigned long long)steamid );
}

static HSteamNetConnection sdr_conn_map_find( uint64_t steamid )
{
	int i;
	for ( i = 0; i < SDR_CONN_MAP_MAX; i++ ) {
		if ( sdr_conn_map[i].in_use && sdr_conn_map[i].steamid == steamid )
			return sdr_conn_map[i].conn;
	}
	return k_HSteamNetConnection_Invalid;
}

static void sdr_conn_map_remove( HSteamNetConnection conn )
{
	int i;
	for ( i = 0; i < SDR_CONN_MAP_MAX; i++ ) {
		if ( sdr_conn_map[i].in_use && sdr_conn_map[i].conn == conn ) {
			sdr_conn_map[i].in_use = qfalse;
			return;
		}
	}
}

static void sdr_queue_packet( const netadr_t *from, const byte *data, int len )
{
	if ( sdr_recv_count >= SDR_RECV_QUEUE_MAX )
		return;
	sdr_packet_t *p = &sdr_recv_queue[sdr_recv_tail];
	p->from = *from;
	if ( len > MAX_PACKETLEN )
		len = MAX_PACKETLEN;
	Com_Memcpy( p->data, data, len );
	p->len = len;
	sdr_recv_tail = ( sdr_recv_tail + 1 ) % SDR_RECV_QUEUE_MAX;
	sdr_recv_count++;
}

void NET_SDR_OnSteamReady( void )
{
	steam_ready = qtrue;
	if ( SteamNetworkingUtils() )
		SteamNetworkingUtils()->SetGlobalCallback( k_ESteamNetworkingConnectionStatusChanged, sdr_connection_status_changed );
	if ( sdr_initialized && net_sdr && net_sdr->integer )
		Com_Printf( "Steam SDR: Steam ready, networking available\n" );
}

void NET_SDR_Init( void )
{
	net_sdr = Cvar_Get( "net_sdr", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	net_p2p = Cvar_Get( "net_p2p", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	Cvar_SetDescription( net_sdr, "Use Steam Datagram Relay for game traffic (0=off, 1=on). Alias: net_p2p. Requires Steamworks SDK and Steam running." );
	Cvar_SetDescription( net_p2p, "Enable optional peer-to-peer transport (0=off, 1=on). Currently backed by Steam SDR." );
	NET_SDR_SyncCvars();

	Com_Memset( sdr_conn_map, 0, sizeof( sdr_conn_map ) );
	sdr_recv_head = sdr_recv_tail = sdr_recv_count = 0;
	sdr_pending_count = 0;

	sdr_initialized = qtrue;
	Com_Printf( "Steam SDR: initialized (net_sdr=%d)\n", net_sdr ? net_sdr->integer : 0 );
}

void NET_SDR_Shutdown( void )
{
	ISteamNetworkingSockets *sockets;

	if ( !sdr_initialized )
		return;

	sockets = SteamNetSockets();
	if ( sockets ) {
		if ( sdr_listen_socket != k_HSteamListenSocket_Invalid ) {
			sockets->CloseListenSocket( sdr_listen_socket );
			sdr_listen_socket = k_HSteamListenSocket_Invalid;
		}
		if ( sdr_poll_group != k_HSteamNetPollGroup_Invalid ) {
			sockets->DestroyPollGroup( sdr_poll_group );
			sdr_poll_group = k_HSteamNetPollGroup_Invalid;
		}
		/* Close all connections in map */
		{
			int i;
			for ( i = 0; i < SDR_CONN_MAP_MAX; i++ ) {
				if ( sdr_conn_map[i].in_use ) {
					sockets->CloseConnection( sdr_conn_map[i].conn, 0, NULL, qfalse );
					sdr_conn_map[i].in_use = qfalse;
				}
			}
		}
	}

	steam_ready = qfalse;
	sdr_initialized = qfalse;
	Com_Printf( "Steam SDR: shutdown\n" );
}

void NET_SDR_Frame( void )
{
	ISteamNetworkingSockets *sockets;
	SteamNetworkingMessage_t *msgs[SDR_RECV_QUEUE_MAX];
	int n;

	NET_SDR_SyncCvars();

	if ( !sdr_initialized || !net_sdr || !net_sdr->integer )
		return;

	sockets = SteamNetSockets();
	if ( !sockets )
		return;

	/* Create listen socket and poll group on first use (server) */
	if ( com_sv_running && com_sv_running->integer &&
	     sdr_listen_socket == k_HSteamListenSocket_Invalid ) {
		sdr_listen_socket = sockets->CreateListenSocketP2P( 0, 0, NULL );
		if ( sdr_listen_socket != k_HSteamListenSocket_Invalid ) {
			sdr_poll_group = sockets->CreatePollGroup();
			if ( sdr_poll_group != k_HSteamNetPollGroup_Invalid )
				sockets->SetListenSocketPollGroup( sdr_listen_socket, sdr_poll_group );
			Com_Printf( "Steam SDR: listen socket created\n" );
		}
	}

	/* Poll for incoming messages */
	if ( sdr_poll_group != k_HSteamNetPollGroup_Invalid ) {
		n = sockets->ReceiveMessagesOnPollGroup( sdr_poll_group, msgs, SDR_RECV_QUEUE_MAX );
		if ( n > 0 ) {
			int i;
			for ( i = 0; i < n; i++ ) {
				SteamNetworkingIdentity_t *identity = &msgs[i]->m_identityPeer;
				netadr_t from;

				if ( identity->m_eType != k_ESteamNetworkingIdentityType_SteamID )
					continue;

				Com_Memset( &from, 0, sizeof( from ) );
				from.type = NA_STEAMID;
				from.ipv.steamid = identity->m_steamID64;
				from.port = 0;

				sdr_queue_packet( &from, (const byte *)msgs[i]->m_pData, msgs[i]->m_cbSize );
				msgs[i]->Release();
			}
		}
	}

	/* Accept pending connections (from callback) */
	while ( sdr_pending_count > 0 && sdr_poll_group != k_HSteamNetPollGroup_Invalid ) {
		HSteamNetConnection conn = sdr_pending_accept[--sdr_pending_count];
		SteamNetworkingIdentity_t identity;
		if ( sockets->GetConnectionIdentity( conn, &identity ) && identity.m_eType == k_ESteamNetworkingIdentityType_SteamID ) {
			if ( sockets->AcceptConnection( conn ) == k_EResultOK )
				sockets->SetConnectionPollGroup( conn, sdr_poll_group );
			sdr_conn_map_add( identity.m_steamID64, conn );
		} else {
			sockets->CloseConnection( conn, 0, NULL, qfalse );
		}
	}

	/* Check for connection state changes (client connections) */
	{
		int i;
		for ( i = 0; i < SDR_CONN_MAP_MAX; i++ ) {
			if ( !sdr_conn_map[i].in_use )
				continue;
			SteamNetConnectionStatus_t status;
			if ( sockets->GetConnectionStatus( sdr_conn_map[i].conn, &status ) ) {
				if ( status.m_eState == k_ESteamNetworkingConnectionState_ClosedByPeer ||
				     status.m_eState == k_ESteamNetworkingConnectionState_ProblemDetectedLocally ) {
					sockets->CloseConnection( sdr_conn_map[i].conn, 0, NULL, qfalse );
					sdr_conn_map[i].in_use = qfalse;
				}
			}
		}
	}
}

qboolean NET_SDR_ReceivePacket( netadr_t *net_from, msg_t *net_message )
{
	if ( !sdr_initialized || sdr_recv_count == 0 )
		return qfalse;

	{
		sdr_packet_t *p = &sdr_recv_queue[sdr_recv_head];
		*net_from = p->from;
		if ( p->len > net_message->maxsize )
			p->len = net_message->maxsize;
		Com_Memcpy( net_message->data, p->data, p->len );
		net_message->cursize = p->len;
		net_message->readcount = 0;
		sdr_recv_head = ( sdr_recv_head + 1 ) % SDR_RECV_QUEUE_MAX;
		sdr_recv_count--;
		return qtrue;
	}
}

qboolean NET_SDR_HasPacket( void )
{
	return sdr_recv_count > 0;
}

qboolean NET_SDR_SendPacket( netsrc_t sock, int length, const void *data, const netadr_t *to )
{
	ISteamNetworkingSockets *sockets;
	HSteamNetConnection conn;
	int64 *outMsgNum;

	NET_SDR_SyncCvars();

	if ( !sdr_initialized || !net_sdr || !net_sdr->integer )
		return qfalse;

	if ( to->type != NA_STEAMID )
		return qfalse;

	sockets = SteamNetSockets();
	if ( !sockets )
		return qfalse;

	conn = sdr_conn_map_find( to->ipv.steamid );

	/* Client: initiate P2P connection if we don't have one */
	if ( conn == k_HSteamNetConnection_Invalid && sock == NS_CLIENT ) {
		SteamNetworkingIdentity_t identity;
		Com_Memset( &identity, 0, sizeof( identity ) );
		identity.m_eType = k_ESteamNetworkingIdentityType_SteamID;
		identity.m_steamID64 = to->ipv.steamid;
		conn = sockets->ConnectP2P( identity, 0, 0, NULL );
		if ( conn != k_HSteamNetConnection_Invalid )
			sdr_conn_map_add( to->ipv.steamid, conn );
	}

	if ( conn == k_HSteamNetConnection_Invalid )
		return qfalse;

	outMsgNum = NULL;
	if ( sockets->SendMessageToConnection( conn, data, length, k_nSteamNetworkingSend_Unreliable, outMsgNum ) != k_EResultOK )
		return qfalse;

	return qtrue;
}

qboolean NET_SDR_IsActive( void )
{
	NET_SDR_SyncCvars();
	return sdr_initialized && net_sdr && net_sdr->integer;
}

qboolean NET_SDR_UseForAddress( const netadr_t *adr )
{
	if ( !NET_SDR_IsActive() )
		return qfalse;
	return adr->type == NA_STEAMID;
}

qboolean NET_SDR_IsReady( void )
{
	uint64_t steamid;

	return NET_SDR_GetLocalSteamID( &steamid );
}

qboolean NET_SDR_GetLocalSteamID( uint64_t *steamid )
{
	if ( !steamid || !NET_SDR_IsActive() || !steam_ready ) {
		return qfalse;
	}

	if ( !SteamAPI_IsSteamRunning() || !SteamUser() ) {
		return qfalse;
	}

	*steamid = SteamUser()->GetSteamID().ConvertToUint64();
	return *steamid != 0 ? qtrue : qfalse;
}

#endif /* STEAMWORKS_AVAILABLE */
#endif /* USE_STEAM_NETWORKING */
