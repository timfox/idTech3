/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Automated gameplay beta testing (record / replay / high-level events).
Inspired by Hernández Bécares et al., Entertainment Computing 18 (2017).
===========================================================================
*/
#pragma once

#include "../qcommon/q_shared.h"

void CL_BetaTrace_Init( void );
void CL_BetaTrace_Shutdown( void );

/* Called each client frame (timeout / test completion). */
void CL_BetaTrace_Frame( void );

/* After a usercmd is built for this frame; may replace cmd during replay. */
void CL_BetaTrace_OnUserCmd( usercmd_t *cmd );

/* After map name is known (CA_ACTIVE). */
void CL_BetaTrace_OnMapLoaded( const char *mapname );

/* Log a high-level gameplay event (JSONL line in .betaevt). */
void CL_BetaTrace_LogEvent( const char *type, const char *source, const char *target );

/* Non-zero while replaying a recorded trace (suppresses live input path). */
qboolean CL_BetaTrace_IsReplaying( void );

/* True during beta_play / beta_test (before each cmd is applied). */
qboolean CL_BetaTrace_ShouldSuppressInput( void );
