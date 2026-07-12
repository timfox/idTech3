/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Choreography / scripted scene system.
Manages timed sequences of events (speech, animations, camera moves,
effects) for cinematic moments and NPC interactions.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"

#define CHOREO_MAX_SCENES    32
#define CHOREO_MAX_EVENTS    64
#define CHOREO_MAX_ACTORS     8

typedef enum {
	CHOREO_EVT_SPEAK,
	CHOREO_EVT_ANIMATE,
	CHOREO_EVT_MOVE_TO,
	CHOREO_EVT_LOOK_AT,
	CHOREO_EVT_CAMERA_CUT,
	CHOREO_EVT_CAMERA_LERP,
	CHOREO_EVT_EFFECT,
	CHOREO_EVT_SOUND,
	CHOREO_EVT_WAIT,
	CHOREO_EVT_CALLBACK
} choreoEventType_t;

typedef struct choreoEvent_s {
	choreoEventType_t type;
	float             startTime;
	float             duration;
	int               actorIndex;
	char              param[MAX_QPATH];
	vec3_t            position;
	vec3_t            angles;
	float             value;
} choreoEvent_t;

typedef struct choreoActor_s {
	int     entityNum;
	char    name[64];
	vec3_t  startPos;
	vec3_t  startAngles;
} choreoActor_t;

typedef int choreoHandle_t;

choreoHandle_t Choreo_CreateScene(const char *name);
void           Choreo_DestroyScene(choreoHandle_t handle);
int            Choreo_AddActor(choreoHandle_t handle, int entityNum, const char *name);
int            Choreo_AddEvent(choreoHandle_t handle, choreoEventType_t type,
                               float startTime, float duration, int actorIndex,
                               const char *param, const vec3_t position);

void Choreo_Play(choreoHandle_t handle);
void Choreo_Stop(choreoHandle_t handle);
void Choreo_Pause(choreoHandle_t handle);
void Choreo_Update(float dt);

qboolean Choreo_IsPlaying(choreoHandle_t handle);
float    Choreo_GetTime(choreoHandle_t handle);
int      Choreo_GetActiveCount(void);

#ifdef __cplusplus
}
#endif
