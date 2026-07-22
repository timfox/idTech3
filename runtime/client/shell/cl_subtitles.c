/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Client subtitle / choice presenter for EngineDialogue + Babble.
===========================================================================
*/

#include "client.h"
#include "../../game/g_engine_systems.h"
#include "cl_subtitles.h"

static cvar_t *cl_subtitles;
static cvar_t *cl_subtitleScale;

void CL_Subtitles_Init( void ) {
	cl_subtitles = Cvar_Get( "cl_subtitles", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cl_subtitles, "Draw EngineDialogue / Babble lines as on-screen subtitles." );
	cl_subtitleScale = Cvar_Get( "cl_subtitleScale", "1.0", CVAR_ARCHIVE_ND );
	Com_Printf( "CL_Subtitles: ready (cl_subtitles=%d)\n", cl_subtitles->integer );
}

void CL_Subtitles_Draw( void ) {
	int n, i, c, choiceCount;
	char speaker[64];
	char text[512];
	char locKey[64];
	float duration;
	float y;
	float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	float dim[4] = { 0.85f, 0.85f, 0.85f, 1.0f };

	if ( !cl_subtitles || !cl_subtitles->integer ) {
		return;
	}
	if ( cls.state != CA_ACTIVE ) {
		return;
	}

	n = EngineDialogue_ActiveCount();
	if ( n <= 0 ) {
		return;
	}

	/* Show the most recent line near the bottom of the virtual 640x480 HUD. */
	i = n - 1;
	if ( !EngineDialogue_Get( i, speaker, sizeof( speaker ), text, sizeof( text ),
			locKey, sizeof( locKey ), &duration, &choiceCount ) ) {
		return;
	}

	y = 400.0f;
	if ( speaker[0] ) {
		SCR_DrawHudString( 64.0f, y, SMALLCHAR_WIDTH * ( cl_subtitleScale ? cl_subtitleScale->value : 1.0f ),
			speaker, color, qfalse, qfalse );
		y += 14.0f;
	}
	SCR_DrawHudString( 64.0f, y, SMALLCHAR_WIDTH * ( cl_subtitleScale ? cl_subtitleScale->value : 1.0f ),
		text, dim, qfalse, qfalse );
	y += 18.0f;

	for ( c = 0; c < choiceCount && c < 8; c++ ) {
		char label[128];
		char nextId[64];
		char line[160];
		if ( !EngineDialogue_GetChoice( i, c, label, sizeof( label ), nextId, sizeof( nextId ) ) ) {
			break;
		}
		Com_sprintf( line, sizeof( line ), "[%d] %s", c + 1, label );
		SCR_DrawHudString( 80.0f, y, SMALLCHAR_WIDTH, line, color, qfalse, qfalse );
		y += 12.0f;
	}
}
