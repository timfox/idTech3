/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Adaptive music system with intensity-driven layered playback.
Responds to game state (Director intensity, combat, danger)
to crossfade between musical layers and trigger stingers.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"

#define MUSIC_MAX_LAYERS    8
#define MUSIC_MAX_STINGERS  16

typedef enum {
	MUSIC_LAYER_AMBIENT,
	MUSIC_LAYER_TENSION,
	MUSIC_LAYER_ACTION,
	MUSIC_LAYER_COMBAT_LOW,
	MUSIC_LAYER_COMBAT_HIGH,
	MUSIC_LAYER_DANGER,
	MUSIC_LAYER_BOSS,
	MUSIC_LAYER_CUSTOM
} musicLayerType_t;

typedef struct musicLayer_s {
	char                track[MAX_QPATH];
	musicLayerType_t    type;
	float               intensityMin;
	float               intensityMax;
	float               volume;
	float               targetVolume;
	float               fadeSpeed;
	qboolean            active;
	qboolean            looping;
} musicLayer_t;

typedef struct musicStinger_s {
	char                track[MAX_QPATH];
	float               intensityTrigger;
	float               cooldown;
	float               lastPlayTime;
	qboolean            oneShot;
} musicStinger_t;

void  Music_Init(void);
void  Music_Shutdown(void);
void  Music_Update(float intensity, float dt);
void  Music_SetIntensity(float intensity);

int   Music_AddLayer(const char *track, musicLayerType_t type,
                     float intensityMin, float intensityMax, float fadeSpeed);
void  Music_RemoveLayer(int layerId);
void  Music_SetLayerVolume(int layerId, float volume);

int   Music_AddStinger(const char *track, float intensityTrigger, float cooldown, qboolean oneShot);
void  Music_TriggerStinger(int stingerId);

void  Music_FadeToSilence(float fadeTime);
void  Music_ClearAll(void);

#ifdef __cplusplus
}
#endif
