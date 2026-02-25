/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Adaptive music implementation.
Crossfades layers based on Director intensity and triggers
contextual stingers for combat/danger events.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "snd_music_adaptive.h"

static musicLayer_t   layers[MUSIC_MAX_LAYERS];
static int            numLayers;
static musicStinger_t stingers[MUSIC_MAX_STINGERS];
static int            numStingers;
static float          currentIntensity;
static float          globalFade = 1.0f;
static float          globalFadeTarget = 1.0f;
static float          globalFadeSpeed = 1.0f;
static qboolean       musicInitialized = qfalse;
static float          musicTime;

void Music_Init(void) {
	if (musicInitialized) return;
	Com_Memset(layers, 0, sizeof(layers));
	Com_Memset(stingers, 0, sizeof(stingers));
	numLayers = 0;
	numStingers = 0;
	currentIntensity = 0;
	globalFade = 1.0f;
	globalFadeTarget = 1.0f;
	musicTime = 0;
	musicInitialized = qtrue;
	Com_Printf("Adaptive music system initialized\n");
}

void Music_Shutdown(void) {
	if (!musicInitialized) return;
	Music_ClearAll();
	musicInitialized = qfalse;
}

void Music_SetIntensity(float intensity) {
	currentIntensity = intensity;
	if (currentIntensity < 0) currentIntensity = 0;
	if (currentIntensity > 1) currentIntensity = 1;
}

void Music_Update(float intensity, float dt) {
	int i;

	if (!musicInitialized) return;
	musicTime += dt;
	currentIntensity = intensity;

	if (globalFade != globalFadeTarget) {
		if (globalFade < globalFadeTarget)
			globalFade += globalFadeSpeed * dt;
		else
			globalFade -= globalFadeSpeed * dt;

		if (fabsf(globalFade - globalFadeTarget) < 0.01f)
			globalFade = globalFadeTarget;
	}

	for (i = 0; i < numLayers; i++) {
		if (!layers[i].active) continue;

		qboolean inRange = (currentIntensity >= layers[i].intensityMin &&
		                    currentIntensity <= layers[i].intensityMax);

		layers[i].targetVolume = inRange ? 1.0f : 0.0f;

		if (layers[i].volume < layers[i].targetVolume) {
			layers[i].volume += layers[i].fadeSpeed * dt;
			if (layers[i].volume > layers[i].targetVolume)
				layers[i].volume = layers[i].targetVolume;
		} else if (layers[i].volume > layers[i].targetVolume) {
			layers[i].volume -= layers[i].fadeSpeed * dt;
			if (layers[i].volume < layers[i].targetVolume)
				layers[i].volume = layers[i].targetVolume;
		}
	}

	for (i = 0; i < numStingers; i++) {
		if (currentIntensity >= stingers[i].intensityTrigger &&
			musicTime - stingers[i].lastPlayTime >= stingers[i].cooldown) {
			stingers[i].lastPlayTime = musicTime;
		}
	}
}

int Music_AddLayer(const char *track, musicLayerType_t type,
                   float intensityMin, float intensityMax, float fadeSpeed) {
	if (numLayers >= MUSIC_MAX_LAYERS) return -1;
	int idx = numLayers++;
	Q_strncpyz(layers[idx].track, track, sizeof(layers[idx].track));
	layers[idx].type = type;
	layers[idx].intensityMin = intensityMin;
	layers[idx].intensityMax = intensityMax;
	layers[idx].fadeSpeed = fadeSpeed > 0 ? fadeSpeed : 0.5f;
	layers[idx].volume = 0;
	layers[idx].targetVolume = 0;
	layers[idx].active = qtrue;
	layers[idx].looping = qtrue;
	return idx;
}

void Music_RemoveLayer(int id) {
	if (id >= 0 && id < numLayers) layers[id].active = qfalse;
}

void Music_SetLayerVolume(int id, float volume) {
	if (id >= 0 && id < numLayers) layers[id].volume = volume;
}

int Music_AddStinger(const char *track, float trigger, float cooldown, qboolean oneShot) {
	if (numStingers >= MUSIC_MAX_STINGERS) return -1;
	int idx = numStingers++;
	Q_strncpyz(stingers[idx].track, track, sizeof(stingers[idx].track));
	stingers[idx].intensityTrigger = trigger;
	stingers[idx].cooldown = cooldown;
	stingers[idx].lastPlayTime = -cooldown;
	stingers[idx].oneShot = oneShot;
	return idx;
}

void Music_TriggerStinger(int id) {
	if (id >= 0 && id < numStingers) {
		stingers[id].lastPlayTime = musicTime;
	}
}

void Music_FadeToSilence(float fadeTime) {
	globalFadeTarget = 0.0f;
	globalFadeSpeed = fadeTime > 0 ? 1.0f / fadeTime : 10.0f;
}

void Music_ClearAll(void) {
	Com_Memset(layers, 0, sizeof(layers));
	Com_Memset(stingers, 0, sizeof(stingers));
	numLayers = 0;
	numStingers = 0;
	globalFade = 1.0f;
	globalFadeTarget = 1.0f;
}
