/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Physics middleware — ties Bullet, materials, events, motors, ProcAnim, and
DMM stress into one per-frame update path for gameplay/animation systems.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"
#include "phys_events.h"
#include "phys_procedural_anim.h"
#include "phys_motor.h"

void    PhysMiddleware_Init( void );
void    PhysMiddleware_Shutdown( void );
void    PhysMiddleware_RegisterCommands( void );
void    PhysMiddleware_Frame( float dt );
void    PhysMiddleware_DispatchHit( int entityNum, procAnimHandle_t anim, physMotorHandle_t motor,
	int bone, int damageType, const vec3_t point, const vec3_t impulse );

#ifdef __cplusplus
}
#endif
