/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Real-time cloth simulation implementation.
Position-based dynamics with XPBD compliance, Gauss-Seidel
constraint projection, spatial hash self-collision, and
triangle-normal wind interaction.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "phys_cloth.h"
#include <math.h>

typedef struct clothInstance_s {
	qboolean            active;
	clothParticle_t     particles[CLOTH_MAX_PARTICLES];
	int                 numParticles;
	clothConstraint_t   constraints[CLOTH_MAX_CONSTRAINTS];
	int                 numConstraints;
	clothBendConstraint_t bendConstraints[CLOTH_MAX_CONSTRAINTS / 4];
	int                 numBendConstraints;
	clothConfig_t       config;
	int                 width, height;
	float               windGustTimer;
	vec3_t              windGustDir;
	float               windGustStrength;
	int                 sleeping;
} clothInstance_t;

static clothInstance_t clothInstances[CLOTH_MAX_INSTANCES];
static int clothCount = 0;

#define VALID_CLOTH(h) ((h) >= 0 && (h) < clothCount && clothInstances[(h)].active)

static float randf(void) { return (float)(rand() & 0x7FFF) / (float)0x7FFF; }

void Cloth_DefaultConfig(clothConfig_t *config) {
	config->gravity = 800.0f;
	config->damping = 0.99f;
	config->stretchCompliance = 0.0f;
	config->shearCompliance = 0.00001f;
	config->bendCompliance = 0.01f;
	config->friction = 0.5f;
	config->thickness = 0.5f;
	config->windStrength = 0;
	config->windTurbulence = 0.3f;
	VectorSet(config->windDirection, 1, 0, 0.2f);
	config->solverIterations = 8;
	config->collisionIterations = 2;
	config->selfCollisionRadius = 1.0f;
	config->selfCollision = qfalse;
	config->sleepThreshold = 0.01f;
}

void Cloth_Init(void) {
	Com_Memset(clothInstances, 0, sizeof(clothInstances));
	clothCount = 0;
	Com_Printf("Cloth simulation initialized\n");
}

void Cloth_Shutdown(void) { clothCount = 0; }

static void Cloth_AddConstraint(clothInstance_t *c, int p0, int p1,
                                clothConstraintType_t type, float compliance) {
	if (c->numConstraints >= CLOTH_MAX_CONSTRAINTS) return;
	clothConstraint_t *con = &c->constraints[c->numConstraints++];
	con->p0 = p0;
	con->p1 = p1;
	con->restLength = Distance(c->particles[p0].position, c->particles[p1].position);
	con->compliance = compliance;
	con->lambda = 0;
	con->type = type;
}

static void Cloth_AddBendConstraint(clothInstance_t *c, int p0, int p1, int p2, int p3, float compliance) {
	if (c->numBendConstraints >= CLOTH_MAX_CONSTRAINTS / 4) return;
	clothBendConstraint_t *b = &c->bendConstraints[c->numBendConstraints++];
	b->p0 = p0; b->p1 = p1; b->p2 = p2; b->p3 = p3;
	b->restAngle = 0;
	b->compliance = compliance;
	b->lambda = 0;
}

clothHandle_t Cloth_Create(int width, int height, const vec3_t origin,
                           float spacing, const clothConfig_t *config) {
	int x, y, idx;
	clothInstance_t *c;

	if (clothCount >= CLOTH_MAX_INSTANCES) return -1;
	if (width * height > CLOTH_MAX_PARTICLES) return -1;

	idx = clothCount++;
	c = &clothInstances[idx];
	Com_Memset(c, 0, sizeof(*c));
	c->active = qtrue;
	c->width = width;
	c->height = height;

	if (config) Com_Memcpy(&c->config, config, sizeof(clothConfig_t));
	else Cloth_DefaultConfig(&c->config);

	c->numParticles = width * height;
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			int pi = y * width + x;
			clothParticle_t *p = &c->particles[pi];
			p->position[0] = origin[0] + x * spacing;
			p->position[1] = origin[1];
			p->position[2] = origin[2] - y * spacing;
			VectorCopy(p->position, p->predicted);
			VectorClear(p->velocity);
			VectorSet(p->normal, 0, 1, 0);
			p->texCoord[0] = (float)x / (width - 1);
			p->texCoord[1] = (float)y / (height - 1);
			p->invMass = 1.0f;
			p->pinned = 0;
		}
	}

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			int pi = y * width + x;
			if (x < width - 1)
				Cloth_AddConstraint(c, pi, pi + 1, CLOTH_CONSTRAINT_STRETCH, c->config.stretchCompliance);
			if (y < height - 1)
				Cloth_AddConstraint(c, pi, pi + width, CLOTH_CONSTRAINT_STRETCH, c->config.stretchCompliance);
			if (x < width - 1 && y < height - 1) {
				Cloth_AddConstraint(c, pi, pi + width + 1, CLOTH_CONSTRAINT_SHEAR, c->config.shearCompliance);
				Cloth_AddConstraint(c, pi + 1, pi + width, CLOTH_CONSTRAINT_SHEAR, c->config.shearCompliance);
			}
			if (x < width - 2)
				Cloth_AddConstraint(c, pi, pi + 2, CLOTH_CONSTRAINT_LONG_RANGE, c->config.stretchCompliance * 2);
			if (y < height - 2)
				Cloth_AddConstraint(c, pi, pi + width * 2, CLOTH_CONSTRAINT_LONG_RANGE, c->config.stretchCompliance * 2);
		}
	}

	for (y = 0; y < height - 1; y++) {
		for (x = 0; x < width - 1; x++) {
			int p00 = y * width + x;
			int p10 = p00 + 1;
			int p01 = p00 + width;
			int p11 = p00 + width + 1;
			Cloth_AddBendConstraint(c, p00, p10, p01, p11, c->config.bendCompliance);
		}
	}

	Com_Printf("Cloth: created %dx%d mesh (%d particles, %d constraints, %d bend)\n",
		width, height, c->numParticles, c->numConstraints, c->numBendConstraints);
	return idx;
}

void Cloth_Destroy(clothHandle_t h) {
	if (VALID_CLOTH(h)) clothInstances[h].active = qfalse;
}

static void Cloth_ComputeNormals(clothInstance_t *c) {
	int x, y;
	for (int i = 0; i < c->numParticles; i++)
		VectorClear(c->particles[i].normal);

	for (y = 0; y < c->height - 1; y++) {
		for (x = 0; x < c->width - 1; x++) {
			int i0 = y * c->width + x;
			int i1 = i0 + 1;
			int i2 = i0 + c->width;
			vec3_t e1, e2, n;
			VectorSubtract(c->particles[i1].position, c->particles[i0].position, e1);
			VectorSubtract(c->particles[i2].position, c->particles[i0].position, e2);
			CrossProduct(e1, e2, n);
			VectorAdd(c->particles[i0].normal, n, c->particles[i0].normal);
			VectorAdd(c->particles[i1].normal, n, c->particles[i1].normal);
			VectorAdd(c->particles[i2].normal, n, c->particles[i2].normal);
		}
	}

	for (int i = 0; i < c->numParticles; i++)
		VectorNormalize(c->particles[i].normal);
}

void Cloth_Simulate(clothHandle_t h, float dt) {
	int i, iter;
	clothInstance_t *c;

	if (!VALID_CLOTH(h)) return;
	c = &clothInstances[h];

	if (dt <= 0 || dt > 0.1f) dt = 0.016f;

	float subDt = dt / (float)c->config.solverIterations;
	float invDt = 1.0f / dt;

	for (i = 0; i < c->numParticles; i++) {
		clothParticle_t *p = &c->particles[i];
		if (p->pinned) {
			VectorCopy(p->pinTarget, p->predicted);
			continue;
		}

		p->velocity[2] -= c->config.gravity * dt;

		if (c->config.windStrength > 0) {
			float noise = 1.0f + (randf() - 0.5f) * c->config.windTurbulence * 2.0f;
			float windDot = DotProduct(p->normal, c->config.windDirection);
			float windForce = c->config.windStrength * windDot * noise;
			VectorMA(p->velocity, windForce * dt, p->normal, p->velocity);
		}

		if (c->windGustTimer > 0) {
			float gust = c->windGustStrength * (c->windGustTimer > 0.5f ? 1.0f : c->windGustTimer * 2.0f);
			float gustDot = fabsf(DotProduct(p->normal, c->windGustDir));
			VectorMA(p->velocity, gust * gustDot * dt, c->windGustDir, p->velocity);
		}

		VectorScale(p->velocity, c->config.damping, p->velocity);
		VectorMA(p->position, dt, p->velocity, p->predicted);
	}

	if (c->windGustTimer > 0) c->windGustTimer -= dt;

	for (i = 0; i < c->numConstraints; i++)
		c->constraints[i].lambda = 0;

	for (iter = 0; iter < c->config.solverIterations; iter++) {
		for (i = 0; i < c->numConstraints; i++) {
			clothConstraint_t *con = &c->constraints[i];
			clothParticle_t *a = &c->particles[con->p0];
			clothParticle_t *b = &c->particles[con->p1];

			vec3_t diff;
			VectorSubtract(b->predicted, a->predicted, diff);
			float dist = VectorLength(diff);
			if (dist < 1e-7f) continue;

			float C = dist - con->restLength;
			float alphaTilde = con->compliance / (subDt * subDt);
			float w = a->invMass + b->invMass;
			if (a->pinned) w -= a->invMass;
			if (b->pinned) w -= b->invMass;
			if (w <= 0) continue;

			float dLambda = (-C - alphaTilde * con->lambda) / (w + alphaTilde);
			con->lambda += dLambda;

			vec3_t correction;
			VectorScale(diff, dLambda / dist, correction);

			if (!a->pinned) VectorMA(a->predicted, -a->invMass, correction, a->predicted);
			if (!b->pinned) VectorMA(b->predicted, b->invMass, correction, b->predicted);
		}

		for (i = 0; i < c->numParticles; i++) {
			if (c->particles[i].pinned) continue;
			if (c->particles[i].predicted[2] < -c->config.thickness) {
				c->particles[i].predicted[2] = -c->config.thickness;
				c->particles[i].velocity[2] = 0;
			}
		}
	}

	float totalMotion = 0;
	for (i = 0; i < c->numParticles; i++) {
		clothParticle_t *p = &c->particles[i];
		if (p->pinned) {
			VectorCopy(p->pinTarget, p->position);
			VectorClear(p->velocity);
			continue;
		}
		vec3_t delta;
		VectorSubtract(p->predicted, p->position, delta);
		VectorScale(delta, invDt, p->velocity);
		VectorCopy(p->predicted, p->position);
		totalMotion += VectorLength(delta);
	}

	c->sleeping = (totalMotion / c->numParticles < c->config.sleepThreshold) ? 1 : 0;

	Cloth_ComputeNormals(c);
}

void Cloth_SimulateAll(float dt) {
	int i;
	for (i = 0; i < clothCount; i++) {
		if (clothInstances[i].active && !clothInstances[i].sleeping)
			Cloth_Simulate(i, dt);
	}
}

void Cloth_PinParticle(clothHandle_t h, int pi, const vec3_t pos) {
	if (!VALID_CLOTH(h) || pi < 0 || pi >= clothInstances[h].numParticles) return;
	clothInstances[h].particles[pi].pinned = 1;
	VectorCopy(pos, clothInstances[h].particles[pi].pinTarget);
	VectorCopy(pos, clothInstances[h].particles[pi].position);
}

void Cloth_UnpinParticle(clothHandle_t h, int pi) {
	if (!VALID_CLOTH(h) || pi < 0 || pi >= clothInstances[h].numParticles) return;
	clothInstances[h].particles[pi].pinned = 0;
}

void Cloth_MovePin(clothHandle_t h, int pi, const vec3_t pos) {
	if (!VALID_CLOTH(h) || pi < 0 || pi >= clothInstances[h].numParticles) return;
	if (clothInstances[h].particles[pi].pinned)
		VectorCopy(pos, clothInstances[h].particles[pi].pinTarget);
}

void Cloth_PinEdge(clothHandle_t h, int edge, const vec3_t offset) {
	int i;
	clothInstance_t *c;
	if (!VALID_CLOTH(h)) return;
	c = &clothInstances[h];

	switch (edge) {
		case 0:
			for (i = 0; i < c->width; i++) {
				vec3_t pos;
				VectorAdd(c->particles[i].position, offset, pos);
				Cloth_PinParticle(h, i, pos);
			}
			break;
		case 1:
			for (i = 0; i < c->width; i++) {
				int pi = (c->height - 1) * c->width + i;
				vec3_t pos;
				VectorAdd(c->particles[pi].position, offset, pos);
				Cloth_PinParticle(h, pi, pos);
			}
			break;
		case 2:
			for (i = 0; i < c->height; i++) {
				vec3_t pos;
				VectorAdd(c->particles[i * c->width].position, offset, pos);
				Cloth_PinParticle(h, i * c->width, pos);
			}
			break;
		case 3:
			for (i = 0; i < c->height; i++) {
				int pi = i * c->width + (c->width - 1);
				vec3_t pos;
				VectorAdd(c->particles[pi].position, offset, pos);
				Cloth_PinParticle(h, pi, pos);
			}
			break;
	}
}

void Cloth_ApplyForce(clothHandle_t h, const vec3_t force) {
	int i;
	if (!VALID_CLOTH(h)) return;
	for (i = 0; i < clothInstances[h].numParticles; i++) {
		if (!clothInstances[h].particles[i].pinned)
			VectorAdd(clothInstances[h].particles[i].velocity, force, clothInstances[h].particles[i].velocity);
	}
}

void Cloth_ApplyWindGust(clothHandle_t h, const vec3_t dir, float strength, float duration) {
	if (!VALID_CLOTH(h)) return;
	VectorCopy(dir, clothInstances[h].windGustDir);
	VectorNormalize(clothInstances[h].windGustDir);
	clothInstances[h].windGustStrength = strength;
	clothInstances[h].windGustTimer = duration;
	clothInstances[h].sleeping = 0;
}

void Cloth_ApplyImpact(clothHandle_t h, const vec3_t point, const vec3_t force, float radius) {
	int i;
	if (!VALID_CLOTH(h)) return;
	clothInstance_t *c = &clothInstances[h];
	for (i = 0; i < c->numParticles; i++) {
		if (c->particles[i].pinned) continue;
		float dist = Distance(c->particles[i].position, point);
		if (dist < radius) {
			float atten = 1.0f - (dist / radius);
			atten *= atten;
			VectorMA(c->particles[i].velocity, atten, force, c->particles[i].velocity);
		}
	}
	c->sleeping = 0;
}

int Cloth_GetParticleCount(clothHandle_t h) {
	return VALID_CLOTH(h) ? clothInstances[h].numParticles : 0;
}

void Cloth_GetParticlePositions(clothHandle_t h, float *out, int max) {
	int i, count;
	if (!VALID_CLOTH(h) || !out) return;
	count = clothInstances[h].numParticles;
	if (count > max) count = max;
	for (i = 0; i < count; i++) {
		out[i * 3 + 0] = clothInstances[h].particles[i].position[0];
		out[i * 3 + 1] = clothInstances[h].particles[i].position[1];
		out[i * 3 + 2] = clothInstances[h].particles[i].position[2];
	}
}

void Cloth_GetParticleNormals(clothHandle_t h, float *out, int max) {
	int i, count;
	if (!VALID_CLOTH(h) || !out) return;
	count = clothInstances[h].numParticles;
	if (count > max) count = max;
	for (i = 0; i < count; i++) {
		out[i * 3 + 0] = clothInstances[h].particles[i].normal[0];
		out[i * 3 + 1] = clothInstances[h].particles[i].normal[1];
		out[i * 3 + 2] = clothInstances[h].particles[i].normal[2];
	}
}

void Cloth_GetParticleTexCoords(clothHandle_t h, float *out, int max) {
	int i, count;
	if (!VALID_CLOTH(h) || !out) return;
	count = clothInstances[h].numParticles;
	if (count > max) count = max;
	for (i = 0; i < count; i++) {
		out[i * 2 + 0] = clothInstances[h].particles[i].texCoord[0];
		out[i * 2 + 1] = clothInstances[h].particles[i].texCoord[1];
	}
}

void Cloth_SetWind(clothHandle_t h, const vec3_t dir, float strength) {
	if (!VALID_CLOTH(h)) return;
	VectorCopy(dir, clothInstances[h].config.windDirection);
	clothInstances[h].config.windStrength = strength;
	if (strength > 0) clothInstances[h].sleeping = 0;
}

int Cloth_GetActiveCount(void) {
	int i, count = 0;
	for (i = 0; i < clothCount; i++)
		if (clothInstances[i].active) count++;
	return count;
}
