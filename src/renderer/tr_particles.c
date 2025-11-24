/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#include "tr_local.h"

static particleSystem_t particleSystem;
static cvar_t *r_particleSystem;

/*
===================
R_InitParticleSystem
===================
*/
void R_InitParticleSystem(void) {
	if (particleSystem.particles != NULL) {
		return; // Already initialized
	}

	particleSystem.maxParticles = MAX_PARTICLES;
	particleSystem.numActive = 0;
	particleSystem.nextFree = 0;

	particleSystem.particles = (particle_t *)ri.Hunk_Alloc(
		sizeof(particle_t) * particleSystem.maxParticles, h_low);
	
	if (particleSystem.particles == NULL) {
		ri.Error(ERR_FATAL, "R_InitParticleSystem: Failed to allocate particle pool");
	}

	Com_Memset(particleSystem.particles, 0, 
		sizeof(particle_t) * particleSystem.maxParticles);

	r_particleSystem = ri.Cvar_Get("r_particleSystem", "1", CVAR_ARCHIVE);
	if (ri.Cvar_SetDescription) {
		ri.Cvar_SetDescription(r_particleSystem, "Enable batch-rendered particle system (0 = old system, 1 = new system)");
	}

	ri.Printf(PRINT_ALL, "Particle system initialized: %d particles\n", 
		particleSystem.maxParticles);
}

/*
===================
R_ShutdownParticleSystem
===================
*/
void R_ShutdownParticleSystem(void) {
	if (particleSystem.particles != NULL) {
		particleSystem.particles = NULL;
		particleSystem.numActive = 0;
		particleSystem.nextFree = 0;
	}
}

/*
===================
R_ClearParticles
===================
*/
void R_ClearParticles(void) {
	if (particleSystem.particles == NULL) {
		return;
	}

	Com_Memset(particleSystem.particles, 0, 
		sizeof(particle_t) * particleSystem.maxParticles);
	particleSystem.numActive = 0;
	particleSystem.nextFree = 0;
}

/*
===================
R_AddParticle
===================
*/
void R_AddParticle(const vec3_t origin, const vec3_t velocity, 
                   const vec3_t color, float size, float life, 
                   qhandle_t shader) {
	particle_t *p;
	int i;

	if (particleSystem.particles == NULL) {
		return; // System not initialized
	}

	if (r_particleSystem && r_particleSystem->integer == 0) {
		return; // Old system enabled
	}

	// Find a free particle slot
	for (i = 0; i < particleSystem.maxParticles; i++) {
		int idx = (particleSystem.nextFree + i) % particleSystem.maxParticles;
		p = &particleSystem.particles[idx];
		
		if (!p->active) {
			VectorCopy(origin, p->origin);
			VectorCopy(velocity, p->velocity);
			VectorCopy(color, p->color);
			p->size = size;
			p->rotation = 0.0f;
			p->life = 1.0f;
			p->fade = 1.0f / life;
			p->shader = shader;
			p->spawnTime = tr.refdef.time;
			p->lifeTime = (int)(life * 1000.0f);
			p->active = qtrue;

			particleSystem.nextFree = (idx + 1) % particleSystem.maxParticles;
			particleSystem.numActive++;
			return;
		}
	}

	// No free slots - overwrite oldest particle
	if (particleSystem.numActive > 0) {
		p = &particleSystem.particles[particleSystem.nextFree];
		VectorCopy(origin, p->origin);
		VectorCopy(velocity, p->velocity);
		VectorCopy(color, p->color);
		p->size = size;
		p->rotation = 0.0f;
		p->life = 1.0f;
		p->fade = 1.0f / life;
		p->shader = shader;
		p->spawnTime = tr.refdef.time;
		p->lifeTime = (int)(life * 1000.0f);
		p->active = qtrue;

		particleSystem.nextFree = (particleSystem.nextFree + 1) % particleSystem.maxParticles;
	}
}

/*
===================
R_UpdateParticles
===================
*/
void R_UpdateParticles(float deltaTime) {
	particle_t *p;
	int i;
	int currentTime = tr.refdef.time;

	if (particleSystem.particles == NULL) {
		return;
	}

	if (r_particleSystem && r_particleSystem->integer == 0) {
		return; // Old system enabled
	}

	for (i = 0; i < particleSystem.maxParticles; i++) {
		p = &particleSystem.particles[i];
		
		if (!p->active) {
			continue;
		}

		// Update life
		int age = currentTime - p->spawnTime;
		if (age >= p->lifeTime) {
			p->active = qfalse;
			particleSystem.numActive--;
			continue;
		}

		p->life = 1.0f - ((float)age / (float)p->lifeTime);
		
		// Update position
		VectorMA(p->origin, deltaTime, p->velocity, p->origin);
		
		// Apply gravity
		p->velocity[2] -= 800.0f * deltaTime; // gravity
		
		// Update rotation
		p->rotation += 180.0f * deltaTime; // 180 degrees per second
	}
}

/*
===================
CompareParticles
===================
*/
static int CompareParticles(const void *a, const void *b) {
	const particle_t *pa = *(const particle_t **)a;
	const particle_t *pb = *(const particle_t **)b;
	
	// Sort by shader first (for batching)
	if (pa->shader < pb->shader) return -1;
	if (pa->shader > pb->shader) return 1;
	
	// Then by distance (back-to-front for transparency)
	vec3_t distA, distB;
	float distASq, distBSq;
	
	VectorSubtract(pa->origin, backEnd.viewParms.or.origin, distA);
	VectorSubtract(pb->origin, backEnd.viewParms.or.origin, distB);
	
	distASq = DotProduct(distA, distA);
	distBSq = DotProduct(distB, distB);
	
	if (distASq > distBSq) return -1;
	if (distASq < distBSq) return 1;
	
	return 0;
}

/*
===================
R_RenderParticles
===================
*/
void R_RenderParticles(void) {
	particle_t *p;
	int i, j;
	particle_t *sortedParticles[MAX_PARTICLES];
	int numSorted = 0;
	vec3_t left, up;
	float radius;
	qhandle_t currentShader = 0;
	qboolean shaderChanged = qfalse;

	if (particleSystem.particles == NULL) {
		return;
	}

	if (r_particleSystem && r_particleSystem->integer == 0) {
		return; // Old system enabled
	}

	if (particleSystem.numActive == 0) {
		return;
	}

	// Collect active particles
	for (i = 0; i < particleSystem.maxParticles; i++) {
		p = &particleSystem.particles[i];
		if (p->active && p->life > 0.0f) {
			sortedParticles[numSorted++] = p;
		}
	}

	if (numSorted == 0) {
		return;
	}

	// Sort particles by shader and distance
	qsort(sortedParticles, numSorted, sizeof(particle_t *), CompareParticles);

	// Batch render particles
	for (i = 0; i < numSorted; i++) {
		p = sortedParticles[i];

		// Check if we need to change shader
		if (currentShader != p->shader) {
			if (shaderChanged) {
				RB_EndSurface();
			}
			
			shader_t *shader = R_GetShaderByHandle(p->shader);
			RB_BeginSurface(shader, -1);
			currentShader = p->shader;
			shaderChanged = qtrue;
		}

		// Calculate billboard vectors
		radius = p->size * p->life;
		
		if (p->rotation == 0.0f) {
			VectorScale(backEnd.viewParms.or.axis[1], radius, left);
			VectorScale(backEnd.viewParms.or.axis[2], radius, up);
		} else {
			float s, c;
			float ang;
			
			ang = M_PI * p->rotation / 180.0f;
			s = sin(ang);
			c = cos(ang);

			VectorScale(backEnd.viewParms.or.axis[1], c * radius, left);
			VectorMA(left, -s * radius, backEnd.viewParms.or.axis[2], left);

			VectorScale(backEnd.viewParms.or.axis[2], c * radius, up);
			VectorMA(up, s * radius, backEnd.viewParms.or.axis[1], up);
		}

		if (backEnd.viewParms.portalView == PV_MIRROR) {
			VectorSubtract(vec3_origin, left, left);
		}

		// Create color with alpha fade
		color4ub_t color;
		byte alpha = (byte)(255.0f * p->life);
		color.rgba[0] = (byte)(p->color[0] * 255.0f);
		color.rgba[1] = (byte)(p->color[1] * 255.0f);
		color.rgba[2] = (byte)(p->color[2] * 255.0f);
		color.rgba[3] = alpha;

		// Add quad to tess
		RB_AddQuadStamp(p->origin, left, up, color);
	}

	// End the last surface
	if (shaderChanged) {
		RB_EndSurface();
	}
}

