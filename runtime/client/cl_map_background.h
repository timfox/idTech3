/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Background map system for menu screens.
Loads and renders a non-playable 3D map behind the UI, similar to
Source Engine / GoldSrc background maps. Supports camera paths,
ambient audio, fog overrides, and per-menu map selection.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"

#define BGMAP_MAX_CAMERA_POINTS  64
#define BGMAP_MAX_MAPS            8

typedef struct bgCameraPoint_s {
	vec3_t  origin;
	vec3_t  angles;
	float   fov;
	float   time;
} bgCameraPoint_t;

typedef struct bgMapDef_s {
	char                name[MAX_QPATH];
	char                bspName[MAX_QPATH];
	char                musicTrack[MAX_QPATH];
	bgCameraPoint_t     cameraPath[BGMAP_MAX_CAMERA_POINTS];
	int                 numCameraPoints;
	float               cameraCycleTime;
	qboolean            cameraLoop;
	float               fogDensity;
	float               fogColor[3];
	float               ambientLight;
	float               timeOfDay;
	qboolean            active;
} bgMapDef_t;

void BgMap_Init(void);
void BgMap_Shutdown(void);
void BgMap_RegisterCvars(void);

qboolean BgMap_Load(const char *mapName);
void     BgMap_Unload(void);
qboolean BgMap_IsLoaded(void);
void     BgMap_SetActive(qboolean active);

void BgMap_Frame(float frametime);
void BgMap_Render(void);

int  BgMap_AddCameraPoint(const vec3_t origin, const vec3_t angles, float fov, float time);
void BgMap_ClearCameraPath(void);
void BgMap_SetCameraLoop(qboolean loop);
void BgMap_SetCameraCycleTime(float seconds);

void BgMap_SetFog(float density, float r, float g, float b);
void BgMap_SetAmbientLight(float intensity);
void BgMap_SetMusic(const char *trackName);
void BgMap_SetTimeOfDay(float hours);

int  BgMap_RegisterMap(const char *name, const char *bspName);
void BgMap_SelectMap(int index);
void BgMap_SelectRandom(void);
int  BgMap_GetMapCount(void);

void BgMap_GetCameraOrigin(vec3_t out);
void BgMap_GetCameraAngles(vec3_t out);
float BgMap_GetCameraFov(void);

#ifdef __cplusplus
}
#endif
