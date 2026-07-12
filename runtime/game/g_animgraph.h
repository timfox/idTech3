#ifndef G_ANIMGRAPH_H
#define G_ANIMGRAPH_H

#include "q_shared.h"

void G_AnimGraph_Init( void );
qboolean G_AnimGraph_Load( const char *path );
void G_AnimGraph_SetState( const char *stateName );
void G_AnimGraph_Update( int msec, int *outFrame, int *outOldFrame, float *outBackLerp );

#endif
