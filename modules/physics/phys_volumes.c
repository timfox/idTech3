/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "phys_bullet.h"
#include "phys_events.h"
#include "phys_volumes.h"

#include <math.h>

#define PHYS_VOLUME_TRACK_MAX 32

typedef struct physVolumeSlot_s {
	qboolean         active;
	physVolumeDef_t  def;
	physBodyHandle_t inside[PHYS_VOLUME_TRACK_MAX];
	int              insideCount;
} physVolumeSlot_t;

static physVolumeSlot_t volumes[PHYS_VOLUME_MAX];
static qboolean volumesReady;

static qboolean PhysVolume_ContainsPoint( const physVolumeSlot_t *v, const vec3_t p ) {
	vec3_t d;

	if ( v->def.radius > 0.0f ) {
		VectorSubtract( p, v->def.center, d );
		return ( VectorLength( d ) <= v->def.radius ) ? qtrue : qfalse;
	}
	d[0] = p[0] - v->def.center[0];
	d[1] = p[1] - v->def.center[1];
	d[2] = p[2] - v->def.center[2];
	if ( fabsf( d[0] ) > v->def.halfExtents[0] ) {
		return qfalse;
	}
	if ( fabsf( d[1] ) > v->def.halfExtents[1] ) {
		return qfalse;
	}
	if ( fabsf( d[2] ) > v->def.halfExtents[2] ) {
		return qfalse;
	}
	return qtrue;
}

static int PhysVolume_FindInside( const physVolumeSlot_t *v, physBodyHandle_t body ) {
	int i;

	for ( i = 0; i < v->insideCount; i++ ) {
		if ( v->inside[i] == body ) {
			return i;
		}
	}
	return -1;
}

static void PhysVolume_AddInside( physVolumeSlot_t *v, physBodyHandle_t body ) {
	if ( v->insideCount >= PHYS_VOLUME_TRACK_MAX ) {
		return;
	}
	v->inside[v->insideCount++] = body;
}

static void PhysVolume_RemoveInsideAt( physVolumeSlot_t *v, int idx ) {
	if ( idx < 0 || idx >= v->insideCount ) {
		return;
	}
	v->inside[idx] = v->inside[v->insideCount - 1];
	v->insideCount--;
}

static void PhysVolume_GatherCandidates( const physVolumeSlot_t *v, physBodyHandle_t *out, int maxOut, int *count ) {
	float r;

	*count = 0;
	if ( v->def.radius > 0.0f ) {
		*count = Phys_OverlapSphere( v->def.center, v->def.radius, out, maxOut );
		return;
	}
	r = v->def.halfExtents[0];
	if ( v->def.halfExtents[1] > r ) {
		r = v->def.halfExtents[1];
	}
	if ( v->def.halfExtents[2] > r ) {
		r = v->def.halfExtents[2];
	}
	r *= 1.7320508f; /* cover AABB corners */
	*count = Phys_OverlapSphere( v->def.center, r, out, maxOut );
}

void PhysVolume_Init( void ) {
	if ( volumesReady ) {
		return;
	}
	Com_Memset( volumes, 0, sizeof( volumes ) );
	volumesReady = qtrue;
	Com_Printf( "PhysVolume: buoyancy / drag / impact / motion volumes ready\n" );
}

void PhysVolume_Shutdown( void ) {
	PhysVolume_Clear();
	volumesReady = qfalse;
}

void PhysVolume_Clear( void ) {
	Com_Memset( volumes, 0, sizeof( volumes ) );
}

physVolumeHandle_t PhysVolume_Create( const physVolumeDef_t *def ) {
	int i;

	if ( !def ) {
		return -1;
	}
	if ( !volumesReady ) {
		PhysVolume_Init();
	}
	for ( i = 0; i < PHYS_VOLUME_MAX; i++ ) {
		if ( !volumes[i].active ) {
			volumes[i].active = qtrue;
			volumes[i].def = *def;
			volumes[i].insideCount = 0;
			return i;
		}
	}
	Com_Printf( S_COLOR_YELLOW "PhysVolume: no free volume slots\n" );
	return -1;
}

void PhysVolume_Destroy( physVolumeHandle_t handle ) {
	if ( handle < 0 || handle >= PHYS_VOLUME_MAX || !volumes[handle].active ) {
		return;
	}
	Com_Memset( &volumes[handle], 0, sizeof( volumes[handle] ) );
}

int PhysVolume_GetActiveCount( void ) {
	int i;
	int n = 0;

	for ( i = 0; i < PHYS_VOLUME_MAX; i++ ) {
		if ( volumes[i].active ) {
			n++;
		}
	}
	return n;
}

static void PhysVolume_ApplyBuoyancy( physVolumeSlot_t *v, physBodyHandle_t body, float dt ) {
	physTransform_t xf;
	vec3_t force, zero;
	float depthScale = 1.0f;
	float density;

	(void)dt;
	if ( !Phys_IsBodyDynamic( body ) ) {
		return;
	}
	Phys_GetBodyTransform( body, &xf );
	if ( !PhysVolume_ContainsPoint( v, xf.position ) ) {
		return;
	}
	density = v->def.density > 0.0f ? v->def.density : 1.0f;
	/* Quake Z-up: buoyancy lifts along +Z */
	VectorSet( force, 0.0f, 0.0f, 800.0f * density * depthScale );
	VectorSet( zero, 0.0f, 0.0f, 0.0f );
	Phys_ApplyForce( body, force, zero );
	if ( v->def.linearDrag > 0.0f ) {
		vec3_t drag;
		VectorScale( xf.linearVelocity, -v->def.linearDrag, drag );
		Phys_ApplyForce( body, drag, zero );
	}
	if ( v->def.angularDrag > 0.0f ) {
		vec3_t torque;
		VectorScale( xf.angularVelocity, -v->def.angularDrag, torque );
		Phys_ApplyTorque( body, torque );
	}
}

static void PhysVolume_ApplyDrag( physVolumeSlot_t *v, physBodyHandle_t body ) {
	physTransform_t xf;
	vec3_t drag, torque, zero;

	if ( !Phys_IsBodyDynamic( body ) ) {
		return;
	}
	Phys_GetBodyTransform( body, &xf );
	if ( !PhysVolume_ContainsPoint( v, xf.position ) ) {
		return;
	}
	VectorSet( zero, 0.0f, 0.0f, 0.0f );
	VectorScale( xf.linearVelocity, -v->def.linearDrag, drag );
	Phys_ApplyForce( body, drag, zero );
	VectorScale( xf.angularVelocity, -v->def.angularDrag, torque );
	Phys_ApplyTorque( body, torque );
}

static void PhysVolume_UpdateMotionLike( physVolumeSlot_t *v, qboolean applyImpact ) {
	physBodyHandle_t hits[64];
	int hitCount = 0;
	int i;
	qboolean present[PHYS_VOLUME_TRACK_MAX];

	PhysVolume_GatherCandidates( v, hits, 64, &hitCount );
	Com_Memset( present, 0, sizeof( present ) );

	for ( i = 0; i < hitCount; i++ ) {
		physBodyHandle_t body = hits[i];
		physTransform_t xf;
		int prev;

		if ( !Phys_IsBodyDynamic( body ) ) {
			continue;
		}
		Phys_GetBodyTransform( body, &xf );
		if ( !PhysVolume_ContainsPoint( v, xf.position ) ) {
			continue;
		}

		prev = PhysVolume_FindInside( v, body );
		if ( prev >= 0 ) {
			present[prev] = qtrue;
			continue;
		}

		PhysVolume_AddInside( v, body );
		if ( applyImpact ) {
			float mag = v->def.impulseMagnitude > 0.0f ? v->def.impulseMagnitude : 500.0f;
			float radius = v->def.impulseRadius > 0.0f ? v->def.impulseRadius : 64.0f;
			Phys_ApplyImpulseRadius( v->def.center, radius, mag, 1.0f );
		} else {
			phys_event_t ev;
			Com_Memset( &ev, 0, sizeof( ev ) );
			ev.type = PHYS_EVENT_MOTION_ENTER;
			ev.entityNum = v->def.entityNum;
			ev.bodyA = body;
			VectorCopy( xf.position, ev.point );
			ev.magnitude = VectorLength( xf.linearVelocity );
			PhysEvent_Post( &ev );
		}
		/* newly added is last index */
		if ( v->insideCount > 0 ) {
			present[v->insideCount - 1] = qtrue;
		}
	}

	for ( i = v->insideCount - 1; i >= 0; i-- ) {
		if ( present[i] ) {
			continue;
		}
		if ( !applyImpact ) {
			phys_event_t ev;
			physTransform_t xf;
			Com_Memset( &ev, 0, sizeof( ev ) );
			Phys_GetBodyTransform( v->inside[i], &xf );
			ev.type = PHYS_EVENT_MOTION_EXIT;
			ev.entityNum = v->def.entityNum;
			ev.bodyA = v->inside[i];
			VectorCopy( xf.position, ev.point );
			PhysEvent_Post( &ev );
		}
		PhysVolume_RemoveInsideAt( v, i );
	}
}

void PhysVolume_Frame( float dt ) {
	int i;
	physBodyHandle_t hits[64];
	int hitCount;
	int h;

	if ( !volumesReady ) {
		return;
	}

	for ( i = 0; i < PHYS_VOLUME_MAX; i++ ) {
		physVolumeSlot_t *v = &volumes[i];

		if ( !v->active ) {
			continue;
		}

		switch ( v->def.type ) {
		case PHYS_VOLUME_BUOYANCY:
			PhysVolume_GatherCandidates( v, hits, 64, &hitCount );
			for ( h = 0; h < hitCount; h++ ) {
				PhysVolume_ApplyBuoyancy( v, hits[h], dt );
			}
			break;
		case PHYS_VOLUME_DRAG:
			PhysVolume_GatherCandidates( v, hits, 64, &hitCount );
			for ( h = 0; h < hitCount; h++ ) {
				PhysVolume_ApplyDrag( v, hits[h] );
			}
			break;
		case PHYS_VOLUME_IMPACT:
			PhysVolume_UpdateMotionLike( v, qtrue );
			break;
		case PHYS_VOLUME_MOTION:
			PhysVolume_UpdateMotionLike( v, qfalse );
			break;
		default:
			break;
		}
	}
}
