/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

OSP2-BE-inspired client features.
Provides engine-level visual enhancements compatible with the
OSP2-BE mod's cvar namespace. Features work standalone or alongside
a compatible cgame module.

Inspired by: OSP2 by Snems, OSP2-BE by diwoc
===========================================================================
*/

#ifndef CL_OSP_H
#define CL_OSP_H

#include "../qcommon/q_shared.h"

void    CL_OSP_Init( void );
void    CL_OSP_Shutdown( void );

void    CL_OSP_DamageFrame( int x, int y, int w, int h );

void    CL_OSP_NotifyDamage( float yaw, int damage );

void    CL_OSP_NotifyHit( int damage );

void    CL_OSP_DrawCrosshairHitFeedback( int x, int y, float size );

void    CL_OSP_DrawCountdown( int x, int y, int secondsLeft );

void    CL_OSP_Frame( float frametime );

#endif /* CL_OSP_H */
