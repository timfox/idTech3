/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Steam platform integration implementation.

When USE_STEAM is defined and the Steamworks SDK is available, this
module provides full Steam API integration. Otherwise, all functions
are no-ops that allow the engine to run without Steam.

Steam Deck detection uses the SteamUtils()->IsSteamRunningOnSteamDeck()
API, with fallback to the SteamDeck environment variable.
===========================================================================
*/

#include "client.h"
#include "cl_steam.h"
#ifdef USE_STEAM_NETWORKING
#include "../qcommon/net_sdr.h"
#endif

static cvar_t *in_steamDeck;
static cvar_t *cl_steamOverlay;
static cvar_t *cl_steamRichPresence;
static qboolean steamInitialized UNUSED_VAR = qfalse;
static qboolean steamDeckDetected = qfalse;

#ifdef USE_STEAM
#include <steam/steam_api.h>

void Steam_Init( void ) {
	in_steamDeck = Cvar_Get( "in_steamDeck", "0", CVAR_ROM );
	cl_steamOverlay = Cvar_Get( "cl_steamOverlay", "1", CVAR_ARCHIVE );
	cl_steamRichPresence = Cvar_Get( "cl_steamRichPresence", "1", CVAR_ARCHIVE );

	if ( !SteamAPI_Init() ) {
		Com_Printf( "Steam: SteamAPI_Init failed (running without Steam)\n" );
		return;
	}

	steamInitialized = qtrue;
#ifdef USE_STEAM_NETWORKING
	NET_SDR_OnSteamReady();
#endif

	steamDeckDetected = SteamUtils()->IsSteamRunningOnSteamDeck();
	if ( steamDeckDetected ) {
		Cvar_Set( "in_steamDeck", "1" );
		Com_Printf( "Steam Deck detected — loading steamdeck.cfg\n" );
		Cbuf_AddText( "exec steamdeck.cfg\n" );
	}

	Com_Printf( "Steam: initialized (user: %s, deck: %s)\n",
		SteamFriends()->GetPersonaName(),
		steamDeckDetected ? "yes" : "no" );
}

void Steam_Shutdown( void ) {
	if ( steamInitialized ) {
		SteamAPI_Shutdown();
		steamInitialized = qfalse;
	}
}

void Steam_Frame( void ) {
	if ( !steamInitialized ) return;
	SteamAPI_RunCallbacks();
}

qboolean Steam_IsInitialized( void ) { return steamInitialized; }
qboolean Steam_IsSteamDeck( void ) { return steamDeckDetected; }

qboolean Steam_IsOverlayActive( void ) {
	return steamInitialized ? SteamUtils()->IsOverlayEnabled() : qfalse;
}

uint64_t Steam_GetSteamID( void ) {
	return steamInitialized ? SteamUser()->GetSteamID().ConvertToUint64() : 0;
}

const char *Steam_GetPersonaName( void ) {
	return steamInitialized ? SteamFriends()->GetPersonaName() : "";
}

void Steam_SetAchievement( const char *name ) {
	if ( !steamInitialized || !name ) return;
	SteamUserStats()->SetAchievement( name );
	SteamUserStats()->StoreStats();
}

void Steam_SetRichPresence( const char *key, const char *value ) {
	if ( !steamInitialized || !cl_steamRichPresence || !cl_steamRichPresence->integer ) return;
	SteamFriends()->SetRichPresence( key, value );
}

#else /* !USE_STEAM */

void Steam_Init( void ) {
	in_steamDeck = Cvar_Get( "in_steamDeck", "0", CVAR_ARCHIVE );
	cl_steamOverlay = Cvar_Get( "cl_steamOverlay", "1", CVAR_ARCHIVE );
	cl_steamRichPresence = Cvar_Get( "cl_steamRichPresence", "1", CVAR_ARCHIVE );

	Cvar_SetDescription( in_steamDeck, "Steam Deck mode (0 = off, 1 = on). Auto-detected or set manually." );
	Cvar_SetDescription( cl_steamOverlay, "Allow Steam overlay (0 = disabled, 1 = enabled)." );

	/* Environment-based Steam Deck detection */
	{
		const char *deckEnv = getenv( "SteamDeck" );
		const char *gamescope = getenv( "GAMESCOPE_WAYLAND_DISPLAY" );

		if ( ( deckEnv && deckEnv[0] == '1' ) || gamescope ) {
			steamDeckDetected = qtrue;
			Cvar_Set( "in_steamDeck", "1" );
			Com_Printf( "Steam Deck detected (environment) — loading steamdeck.cfg\n" );
			Cbuf_AddText( "exec steamdeck.cfg\n" );
		}
	}

	Com_Printf( "Steam: not compiled (USE_STEAM not defined), deck: %s\n",
		steamDeckDetected ? "yes (env)" : "no" );
}

void Steam_Shutdown( void ) {}
void Steam_Frame( void ) {}
qboolean Steam_IsInitialized( void ) { return qfalse; }
qboolean Steam_IsSteamDeck( void ) { return steamDeckDetected; }
qboolean Steam_IsOverlayActive( void ) { return qfalse; }
uint64_t Steam_GetSteamID( void ) { return 0; }
const char *Steam_GetPersonaName( void ) { return ""; }
void Steam_SetAchievement( const char *name ) { (void)name; }
void Steam_SetRichPresence( const char *key, const char *value ) { (void)key; (void)value; }

#endif /* USE_STEAM */
