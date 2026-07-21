/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Shared Steam bootstrap used by both client and dedicated runtime paths.
===========================================================================
*/

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"
#include "qcommon.h"
#include "steam_shared.h"

#ifdef __cplusplus
}
#endif

#if ( defined(USE_STEAM) || defined(USE_STEAM_NETWORKING) ) && defined(STEAMWORKS_AVAILABLE) && STEAMWORKS_AVAILABLE
#include <steam/steam_api.h>
#ifdef USE_STEAM_NETWORKING
#ifdef __cplusplus
extern "C" {
#endif
#include "net_sdr.h"
#ifdef __cplusplus
}
#endif
#endif

static qboolean steamSharedInitialized = qfalse;

qboolean SteamShared_Init( void )
{
	if ( steamSharedInitialized ) {
		return qtrue;
	}

	if ( !SteamAPI_Init() ) {
		Com_Printf( "Steam: SteamAPI_Init failed (running without Steam)\n" );
		return qfalse;
	}

	steamSharedInitialized = qtrue;
#ifdef USE_STEAM_NETWORKING
	NET_SDR_OnSteamReady();
#endif
	Com_Printf( "Steam: shared runtime initialized\n" );
	return qtrue;
}

void SteamShared_Shutdown( void )
{
	if ( !steamSharedInitialized ) {
		return;
	}

	SteamAPI_Shutdown();
	steamSharedInitialized = qfalse;
}

void SteamShared_Frame( void )
{
	if ( !steamSharedInitialized ) {
		return;
	}

	SteamAPI_RunCallbacks();
}

qboolean SteamShared_IsInitialized( void )
{
	return steamSharedInitialized;
}

uint64_t SteamShared_GetSteamID( void )
{
	if ( !steamSharedInitialized || !SteamUser() ) {
		return 0;
	}

	return SteamUser()->GetSteamID().ConvertToUint64();
}

const char *SteamShared_GetPersonaName( void )
{
	if ( !steamSharedInitialized || !SteamFriends() ) {
		return "";
	}

	return SteamFriends()->GetPersonaName();
}

#else

qboolean SteamShared_Init( void )
{
	return qfalse;
}

void SteamShared_Shutdown( void )
{
}

void SteamShared_Frame( void )
{
}

qboolean SteamShared_IsInitialized( void )
{
	return qfalse;
}

uint64_t SteamShared_GetSteamID( void )
{
	return 0;
}

const char *SteamShared_GetPersonaName( void )
{
	return "";
}

#endif
