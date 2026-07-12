/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Enhanced server features.
Provides engine-level competitive and quality-of-life improvements:
  - Backward reconciliation (unlagged) history buffer
  - Server pause/unpause
  - Latency equalizer
  - Configurable movement physics cvars
  - Extended voting system cvars
  - Anti-wallhack server support
===========================================================================
*/

#ifndef SV_ENHANCED_H
#define SV_ENHANCED_H

#include "q_shared.h"

#define UNLAGGED_HISTORY_SIZE   64
#define UNLAGGED_MAX_REWIND     1000

typedef struct {
	vec3_t      mins;
	vec3_t      maxs;
	vec3_t      origin;
	int         time;
	qboolean    valid;
} entityHistory_t;

typedef struct {
	entityHistory_t history[MAX_GENTITIES][UNLAGGED_HISTORY_SIZE];
	int             historyHead;
} unlaggedState_t;

void        SV_Enhanced_Init( void );
void        SV_Enhanced_Shutdown( void );

void        SV_Unlagged_Record( int serverTime );
void        SV_Unlagged_Rewind( int clientNum, int targetTime );
void        SV_Unlagged_Restore( void );

void        SV_Pause( void );
void        SV_Unpause( void );
qboolean    SV_IsPaused( void );

#endif /* SV_ENHANCED_H */
