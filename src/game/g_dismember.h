/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Dismemberment and extended gibs system.
Provides limb separation, wound channels, configurable gore levels,
physics-driven gib spawning, and blood trail emitters.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"

#define DISMEMBER_MAX_LIMBS      16
#define DISMEMBER_MAX_GIBS       128
#define DISMEMBER_MAX_WOUNDS     32
#define DISMEMBER_MAX_CONFIGS    32

typedef enum {
	LIMB_HEAD,
	LIMB_NECK,
	LIMB_TORSO_UPPER,
	LIMB_TORSO_LOWER,
	LIMB_ARM_UPPER_L,
	LIMB_ARM_LOWER_L,
	LIMB_HAND_L,
	LIMB_ARM_UPPER_R,
	LIMB_ARM_LOWER_R,
	LIMB_HAND_R,
	LIMB_LEG_UPPER_L,
	LIMB_LEG_LOWER_L,
	LIMB_FOOT_L,
	LIMB_LEG_UPPER_R,
	LIMB_LEG_LOWER_R,
	LIMB_FOOT_R,
	LIMB_COUNT
} limbId_t;

typedef enum {
	GIB_FLESH,
	GIB_BONE,
	GIB_ORGAN,
	GIB_ARMOR_FRAGMENT,
	GIB_CLOTH,
	GIB_MECHANICAL,
	GIB_CUSTOM
} gibType_t;

typedef enum {
	WOUND_NONE,
	WOUND_SCRATCH,
	WOUND_LACERATION,
	WOUND_PUNCTURE,
	WOUND_GUNSHOT,
	WOUND_BURN,
	WOUND_AMPUTATION,
	WOUND_EXPLOSION
} woundType_t;

typedef enum {
	GORE_NONE,
	GORE_MINIMAL,
	GORE_MODERATE,
	GORE_FULL,
	GORE_EXTREME
} goreLevel_t;

typedef struct limbDef_s {
	limbId_t    id;
	char        modelName[MAX_QPATH];
	char        gibModel[MAX_QPATH];
	int         parentLimb;
	int         boneIndex;
	vec3_t      attachOffset;
	float       health;
	float       severThreshold;
	float       mass;
	qboolean    vital;
	int         gibCount;
	gibType_t   gibTypes[8];
} limbDef_t;

typedef struct limbState_s {
	limbId_t    id;
	float       health;
	float       maxHealth;
	qboolean    attached;
	qboolean    severed;
	woundType_t activeWound;
	float       bleedRate;
	float       bleedTimer;
	int         entityNum;
	vec3_t      lastPosition;
	vec3_t      velocity;
} limbState_t;

typedef struct gibDef_s {
	gibType_t   type;
	char        model[MAX_QPATH];
	char        shader[MAX_QPATH];
	float       minSize;
	float       maxSize;
	float       mass;
	float       bounce;
	float       lifetime;
	float       fadeTime;
	qboolean    leaveDecal;
	qboolean    trailBlood;
	int         trailDensity;
} gibDef_t;

typedef struct activeGib_s {
	qboolean    active;
	gibType_t   type;
	vec3_t      origin;
	vec3_t      velocity;
	vec3_t      angles;
	vec3_t      angularVel;
	float       size;
	float       spawnTime;
	float       lifetime;
	float       alpha;
	qhandle_t   model;
	qhandle_t   shader;
	int         physBody;
	qboolean    onGround;
	float       groundTime;
	float       bleedInterval;
	float       lastBleed;
} activeGib_t;

typedef struct woundEntry_s {
	woundType_t type;
	limbId_t    limb;
	vec3_t      position;
	vec3_t      direction;
	float       severity;
	float       bleedRate;
	float       time;
	qboolean    active;
} woundEntry_t;

typedef struct dismemberConfig_s {
	char        name[64];
	goreLevel_t goreLevel;
	float       severDamageScale;
	float       gibVelocityScale;
	float       bleedRateScale;
	float       gibLifetime;
	float       gibFadeTime;
	int         maxActiveGibs;
	int         gibsPerSever;
	float       decalSize;
	qboolean    physicsGibs;
	qboolean    bloodTrails;
	qboolean    persistentGibs;
	float       persistDuration;
} dismemberConfig_t;

typedef int dismemberHandle_t;

void Dismember_Init(void);
void Dismember_Shutdown(void);
void Dismember_RegisterCvars(void);
void Dismember_Update(float dt);

dismemberHandle_t Dismember_CreateInstance(int entityNum);
void              Dismember_DestroyInstance(dismemberHandle_t handle);
void              Dismember_SetConfig(const dismemberConfig_t *config);
void              Dismember_DefaultConfig(dismemberConfig_t *config);
void              Dismember_SetLimbDef(dismemberHandle_t handle, const limbDef_t *def);

void Dismember_ApplyDamage(dismemberHandle_t handle, limbId_t limb,
                           float damage, woundType_t woundType,
                           const vec3_t hitPoint, const vec3_t hitDir);

qboolean Dismember_SeverLimb(dismemberHandle_t handle, limbId_t limb,
                             const vec3_t force);

void Dismember_Explode(dismemberHandle_t handle, const vec3_t origin,
                       float force, float radius);

void Dismember_GetLimbState(dismemberHandle_t handle, limbId_t limb,
                            limbState_t *state);

qboolean Dismember_IsLimbAttached(dismemberHandle_t handle, limbId_t limb);
int      Dismember_GetSeveredCount(dismemberHandle_t handle);
float    Dismember_GetBleedRate(dismemberHandle_t handle);

int  Dismember_SpawnGibs(const vec3_t origin, const vec3_t velocity,
                         int count, gibType_t type, float force);
void Dismember_ClearGibs(void);
int  Dismember_GetActiveGibCount(void);

#ifdef __cplusplus
}
#endif
