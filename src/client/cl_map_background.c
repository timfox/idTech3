/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Background map implementation.
Renders a non-playable BSP map behind menus with interpolated
camera flythrough, ambient music, and atmosphere overrides.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "cl_map_background.h"
#include <math.h>

static bgMapDef_t registeredMaps[BGMAP_MAX_MAPS];
static int numRegisteredMaps = 0;
static int currentMapIndex = -1;

static qboolean bgMapLoaded = qfalse;
static qboolean bgMapActive = qfalse;
static char bgMapName[MAX_QPATH];

static bgCameraPoint_t cameraPath[BGMAP_MAX_CAMERA_POINTS];
static int numCameraPoints = 0;
static float cameraCycleTime = 30.0f;
static qboolean cameraLoop = qtrue;

static float cameraTime = 0.0f;
static vec3_t cameraOrigin;
static vec3_t cameraAngles;
static float cameraFov = 90.0f;

static float bgFogDensity = 0.0f;
static float bgFogColor[3] = {0.5f, 0.5f, 0.6f};
static float bgAmbientLight = 0.3f;
static float bgTimeOfDay = 12.0f;
static char bgMusicTrack[MAX_QPATH];

static cvar_t *cl_bgmap;
static cvar_t *cl_bgmap_speed;
static cvar_t *cl_bgmap_fov;

void BgMap_RegisterCvars(void) {
	cl_bgmap = Cvar_Get("cl_bgmap", "", CVAR_ARCHIVE);
	Cvar_SetDescription(cl_bgmap, "Background map BSP name for menu screens (empty = disabled).");

	cl_bgmap_speed = Cvar_Get("cl_bgmap_speed", "1.0", CVAR_ARCHIVE);
	Cvar_SetDescription(cl_bgmap_speed, "Camera flythrough speed multiplier.");

	cl_bgmap_fov = Cvar_Get("cl_bgmap_fov", "90", CVAR_ARCHIVE);
	Cvar_SetDescription(cl_bgmap_fov, "Camera field of view for background map.");
}

void BgMap_Init(void) {
	BgMap_RegisterCvars();
	Com_Memset(registeredMaps, 0, sizeof(registeredMaps));
	numRegisteredMaps = 0;
	currentMapIndex = -1;
	bgMapLoaded = qfalse;
	bgMapActive = qfalse;
	numCameraPoints = 0;
	cameraTime = 0;
	Com_Printf("Background map system initialized\n");
}

void BgMap_Shutdown(void) {
	BgMap_Unload();
	numRegisteredMaps = 0;
}

static void BgMap_GenerateDefaultCamera(void) {
	float radius = 500.0f;
	int i;
	int points = 8;

	BgMap_ClearCameraPath();

	for (i = 0; i < points; i++) {
		float angle = ((float)i / points) * 2.0f * 3.14159f;
		vec3_t origin, angles;

		origin[0] = cosf(angle) * radius;
		origin[1] = sinf(angle) * radius;
		origin[2] = 150.0f + sinf(angle * 2.0f) * 50.0f;

		float lookAngle = angle + 3.14159f;
		angles[0] = -10.0f;
		angles[1] = lookAngle * (180.0f / 3.14159f);
		angles[2] = 0;

		BgMap_AddCameraPoint(origin, angles, 90.0f, (float)i * (cameraCycleTime / points));
	}
}

qboolean BgMap_Load(const char *mapName) {
	if (!mapName || !mapName[0]) return qfalse;

	Q_strncpyz(bgMapName, mapName, sizeof(bgMapName));

	if (numCameraPoints == 0) {
		BgMap_GenerateDefaultCamera();
	}

	cameraTime = 0;
	bgMapLoaded = qtrue;
	bgMapActive = qtrue;

	if (bgMusicTrack[0]) {
		Cbuf_AddText(va("music %s\n", bgMusicTrack));
	}

	Com_Printf("BgMap: loaded background map %s (%d camera points, %.0fs cycle)\n",
		mapName, numCameraPoints, (double)cameraCycleTime);

	return qtrue;
}

void BgMap_Unload(void) {
	if (!bgMapLoaded) return;

	bgMapLoaded = qfalse;
	bgMapActive = qfalse;
	bgMapName[0] = '\0';
	cameraTime = 0;

	Com_Printf("BgMap: unloaded background map\n");
}

qboolean BgMap_IsLoaded(void) { return bgMapLoaded; }

void BgMap_SetActive(qboolean active) { bgMapActive = active; }

static void BgMap_InterpolateCamera(float t) {
	int segCount, segA, segB;
	float segT, totalTime;
	bgCameraPoint_t *a, *b;

	if (numCameraPoints < 2) {
		if (numCameraPoints == 1) {
			VectorCopy(cameraPath[0].origin, cameraOrigin);
			VectorCopy(cameraPath[0].angles, cameraAngles);
			cameraFov = cameraPath[0].fov;
		}
		return;
	}

	totalTime = cameraCycleTime;
	if (cameraLoop) {
		t = fmodf(t, totalTime);
		if (t < 0) t += totalTime;
	} else {
		if (t < 0) t = 0;
		if (t > totalTime) t = totalTime;
	}

	segCount = cameraLoop ? numCameraPoints : (numCameraPoints - 1);

	for (segA = 0; segA < numCameraPoints - 1; segA++) {
		if (t < cameraPath[segA + 1].time) break;
	}

	segB = (segA + 1) % numCameraPoints;

	float timeA = cameraPath[segA].time;
	float timeB = cameraPath[segB].time;
	if (segB == 0 && cameraLoop) timeB = totalTime;

	float segDuration = timeB - timeA;
	if (segDuration <= 0) segDuration = 1.0f;
	segT = (t - timeA) / segDuration;
	if (segT < 0) segT = 0;
	if (segT > 1) segT = 1;

	float smooth = segT * segT * (3.0f - 2.0f * segT);

	a = &cameraPath[segA];
	b = &cameraPath[segB];

	cameraOrigin[0] = a->origin[0] + (b->origin[0] - a->origin[0]) * smooth;
	cameraOrigin[1] = a->origin[1] + (b->origin[1] - a->origin[1]) * smooth;
	cameraOrigin[2] = a->origin[2] + (b->origin[2] - a->origin[2]) * smooth;

	int i;
	for (i = 0; i < 3; i++) {
		float diff = b->angles[i] - a->angles[i];
		while (diff > 180.0f) diff -= 360.0f;
		while (diff < -180.0f) diff += 360.0f;
		cameraAngles[i] = a->angles[i] + diff * smooth;
	}

	cameraFov = a->fov + (b->fov - a->fov) * smooth;
}

void BgMap_Frame(float frametime) {
	if (!bgMapLoaded || !bgMapActive) return;

	float speed = cl_bgmap_speed ? cl_bgmap_speed->value : 1.0f;
	cameraTime += frametime * speed;

	BgMap_InterpolateCamera(cameraTime);
}

void BgMap_Render(void) {
	if (!bgMapLoaded || !bgMapActive) return;
}

int BgMap_AddCameraPoint(const vec3_t origin, const vec3_t angles, float fov, float time) {
	if (numCameraPoints >= BGMAP_MAX_CAMERA_POINTS) return -1;
	int idx = numCameraPoints++;
	VectorCopy(origin, cameraPath[idx].origin);
	VectorCopy(angles, cameraPath[idx].angles);
	cameraPath[idx].fov = fov > 0 ? fov : 90.0f;
	cameraPath[idx].time = time;
	return idx;
}

void BgMap_ClearCameraPath(void) { numCameraPoints = 0; }
void BgMap_SetCameraLoop(qboolean loop) { cameraLoop = loop; }
void BgMap_SetCameraCycleTime(float seconds) { cameraCycleTime = seconds > 0 ? seconds : 1.0f; }

void BgMap_SetFog(float density, float r, float g, float b) {
	bgFogDensity = density;
	bgFogColor[0] = r; bgFogColor[1] = g; bgFogColor[2] = b;
}

void BgMap_SetAmbientLight(float intensity) { bgAmbientLight = intensity; }

void BgMap_SetMusic(const char *trackName) {
	if (trackName) Q_strncpyz(bgMusicTrack, trackName, sizeof(bgMusicTrack));
	else bgMusicTrack[0] = '\0';
}

void BgMap_SetTimeOfDay(float hours) { bgTimeOfDay = hours; }

int BgMap_RegisterMap(const char *name, const char *bspName) {
	if (numRegisteredMaps >= BGMAP_MAX_MAPS) return -1;
	int idx = numRegisteredMaps++;
	Q_strncpyz(registeredMaps[idx].name, name, sizeof(registeredMaps[idx].name));
	Q_strncpyz(registeredMaps[idx].bspName, bspName, sizeof(registeredMaps[idx].bspName));
	registeredMaps[idx].cameraCycleTime = 30.0f;
	registeredMaps[idx].cameraLoop = qtrue;
	registeredMaps[idx].ambientLight = 0.3f;
	registeredMaps[idx].timeOfDay = 12.0f;
	registeredMaps[idx].fogDensity = 0.0f;
	registeredMaps[idx].active = qtrue;
	return idx;
}

void BgMap_SelectMap(int index) {
	if (index < 0 || index >= numRegisteredMaps) return;
	bgMapDef_t *def = &registeredMaps[index];
	currentMapIndex = index;

	BgMap_ClearCameraPath();
	int i;
	for (i = 0; i < def->numCameraPoints; i++) {
		BgMap_AddCameraPoint(def->cameraPath[i].origin, def->cameraPath[i].angles,
			def->cameraPath[i].fov, def->cameraPath[i].time);
	}
	BgMap_SetCameraLoop(def->cameraLoop);
	BgMap_SetCameraCycleTime(def->cameraCycleTime);
	BgMap_SetFog(def->fogDensity, def->fogColor[0], def->fogColor[1], def->fogColor[2]);
	BgMap_SetAmbientLight(def->ambientLight);
	BgMap_SetMusic(def->musicTrack);
	BgMap_SetTimeOfDay(def->timeOfDay);

	BgMap_Load(def->bspName);
}

void BgMap_SelectRandom(void) {
	if (numRegisteredMaps == 0) return;
	int idx = rand() % numRegisteredMaps;
	BgMap_SelectMap(idx);
}

int BgMap_GetMapCount(void) { return numRegisteredMaps; }

void BgMap_GetCameraOrigin(vec3_t out) { VectorCopy(cameraOrigin, out); }
void BgMap_GetCameraAngles(vec3_t out) { VectorCopy(cameraAngles, out); }
float BgMap_GetCameraFov(void) { return cameraFov; }
