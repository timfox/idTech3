/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Optional peer-to-peer networking facade.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "net_p2p.h"
#include "net_sdr.h"

void NET_P2P_Init( void )
{
}

void NET_P2P_Shutdown( void )
{
}

void NET_P2P_Frame( void )
{
}

qboolean NET_P2P_IsSupported( void )
{
#if defined(USE_STEAM_NETWORKING) && defined(STEAMWORKS_AVAILABLE) && STEAMWORKS_AVAILABLE
	return qtrue;
#else
	return qfalse;
#endif
}

qboolean NET_P2P_IsEnabled( void )
{
	cvar_t *netP2p;

	if ( !NET_P2P_IsSupported() ) {
		return qfalse;
	}

	netP2p = Cvar_Get( "net_p2p", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	return ( netP2p && netP2p->integer ) ? qtrue : qfalse;
}

qboolean NET_P2P_IsReady( void )
{
	if ( !NET_P2P_IsSupported() ) {
		return qfalse;
	}

	return NET_SDR_IsReady();
}

const char *NET_P2P_BackendName( void )
{
	if ( !NET_P2P_IsSupported() ) {
		return "none";
	}

	return "steam_sdr";
}

qboolean NET_P2P_GetLocalAddressString( char *buffer, int bufferSize )
{
	uint64_t steamid;

	if ( !buffer || bufferSize <= 0 ) {
		return qfalse;
	}

	buffer[0] = '\0';

	if ( !NET_SDR_GetLocalSteamID( &steamid ) ) {
		return qfalse;
	}

	Com_sprintf( buffer, bufferSize, "steam:%llu", (unsigned long long)steamid );
	return qtrue;
}
