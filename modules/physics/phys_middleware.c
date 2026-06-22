/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "phys_bullet.h"
#include "phys_events.h"
#include "phys_materials.h"
#include "phys_motor.h"
#include "phys_procedural_anim.h"
#include "phys_middleware.h"

static qboolean middlewareReady;

static void PhysMiddleware_LogImpact( const phys_event_t *ev, void *userData ) {
	(void)userData;
	if ( !ev ) {
		return;
	}
	if ( ev->type == PHYS_EVENT_IMPACT && ev->magnitude > 50.0f ) {
		Com_DPrintf( "PhysEvent impact: mag=%.1f matA=%d matB=%d\n",
			ev->magnitude, ev->matA, ev->matB );
	}
}

static void PhysMiddleware_Status_f( void ) {
	Com_Printf( "Physics middleware status:\n" );
	Com_Printf( "  bodies:       %d\n", Phys_GetBodyCount() );
	Com_Printf( "  constraints:  %d\n", Phys_GetConstraintCount() );
	Com_Printf( "  procAnim:     %d active\n", ProcAnim_GetActiveCount() );
	Com_Printf( "  event queue:  %d pending\n", PhysEvent_QueueDepth() );
}

void PhysMiddleware_RegisterCommands( void ) {
	Cmd_AddCommand( "phys_status", PhysMiddleware_Status_f );
}

void PhysMiddleware_Init( void ) {
	if ( middlewareReady ) {
		return;
	}
	PhysMat_Init();
	PhysEvent_Init();
	PhysMotor_Init();
	PhysEvent_Subscribe( PHYS_EVENT_IMPACT, PhysMiddleware_LogImpact, NULL );
	PhysMiddleware_RegisterCommands();
	middlewareReady = qtrue;
	Com_Printf( "PhysMiddleware: Euphoria/DMM gameplay layer ready\n" );
}

void PhysMiddleware_Shutdown( void ) {
	if ( !middlewareReady ) {
		return;
	}
	PhysEvent_UnsubscribeAll();
	PhysMotor_Shutdown();
	PhysEvent_Shutdown();
	middlewareReady = qfalse;
}

void PhysMiddleware_DispatchHit( int entityNum, procAnimHandle_t anim, physMotorHandle_t motor,
	int bone, int damageType, const vec3_t point, const vec3_t impulse ) {
	phys_hit_event_t hit;
	phys_event_t ev;
	phys_impact_response_t response;
	float mag;

	if ( !middlewareReady ) {
		return;
	}

	PhysEvent_BuildHitFromImpulse( &hit, bone, damageType, point, impulse, 1.0f );
	mag = VectorLength( impulse );

	Com_Memset( &ev, 0, sizeof( ev ) );
	ev.type = PHYS_EVENT_IMPACT;
	ev.entityNum = entityNum;
	ev.hit = hit;
	VectorCopy( point, ev.point );
	VectorCopy( impulse, ev.impulse );
	ev.magnitude = mag;
	PhysEvent_Post( &ev );

	if ( motor >= 0 ) {
		PhysMotor_ApplyHit( motor, &hit );
	} else if ( anim >= 0 ) {
		ProcAnim_ApplyImpact( anim, point, impulse, 24.0f );
	}

	PhysMat_ComputeImpactResponse( PHYS_MAT_FLESH, PHYS_MAT_DEFAULT, mag, 1.0f, &response );
	if ( response.shouldSplash ) {
		phys_event_t splash;
		Com_Memset( &splash, 0, sizeof( splash ) );
		splash.type = PHYS_EVENT_SPLASH;
		VectorCopy( point, splash.point );
		splash.magnitude = response.particleScale;
		PhysEvent_Post( &splash );
	}
}

void PhysMiddleware_Frame( float dt ) {
	if ( !middlewareReady ) {
		return;
	}

	ProcAnim_UpdateAll( dt );
	PhysMotor_UpdateAll( dt );
	PhysEvent_DispatchQueued();
}
