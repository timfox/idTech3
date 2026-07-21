/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Steam platform integration implementation.

When USE_STEAM is defined and the Steamworks SDK is available, this
module provides full Steam API integration. Otherwise, all functions
are no-ops that allow the engine to run without Steam.

SteamAPI_Init / Shutdown / RunCallbacks live in steam_shared.c
(SteamShared_*). This module owns client-only state: overlay latch,
pause-on-overlay, Steam Input status, and console commands.

Steam Deck detection uses SteamUtils()->IsSteamRunningOnSteamDeck()
API, with fallback to the SteamDeck environment variable.
===========================================================================
*/

#ifdef __cplusplus
extern "C" {
#endif

#include "client.h"
#include "cl_steam.h"
#include "steam_shared.h"
#ifdef USE_STEAM_NETWORKING
#include "net_sdr.h"
#endif

#ifdef __cplusplus
}
#endif

static cvar_t *in_steamDeck;
static cvar_t *cl_steamOverlay;
static cvar_t *cl_steamRichPresence;
static cvar_t *cl_steamPauseOnOverlay;
static qboolean steamDeckDetected = qfalse;
static qboolean steamOverlayActive = qfalse;
static qboolean steamPausedUs = qfalse;
static qboolean steamInputInited = qfalse;
static int steamConnectedControllers = 0;
static qboolean steamCmdsRegistered = qfalse;

static void Steam_Status_f( void );
static void Steam_Achievement_f( void );
static void Steam_AchievementClear_f( void );
static void Steam_RegisterCommands( void );
static void Steam_UnregisterCommands( void );

#ifdef USE_STEAM
#include <steam/steam_api.h>
#include <steam/isteaminput.h>

/*
 * C++ callback object — Steamworks STEAM_CALLBACK requires a class.
 * Compiled as C++ via CMake LANGUAGE CXX when STEAMWORKS_FOUND.
 */
class SteamClientCallbacks {
public:
	SteamClientCallbacks()
		: m_GameOverlayActivated( this, &SteamClientCallbacks::OnGameOverlayActivated )
	{
	}

	STEAM_CALLBACK( SteamClientCallbacks, OnGameOverlayActivated, GameOverlayActivated_t, m_GameOverlayActivated );
};

static SteamClientCallbacks *s_steamCallbacks = NULL;

void SteamClientCallbacks::OnGameOverlayActivated( GameOverlayActivated_t *pCallback )
{
	if ( !pCallback ) {
		return;
	}
	steamOverlayActive = pCallback->m_bActive ? qtrue : qfalse;
	if ( steamOverlayActive ) {
		Com_Printf( "Steam: overlay activated\n" );
	} else {
		Com_Printf( "Steam: overlay deactivated\n" );
	}
}

static void Steam_ApplyOverlayPause( void )
{
	if ( !cl_steamPauseOnOverlay || !cl_steamPauseOnOverlay->integer ) {
		if ( steamPausedUs && cl_paused ) {
			Cvar_Set( "cl_paused", "0" );
			steamPausedUs = qfalse;
		}
		return;
	}
	if ( !cl_steamOverlay || !cl_steamOverlay->integer ) {
		return;
	}

	if ( steamOverlayActive ) {
		if ( !cl_paused || !cl_paused->integer ) {
			Cvar_Set( "cl_paused", "1" );
			steamPausedUs = qtrue;
		}
	} else if ( steamPausedUs ) {
		Cvar_Set( "cl_paused", "0" );
		steamPausedUs = qfalse;
	}
}

static void Steam_UpdateInputStatus( void )
{
	InputHandle_t handles[STEAM_INPUT_MAX_COUNT];
	int n;

	steamConnectedControllers = 0;
	if ( !steamInputInited || !SteamInput() ) {
		return;
	}
	/* RunFrame is optional when Init(true) asked the API to call it; still safe. */
	SteamInput()->RunFrame();
	n = SteamInput()->GetConnectedControllers( handles );
	if ( n < 0 ) {
		n = 0;
	}
	if ( n > STEAM_INPUT_MAX_COUNT ) {
		n = STEAM_INPUT_MAX_COUNT;
	}
	steamConnectedControllers = n;
}

static void Steam_RegisterCommands( void )
{
	if ( steamCmdsRegistered ) {
		return;
	}
	Cmd_AddCommand( "steam_status", Steam_Status_f );
	Cmd_AddCommand( "steam_achievement", Steam_Achievement_f );
	Cmd_AddCommand( "steam_achievement_clear", Steam_AchievementClear_f );
	steamCmdsRegistered = qtrue;
}

static void Steam_UnregisterCommands( void )
{
	if ( !steamCmdsRegistered ) {
		return;
	}
	Cmd_RemoveCommand( "steam_status" );
	Cmd_RemoveCommand( "steam_achievement" );
	Cmd_RemoveCommand( "steam_achievement_clear" );
	steamCmdsRegistered = qfalse;
}

static void Steam_Status_f( void )
{
	Com_Printf( "Steam status:\n" );
	Com_Printf( "  compiled: yes (USE_STEAM)\n" );
	Com_Printf( "  initialized: %s\n", SteamShared_IsInitialized() ? "yes" : "no" );
	Com_Printf( "  persona: %s\n", Steam_GetPersonaName() );
	Com_Printf( "  steamid: %llu\n", (unsigned long long)Steam_GetSteamID() );
	Com_Printf( "  deck: %s\n", steamDeckDetected ? "yes" : "no" );
	Com_Printf( "  overlayActive: %s (cl_steamOverlay=%d)\n",
		steamOverlayActive ? "yes" : "no",
		cl_steamOverlay ? cl_steamOverlay->integer : 0 );
	Com_Printf( "  pauseOnOverlay: %d (pausedUs=%s)\n",
		cl_steamPauseOnOverlay ? cl_steamPauseOnOverlay->integer : 0,
		steamPausedUs ? "yes" : "no" );
	Com_Printf( "  steamInput: %s, controllers=%d\n",
		steamInputInited ? "init" : "off", steamConnectedControllers );
}

static void Steam_Achievement_f( void )
{
	const char *name;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: steam_achievement <api_name>\n" );
		return;
	}
	name = Cmd_Argv( 1 );
	Steam_SetAchievement( name );
	Com_Printf( "Steam: SetAchievement \"%s\"\n", name );
}

static void Steam_AchievementClear_f( void )
{
	const char *name;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: steam_achievement_clear <api_name>\n" );
		return;
	}
	name = Cmd_Argv( 1 );
	Steam_ClearAchievement( name );
	Com_Printf( "Steam: ClearAchievement \"%s\"\n", name );
}

void Steam_Init( void )
{
	in_steamDeck = Cvar_Get( "in_steamDeck", "0", CVAR_ROM );
	cl_steamOverlay = Cvar_Get( "cl_steamOverlay", "1", CVAR_ARCHIVE );
	cl_steamRichPresence = Cvar_Get( "cl_steamRichPresence", "1", CVAR_ARCHIVE );
	cl_steamPauseOnOverlay = Cvar_Get( "cl_steamPauseOnOverlay", "1", CVAR_ARCHIVE );

	Cvar_SetDescription( in_steamDeck, "Steam Deck mode (0 = off, 1 = on). Auto-detected via Steam API." );
	Cvar_SetDescription( cl_steamOverlay, "Allow Steam overlay features (0 = ignore overlay, 1 = honor overlay)." );
	Cvar_SetDescription( cl_steamRichPresence, "Publish Steam rich presence status (0 = off, 1 = on)." );
	Cvar_SetDescription( cl_steamPauseOnOverlay,
		"Pause the client while the Steam overlay is open (0 = never, 1 = pause)." );

	Steam_RegisterCommands();

	if ( !SteamShared_Init() ) {
		Com_Printf( "Steam: client init skipped (shared runtime unavailable)\n" );
		return;
	}

	if ( !s_steamCallbacks ) {
		s_steamCallbacks = new SteamClientCallbacks();
	}

	steamDeckDetected = SteamUtils() && SteamUtils()->IsSteamRunningOnSteamDeck() ? qtrue : qfalse;
	if ( steamDeckDetected ) {
		Cvar_Set( "in_steamDeck", "1" );
		Com_Printf( "Steam Deck detected - loading steamdeck.cfg\n" );
		Cbuf_AddText( "exec steamdeck.cfg\n" );
	}

	steamInputInited = qfalse;
	steamConnectedControllers = 0;
	if ( SteamInput() ) {
		/* true = Steam Input calls RunFrame itself; we still poll status in Steam_Frame. */
		if ( SteamInput()->Init( true ) ) {
			steamInputInited = qtrue;
			Steam_UpdateInputStatus();
		} else {
			Com_Printf( "Steam: SteamInput()->Init failed\n" );
		}
	}

	Com_Printf( "Steam: initialized (user: %s, deck: %s, input: %s)\n",
		SteamFriends() ? SteamFriends()->GetPersonaName() : "?",
		steamDeckDetected ? "yes" : "no",
		steamInputInited ? "yes" : "no" );
}

void Steam_Shutdown( void )
{
	/* Client-only teardown. SteamAPI_Shutdown is owned by SteamShared_Shutdown. */
	Steam_UnregisterCommands();

	if ( steamInputInited && SteamInput() ) {
		SteamInput()->Shutdown();
		steamInputInited = qfalse;
	}
	steamConnectedControllers = 0;

	if ( s_steamCallbacks ) {
		delete s_steamCallbacks;
		s_steamCallbacks = NULL;
	}

	if ( steamPausedUs && cl_paused ) {
		Cvar_Set( "cl_paused", "0" );
	}
	steamPausedUs = qfalse;
	steamOverlayActive = qfalse;
}

void Steam_Frame( void )
{
	if ( !SteamShared_IsInitialized() ) {
		return;
	}
	Steam_ApplyOverlayPause();
	Steam_UpdateInputStatus();
}

qboolean Steam_IsInitialized( void ) { return SteamShared_IsInitialized(); }
qboolean Steam_IsSteamDeck( void ) { return steamDeckDetected; }

qboolean Steam_IsOverlayActive( void )
{
	if ( !cl_steamOverlay || !cl_steamOverlay->integer ) {
		return qfalse;
	}
	return steamOverlayActive;
}

uint64_t Steam_GetSteamID( void )
{
	return SteamShared_GetSteamID();
}

const char *Steam_GetPersonaName( void )
{
	return SteamShared_GetPersonaName();
}

void Steam_SetAchievement( const char *name )
{
	if ( !SteamShared_IsInitialized() || !name || !name[0] || !SteamUserStats() ) {
		return;
	}
	SteamUserStats()->SetAchievement( name );
	SteamUserStats()->StoreStats();
}

void Steam_ClearAchievement( const char *name )
{
	if ( !SteamShared_IsInitialized() || !name || !name[0] || !SteamUserStats() ) {
		return;
	}
	SteamUserStats()->ClearAchievement( name );
	SteamUserStats()->StoreStats();
}

qboolean Steam_GetAchievement( const char *name )
{
	bool achieved = false;

	if ( !SteamShared_IsInitialized() || !name || !name[0] || !SteamUserStats() ) {
		return qfalse;
	}
	if ( !SteamUserStats()->GetAchievement( name, &achieved ) ) {
		return qfalse;
	}
	return achieved ? qtrue : qfalse;
}

void Steam_IndicateAchievementProgress( const char *name, uint32_t current, uint32_t max )
{
	if ( !SteamShared_IsInitialized() || !name || !name[0] || !SteamUserStats() ) {
		return;
	}
	SteamUserStats()->IndicateAchievementProgress( name, current, max );
}

void Steam_SetRichPresence( const char *key, const char *value )
{
	if ( !SteamShared_IsInitialized() || !cl_steamRichPresence || !cl_steamRichPresence->integer ) {
		return;
	}
	if ( !key || !SteamFriends() ) {
		return;
	}
	SteamFriends()->SetRichPresence( key, value ? value : "" );
}

int Steam_GetConnectedControllerCount( void )
{
	return steamConnectedControllers;
}

#else /* !USE_STEAM */

static void Steam_Status_f( void )
{
	Com_Printf( "Steam status:\n" );
	Com_Printf( "  compiled: no (USE_STEAM not defined)\n" );
	Com_Printf( "  deck (env): %s\n", steamDeckDetected ? "yes" : "no" );
}

static void Steam_Achievement_f( void )
{
	Com_Printf( "steam_achievement: Steam not compiled (USE_STEAM)\n" );
}

static void Steam_AchievementClear_f( void )
{
	Com_Printf( "steam_achievement_clear: Steam not compiled (USE_STEAM)\n" );
}

static void Steam_RegisterCommands( void )
{
	if ( steamCmdsRegistered ) {
		return;
	}
	Cmd_AddCommand( "steam_status", Steam_Status_f );
	Cmd_AddCommand( "steam_achievement", Steam_Achievement_f );
	Cmd_AddCommand( "steam_achievement_clear", Steam_AchievementClear_f );
	steamCmdsRegistered = qtrue;
}

static void Steam_UnregisterCommands( void )
{
	if ( !steamCmdsRegistered ) {
		return;
	}
	Cmd_RemoveCommand( "steam_status" );
	Cmd_RemoveCommand( "steam_achievement" );
	Cmd_RemoveCommand( "steam_achievement_clear" );
	steamCmdsRegistered = qfalse;
}

void Steam_Init( void )
{
	in_steamDeck = Cvar_Get( "in_steamDeck", "0", CVAR_ARCHIVE );
	cl_steamOverlay = Cvar_Get( "cl_steamOverlay", "1", CVAR_ARCHIVE );
	cl_steamRichPresence = Cvar_Get( "cl_steamRichPresence", "1", CVAR_ARCHIVE );
	cl_steamPauseOnOverlay = Cvar_Get( "cl_steamPauseOnOverlay", "1", CVAR_ARCHIVE );

	Cvar_SetDescription( in_steamDeck, "Steam Deck mode (0 = off, 1 = on). Auto-detected or set manually." );
	Cvar_SetDescription( cl_steamOverlay, "Allow Steam overlay (0 = disabled, 1 = enabled)." );
	Cvar_SetDescription( cl_steamRichPresence, "Publish Steam rich presence status (0 = off, 1 = on)." );
	Cvar_SetDescription( cl_steamPauseOnOverlay,
		"Pause the client while the Steam overlay is open (0 = never, 1 = pause)." );

	Steam_RegisterCommands();

	/* Environment-based Steam Deck detection */
	{
		const char *deckEnv = getenv( "SteamDeck" );
		const char *gamescope = getenv( "GAMESCOPE_WAYLAND_DISPLAY" );

		if ( ( deckEnv && deckEnv[0] == '1' ) || gamescope ) {
			steamDeckDetected = qtrue;
			Cvar_Set( "in_steamDeck", "1" );
			Com_Printf( "Steam Deck detected (environment) - loading steamdeck.cfg\n" );
			Cbuf_AddText( "exec steamdeck.cfg\n" );
		}
	}

	Com_Printf( "Steam: not compiled (USE_STEAM not defined), deck: %s\n",
		steamDeckDetected ? "yes (env)" : "no" );
}

void Steam_Shutdown( void )
{
	Steam_UnregisterCommands();
}

void Steam_Frame( void ) {}
qboolean Steam_IsInitialized( void ) { return qfalse; }
qboolean Steam_IsSteamDeck( void ) { return steamDeckDetected; }
qboolean Steam_IsOverlayActive( void ) { return qfalse; }
uint64_t Steam_GetSteamID( void ) { return 0; }
const char *Steam_GetPersonaName( void ) { return ""; }
void Steam_SetAchievement( const char *name ) { (void)name; }
void Steam_ClearAchievement( const char *name ) { (void)name; }
qboolean Steam_GetAchievement( const char *name ) { (void)name; return qfalse; }
void Steam_IndicateAchievementProgress( const char *name, uint32_t current, uint32_t max )
{
	(void)name;
	(void)current;
	(void)max;
}
void Steam_SetRichPresence( const char *key, const char *value ) { (void)key; (void)value; }
int Steam_GetConnectedControllerCount( void ) { return 0; }

#endif /* USE_STEAM */
