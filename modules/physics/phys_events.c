/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "phys_events.h"

typedef struct {
	phys_event_type_t   type;
	PhysEventHandler_fn fn;
	void               *userData;
} physEventHandlerEntry_t;

static phys_event_t           eventQueue[PHYS_EVENT_MAX_QUEUE];
static int                      queueHead;
static int                      queueTail;
static physEventHandlerEntry_t  handlers[PHYS_EVENT_MAX_HANDLERS];
static int                      handlerCount;
static qboolean                 eventInitialized;

static cvar_t *phys_events;

void PhysEvent_RegisterCvars( void ) {
	phys_events = Cvar_Get( "phys_events", "1", CVAR_ARCHIVE );
}

void PhysEvent_Init( void ) {
	if ( eventInitialized ) {
		return;
	}
	PhysEvent_RegisterCvars();
	queueHead = queueTail = 0;
	handlerCount = 0;
	eventInitialized = qtrue;
	Com_Printf( "PhysEvent: event bus initialized\n" );
}

void PhysEvent_Shutdown( void ) {
	queueHead = queueTail = 0;
	handlerCount = 0;
	eventInitialized = qfalse;
}

void PhysEvent_Subscribe( phys_event_type_t type, PhysEventHandler_fn fn, void *userData ) {
	if ( !fn || handlerCount >= PHYS_EVENT_MAX_HANDLERS ) {
		return;
	}
	handlers[handlerCount].type = type;
	handlers[handlerCount].fn = fn;
	handlers[handlerCount].userData = userData;
	handlerCount++;
}

void PhysEvent_UnsubscribeAll( void ) {
	handlerCount = 0;
}

static void PhysEvent_DispatchOne( const phys_event_t *ev ) {
	int i;

	for ( i = 0; i < handlerCount; i++ ) {
		if ( handlers[i].type == ev->type && handlers[i].fn ) {
			handlers[i].fn( ev, handlers[i].userData );
		}
	}
}

void PhysEvent_Post( const phys_event_t *ev ) {
	int next;

	if ( !eventInitialized || !ev || !phys_events || !phys_events->integer ) {
		return;
	}

	next = ( queueTail + 1 ) % PHYS_EVENT_MAX_QUEUE;
	if ( next == queueHead ) {
		Com_DPrintf( S_COLOR_YELLOW "PhysEvent: queue full, dropping event %d\n", (int)ev->type );
		return;
	}

	eventQueue[queueTail] = *ev;
	queueTail = next;
}

void PhysEvent_DispatchQueued( void ) {
	while ( queueHead != queueTail ) {
		PhysEvent_DispatchOne( &eventQueue[queueHead] );
		queueHead = ( queueHead + 1 ) % PHYS_EVENT_MAX_QUEUE;
	}
}

int PhysEvent_QueueDepth( void ) {
	if ( queueTail >= queueHead ) {
		return queueTail - queueHead;
	}
	return PHYS_EVENT_MAX_QUEUE - queueHead + queueTail;
}

void PhysEvent_BuildHitFromImpulse( phys_hit_event_t *out, int bone, int damageType,
	const vec3_t point, const vec3_t impulse, float painScale ) {
	float mag;

	if ( !out ) {
		return;
	}

	Com_Memset( out, 0, sizeof( *out ) );
	VectorCopy( point, out->point );
	VectorCopy( impulse, out->impulse );
	out->bone = bone;
	out->damageType = damageType;

	mag = VectorLength( impulse );
	out->pain = ( mag * 0.002f ) * painScale;
	if ( out->pain > 1.0f ) {
		out->pain = 1.0f;
	}
	out->surprise = out->pain * 0.75f;
	out->balanceLoss = ( mag > 200.0f ) ? ( mag - 200.0f ) * 0.001f : 0.0f;
	if ( out->balanceLoss > 1.0f ) {
		out->balanceLoss = 1.0f;
	}
}

void PhysEvent_PostImpact( int entityNum, int bodyA, int bodyB,
	physMaterialId_t matA, physMaterialId_t matB,
	const vec3_t point, const vec3_t normal, const vec3_t impulse, float magnitude ) {
	phys_event_t ev;

	Com_Memset( &ev, 0, sizeof( ev ) );
	ev.type = PHYS_EVENT_IMPACT;
	ev.entityNum = entityNum;
	ev.bodyA = bodyA;
	ev.bodyB = bodyB;
	ev.matA = matA;
	ev.matB = matB;
	VectorCopy( point, ev.point );
	VectorCopy( normal, ev.normal );
	VectorCopy( impulse, ev.impulse );
	ev.magnitude = magnitude;
	PhysEvent_Post( &ev );
}

void PhysEvent_PostCharacterHit( int entityNum, int bone, int damageType,
	const vec3_t point, const vec3_t impulse, float pain, float balanceLoss ) {
	phys_event_t ev;

	Com_Memset( &ev, 0, sizeof( ev ) );
	ev.type = PHYS_EVENT_IMPACT;
	ev.entityNum = entityNum;
	PhysEvent_BuildHitFromImpulse( &ev.hit, bone, damageType, point, impulse, 1.0f );
	ev.hit.pain = pain;
	ev.hit.balanceLoss = balanceLoss;
	VectorCopy( point, ev.point );
	VectorCopy( impulse, ev.impulse );
	ev.magnitude = VectorLength( impulse );
	PhysEvent_Post( &ev );
}
