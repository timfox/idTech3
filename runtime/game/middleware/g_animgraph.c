/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Minimal animgraph JSON evaluator (states + clip indices). See docs/ANIMGRAPH.md.
===========================================================================
*/

#include "g_animgraph.h"
#include "qcommon.h"

#define MAX_AG_STATES 32

typedef struct {
	char name[64];
	int clipIndex;
	float blendSec;
} agState_t;

static agState_t s_states[MAX_AG_STATES];
static int s_stateCount;
static int s_current;
static cvar_t *g_animgraph;

void G_AnimGraph_Init( void ) {
	g_animgraph = Cvar_Get( "g_animgraph", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( g_animgraph, "Enable animgraph JSON runtime (chocolate layer)." );
	s_stateCount = 0;
	s_current = 0;
	if ( g_animgraph->integer ) {
		Com_Printf( "[animgraph] g_animgraph=1\n" );
	}
}

qboolean G_AnimGraph_Load( const char *path ) {
	void *buf;
	int len;
	const char *p;

	if ( !g_animgraph || !g_animgraph->integer || !path || !path[0] ) {
		return qfalse;
	}

	len = FS_ReadFile( path, &buf );
	if ( len <= 0 || !buf ) {
		return qfalse;
	}

	s_stateCount = 0;
	p = (const char *)buf;
	while ( 1 ) {
		const char *tok = COM_Parse( &p );
		if ( !tok[0] ) {
			break;
		}
		if ( !Q_stricmp( tok, "state" ) && s_stateCount < MAX_AG_STATES ) {
			agState_t *st = &s_states[s_stateCount++];
			Com_Memset( st, 0, sizeof( *st ) );
			tok = COM_Parse( &p );
			Q_strncpyz( st->name, tok, sizeof( st->name ) );
			tok = COM_Parse( &p );
			st->clipIndex = atoi( tok );
			tok = COM_Parse( &p );
			st->blendSec = (float)atof( tok );
		}
	}

	FS_FreeFile( buf );
	Com_Printf( "[animgraph] loaded %d states from %s\n", s_stateCount, path );
	return s_stateCount > 0 ? qtrue : qfalse;
}

void G_AnimGraph_SetState( const char *stateName ) {
	int i;

	if ( !stateName ) {
		return;
	}
	for ( i = 0; i < s_stateCount; i++ ) {
		if ( !Q_stricmp( s_states[i].name, stateName ) ) {
			s_current = i;
			return;
		}
	}
}

void G_AnimGraph_Update( int msec, int *outFrame, int *outOldFrame, float *outBackLerp ) {
	if ( s_stateCount <= 0 ) {
		return;
	}
	if ( outFrame ) {
		*outFrame = s_states[s_current].clipIndex;
	}
	if ( outOldFrame ) {
		*outOldFrame = s_states[s_current].clipIndex;
	}
	if ( outBackLerp ) {
		*outBackLerp = (float)msec * 0.001f / ( s_states[s_current].blendSec > 0.0f ?
			s_states[s_current].blendSec : 0.2f );
		if ( *outBackLerp > 1.0f ) {
			*outBackLerp = 1.0f;
		}
	}
}
