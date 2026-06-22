/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Dismemberment and extended gibs implementation.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "g_dismember.h"
#include "../physics/phys_bullet.h"
#include <math.h>

#define MAX_DISMEMBER_INSTANCES 64

typedef struct dismemberInstance_s {
	qboolean        active;
	int             entityNum;
	limbState_t     limbs[LIMB_COUNT];
	limbDef_t       limbDefs[LIMB_COUNT];
	woundEntry_t    wounds[DISMEMBER_MAX_WOUNDS];
	int             numWounds;
	int             severedCount;
	float           totalBleedRate;
} dismemberInstance_t;

static dismemberInstance_t instances[MAX_DISMEMBER_INSTANCES];
static int instanceCount = 0;
static dismemberConfig_t globalConfig;
static activeGib_t gibs[DISMEMBER_MAX_GIBS];
static int activeGibCount = 0;

static cvar_t *g_dismemberEnabled;
static cvar_t *g_goreLevel;
static cvar_t *g_gibLifetime;
static cvar_t *g_maxGibs;

static float randf(void) {
	return (float)(rand() & 0x7FFF) / (float)0x7FFF;
}

#define VALID_INST(h) ((h) >= 0 && (h) < instanceCount && instances[(h)].active)

void Dismember_DefaultConfig(dismemberConfig_t *config) {
	Com_Memset(config, 0, sizeof(*config));
	Q_strncpyz(config->name, "default", sizeof(config->name));
	config->goreLevel = GORE_FULL;
	config->severDamageScale = 1.0f;
	config->gibVelocityScale = 1.0f;
	config->bleedRateScale = 1.0f;
	config->gibLifetime = 10.0f;
	config->gibFadeTime = 2.0f;
	config->maxActiveGibs = 64;
	config->gibsPerSever = 5;
	config->decalSize = 32.0f;
	config->physicsGibs = qtrue;
	config->bloodTrails = qtrue;
	config->persistentGibs = qfalse;
	config->persistDuration = 30.0f;
}

void Dismember_RegisterCvars(void) {
	g_dismemberEnabled = Cvar_Get("g_dismemberEnabled", "1", CVAR_ARCHIVE);
	g_goreLevel        = Cvar_Get("g_goreLevel",        "3", CVAR_ARCHIVE);
	g_gibLifetime      = Cvar_Get("g_gibLifetime",       "10", CVAR_ARCHIVE);
	g_maxGibs          = Cvar_Get("g_maxGibs",           "64", CVAR_ARCHIVE);
}

void Dismember_Init(void) {
	Dismember_RegisterCvars();
	Com_Memset(instances, 0, sizeof(instances));
	Com_Memset(gibs, 0, sizeof(gibs));
	instanceCount = 0;
	activeGibCount = 0;
	Dismember_DefaultConfig(&globalConfig);
	Com_Printf("Dismemberment system initialized (gore level %d)\n", g_goreLevel->integer);
}

void Dismember_Shutdown(void) {
	instanceCount = 0;
	activeGibCount = 0;
}

void Dismember_SetConfig(const dismemberConfig_t *config) {
	if (config) Com_Memcpy(&globalConfig, config, sizeof(dismemberConfig_t));
}

dismemberHandle_t Dismember_CreateInstance(int entityNum) {
	int i, idx;
	dismemberInstance_t *inst;

	if (instanceCount >= MAX_DISMEMBER_INSTANCES) return -1;

	idx = instanceCount++;
	inst = &instances[idx];
	Com_Memset(inst, 0, sizeof(*inst));
	inst->active = qtrue;
	inst->entityNum = entityNum;

	for (i = 0; i < LIMB_COUNT; i++) {
		inst->limbs[i].id = (limbId_t)i;
		inst->limbs[i].health = 100.0f;
		inst->limbs[i].maxHealth = 100.0f;
		inst->limbs[i].attached = qtrue;
		inst->limbs[i].severed = qfalse;
		inst->limbs[i].activeWound = WOUND_NONE;
		inst->limbs[i].bleedRate = 0;
	}

	return idx;
}

void Dismember_DestroyInstance(dismemberHandle_t h) {
	if (VALID_INST(h)) instances[h].active = qfalse;
}

void Dismember_SetLimbDef(dismemberHandle_t h, const limbDef_t *def) {
	if (!VALID_INST(h) || !def || def->id >= LIMB_COUNT) return;
	Com_Memcpy(&instances[h].limbDefs[def->id], def, sizeof(limbDef_t));
	instances[h].limbs[def->id].maxHealth = def->health;
	instances[h].limbs[def->id].health = def->health;
}

void Dismember_ApplyDamage(dismemberHandle_t h, limbId_t limb,
                           float damage, woundType_t woundType,
                           const vec3_t hitPoint, const vec3_t hitDir) {
	dismemberInstance_t *inst;
	limbState_t *ls;
	float scaledDamage;

	if (!VALID_INST(h) || limb >= LIMB_COUNT) return;
	if (!g_dismemberEnabled || !g_dismemberEnabled->integer) return;

	inst = &instances[h];
	ls = &inst->limbs[limb];

	if (ls->severed) return;

	scaledDamage = damage * globalConfig.severDamageScale;
	ls->health -= scaledDamage;

	if (woundType > ls->activeWound) {
		ls->activeWound = woundType;
	}

	switch (woundType) {
		case WOUND_GUNSHOT:  ls->bleedRate += 2.0f; break;
		case WOUND_LACERATION: ls->bleedRate += 3.0f; break;
		case WOUND_EXPLOSION: ls->bleedRate += 5.0f; break;
		case WOUND_BURN:     ls->bleedRate += 0.5f; break;
		default:             ls->bleedRate += 1.0f; break;
	}
	ls->bleedRate *= globalConfig.bleedRateScale;

	if (inst->numWounds < DISMEMBER_MAX_WOUNDS) {
		woundEntry_t *w = &inst->wounds[inst->numWounds++];
		w->type = woundType;
		w->limb = limb;
		if (hitPoint) VectorCopy(hitPoint, w->position);
		if (hitDir) VectorCopy(hitDir, w->direction);
		w->severity = scaledDamage / ls->maxHealth;
		w->bleedRate = ls->bleedRate;
		w->active = qtrue;
	}

	if (ls->health <= 0 && ls->attached) {
		vec3_t sevForce;
		if (hitDir) {
			VectorScale(hitDir, scaledDamage * 2.0f, sevForce);
		} else {
			VectorSet(sevForce, 0, 0, 100);
		}
		Dismember_SeverLimb(h, limb, sevForce);
	}
}

qboolean Dismember_SeverLimb(dismemberHandle_t h, limbId_t limb,
                             const vec3_t force) {
	dismemberInstance_t *inst;
	limbState_t *ls;
	int i;

	if (!VALID_INST(h) || limb >= LIMB_COUNT) return qfalse;

	inst = &instances[h];
	ls = &inst->limbs[limb];

	if (ls->severed || !ls->attached) return qfalse;

	ls->severed = qtrue;
	ls->attached = qfalse;
	ls->activeWound = WOUND_AMPUTATION;
	ls->bleedRate = 10.0f * globalConfig.bleedRateScale;
	inst->severedCount++;

	for (i = 0; i < LIMB_COUNT; i++) {
		if (inst->limbDefs[i].parentLimb == (int)limb && inst->limbs[i].attached) {
			Dismember_SeverLimb(h, (limbId_t)i, force);
		}
	}

	vec3_t gibOrigin;
	VectorCopy(ls->lastPosition, gibOrigin);
	Dismember_SpawnGibs(gibOrigin, force, globalConfig.gibsPerSever, GIB_FLESH, VectorLength(force));

	return qtrue;
}

void Dismember_Explode(dismemberHandle_t h, const vec3_t origin,
                       float force, float radius) {
	dismemberInstance_t *inst;
	int i;

	if (!VALID_INST(h)) return;
	inst = &instances[h];

	for (i = 0; i < LIMB_COUNT; i++) {
		if (!inst->limbs[i].attached) continue;

		float dist = Distance(inst->limbs[i].lastPosition, origin);
		if (dist < radius) {
			float atten = 1.0f - (dist / radius);
			vec3_t dir;
			VectorSubtract(inst->limbs[i].lastPosition, origin, dir);
			VectorNormalize(dir);
			VectorScale(dir, force * atten, dir);

			if (atten > 0.3f) {
				Dismember_SeverLimb(h, (limbId_t)i, dir);
			} else {
				Dismember_ApplyDamage(h, (limbId_t)i, force * atten * 0.5f,
					WOUND_EXPLOSION, inst->limbs[i].lastPosition, dir);
			}
		}
	}

	Dismember_SpawnGibs(origin, NULL, globalConfig.gibsPerSever * 3,
		GIB_FLESH, force * 0.5f);
}

void Dismember_GetLimbState(dismemberHandle_t h, limbId_t limb,
                            limbState_t *state) {
	if (!VALID_INST(h) || limb >= LIMB_COUNT || !state) return;
	Com_Memcpy(state, &instances[h].limbs[limb], sizeof(limbState_t));
}

qboolean Dismember_IsLimbAttached(dismemberHandle_t h, limbId_t limb) {
	if (!VALID_INST(h) || limb >= LIMB_COUNT) return qfalse;
	return instances[h].limbs[limb].attached;
}

int Dismember_GetSeveredCount(dismemberHandle_t h) {
	return VALID_INST(h) ? instances[h].severedCount : 0;
}

float Dismember_GetBleedRate(dismemberHandle_t h) {
	if (!VALID_INST(h)) return 0;
	float total = 0;
	int i;
	for (i = 0; i < LIMB_COUNT; i++) {
		total += instances[h].limbs[i].bleedRate;
	}
	return total;
}

int Dismember_SpawnGibs(const vec3_t origin, const vec3_t velocity,
                        int count, gibType_t type, float force) {
	int i, spawned = 0;

	if ((int)globalConfig.goreLevel < GORE_MODERATE) return 0;

	for (i = 0; i < count && activeGibCount < globalConfig.maxActiveGibs; i++) {
		int slot;
		activeGib_t *g = NULL;

		for (slot = 0; slot < DISMEMBER_MAX_GIBS; slot++) {
			if (!gibs[slot].active) { g = &gibs[slot]; break; }
		}
		if (!g) break;

		Com_Memset(g, 0, sizeof(*g));
		g->active = qtrue;
		g->type = type;
		g->spawnTime = 0;
		g->lifetime = globalConfig.gibLifetime + (randf() - 0.5f) * 4.0f;
		g->alpha = 1.0f;

		VectorCopy(origin, g->origin);
		g->origin[0] += (randf() - 0.5f) * 16.0f;
		g->origin[1] += (randf() - 0.5f) * 16.0f;
		g->origin[2] += (randf() - 0.5f) * 16.0f;

		if (velocity) {
			VectorCopy(velocity, g->velocity);
		}
		g->velocity[0] += (randf() - 0.5f) * force * 0.5f;
		g->velocity[1] += (randf() - 0.5f) * force * 0.5f;
		g->velocity[2] += randf() * force * 0.8f + force * 0.2f;

		VectorScale(g->velocity, globalConfig.gibVelocityScale, g->velocity);

		g->angularVel[0] = (randf() - 0.5f) * 720.0f;
		g->angularVel[1] = (randf() - 0.5f) * 720.0f;
		g->angularVel[2] = (randf() - 0.5f) * 720.0f;

		g->size = 2.0f + randf() * 6.0f;

		if (globalConfig.bloodTrails) {
			g->bleedInterval = 0.05f + randf() * 0.1f;
			g->lastBleed = 0;
		}

		if (globalConfig.physicsGibs) {
			physBodyDef_t bodyDef;
			Com_Memset(&bodyDef, 0, sizeof(bodyDef));
			bodyDef.shape = PHYS_SHAPE_SPHERE;
			bodyDef.type = PHYS_BODY_DYNAMIC;
			VectorCopy(g->origin, bodyDef.position);
			bodyDef.radius = g->size * 0.5f;
			bodyDef.mass = g->size * 0.5f;
			bodyDef.friction = 0.6f;
			bodyDef.restitution = 0.3f;
			bodyDef.linearDamping = 0.1f;
			bodyDef.angularDamping = 0.3f;
			bodyDef.collisionGroup = 1;
			bodyDef.collisionMask = -1;

			g->physBody = Phys_CreateBody(&bodyDef);
			if (g->physBody >= 0) {
				vec3_t zero;
				VectorClear(zero);
				Phys_ApplyImpulse(g->physBody, g->velocity, zero);
			}
		} else {
			g->physBody = -1;
		}

		activeGibCount++;
		spawned++;
	}

	return spawned;
}

void Dismember_Update(float dt) {
	int i;

	if (!g_dismemberEnabled || !g_dismemberEnabled->integer) return;

	for (i = 0; i < DISMEMBER_MAX_GIBS; i++) {
		activeGib_t *g = &gibs[i];
		if (!g->active) continue;

		g->spawnTime += dt;

		if (g->physBody >= 0) {
			physTransform_t t;
			Phys_GetBodyTransform(g->physBody, &t);
			VectorCopy(t.position, g->origin);
		} else {
			g->velocity[2] -= 800.0f * dt;
			VectorMA(g->origin, dt, g->velocity, g->origin);

			g->angles[0] += g->angularVel[0] * dt;
			g->angles[1] += g->angularVel[1] * dt;
			g->angles[2] += g->angularVel[2] * dt;
		}

		float fadeStart = g->lifetime - globalConfig.gibFadeTime;
		if (g->spawnTime > fadeStart) {
			g->alpha = 1.0f - (g->spawnTime - fadeStart) / globalConfig.gibFadeTime;
			if (g->alpha < 0) g->alpha = 0;
		}

		if (g->spawnTime >= g->lifetime) {
			if (g->physBody >= 0) {
				Phys_DestroyBody(g->physBody);
			}
			g->active = qfalse;
			activeGibCount--;
		}
	}

	for (i = 0; i < instanceCount; i++) {
		dismemberInstance_t *inst = &instances[i];
		if (!inst->active) continue;

		int li;
		inst->totalBleedRate = 0;
		for (li = 0; li < LIMB_COUNT; li++) {
			if (inst->limbs[li].bleedRate > 0) {
				inst->limbs[li].bleedRate -= dt * 0.5f;
				if (inst->limbs[li].bleedRate < 0) inst->limbs[li].bleedRate = 0;
				inst->totalBleedRate += inst->limbs[li].bleedRate;
			}
		}
	}
}

void Dismember_ClearGibs(void) {
	int i;
	for (i = 0; i < DISMEMBER_MAX_GIBS; i++) {
		if (gibs[i].active && gibs[i].physBody >= 0) {
			Phys_DestroyBody(gibs[i].physBody);
		}
	}
	Com_Memset(gibs, 0, sizeof(gibs));
	activeGibCount = 0;
}

int Dismember_GetActiveGibCount(void) { return activeGibCount; }
