/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Particle system for id Tech 3 engine.
Pool-based particle manager with freelist allocation,
supporting weather, smoke, debris, sprites, animated sequences,
volumetric effects, and dynamic light-emitting particles.
===========================================================================
*/

#pragma once

#include "../qcommon/q_shared.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PARTICLES           8192
#define MAX_PARTICLE_ANIMS      8
#define MAX_PARTICLE_ANIM_FRAMES 64

typedef enum {
	PT_NONE = 0,
	PT_WEATHER_RAIN,
	PT_WEATHER_SNOW,
	PT_WEATHER_TURBULENT,
	PT_WEATHER_FLURRY,
	PT_SMOKE,
	PT_SMOKE_IMPACT,
	PT_SMOKE_TRAIL,
	PT_FIRE,
	PT_FIRE_EMBERS,
	PT_SPRITE,
	PT_SPRITE_ANIMATED,
	PT_SPRITE_DLIGHT,
	PT_FLAT,
	PT_FLAT_SCALEUP,
	PT_FLAT_FADEOUT,
	PT_DEBRIS,
	PT_SPARKS,
	PT_BLOOD_SPRAY,
	PT_BLOOD_DROP,
	PT_BUBBLE,
	PT_BUBBLE_TURBULENT,
	PT_DUST,
	PT_EXPLOSION,
	PT_SHOCKWAVE,
	PT_BEAM,
	PT_COUNT
} particleType_t;

typedef enum {
	PC_WHITE = 0,
	PC_BLOOD_RED,
	PC_FIRE_ORANGE,
	PC_SMOKE_GREY,
	PC_SMOKE_DARK,
	PC_MUSTARD,
	PC_EMISSIVE_FADE,
	PC_CUSTOM
} particleColor_t;

typedef struct particle_s {
	struct particle_s *next;

	float           spawnTime;
	float           endTime;
	float           fadeStartTime;

	vec3_t          origin;
	vec3_t          velocity;
	vec3_t          acceleration;

	particleType_t  type;
	particleColor_t colorPreset;
	vec4_t          color;

	float           alpha;
	float           alphaRate;

	float           width;
	float           height;
	float           endWidth;
	float           endHeight;

	float           startZ;
	float           endZ;

	qhandle_t       shader;
	int             animIndex;
	int             roll;
	float           rollAccum;
	qboolean        rotate;

	int             sortKey;
	qboolean        linked;
} particle_t;

typedef struct particleAnim_s {
	char        name[MAX_QPATH];
	int         frameCount;
	float       aspectRatio;
	qhandle_t   frames[MAX_PARTICLE_ANIM_FRAMES];
} particleAnim_t;

void Particles_Init(void);
void Particles_Clear(void);
void Particles_Update(float time, float frametime);
void Particles_Render(const vec3_t viewOrigin, const vec3_t viewForward,
                      const vec3_t viewRight, const vec3_t viewUp);
int  Particles_ActiveCount(void);

particle_t *Particles_Alloc(void);

void Particles_EmitSmoke(qhandle_t shader, const vec3_t origin, const vec3_t dir,
                         float lifetime, float startSize, float endSize,
                         float alpha, particleColor_t color);

void Particles_EmitSparks(const vec3_t origin, const vec3_t baseVel,
                          int count, float speed, float lifetime);

void Particles_EmitDebris(const vec3_t origin, const vec3_t baseVel,
                          int count, float lifetime, qhandle_t shader);

void Particles_EmitExplosion(const char *animName, const vec3_t origin,
                             const vec3_t vel, float lifetime,
                             float startSize, float endSize, qboolean dlight);

void Particles_EmitWeather(qhandle_t shader, const vec3_t origin,
                           float ceilHeight, float floorHeight,
                           float range, particleType_t type);

void Particles_EmitBubbles(qhandle_t shader, const vec3_t origin,
                           float floorDepth, float surfaceHeight,
                           float range, qboolean turbulent);

void Particles_EmitBlood(qhandle_t shader, const vec3_t origin,
                         const vec3_t dir, float duration);

void Particles_EmitDust(const vec3_t origin, const vec3_t dir,
                        float size, qhandle_t shader);

void Particles_EmitImpactPuff(qhandle_t shader, const vec3_t origin,
                              float lifetime, float vel, float accel,
                              float alpha, float size);

#ifdef __cplusplus
}
#endif
