/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Petri-net models for high-level beta testing (Hernández et al. 2017).
===========================================================================
*/
#pragma once

#include "../qcommon/q_shared.h"

void CL_BetaPetri_Init( void );
void CL_BetaPetri_Shutdown( void );

/* Load beta_traces/<basename>.petrinet.json (optional initial marking in JSON). */
qboolean CL_BetaPetri_Load( const char *basename );
void CL_BetaPetri_Unload( void );

/* Try to fire transitions whose message matches the gameplay event. */
void CL_BetaPetri_OnGameplayEvent( const char *type, const char *source, const char *target );

/* Console: beta_petri_load / beta_petri_status / beta_petri_validate */
void CL_BetaPetri_RegisterCommands( void );
void CL_BetaPetri_RemoveCommands( void );

/* True when a net is loaded and goal place (if any) is marked. */
qboolean CL_BetaPetri_GoalReached( void );

void CL_BetaPetri_SetGoalPlace( const char *placeId );
