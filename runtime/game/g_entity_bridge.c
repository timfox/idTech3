/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Entity bridge implementation.
Reads BSP entity key/values and configures engine systems.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "g_entity_bridge.h"
#ifdef USE_GAME_AI_MIDDLEWARE
#include "g_director.h"
#endif
#include "../client/shell/cl_map_background.h"

static const char *EB_ParseToken(const char **text) {
	return COM_Parse(text);
}

static float EB_GetFloat(const char *value, float def) {
	if (!value || !value[0]) return def;
	return (float)atof(value);
}

static int EB_GetInt(const char *value, int def) {
	if (!value || !value[0]) return def;
	return atoi(value);
}

void EntityBridge_Clear(void) {
	Com_Printf("EntityBridge: cleared\n");
}

void EntityBridge_ParseEntities(const char *entityString) {
	const char *p;
	char key[MAX_TOKEN_CHARS], value[MAX_TOKEN_CHARS];
	char classname[MAX_TOKEN_CHARS];
	int entityCount = 0;
	int directorZones = 0, directorSpawns = 0, bgCameras = 0;

	if (!entityString || !entityString[0]) return;

	EntityBridge_Clear();
	p = entityString;

	while (1) {
		const char *token = EB_ParseToken(&p);
		if (!token[0]) break;
		if (token[0] != '{') continue;

		classname[0] = '\0';

		char keys[32][MAX_TOKEN_CHARS];
		char values[32][MAX_TOKEN_CHARS];
		int numKV = 0;

		while (1) {
			token = EB_ParseToken(&p);
			if (!token[0] || token[0] == '}') break;

			Q_strncpyz(key, token, sizeof(key));
			token = EB_ParseToken(&p);
			Q_strncpyz(value, token, sizeof(value));

			if (!Q_stricmp(key, "classname")) {
				Q_strncpyz(classname, value, sizeof(classname));
			}

			if (numKV < 32) {
				Q_strncpyz(keys[numKV], key, MAX_TOKEN_CHARS);
				Q_strncpyz(values[numKV], value, MAX_TOKEN_CHARS);
				numKV++;
			}
		}

		if (!classname[0]) continue;
		entityCount++;

		if (!Q_stricmp(classname, "info_director_zone")) {
			vec3_t mins = {0}, maxs = {0};
			const char *name = "";
			int threat = 2;
			float budgetMult = 1.0f;
			int i;

			for (i = 0; i < numKV; i++) {
				if (!Q_stricmp(keys[i], "targetname")) name = values[i];
				else if (!Q_stricmp(keys[i], "threat")) threat = EB_GetInt(values[i], 2);
				else if (!Q_stricmp(keys[i], "budget_mult")) budgetMult = EB_GetFloat(values[i], 1.0f);
				else if (!Q_stricmp(keys[i], "origin")) sscanf(values[i], "%f %f %f", &mins[0], &mins[1], &mins[2]);
			}

			VectorCopy(mins, maxs);
			maxs[0] += 256; maxs[1] += 256; maxs[2] += 128;

#ifdef USE_GAME_AI_MIDDLEWARE
			Director_AddZone(name, mins, maxs, (dirThreat_t)threat, budgetMult);
			directorZones++;
#endif
		}

		else if (!Q_stricmp(classname, "info_bgmap_camera")) {
			vec3_t origin = {0}, angles = {0};
			float fov = 90, time = 0;
			int i;

			for (i = 0; i < numKV; i++) {
				if (!Q_stricmp(keys[i], "origin")) sscanf(values[i], "%f %f %f", &origin[0], &origin[1], &origin[2]);
				else if (!Q_stricmp(keys[i], "angles")) sscanf(values[i], "%f %f %f", &angles[0], &angles[1], &angles[2]);
				else if (!Q_stricmp(keys[i], "fov")) fov = EB_GetFloat(values[i], 90);
				else if (!Q_stricmp(keys[i], "time")) time = EB_GetFloat(values[i], 0);
			}

			BgMap_AddCameraPoint(origin, angles, fov, time);
			bgCameras++;
		}
	}

	Com_Printf("EntityBridge: parsed %d entities (%d director zones, %d spawns, %d bg cameras)\n",
		entityCount, directorZones, directorSpawns, bgCameras);
}
