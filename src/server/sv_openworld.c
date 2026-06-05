/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "server.h"
#include "sv_openworld.h"
#include "../qcommon/cm_stream.h"
#include "../game/bg_public.h"

static cvar_t *sv_openWorld;
static cvar_t *sv_openWorldCollision;
static cvar_t *sv_openWorldSync;
static char sv_openWorldSectorList[256];

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
		"Publish loaded sector cells to clients via CS_ENGINE_OPENWORLD_SECTORS." );
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
	float sectorSize;
	float radius;
	qboolean mergeCollision;

	if ( !sv_openWorld || !sv_openWorld->integer || !com_sv_running->integer ) {
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
	mergeCollision = ( sv_openWorldCollision && sv_openWorldCollision->integer ) ? qtrue : qfalse;

	for ( i = 0; i < sv.maxclients; i++ ) {
		client_t *cl = &svs.clients[i];
		vec3_t origin;

		if ( cl->state != CS_ACTIVE || !cl->gentity ) {
			continue;
		}
		VectorCopy( cl->gentity->r.currentOrigin, origin );
		CM_Stream_UpdateView( origin, radius, sectorSize, mergeCollision );
	}

	SV_OpenWorld_SyncConfigstring();
}
