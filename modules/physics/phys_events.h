/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Physics event bus — gameplay-facing notifications for impacts, breaks,
balance loss, splashes, and ragdoll state changes. Subscribers include
audio, particles, AI, renderer decals, and save-game persistence.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"
#include "phys_materials.h"

#define PHYS_EVENT_MAX_QUEUE   256
#define PHYS_EVENT_MAX_HANDLERS 32

typedef enum {
	PHYS_EVENT_IMPACT,
	PHYS_EVENT_BREAK,
	PHYS_EVENT_DENT,
	PHYS_EVENT_SPLASH,
	PHYS_EVENT_FALL,
	PHYS_EVENT_BALANCE_LOST,
	PHYS_EVENT_RAGDOLL_SLEEP,
	PHYS_EVENT_GRAB,
	PHYS_EVENT_TEAR,
	PHYS_EVENT_MOTION_ENTER,
	PHYS_EVENT_MOTION_EXIT,
	PHYS_EVENT_CONTACT_BEGIN, /* Soft Step non-sensor contact begin */
	PHYS_EVENT_CONTACT_END,
	PHYS_EVENT_BODY_SLEEP     /* Soft Step body fell asleep this step */
} phys_event_type_t;

typedef struct phys_hit_event_s {
	vec3_t  point;
	vec3_t  impulse;
	int     bone;
	int     damageType;
	float   pain;
	float   surprise;
	float   balanceLoss;
} phys_hit_event_t;

typedef struct phys_event_s {
	phys_event_type_t   type;
	int                 entityNum;
	int                 bodyA;
	int                 bodyB;
	int                 ragdoll; /* Soft Step ragdoll handle when a bone was hit; -1 else */
	int                 bone;    /* ragdoll bone index when ragdoll >= 0 */
	physMaterialId_t    matA;
	physMaterialId_t    matB;
	vec3_t              point;
	vec3_t              normal;
	vec3_t              impulse;
	float               magnitude;
	phys_hit_event_t    hit;
} phys_event_t;

typedef void (*PhysEventHandler_fn)( const phys_event_t *ev, void *userData );

void        PhysEvent_Init( void );
void        PhysEvent_Shutdown( void );
void        PhysEvent_RegisterCvars( void );
void        PhysEvent_Subscribe( phys_event_type_t type, PhysEventHandler_fn fn, void *userData );
void        PhysEvent_UnsubscribeAll( void );
void        PhysEvent_Post( const phys_event_t *ev );
void        PhysEvent_DispatchQueued( void );
int         PhysEvent_QueueDepth( void );
/* Script-facing ring: copies of posted events (survives DispatchQueued). */
qboolean    PhysEvent_Poll( phys_event_t *out );
void        PhysEvent_ClearPoll( void );
int         PhysEvent_PollDepth( void );

void        PhysEvent_PostImpact( int entityNum, int bodyA, int bodyB,
	physMaterialId_t matA, physMaterialId_t matB,
	const vec3_t point, const vec3_t normal, const vec3_t impulse, float magnitude );
void        PhysEvent_PostCharacterHit( int entityNum, int bone, int damageType,
	const vec3_t point, const vec3_t impulse, float pain, float balanceLoss );
void        PhysEvent_BuildHitFromImpulse( phys_hit_event_t *out, int bone, int damageType,
	const vec3_t point, const vec3_t impulse, float painScale );

#ifdef __cplusplus
}
#endif
