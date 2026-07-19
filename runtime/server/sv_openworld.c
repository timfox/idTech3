/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "server.h"
#include "sv_openworld.h"
#include "cm_stream.h"
#include "../game/bg_public.h"
#include "../world/world_residency.h"

static cvar_t *sv_openWorld;
static cvar_t *sv_openWorldCollision;
static cvar_t *sv_openWorldSync;
static char sv_openWorldSectorList[256];
static qboolean sv_openWorldClassicMap;

static qboolean SV_OpenWorld_MapIsClassic( const char *mapname ) {
	if ( !mapname || !mapname[0] ) {
		return qtrue;
	}
	return Q_stricmpn( mapname, "sector_", 7 ) != 0;
}

/*
===============
SV_OpenWorld_OnMapLoad
===============
*/
void SV_OpenWorld_OnMapLoad( const char *mapname ) {
	sv_openWorldClassicMap = SV_OpenWorld_MapIsClassic( mapname );
	sv_openWorldSectorList[0] = '\0';
	SV_SetConfigstring( CS_ENGINE_OPENWORLD_SECTORS, "" );

	if ( !sv_openWorldClassicMap ) {
		return;
	}

	/* Classic hub BSPs own the world. Streaming only augments sector_* maps
	 * (or classic maps when r_openWorld is explicitly enabled). */
	if ( !Cvar_VariableIntegerValue( "r_openWorld" ) &&
		!Cvar_VariableIntegerValue( "com_openWorldSmoke" ) ) {
		if ( Cvar_VariableIntegerValue( "r_bspStream" ) ) {
			Cvar_Set( "r_bspStream", "0" );
			Com_Printf( S_COLOR_YELLOW
				"[sv_openworld] classic map %s: forced r_bspStream 0 (hub BSP ownership)\n",
				mapname );
		}
		CM_Stream_Merge_ClearAll();
	}

	if ( sv_openWorld && sv_openWorld->integer &&
		!Cvar_VariableIntegerValue( "r_openWorld" ) &&
		!Cvar_VariableIntegerValue( "com_openWorldSmoke" ) ) {
		Com_Printf( S_COLOR_YELLOW
			"[sv_openworld] ignoring sv_openWorld on classic map %s (set r_openWorld 1 for sector overlays)\n",
			mapname );
	}
}

/*
===============
SV_OpenWorld_Init
===============
*/
void SV_OpenWorld_Init( void ) {
	sv_openWorld = Cvar_Get( "sv_openWorld", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_openWorld,
		"View-driven sector collision residency on dedicated server (cm_stream merge)." );
	sv_openWorldCollision = Cvar_Get( "sv_openWorldCollision", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_openWorldCollision,
		"When sv_openWorld 1, merge sector BSP collision around connected players." );
	sv_openWorldSync = Cvar_Get( "sv_openWorldSync", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( sv_openWorldSync,
		"Publish loaded sector cells to clients via CS_ENGINE_OPENWORLD_SECTORS (collision authority; clients also load nav when r_openWorldNav 1)." );
	(void)Cvar_Get( "sv_openWorldResidency", "0", CVAR_ARCHIVE );
	WorldResidency_Init();
	sv_openWorldSectorList[0] = '\0';
	SV_SetConfigstring( CS_ENGINE_OPENWORLD_SECTORS, "" );
	Com_Printf( "[sv_openworld] sector collision streaming initialized (sv_openWorld 0)\n" );
}

static void SV_OpenWorld_SyncConfigstring( void ) {
	char list[256];

	if ( !sv_openWorldSync || !sv_openWorldSync->integer ) {
		return;
	}

	CM_Stream_BuildLoadedList( list, sizeof( list ) );
	if ( !strcmp( list, sv_openWorldSectorList ) ) {
		return;
	}
	Q_strncpyz( sv_openWorldSectorList, list, sizeof( sv_openWorldSectorList ) );
	SV_SetConfigstring( CS_ENGINE_OPENWORLD_SECTORS, sv_openWorldSectorList );
	Com_DPrintf( "[sv_openworld] sector sync -> %s\n", sv_openWorldSectorList );
}

/*
===============
SV_OpenWorld_Frame

Updates cm_stream residency from each active in-game client origin.
===============
*/
void SV_OpenWorld_Frame( void ) {
	int i;
	int originCount = 0;
	vec3_t origins[MAX_CLIENTS];
	float sectorSize;
	float radius;

	if ( !sv_openWorld || !sv_openWorld->integer || !com_sv_running->integer ) {
		return;
	}
	if ( sv_openWorldClassicMap &&
		!Cvar_VariableIntegerValue( "r_openWorld" ) &&
		!Cvar_VariableIntegerValue( "com_openWorldSmoke" ) ) {
		return;
	}
	if ( !Cvar_VariableIntegerValue( "cm_stream" ) ) {
		return;
	}

	sectorSize = Cvar_VariableValue( "r_openWorldSectorSize" );
	if ( sectorSize < 256.0f ) {
		sectorSize = 4096.0f;
	}
	radius = Cvar_VariableValue( "r_openWorldRadius" );
	if ( radius <= 0.0f ) {
		radius = 12288.0f;
	}

	for ( i = 0; i < sv.maxclients && originCount < MAX_CLIENTS; i++ ) {
		client_t *cl = &svs.clients[i];

		if ( cl->state != CS_ACTIVE || !cl->gentity ) {
			continue;
		}
		VectorCopy( cl->gentity->r.currentOrigin, origins[originCount] );
		originCount++;
	}

	if ( originCount <= 0 ) {
		return;
	}

	if ( WorldResidency_ServerEnabled() && sv_openWorldCollision && sv_openWorldCollision->integer ) {
		WorldResidency_UpdateServerOrigins( origins, originCount, radius );
	} else {
		qboolean mergeCollision = sv_openWorldCollision && sv_openWorldCollision->integer;
		for ( i = 0; i < originCount; i++ ) {
			CM_Stream_UpdateView( origins[i], radius, sectorSize, mergeCollision );
		}
	}

	SV_OpenWorld_SyncConfigstring();
}
