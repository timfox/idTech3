/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Particle system implementation.
Pool-based freelist allocator with 8192 particles, per-type update
and billboard rendering. Supports weather, smoke, fire, debris,
sparks, blood, explosions, and animated sprite sequences.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "../renderers/common/tr_types.h"
#include "../renderers/common/tr_public.h"
#include "cl_particles.h"

extern refexport_t re;

static particle_t   particles[MAX_PARTICLES];
static particle_t  *freeParticles;
static particle_t  *activeParticles;

static particleAnim_t particleAnims[MAX_PARTICLE_ANIMS];
static int numParticleAnims;

static float currentTime;
static int activeCount;

static void Particle_GetColor(particleColor_t preset, vec4_t out) {
	switch (preset) {
		case PC_BLOOD_RED:      Vector4Set(out, 0.55f, 0.08f, 0.04f, 1.0f); break;
		case PC_FIRE_ORANGE:    Vector4Set(out, 1.0f, 0.6f, 0.1f, 1.0f); break;
		case PC_SMOKE_GREY:     Vector4Set(out, 0.5f, 0.5f, 0.5f, 1.0f); break;
		case PC_SMOKE_DARK:     Vector4Set(out, 0.2f, 0.2f, 0.2f, 1.0f); break;
		case PC_MUSTARD:        Vector4Set(out, 0.42f, 0.33f, 0.19f, 1.0f); break;
		case PC_EMISSIVE_FADE:  Vector4Set(out, 1.0f, 1.0f, 1.0f, 1.0f); break;
		default:                Vector4Set(out, 1.0f, 1.0f, 1.0f, 1.0f); break;
	}
}

/*
===============
Particles_Init
===============
*/
void Particles_Init(void) {
	Particles_Clear();
	numParticleAnims = 0;
	Com_Memset(particleAnims, 0, sizeof(particleAnims));
	Com_Printf("Particle system initialized (%d pool)\n", MAX_PARTICLES);
}

/*
===============
Particles_Clear
===============
*/
void Particles_Clear(void) {
	int i;

	Com_Memset(particles, 0, sizeof(particles));
	freeParticles = &particles[0];
	activeParticles = NULL;
	activeCount = 0;

	for (i = 0; i < MAX_PARTICLES - 1; i++) {
		particles[i].next = &particles[i + 1];
	}
	particles[MAX_PARTICLES - 1].next = NULL;
}

/*
===============
Particles_Alloc
===============
*/
particle_t *Particles_Alloc(void) {
	particle_t *p;

	if (!freeParticles) {
		return NULL;
	}

	p = freeParticles;
	freeParticles = p->next;
	p->next = activeParticles;
	activeParticles = p;
	activeCount++;

	return p;
}

static void Particle_Free(particle_t *p) {
	Com_Memset(p, 0, sizeof(*p));
	p->next = freeParticles;
	freeParticles = p;
	activeCount--;
}

/*
===============
Particles_Update
===============
*/
void Particles_Update(float time, float frametime) {
	particle_t *p, *next;
	particle_t *newActive = NULL;
	particle_t *tail = NULL;
	float t;

	currentTime = time;
	(void)frametime;

	for (p = activeParticles; p; p = next) {
		next = p->next;

		t = (currentTime - p->spawnTime) * 0.001f;

		if (p->alpha + t * p->alphaRate <= 0.0f) {
			Particle_Free(p);
			continue;
		}

		if (p->endTime > 0 && currentTime > p->endTime) {
			switch (p->type) {
				case PT_SMOKE:
				case PT_SMOKE_IMPACT:
				case PT_SMOKE_TRAIL:
				case PT_FIRE:
				case PT_FIRE_EMBERS:
				case PT_SPRITE_ANIMATED:
				case PT_SPRITE_DLIGHT:
				case PT_BLOOD_SPRAY:
				case PT_BLOOD_DROP:
				case PT_FLAT_FADEOUT:
				case PT_WEATHER_FLURRY:
				case PT_EXPLOSION:
				case PT_SHOCKWAVE:
					Particle_Free(p);
					continue;
				default:
					break;
			}
		}

		switch (p->type) {
			case PT_WEATHER_RAIN:
			case PT_WEATHER_SNOW:
				if (p->origin[2] < p->endZ) {
					p->spawnTime = currentTime;
					p->origin[2] = p->startZ + ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * (p->startZ - p->endZ);
					if (p->type == PT_WEATHER_SNOW) {
						p->velocity[0] = ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 16.0f;
						p->velocity[1] = ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 16.0f;
					}
				}
				break;

			case PT_BUBBLE:
			case PT_BUBBLE_TURBULENT:
				if (p->origin[2] > p->endZ) {
					p->spawnTime = currentTime;
					p->origin[2] = p->startZ + ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 4.0f;
					if (p->type == PT_BUBBLE_TURBULENT) {
						p->velocity[0] = ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 4.0f;
						p->velocity[1] = ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 4.0f;
					}
				}
				break;

			default:
				break;
		}

		p->next = NULL;
		if (!tail) {
			newActive = tail = p;
		} else {
			tail->next = p;
			tail = p;
		}
	}

	activeParticles = newActive;
}

/*
===============
Particles_Render
===============
*/
void Particles_Render(const vec3_t viewOrigin, const vec3_t viewForward,
                      const vec3_t viewRight, const vec3_t viewUp) {
	particle_t *p;
	float t, t2, alpha, lifeRatio, w, h;
	vec3_t org;
	polyVert_t verts[4];
	vec3_t point;
	vec4_t pcolor;
	byte r, g, b, a;

	(void)viewOrigin;
	(void)viewForward;

	for (p = activeParticles; p; p = p->next) {
		if (p->type == PT_NONE) {
			continue;
		}

		if (!p->shader) {
			p->shader = re.RegisterShader("white");
			if (!p->shader) continue;
		}

		t = (currentTime - p->spawnTime) * 0.001f;
		t2 = t * t;

		org[0] = p->origin[0] + p->velocity[0] * t + p->acceleration[0] * t2;
		org[1] = p->origin[1] + p->velocity[1] * t + p->acceleration[1] * t2;
		org[2] = p->origin[2] + p->velocity[2] * t + p->acceleration[2] * t2;

		alpha = p->alpha + t * p->alphaRate;
		if (alpha > 1.0f) alpha = 1.0f;
		if (alpha <= 0.0f) continue;

		if (p->fadeStartTime > 0 && currentTime > p->fadeStartTime && p->endTime > p->fadeStartTime) {
			float fadeRatio = (currentTime - p->fadeStartTime) / (p->endTime - p->fadeStartTime);
			alpha *= (1.0f - fadeRatio);
		}

		lifeRatio = (p->endTime > p->spawnTime) ?
			(currentTime - p->spawnTime) / (p->endTime - p->spawnTime) : 0.0f;
		if (lifeRatio > 1.0f) lifeRatio = 1.0f;

		w = p->width + lifeRatio * (p->endWidth - p->width);
		h = p->height + lifeRatio * (p->endHeight - p->height);

		Particle_GetColor(p->colorPreset, pcolor);
		if (p->colorPreset == PC_CUSTOM) {
			Vector4Copy(p->color, pcolor);
		}
		r = (byte)(pcolor[0] * 255);
		g = (byte)(pcolor[1] * 255);
		b = (byte)(pcolor[2] * 255);
		a = (byte)(alpha * 255);

		if (p->type == PT_FLAT || p->type == PT_FLAT_SCALEUP || p->type == PT_FLAT_FADEOUT) {
			VectorCopy(org, verts[0].xyz);
			verts[0].xyz[0] -= h; verts[0].xyz[1] -= w;
			verts[0].st[0] = 0; verts[0].st[1] = 0;
			verts[0].modulate.rgba[0] = r; verts[0].modulate.rgba[1] = g; verts[0].modulate.rgba[2] = b; verts[0].modulate.rgba[3] = a;

			VectorCopy(org, verts[1].xyz);
			verts[1].xyz[0] -= h; verts[1].xyz[1] += w;
			verts[1].st[0] = 0; verts[1].st[1] = 1;
			verts[1].modulate.rgba[0] = r; verts[1].modulate.rgba[1] = g; verts[1].modulate.rgba[2] = b; verts[1].modulate.rgba[3] = a;

			VectorCopy(org, verts[2].xyz);
			verts[2].xyz[0] += h; verts[2].xyz[1] += w;
			verts[2].st[0] = 1; verts[2].st[1] = 1;
			verts[2].modulate.rgba[0] = r; verts[2].modulate.rgba[1] = g; verts[2].modulate.rgba[2] = b; verts[2].modulate.rgba[3] = a;

			VectorCopy(org, verts[3].xyz);
			verts[3].xyz[0] += h; verts[3].xyz[1] -= w;
			verts[3].st[0] = 1; verts[3].st[1] = 0;
			verts[3].modulate.rgba[0] = r; verts[3].modulate.rgba[1] = g; verts[3].modulate.rgba[2] = b; verts[3].modulate.rgba[3] = a;
		} else {
			VectorMA(org, -h, viewUp, point);
			VectorMA(point, -w, viewRight, point);
			VectorCopy(point, verts[0].xyz);
			verts[0].st[0] = 0; verts[0].st[1] = 0;
			verts[0].modulate.rgba[0] = r; verts[0].modulate.rgba[1] = g; verts[0].modulate.rgba[2] = b; verts[0].modulate.rgba[3] = a;

			VectorMA(org, -h, viewUp, point);
			VectorMA(point, w, viewRight, point);
			VectorCopy(point, verts[1].xyz);
			verts[1].st[0] = 0; verts[1].st[1] = 1;
			verts[1].modulate.rgba[0] = r; verts[1].modulate.rgba[1] = g; verts[1].modulate.rgba[2] = b; verts[1].modulate.rgba[3] = a;

			VectorMA(org, h, viewUp, point);
			VectorMA(point, w, viewRight, point);
			VectorCopy(point, verts[2].xyz);
			verts[2].st[0] = 1; verts[2].st[1] = 1;
			verts[2].modulate.rgba[0] = r; verts[2].modulate.rgba[1] = g; verts[2].modulate.rgba[2] = b; verts[2].modulate.rgba[3] = a;

			VectorMA(org, h, viewUp, point);
			VectorMA(point, -w, viewRight, point);
			VectorCopy(point, verts[3].xyz);
			verts[3].st[0] = 1; verts[3].st[1] = 0;
			verts[3].modulate.rgba[0] = r; verts[3].modulate.rgba[1] = g; verts[3].modulate.rgba[2] = b; verts[3].modulate.rgba[3] = a;
		}

		re.AddPolyToScene(p->shader, 4, verts, 1);
	}
}

/*
===============
Particles_ActiveCount
===============
*/
int Particles_ActiveCount(void) {
	return activeCount;
}

/*
===============
Particles_EmitSmoke
===============
*/
void Particles_EmitSmoke(qhandle_t shader, const vec3_t origin, const vec3_t dir,
                         float lifetime, float startSize, float endSize,
                         float alpha, particleColor_t color) {
	particle_t *p = Particles_Alloc();
	if (!p) return;

	if (!shader) shader = re.RegisterShader("white");

	p->spawnTime = currentTime;
	p->endTime = currentTime + lifetime;
	p->fadeStartTime = currentTime + lifetime * 0.3f;
	p->type = PT_SMOKE;
	p->shader = shader;
	p->alpha = alpha;
	p->alphaRate = 0;
	p->colorPreset = color;
	Particle_GetColor(color, p->color);

	p->width = startSize;
	p->height = startSize;
	p->endWidth = endSize;
	p->endHeight = endSize;

	VectorCopy(origin, p->origin);
	if (dir) {
		VectorCopy(dir, p->velocity);
	} else {
		VectorSet(p->velocity, 0, 0, 5);
	}
	VectorSet(p->acceleration, 0, 0, 0);

	p->rotate = qtrue;
	p->roll = (rand() % 16) - 8;
}

/*
===============
Particles_EmitSparks
===============
*/
void Particles_EmitSparks(const vec3_t origin, const vec3_t baseVel,
                          int count, float speed, float lifetime) {
	int i;
	for (i = 0; i < count; i++) {
		particle_t *p = Particles_Alloc();
		if (!p) return;

		p->spawnTime = currentTime;
		p->endTime = currentTime + lifetime;
		p->fadeStartTime = currentTime + lifetime * 0.5f;
		p->type = PT_SPARKS;
		p->alpha = 0.4f;
		p->alphaRate = 0;
		p->colorPreset = PC_EMISSIVE_FADE;
		Particle_GetColor(PC_EMISSIVE_FADE, p->color);

		p->width = 0.5f;
		p->height = 0.5f;
		p->endWidth = 0.5f;
		p->endHeight = 0.5f;

		VectorCopy(origin, p->origin);
		p->origin[0] += ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 4.0f;
		p->origin[1] += ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 4.0f;

		if (baseVel) {
			VectorCopy(baseVel, p->velocity);
		} else {
			VectorClear(p->velocity);
		}
		p->velocity[0] += ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 4.0f;
		p->velocity[1] += ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 4.0f;
		p->velocity[2] += (20.0f + ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 10.0f) * speed;

		p->acceleration[0] = ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 3.0f;
		p->acceleration[1] = ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 3.0f;
		p->acceleration[2] = -60.0f;
	}
}

/*
===============
Particles_EmitDebris
===============
*/
void Particles_EmitDebris(const vec3_t origin, const vec3_t baseVel,
                          int count, float lifetime, qhandle_t shader) {
	int i;
	for (i = 0; i < count; i++) {
		particle_t *p = Particles_Alloc();
		if (!p) return;

		p->spawnTime = currentTime;
		p->endTime = currentTime + lifetime;
		p->fadeStartTime = currentTime + lifetime * 0.5f;
		p->type = PT_DEBRIS;
		p->shader = shader;
		p->alpha = 1.0f;
		p->alphaRate = 0;
		p->colorPreset = PC_EMISSIVE_FADE;
		Particle_GetColor(PC_EMISSIVE_FADE, p->color);

		p->width = 0.5f;
		p->height = 0.5f;
		p->endWidth = 0.5f;
		p->endHeight = 0.5f;

		VectorCopy(origin, p->origin);
		if (baseVel) {
			VectorCopy(baseVel, p->velocity);
		}
		p->velocity[2] -= 20.0f;
		VectorSet(p->acceleration, 0, 0, -60.0f);
	}
}

/*
===============
Particles_EmitExplosion
===============
*/
void Particles_EmitExplosion(const char *animName, const vec3_t origin,
                             const vec3_t vel, float lifetime,
                             float startSize, float endSize, qboolean dlight) {
	particle_t *p;
	int anim, i;

	for (anim = 0; anim < numParticleAnims; anim++) {
		if (!Q_stricmp(animName, particleAnims[anim].name)) {
			break;
		}
	}

	p = Particles_Alloc();
	if (!p) return;

	p->spawnTime = currentTime;
	p->endTime = currentTime + lifetime;
	p->fadeStartTime = currentTime;
	p->type = dlight ? PT_SPRITE_DLIGHT : PT_SPRITE_ANIMATED;
	p->alpha = 1.0f;
	p->alphaRate = 0;

	if (anim < numParticleAnims) {
		p->animIndex = anim;
		p->height = startSize * particleAnims[anim].aspectRatio;
		p->endHeight = endSize * particleAnims[anim].aspectRatio;
	} else {
		p->height = startSize;
		p->endHeight = endSize;
	}
	p->width = startSize;
	p->endWidth = endSize;

	p->roll = (rand() % 358) - 179;

	VectorCopy(origin, p->origin);
	if (vel) {
		VectorCopy(vel, p->velocity);
	}
	VectorClear(p->acceleration);

	(void)i;
}

/*
===============
Particles_EmitWeather
===============
*/
void Particles_EmitWeather(qhandle_t shader, const vec3_t origin,
                           float ceilHeight, float floorHeight,
                           float range, particleType_t type) {
	particle_t *p = Particles_Alloc();
	if (!p) return;

	p->spawnTime = currentTime;
	p->endTime = 0;
	p->type = type;
	p->shader = shader;
	p->alpha = 0.4f;
	p->alphaRate = 0;

	p->width = 1.0f;
	p->height = 1.0f;
	p->endWidth = 1.0f;
	p->endHeight = 1.0f;

	p->startZ = ceilHeight;
	p->endZ = floorHeight;

	VectorCopy(origin, p->origin);
	p->origin[0] += ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * range;
	p->origin[1] += ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * range;
	p->origin[2] += ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * (ceilHeight - floorHeight);

	VectorSet(p->velocity, 0, 0, -50.0f);
	if (type == PT_WEATHER_TURBULENT || type == PT_WEATHER_SNOW) {
		p->velocity[0] = ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 16.0f;
		p->velocity[1] = ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 16.0f;
		if (type == PT_WEATHER_SNOW) {
			p->velocity[2] = -30.0f;
		}
	}
	VectorClear(p->acceleration);
	p->linked = qtrue;
}

/*
===============
Particles_EmitBubbles
===============
*/
void Particles_EmitBubbles(qhandle_t shader, const vec3_t origin,
                           float floorDepth, float surfaceHeight,
                           float range, qboolean turbulent) {
	particle_t *p = Particles_Alloc();
	float rsize;
	if (!p) return;

	p->spawnTime = currentTime;
	p->endTime = 0;
	p->type = turbulent ? PT_BUBBLE_TURBULENT : PT_BUBBLE;
	p->shader = shader;
	p->alpha = 0.4f;
	p->alphaRate = 0;

	rsize = 1.0f + ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 0.5f;
	p->width = rsize;
	p->height = rsize;
	p->endWidth = rsize;
	p->endHeight = rsize;

	p->startZ = floorDepth;
	p->endZ = surfaceHeight;

	VectorCopy(origin, p->origin);
	p->origin[0] += ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * range;
	p->origin[1] += ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * range;

	VectorSet(p->velocity, 0, 0, 50.0f + ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 10.0f);
	if (turbulent) {
		p->velocity[0] = ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 4.0f;
		p->velocity[1] = ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 4.0f;
	}
	VectorClear(p->acceleration);
	p->linked = qtrue;
}

/*
===============
Particles_EmitBlood
===============
*/
void Particles_EmitBlood(qhandle_t shader, const vec3_t origin,
                         const vec3_t dir, float duration) {
	particle_t *p = Particles_Alloc();
	if (!p) return;

	p->spawnTime = currentTime;
	p->endTime = currentTime + duration;
	p->fadeStartTime = currentTime + 100.0f;
	p->type = PT_BLOOD_SPRAY;
	p->shader = shader;
	p->alpha = 0.75f;
	p->alphaRate = 0;
	p->colorPreset = PC_BLOOD_RED;
	Particle_GetColor(PC_BLOOD_RED, p->color);

	p->width = 4.0f;
	p->height = 4.0f;
	p->endWidth = 4.0f + (float)(rand() % 3);
	p->endHeight = p->endWidth;

	VectorCopy(origin, p->origin);
	VectorSet(p->velocity, 0, 0, -20.0f);
	VectorClear(p->acceleration);

	p->rotate = qfalse;
	p->roll = rand() % 179;

	(void)dir;
}

/*
===============
Particles_EmitDust
===============
*/
void Particles_EmitDust(const vec3_t origin, const vec3_t dir,
                        float size, qhandle_t shader) {
	particle_t *p = Particles_Alloc();
	if (!p) return;

	p->spawnTime = currentTime;
	p->endTime = currentTime + 4500.0f + ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 3500.0f;
	p->fadeStartTime = currentTime;
	p->type = PT_DUST;
	p->shader = shader;
	p->alpha = 0.75f;
	p->alphaRate = 0;
	p->colorPreset = PC_MUSTARD;
	Particle_GetColor(PC_MUSTARD, p->color);

	p->width = size;
	p->height = size;
	p->endWidth = size * 4.0f;
	p->endHeight = size * 4.0f;

	VectorCopy(origin, p->origin);
	p->velocity[0] = ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 6.0f;
	p->velocity[1] = ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 6.0f;
	p->velocity[2] = ((float)(rand() & 0x7FFF) / 0x7FFF) * 20.0f;

	p->acceleration[0] = ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 3.0f;
	p->acceleration[1] = ((float)(rand() & 0x7FFF) / 0x7FFF - 0.5f) * 3.0f;
	p->acceleration[2] = -16.0f;

	p->rotate = qfalse;
	p->roll = rand() % 179;

	(void)dir;
}

/*
===============
Particles_EmitImpactPuff
===============
*/
void Particles_EmitImpactPuff(qhandle_t shader, const vec3_t origin,
                              float lifetime, float vel, float accel,
                              float alpha, float size) {
	particle_t *p = Particles_Alloc();
	if (!p) return;

	p->spawnTime = currentTime;
	p->endTime = currentTime + lifetime;
	p->fadeStartTime = currentTime + 100.0f;
	p->type = PT_SMOKE_IMPACT;
	p->shader = shader;
	p->alpha = alpha;
	p->alphaRate = 0;

	p->width = size * (1.0f + ((float)(rand() & 0x7FFF) / 0x7FFF) * 0.5f);
	p->height = size * (1.0f + ((float)(rand() & 0x7FFF) / 0x7FFF) * 0.5f);
	p->endWidth = p->width * 2.0f;
	p->endHeight = p->height * 2.0f;

	p->roll = (rand() % 60) - 30;
	p->rotate = qtrue;

	VectorCopy(origin, p->origin);
	VectorSet(p->velocity, 0, 0, vel);
	VectorSet(p->acceleration, 0, 0, accel);
}
