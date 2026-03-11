/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Defer system: queue callbacks to run on the main thread.
Avoids race conditions when job workers or AI need to update
game state. Workers call Defer_Add(); main thread calls Defer_Flush().

Usage (from job worker):
  static void apply_path_result( void *data ) {
    pathResult_t *r = (pathResult_t *)data;
    Nav_SetAgentPath( r->agentId, r->waypoints, r->count );
    Z_Free( r );
  }
  pathResult_t *r = Z_Malloc( sizeof(*r) );
  ... compute path in job ...
  Defer_Add( apply_path_result, r );

Defer_Flush() runs at start of each Com_Frame, before AI/game logic.
===========================================================================
*/

#ifndef DEFER_H
#define DEFER_H

#include "q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*deferFunc_t)( void *data );

void  Defer_Init( void );
void  Defer_Shutdown( void );

/* Queue a callback to run on main thread. Thread-safe (callable from workers). */
qboolean Defer_Add( deferFunc_t func, void *data );

/* Run all queued callbacks. Must be called from main thread only. */
void  Defer_Flush( void );

/* Returns number of pending deferred callbacks (for debug). */
int   Defer_PendingCount( void );

#ifdef __cplusplus
}
#endif

#endif /* DEFER_H */
