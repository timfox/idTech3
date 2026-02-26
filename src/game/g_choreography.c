/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Choreography implementation.
Timeline-based scene playback with event dispatch.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "g_choreography.h"

typedef struct choreoScene_s {
	char            name[64];
	choreoActor_t   actors[CHOREO_MAX_ACTORS];
	int             numActors;
	choreoEvent_t   events[CHOREO_MAX_EVENTS];
	int             numEvents;
	float           currentTime;
	float           totalDuration;
	qboolean        playing;
	qboolean        paused;
	qboolean        active;
	int             lastDispatchedEvent;
} choreoScene_t;

static choreoScene_t scenes[CHOREO_MAX_SCENES];
static int numScenes = 0;

#define VALID_SCENE(h) ((h) >= 0 && (h) < numScenes && scenes[(h)].active)

choreoHandle_t Choreo_CreateScene(const char *name) {
	if (numScenes >= CHOREO_MAX_SCENES) return -1;
	int idx = numScenes++;
	Com_Memset(&scenes[idx], 0, sizeof(choreoScene_t));
	Q_strncpyz(scenes[idx].name, name, sizeof(scenes[idx].name));
	scenes[idx].active = qtrue;
	scenes[idx].lastDispatchedEvent = -1;
	return idx;
}

void Choreo_DestroyScene(choreoHandle_t h) {
	if (VALID_SCENE(h)) {
		scenes[h].active = qfalse;
		scenes[h].playing = qfalse;
	}
}

int Choreo_AddActor(choreoHandle_t h, int entityNum, const char *name) {
	if (!VALID_SCENE(h) || scenes[h].numActors >= CHOREO_MAX_ACTORS) return -1;
	int idx = scenes[h].numActors++;
	scenes[h].actors[idx].entityNum = entityNum;
	Q_strncpyz(scenes[h].actors[idx].name, name, sizeof(scenes[h].actors[idx].name));
	return idx;
}

int Choreo_AddEvent(choreoHandle_t h, choreoEventType_t type,
                    float startTime, float duration, int actorIndex,
                    const char *param, const vec3_t position) {
	if (!VALID_SCENE(h) || scenes[h].numEvents >= CHOREO_MAX_EVENTS) return -1;
	int idx = scenes[h].numEvents++;
	choreoEvent_t *evt = &scenes[h].events[idx];
	evt->type = type;
	evt->startTime = startTime;
	evt->duration = duration;
	evt->actorIndex = actorIndex;
	if (param) Q_strncpyz(evt->param, param, sizeof(evt->param));
	if (position) VectorCopy(position, evt->position);

	float endTime = startTime + duration;
	if (endTime > scenes[h].totalDuration) scenes[h].totalDuration = endTime;
	return idx;
}

void Choreo_Play(choreoHandle_t h) {
	if (!VALID_SCENE(h)) return;
	scenes[h].playing = qtrue;
	scenes[h].paused = qfalse;
	scenes[h].currentTime = 0;
	scenes[h].lastDispatchedEvent = -1;
}

void Choreo_Stop(choreoHandle_t h) {
	if (!VALID_SCENE(h)) return;
	scenes[h].playing = qfalse;
	scenes[h].currentTime = 0;
}

void Choreo_Pause(choreoHandle_t h) {
	if (!VALID_SCENE(h)) return;
	scenes[h].paused = !scenes[h].paused;
}

extern sfxHandle_t S_RegisterSound(const char *name, qboolean compressed);
extern void S_StartLocalSound(sfxHandle_t sfx, int channelNum);

static void Choreo_DispatchEvent(choreoScene_t *scene, const choreoEvent_t *evt) {
	switch (evt->type) {
		case CHOREO_EVT_SPEAK:
		case CHOREO_EVT_SOUND:
			if (evt->param[0]) {
				sfxHandle_t sfx = S_RegisterSound(evt->param, qfalse);
				if (sfx) S_StartLocalSound(sfx, 0);
			}
			Com_DPrintf("Choreo [%s]: %s \"%s\"\n", scene->name,
				evt->type == CHOREO_EVT_SPEAK ? "speak" : "sound", evt->param);
			break;
		case CHOREO_EVT_ANIMATE:
			Com_DPrintf("Choreo [%s]: actor %d animate \"%s\"\n", scene->name, evt->actorIndex, evt->param);
			if (evt->param[0]) {
				Cbuf_AddText(va("lua Engine.Choreo.onAnimate(%d, \"%s\")\n", evt->actorIndex, evt->param));
			}
			break;
		case CHOREO_EVT_CAMERA_CUT:
			Com_DPrintf("Choreo [%s]: camera cut (%.0f, %.0f, %.0f)\n",
				scene->name, (double)evt->position[0], (double)evt->position[1], (double)evt->position[2]);
			break;
		default:
			break;
	}
}

void Choreo_Update(float dt) {
	int s, e;

	for (s = 0; s < numScenes; s++) {
		choreoScene_t *scene = &scenes[s];
		if (!scene->active || !scene->playing || scene->paused) continue;

		scene->currentTime += dt;

		for (e = 0; e < scene->numEvents; e++) {
			choreoEvent_t *evt = &scene->events[e];
			if (scene->currentTime >= evt->startTime &&
				scene->currentTime < evt->startTime + dt + 0.001f) {
				Choreo_DispatchEvent(scene, evt);
			}
		}

		if (scene->currentTime >= scene->totalDuration) {
			scene->playing = qfalse;
		}
	}
}

qboolean Choreo_IsPlaying(choreoHandle_t h) {
	return VALID_SCENE(h) ? scenes[h].playing : qfalse;
}

float Choreo_GetTime(choreoHandle_t h) {
	return VALID_SCENE(h) ? scenes[h].currentTime : 0;
}

int Choreo_GetActiveCount(void) {
	int count = 0, i;
	for (i = 0; i < numScenes; i++)
		if (scenes[i].active && scenes[i].playing) count++;
	return count;
}
