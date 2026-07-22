/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Babble runtime: load graphs, drive EngineDialogue + optional voice.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "babble.h"
#include "com_loc.h"
#include "g_engine_systems.h"

#ifdef USE_BABBLE

static babbleGraph_t s_graphs[BABBLE_MAX_GRAPHS];
static int s_graphCount;
static cvar_t *g_babble;
static int s_activeGraph = -1;
static int s_activeNode = -1;
static float s_nodeEndTime;

extern sfxHandle_t S_RegisterSound( const char *name, qboolean compressed );
extern void S_StartLocalSound( sfxHandle_t sfx, int channelNum );
extern void S_Mixer_NotifyVoiceActive( void );

void Babble_Init( void ) {
	Com_Memset( s_graphs, 0, sizeof( s_graphs ) );
	s_graphCount = 0;
	s_activeGraph = -1;
	s_activeNode = -1;
	g_babble = Cvar_Get( "g_babble", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( g_babble, "Enable clean-room Babble dialogue graphs (USE_BABBLE)." );
	Com_Printf( "Babble: runtime ready (g_babble=%d)\n", g_babble->integer );
}

void Babble_Shutdown( void ) {
	s_graphCount = 0;
	s_activeGraph = -1;
	s_activeNode = -1;
}

int Babble_LoadFile( const char *path ) {
	void *buf;
	int len;
	char err[128];
	babbleGraph_t tmp[BABBLE_MAX_GRAPHS];
	int n, i, j;

	if ( !g_babble || !g_babble->integer ) {
		Com_Printf( S_COLOR_YELLOW "Babble: disabled (g_babble 0)\n" );
		return 0;
	}
	if ( !path || strstr( path, ".." ) ) {
		Com_Printf( S_COLOR_YELLOW "Babble: rejected path\n" );
		return 0;
	}

	len = FS_ReadFile( path, &buf );
	if ( len <= 0 || !buf ) {
		Com_Printf( S_COLOR_YELLOW "Babble: failed to read %s\n", path );
		return 0;
	}

	n = Babble_ParseBuffer( (const char *)buf, len, tmp, BABBLE_MAX_GRAPHS, err, sizeof( err ) );
	FS_FreeFile( buf );
	if ( n <= 0 ) {
		Com_Printf( S_COLOR_YELLOW "Babble: parse failed (%s): %s\n", path, err[0] ? err : "unknown" );
		return 0;
	}

	for ( i = 0; i < n; i++ ) {
		qboolean replaced = qfalse;
		for ( j = 0; j < s_graphCount; j++ ) {
			if ( !Q_stricmp( s_graphs[j].name, tmp[i].name ) ) {
				s_graphs[j] = tmp[i];
				replaced = qtrue;
				break;
			}
		}
		if ( !replaced ) {
			if ( s_graphCount >= BABBLE_MAX_GRAPHS ) {
				Com_Printf( S_COLOR_YELLOW "Babble: graph table full\n" );
				break;
			}
			s_graphs[s_graphCount++] = tmp[i];
		}
	}
	Com_Printf( "Babble: loaded %d graph(s) from %s (total %d)\n", n, path, s_graphCount );
	return n;
}

int Babble_GraphCount( void ) {
	return s_graphCount;
}

const babbleGraph_t *Babble_FindGraph( const char *name ) {
	int i;
	if ( !name ) {
		return NULL;
	}
	for ( i = 0; i < s_graphCount; i++ ) {
		if ( s_graphs[i].used && !Q_stricmp( s_graphs[i].name, name ) ) {
			return &s_graphs[i];
		}
	}
	return NULL;
}

const babbleNode_t *Babble_FindNode( const babbleGraph_t *g, const char *nodeName ) {
	int i;
	if ( !g || !nodeName ) {
		return NULL;
	}
	for ( i = 0; i < g->numNodes; i++ ) {
		if ( g->nodes[i].used && !Q_stricmp( g->nodes[i].name, nodeName ) ) {
			return &g->nodes[i];
		}
	}
	return NULL;
}

static void Babble_PresentNode( const babbleNode_t *node ) {
	char text[COM_LOC_VALUE_SIZE];
	int idx;
	int c;

	if ( !node ) {
		return;
	}
	if ( node->locKey[0] ) {
		Com_Loc_Lookup( node->locKey, text, sizeof( text ) );
	} else {
		Q_strncpyz( text, node->name, sizeof( text ) );
	}

	idx = EngineDialogue_StartEx( node->speaker, text, node->locKey, node->voice, node->duration );
	if ( idx >= 0 ) {
		for ( c = 0; c < node->numChoices; c++ ) {
			char label[BABBLE_CHOICE_LABEL];
			if ( node->choices[c].labelLoc[0] ) {
				Com_Loc_Lookup( node->choices[c].labelLoc, label, sizeof( label ) );
			} else {
				Q_strncpyz( label, node->choices[c].label, sizeof( label ) );
			}
			EngineDialogue_AddChoice( idx, label, node->choices[c].nextNode );
		}
	}

	if ( node->voice[0] ) {
		sfxHandle_t sfx = S_RegisterSound( node->voice, qfalse );
		if ( sfx ) {
			S_StartLocalSound( sfx, 0 );
			S_Mixer_NotifyVoiceActive();
		}
	}
	s_nodeEndTime = (float)Sys_Milliseconds() * 0.001f + node->duration;
}

int Babble_Start( const char *graphName ) {
	const babbleGraph_t *g;
	const babbleNode_t *n;
	int i;

	if ( !g_babble || !g_babble->integer ) {
		return -1;
	}
	g = Babble_FindGraph( graphName );
	if ( !g ) {
		Com_Printf( S_COLOR_YELLOW "Babble: unknown graph '%s'\n", graphName ? graphName : "" );
		return -1;
	}
	n = Babble_FindNode( g, g->startNode );
	if ( !n ) {
		Com_Printf( S_COLOR_YELLOW "Babble: graph '%s' missing start node\n", g->name );
		return -1;
	}
	for ( i = 0; i < s_graphCount; i++ ) {
		if ( &s_graphs[i] == g ) {
			s_activeGraph = i;
			break;
		}
	}
	s_activeNode = (int)( n - g->nodes );
	EngineDialogue_Clear();
	Babble_PresentNode( n );
	return s_activeGraph;
}

int Babble_Advance( int choiceIndex ) {
	const babbleGraph_t *g;
	const babbleNode_t *cur;
	const babbleNode_t *next;
	const char *nextName;

	if ( s_activeGraph < 0 || s_activeGraph >= s_graphCount ) {
		return -1;
	}
	g = &s_graphs[s_activeGraph];
	if ( s_activeNode < 0 || s_activeNode >= g->numNodes ) {
		return -1;
	}
	cur = &g->nodes[s_activeNode];
	nextName = NULL;
	if ( choiceIndex >= 0 && choiceIndex < cur->numChoices ) {
		nextName = cur->choices[choiceIndex].nextNode;
	} else if ( cur->nextNode[0] ) {
		nextName = cur->nextNode;
	}
	if ( !nextName || !nextName[0] ) {
		Babble_Stop();
		return 0;
	}
	next = Babble_FindNode( g, nextName );
	if ( !next ) {
		Com_Printf( S_COLOR_YELLOW "Babble: missing node '%s'\n", nextName );
		Babble_Stop();
		return -1;
	}
	s_activeNode = (int)( next - g->nodes );
	EngineDialogue_Clear();
	Babble_PresentNode( next );
	return 1;
}

void Babble_Stop( void ) {
	s_activeGraph = -1;
	s_activeNode = -1;
	EngineDialogue_Clear();
}

qboolean Babble_IsActive( void ) {
	return ( s_activeGraph >= 0 ) ? qtrue : qfalse;
}

const babbleNode_t *Babble_CurrentNode( void ) {
	if ( s_activeGraph < 0 || s_activeNode < 0 ) {
		return NULL;
	}
	return &s_graphs[s_activeGraph].nodes[s_activeNode];
}

const char *Babble_CurrentGraphName( void ) {
	if ( s_activeGraph < 0 ) {
		return "";
	}
	return s_graphs[s_activeGraph].name;
}

#else /* !USE_BABBLE */

void Babble_Init( void ) {}
void Babble_Shutdown( void ) {}
int Babble_LoadFile( const char *path ) { (void)path; return 0; }
int Babble_GraphCount( void ) { return 0; }
const babbleGraph_t *Babble_FindGraph( const char *name ) { (void)name; return NULL; }
const babbleNode_t *Babble_FindNode( const babbleGraph_t *g, const char *nodeName ) {
	(void)g; (void)nodeName; return NULL;
}
int Babble_Start( const char *graphName ) { (void)graphName; return -1; }
int Babble_Advance( int choiceIndex ) { (void)choiceIndex; return -1; }
void Babble_Stop( void ) {}
qboolean Babble_IsActive( void ) { return qfalse; }
const babbleNode_t *Babble_CurrentNode( void ) { return NULL; }
const char *Babble_CurrentGraphName( void ) { return ""; }

#endif /* USE_BABBLE */
