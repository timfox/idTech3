/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Server-authoritative world config transitions (CS_ENGINE_WORLD_CONFIG).
===========================================================================
*/

#include "server.h"
#include "sv_world_config.h"
#include "../game/bg_public.h"
#include "../world/world_config.h"

static cvar_t *sv_worldConfigEnable;
static cvar_t *sv_worldConfig;
static char sv_worldConfigPayload[128];

static void SV_WorldConfig_Publish( void ) {
	char payload[128];

	if ( !sv_worldConfigEnable || !sv_worldConfigEnable->integer ) {
		SV_SetConfigstring( CS_ENGINE_WORLD_CONFIG, "" );
		sv_worldConfigPayload[0] = '\0';
		return;
	}

	Com_sprintf( payload, sizeof( payload ), "%s %d",
		WorldConfig_GetActive(), WorldConfig_GetGeneration() );
	if ( !strcmp( payload, sv_worldConfigPayload ) ) {
		return;
	}
	Q_strncpyz( sv_worldConfigPayload, payload, sizeof( sv_worldConfigPayload ) );
	SV_SetConfigstring( CS_ENGINE_WORLD_CONFIG, sv_worldConfigPayload );
	Com_Printf( "[sv_world_config] publish -> %s\n", sv_worldConfigPayload );
}

static void SV_WorldConfig_Set_f( void ) {
	const char *name;

	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: world_config <name>\n" );
		WorldConfig_Status();
		return;
	}
	name = Cmd_Argv( 1 );
	Cvar_Set( "sv_worldConfigEnable", "1" );
	Cvar_Set( "r_worldConfigEnable", "1" );
	if ( !WorldConfig_SetActive( name ) ) {
		Com_Printf( S_COLOR_YELLOW "[sv_world_config] failed to set '%s'\n", name );
		return;
	}
	Cvar_Set( "sv_worldConfig", WorldConfig_GetActive() );
	SV_WorldConfig_Publish();
}

static void SV_WorldConfig_List_f( void ) {
	WorldConfig_List();
}

static void SV_WorldConfig_SpawnLayout_f( void ) {
	if ( Cmd_Argc() < 2 ) {
		Com_Printf( "Usage: world_config_spawnlayout <layout>\n" );
		Com_Printf( "Active layout: %s\n", WorldConfig_GetSpawnLayout() );
		return;
	}
	WorldConfig_SetSpawnLayout( Cmd_Argv( 1 ) );
}

static void SV_WorldConfig_Validate_f( void ) {
	char report[4096];
	const char *name = Cmd_Argc() >= 2 ? Cmd_Argv( 1 ) : "all";
	int fails;

	fails = WorldConfig_Validate( name, report, sizeof( report ) );
	Com_Printf( "%s", report );
	Com_Printf( "[sv_world_config] validate %s: %s (%d failure(s))\n",
		name, fails ? "FAIL" : "OK", fails );
}

static void SV_WorldConfig_Load_f( void ) {
	const char *mapname;

	if ( Cmd_Argc() >= 2 ) {
		if ( !WorldConfig_LoadManifestPath( Cmd_Argv( 1 ) ) ) {
			Com_Printf( S_COLOR_YELLOW "[sv_world_config] load failed: %s\n", Cmd_Argv( 1 ) );
		}
		return;
	}
	mapname = Cvar_VariableString( "mapname" );
	if ( !WorldConfig_LoadManifest( mapname ) ) {
		Com_Printf( S_COLOR_YELLOW "[sv_world_config] no world/%s.wcfg\n", mapname );
	}
}

/*
===============
SV_WorldConfig_OnMapLoad
===============
*/
void SV_WorldConfig_OnMapLoad( const char *mapname ) {
	sv_worldConfigPayload[0] = '\0';
	SV_SetConfigstring( CS_ENGINE_WORLD_CONFIG, "" );

	WorldConfig_LoadManifest( mapname );
	if ( sv_worldConfigEnable && sv_worldConfigEnable->integer &&
		sv_worldConfig && sv_worldConfig->string[0] ) {
		WorldConfig_SetActive( sv_worldConfig->string );
		SV_WorldConfig_Publish();
	}
}

/*
===============
SV_WorldConfig_Init
===============
*/
void SV_WorldConfig_Init( void ) {
	/* ARCHIVE only — SERVERINFO budget must keep OA videoflags/voteflags. */
	sv_worldConfigEnable = Cvar_Get( "sv_worldConfigEnable", "0", CVAR_ARCHIVE );
	sv_worldConfigEnable->flags &= ~(int)CVAR_SERVERINFO;
	Cvar_SetDescription( sv_worldConfigEnable,
		"Publish CS_ENGINE_WORLD_CONFIG and authorize named world-config transitions." );
	sv_worldConfig = Cvar_Get( "sv_worldConfig", "default", CVAR_ARCHIVE );
	sv_worldConfig->flags &= ~(int)CVAR_SERVERINFO;
	Cvar_SetDescription( sv_worldConfig,
		"Server-authoritative world config name (geometry/nav/spawns/lighting)." );

	WorldConfig_Init();

	Cmd_AddCommand( "world_config", SV_WorldConfig_Set_f );
	Cmd_AddCommand( "world_config_list", SV_WorldConfig_List_f );
	Cmd_AddCommand( "world_config_spawnlayout", SV_WorldConfig_SpawnLayout_f );
	Cmd_AddCommand( "world_config_validate", SV_WorldConfig_Validate_f );
	Cmd_AddCommand( "world_config_load", SV_WorldConfig_Load_f );

	sv_worldConfigPayload[0] = '\0';
	SV_SetConfigstring( CS_ENGINE_WORLD_CONFIG, "" );
	Com_Printf( "[sv_world_config] world config replication initialized (sv_worldConfigEnable 0)\n" );
}
