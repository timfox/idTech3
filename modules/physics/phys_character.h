/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Kinematic character controller v1 (capsule on Bullet). Does not replace Pmove.
===========================================================================
*/

#ifndef PHYS_CHARACTER_H
#define PHYS_CHARACTER_H

#include "q_shared.h"

int Phys_CharacterCreate( float radius, float height, float stepHeight );
int Phys_CharacterMove( int handle, const float *wishDir, float wishSpeed, qboolean jump );
void Phys_CharacterDestroy( int handle );
void Phys_CharacterGetState( int handle, vec3_t origin, vec3_t velocity, qboolean *grounded );
void Phys_CharacterInit( void );

/*
===============
Phys_PmoveCorrect

Opt-in Soft Step CastMover correction for classic Pmove results.
phys_pmove 0 (default): no-op, returns 0.
When enabled, mutates origin/velocity in place; returns 1 if CastMover ran.
===============
*/
int Phys_PmoveCorrect( vec3_t origin, vec3_t velocity, float radius, float height, float dt );

#endif /* PHYS_CHARACTER_H */
