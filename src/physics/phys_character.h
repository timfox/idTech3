/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Kinematic character controller v1 (capsule on Bullet). Does not replace Pmove.
===========================================================================
*/

#ifndef PHYS_CHARACTER_H
#define PHYS_CHARACTER_H

#include "../qcommon/q_shared.h"

int Phys_CharacterCreate( float radius, float height, float stepHeight );
int Phys_CharacterMove( int handle, const float *wishDir, float wishSpeed, qboolean jump );
void Phys_CharacterDestroy( int handle );
void Phys_CharacterGetState( int handle, vec3_t origin, vec3_t velocity, qboolean *grounded );
void Phys_CharacterInit( void );

#endif /* PHYS_CHARACTER_H */
