/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#ifndef CL_UBER_EFFECTS_H
#define CL_UBER_EFFECTS_H

#include "q_shared.h"

void CL_UberEffects_Init( void );
int CL_UberEffects_Load( const char *path );
qboolean CL_UberEffects_Emit( const char *name, float x, float y, float z );

#endif
