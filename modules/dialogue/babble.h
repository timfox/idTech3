/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Clean-room Babble-compatible dialogue graph interchange (FAKK2/EF2 dialect
notes only). Not derived from Ritual SDK. GPLv2.
===========================================================================
*/

#ifndef BABBLE_H
#define BABBLE_H

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BABBLE_MAX_GRAPHS       64
#define BABBLE_MAX_NODES        256
#define BABBLE_MAX_CHOICES      8
#define BABBLE_SPEAKER_SIZE     64
#define BABBLE_KEY_SIZE         64
#define BABBLE_NODE_NAME_SIZE   64
#define BABBLE_CHOICE_LABEL     128

typedef struct babbleChoice_s {
	char label[BABBLE_CHOICE_LABEL];
	char labelLoc[BABBLE_KEY_SIZE];
	char nextNode[BABBLE_NODE_NAME_SIZE];
} babbleChoice_t;

typedef struct babbleNode_s {
	char name[BABBLE_NODE_NAME_SIZE];
	char speaker[BABBLE_SPEAKER_SIZE];
	char locKey[BABBLE_KEY_SIZE];
	char voice[MAX_QPATH];
	char anim[64];
	char nextNode[BABBLE_NODE_NAME_SIZE];
	float duration;
	float weight;
	float cooldown;
	babbleChoice_t choices[BABBLE_MAX_CHOICES];
	int numChoices;
	qboolean used;
} babbleNode_t;

typedef struct babbleGraph_s {
	char name[BABBLE_NODE_NAME_SIZE];
	char startNode[BABBLE_NODE_NAME_SIZE];
	babbleNode_t nodes[BABBLE_MAX_NODES];
	int numNodes;
	qboolean used;
} babbleGraph_t;

/* Pure parse (no filesystem). Returns number of graphs filled (0 on failure). */
int Babble_ParseBuffer( const char *buf, int bufLen, babbleGraph_t *graphs, int maxGraphs, char *err, int errSize );

/* Runtime (optional USE_BABBLE). */
void Babble_Init( void );
void Babble_Shutdown( void );
int Babble_LoadFile( const char *path );
int Babble_GraphCount( void );
const babbleGraph_t *Babble_FindGraph( const char *name );
const babbleNode_t *Babble_FindNode( const babbleGraph_t *g, const char *nodeName );
int Babble_Start( const char *graphName );
int Babble_Advance( int choiceIndex );
void Babble_Stop( void );
qboolean Babble_IsActive( void );
const babbleNode_t *Babble_CurrentNode( void );
const char *Babble_CurrentGraphName( void );

#ifdef __cplusplus
}
#endif

#endif /* BABBLE_H */
