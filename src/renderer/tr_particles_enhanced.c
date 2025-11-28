/*
===========================================================================
Enhanced Particle System
Implements emitters, physics, trails, ribbons, and entity attachments
===========================================================================
*/

#include "tr_local.h"
#include "tr_particles_enhanced.h"

static particleSystemEnhanced_t particleSystemEnhanced;
cvar_t *r_particlesEnhanced;
static cvar_t *r_particlesWind;
static cvar_t *r_particlesGravity;
static cvar_t *r_particlesTurbulence;

// Random number helpers
static float RandomFloat(float min, float max)
{
	return min + ((float)rand() / (float)RAND_MAX) * (max - min);
}

static void RandomVector(vec3_t min, vec3_t max, vec3_t out)
{
	out[0] = RandomFloat(min[0], max[0]);
	out[1] = RandomFloat(min[1], max[1]);
	out[2] = RandomFloat(min[2], max[2]);
}

/*
===================
R_InitParticleSystemEnhanced
===================
*/
void R_InitParticleSystemEnhanced(void)
{
	if (particleSystemEnhanced.particles != NULL) {
		return; // Already initialized
	}

	particleSystemEnhanced.maxParticles = MAX_PARTICLES;
	particleSystemEnhanced.numActive = 0;
	particleSystemEnhanced.nextFree = 0;
	particleSystemEnhanced.numEmitters = 0;
	particleSystemEnhanced.numTrails = 0;
	particleSystemEnhanced.numRibbons = 0;

	particleSystemEnhanced.particles = (particleEnhanced_t *)ri.Hunk_Alloc(
		sizeof(particleEnhanced_t) * particleSystemEnhanced.maxParticles, h_low);
	
	if (particleSystemEnhanced.particles == NULL) {
		ri.Error(ERR_FATAL, "R_InitParticleSystemEnhanced: Failed to allocate particle pool");
	}

	Com_Memset(particleSystemEnhanced.particles, 0, 
		sizeof(particleEnhanced_t) * particleSystemEnhanced.maxParticles);
	Com_Memset(particleSystemEnhanced.emitters, 0, sizeof(particleSystemEnhanced.emitters));
	Com_Memset(particleSystemEnhanced.trails, 0, sizeof(particleSystemEnhanced.trails));
	Com_Memset(particleSystemEnhanced.ribbons, 0, sizeof(particleSystemEnhanced.ribbons));

	VectorClear(particleSystemEnhanced.globalWind);
	particleSystemEnhanced.globalGravity = 800.0f;

	r_particlesEnhanced = ri.Cvar_Get("r_particlesEnhanced", "1", CVAR_ARCHIVE);
	if (ri.Cvar_SetDescription) {
		ri.Cvar_SetDescription(r_particlesEnhanced, "Enable enhanced particle system");
	}

	r_particlesWind = ri.Cvar_Get("r_particlesWind", "0 0 0", CVAR_ARCHIVE);
	if (ri.Cvar_SetDescription) {
		ri.Cvar_SetDescription(r_particlesWind, "Global wind vector for particles");
	}

	r_particlesGravity = ri.Cvar_Get("r_particlesGravity", "800", CVAR_ARCHIVE);
	if (ri.Cvar_SetDescription) {
		ri.Cvar_SetDescription(r_particlesGravity, "Global gravity for particles");
	}

	r_particlesTurbulence = ri.Cvar_Get("r_particlesTurbulence", "0.1", CVAR_ARCHIVE);
	if (ri.Cvar_SetDescription) {
		ri.Cvar_SetDescription(r_particlesTurbulence, "Global turbulence strength");
	}

	ri.Printf(PRINT_ALL, "Enhanced particle system initialized: %d particles\n", 
		particleSystemEnhanced.maxParticles);
}

/*
===================
R_ShutdownParticleSystemEnhanced
===================
*/
void R_ShutdownParticleSystemEnhanced(void)
{
	if (particleSystemEnhanced.particles != NULL) {
		particleSystemEnhanced.particles = NULL;
		particleSystemEnhanced.numActive = 0;
		particleSystemEnhanced.nextFree = 0;
		particleSystemEnhanced.numEmitters = 0;
		particleSystemEnhanced.numTrails = 0;
		particleSystemEnhanced.numRibbons = 0;
	}
}

/*
===================
R_ClearParticleSystemEnhanced
===================
*/
void R_ClearParticleSystemEnhanced(void)
{
	if (particleSystemEnhanced.particles == NULL) {
		return;
	}

	Com_Memset(particleSystemEnhanced.particles, 0, 
		sizeof(particleEnhanced_t) * particleSystemEnhanced.maxParticles);
	Com_Memset(particleSystemEnhanced.emitters, 0, sizeof(particleSystemEnhanced.emitters));
	Com_Memset(particleSystemEnhanced.trails, 0, sizeof(particleSystemEnhanced.trails));
	Com_Memset(particleSystemEnhanced.ribbons, 0, sizeof(particleSystemEnhanced.ribbons));
	
	particleSystemEnhanced.numActive = 0;
	particleSystemEnhanced.nextFree = 0;
	particleSystemEnhanced.numEmitters = 0;
	particleSystemEnhanced.numTrails = 0;
	particleSystemEnhanced.numRibbons = 0;
}

/*
===================
R_FindFreeParticleSlot
===================
*/
static int R_FindFreeParticleSlot(void)
{
	int i;
	
	for (i = 0; i < particleSystemEnhanced.maxParticles; i++) {
		int idx = (particleSystemEnhanced.nextFree + i) % particleSystemEnhanced.maxParticles;
		particleEnhanced_t *p = &particleSystemEnhanced.particles[idx];
		
		if (!p->active) {
			particleSystemEnhanced.nextFree = (idx + 1) % particleSystemEnhanced.maxParticles;
			return idx;
		}
	}
	
	// No free slots - overwrite oldest
	if (particleSystemEnhanced.numActive > 0) {
		int idx = particleSystemEnhanced.nextFree;
		particleSystemEnhanced.nextFree = (idx + 1) % particleSystemEnhanced.maxParticles;
		return idx;
	}
	
	return -1;
}

/*
===================
R_EmitParticleFromEmitter
===================
*/
static void R_EmitParticleFromEmitter(particleEmitter_t *emitter)
{
	particleEnhanced_t *p;
	vec3_t origin, velocity, color, colorEnd;
	float size, sizeEnd, life, rotationSpeed;
	int slot;
	
	slot = R_FindFreeParticleSlot();
	if (slot < 0) {
		return;
	}
	
	p = &particleSystemEnhanced.particles[slot];
	
	// Calculate emission origin based on emitter type
	switch (emitter->type) {
		case EMITTER_POINT:
			VectorCopy(emitter->origin, origin);
			break;
			
		case EMITTER_BOX:
			origin[0] = RandomFloat(emitter->mins[0], emitter->maxs[0]);
			origin[1] = RandomFloat(emitter->mins[1], emitter->maxs[1]);
			origin[2] = RandomFloat(emitter->mins[2], emitter->maxs[2]);
			break;
			
		case EMITTER_SPHERE:
			{
				vec3_t dir;
				float dist = RandomFloat(0.0f, emitter->radius);
				float theta = RandomFloat(0.0f, M_PI * 2.0f);
				float phi = RandomFloat(0.0f, M_PI);
				
				dir[0] = sin(phi) * cos(theta);
				dir[1] = sin(phi) * sin(theta);
				dir[2] = cos(phi);
				
				VectorMA(emitter->origin, dist, dir, origin);
			}
			break;
			
		case EMITTER_CYLINDER:
			{
				vec3_t dir;
				float dist = RandomFloat(0.0f, emitter->radius);
				float theta = RandomFloat(0.0f, M_PI * 2.0f);
				
				dir[0] = cos(theta);
				dir[1] = sin(theta);
				dir[2] = RandomFloat(0.0f, emitter->height);
				
				VectorMA(emitter->origin, dist, dir, origin);
			}
			break;
			
		default:
			VectorCopy(emitter->origin, origin);
			break;
	}
	
	// Apply attachment offset if attached to entity
	if (emitter->attachEntity >= 0) {
		// TODO: Get entity origin and add offset
		// For now, just use emitter origin
	}
	
	// Randomize velocity
	RandomVector(emitter->velocityMin, emitter->velocityMax, velocity);
	
	// Randomize color
	RandomVector(emitter->colorMin, emitter->colorMax, color);
	RandomVector(emitter->colorEndMin, emitter->colorEndMax, colorEnd);
	
	// Randomize size
	size = RandomFloat(emitter->sizeMin, emitter->sizeMax);
	sizeEnd = RandomFloat(emitter->sizeEndMin, emitter->sizeEndMax);
	
	// Randomize life
	life = RandomFloat(emitter->lifeMin, emitter->lifeMax);
	
	// Randomize rotation speed
	rotationSpeed = RandomFloat(emitter->rotationSpeedMin, emitter->rotationSpeedMax);
	
	// Initialize particle
	VectorCopy(origin, p->origin);
	VectorCopy(velocity, p->velocity);
	VectorCopy(color, p->color);
	VectorCopy(colorEnd, p->colorEnd);
	p->size = size;
	p->sizeEnd = sizeEnd;
	p->rotation = RandomFloat(0.0f, 360.0f);
	p->rotationSpeed = rotationSpeed;
	p->life = 1.0f;
	p->fade = 1.0f / life;
	p->shader = emitter->shader;
	p->spawnTime = tr.refdef.time;
	p->lifeTime = (int)(life * 1000.0f);
	p->active = qtrue;
	
	// Physics
	p->physicsFlags = emitter->physicsFlags;
	p->mass = 1.0f;
	VectorClear(p->acceleration);
	VectorCopy(emitter->wind, p->wind);
	p->turbulence = emitter->turbulence;
	p->drag = emitter->drag;
	
	// Attachment
	p->attachEntity = emitter->attachEntity;
	VectorCopy(emitter->attachOffset, p->attachOffset);
	
	// Trail/Ribbon
	p->trailId = -1;
	p->ribbonId = -1;
	
	// Scripting
	p->scriptId = emitter->scriptId;
	
	particleSystemEnhanced.numActive++;
}

/*
===================
R_UpdateParticleSystemEnhanced
===================
*/
void R_UpdateParticleSystemEnhanced(float deltaTime)
{
	particleEnhanced_t *p;
	particleEmitter_t *emitter;
	int i, j;
	int currentTime = tr.refdef.time;
	vec3_t wind;
	float gravity, turbulence;
	
	if (particleSystemEnhanced.particles == NULL) {
		return;
	}
	
	if (!r_particlesEnhanced || r_particlesEnhanced->integer == 0) {
		return;
	}
	
	// Get global physics values
	if (r_particlesWind) {
		sscanf(r_particlesWind->string, "%f %f %f", &wind[0], &wind[1], &wind[2]);
	} else {
		VectorCopy(particleSystemEnhanced.globalWind, wind);
	}
	
	gravity = r_particlesGravity ? r_particlesGravity->value : particleSystemEnhanced.globalGravity;
	turbulence = r_particlesTurbulence ? r_particlesTurbulence->value : 0.1f;
	
	// Update emitters
	for (i = 0; i < MAX_PARTICLE_EMITTERS; i++) {
		emitter = &particleSystemEnhanced.emitters[i];
		
		if (!emitter->active) {
			continue;
		}
		
		// Check duration
		if (emitter->duration > 0) {
			int elapsed = currentTime - emitter->startTime;
			if (elapsed >= emitter->duration) {
				emitter->active = qfalse;
				continue;
			}
		}
		
		// Check total particles
		if (emitter->totalParticles > 0 && emitter->particlesEmitted >= emitter->totalParticles) {
			emitter->active = qfalse;
			continue;
		}
		
		// Update attachment position
		if (emitter->attachEntity >= 0) {
			// TODO: Update emitter origin from entity
		}
		
		// Emit particles
		if (emitter->type == EMITTER_BURST) {
			// Burst emission
			if (currentTime - emitter->lastEmitTime >= (int)(emitter->burstInterval * 1000.0f)) {
				for (j = 0; j < (int)emitter->burstCount; j++) {
					R_EmitParticleFromEmitter(emitter);
					emitter->particlesEmitted++;
				}
				emitter->lastEmitTime = currentTime;
			}
		} else {
			// Continuous emission
			float particlesToEmit = emitter->rate * deltaTime;
			int emitCount = (int)particlesToEmit;
			float remainder = particlesToEmit - emitCount;
			
			// Handle remainder with probability
			if (RandomFloat(0.0f, 1.0f) < remainder) {
				emitCount++;
			}
			
			for (j = 0; j < emitCount; j++) {
				R_EmitParticleFromEmitter(emitter);
				emitter->particlesEmitted++;
			}
		}
	}
	
	// Update particles
	for (i = 0; i < particleSystemEnhanced.maxParticles; i++) {
		p = &particleSystemEnhanced.particles[i];
		
		if (!p->active) {
			continue;
		}
		
		// Update life
		int age = currentTime - p->spawnTime;
		if (age >= p->lifeTime) {
			p->active = qfalse;
			particleSystemEnhanced.numActive--;
			continue;
		}
		
		p->life = 1.0f - ((float)age / (float)p->lifeTime);
		
		// Update attachment position
		if (p->attachEntity >= 0) {
			// TODO: Update origin from entity
		}
		
		// Apply physics
		vec3_t force;
		VectorClear(force);
		
		// Gravity
		if (p->physicsFlags & PARTICLE_PHYSICS_GRAVITY) {
			force[2] -= gravity * p->mass * deltaTime;
		}
		
		// Wind
		if (p->physicsFlags & PARTICLE_PHYSICS_WIND) {
			vec3_t windForce;
			VectorAdd(wind, p->wind, windForce);
			VectorMA(force, deltaTime, windForce, force);
		}
		
		// Turbulence
		if (p->physicsFlags & PARTICLE_PHYSICS_TURBULENCE) {
			vec3_t turb;
			float strength = turbulence * p->turbulence;
			turb[0] = RandomFloat(-strength, strength);
			turb[1] = RandomFloat(-strength, strength);
			turb[2] = RandomFloat(-strength, strength);
			VectorAdd(force, turb, force);
		}
		
		// Apply force to velocity
		VectorAdd(p->velocity, force, p->velocity);
		
		// Apply drag
		if (p->drag > 0.0f) {
			VectorScale(p->velocity, 1.0f - (p->drag * deltaTime), p->velocity);
		}
		
		// Update position
		VectorMA(p->origin, deltaTime, p->velocity, p->origin);
		
		// Update rotation
		p->rotation += p->rotationSpeed * deltaTime;
		if (p->rotation >= 360.0f) {
			p->rotation -= 360.0f;
		}
		
		// Update trail
		if (p->trailId >= 0 && p->trailId < MAX_PARTICLE_TRAILS) {
			particleTrail_t *trail = &particleSystemEnhanced.trails[p->trailId];
			if (trail->active) {
				R_AddTrailPoint(p->trailId, p->origin);
			}
		}
	}
	
	// Update trails
	for (i = 0; i < MAX_PARTICLE_TRAILS; i++) {
		particleTrail_t *trail = &particleSystemEnhanced.trails[i];
		
		if (!trail->active) {
			continue;
		}
		
		// Remove old points
		int trailCurrentTime = tr.refdef.time;
		for (j = 0; j < trail->numPoints; j++) {
			int pointAge = trailCurrentTime - (int)trail->times[j];
			if (pointAge >= trail->lifeTime) {
				// Remove point by shifting array
				int k;
				for (k = j; k < trail->numPoints - 1; k++) {
					VectorCopy(trail->points[k + 1], trail->points[k]);
					trail->times[k] = trail->times[k + 1];
				}
				trail->numPoints--;
				j--;
			}
		}
		
		if (trail->numPoints == 0) {
			trail->active = qfalse;
		}
	}
	
	// Update ribbons
	for (i = 0; i < MAX_PARTICLE_RIBBONS; i++) {
		particleRibbon_t *ribbon = &particleSystemEnhanced.ribbons[i];
		
		if (!ribbon->active) {
			continue;
		}
		
		// Check lifetime
		int age = currentTime - ribbon->spawnTime;
		if (age >= ribbon->lifeTime) {
			ribbon->active = qfalse;
		}
	}
}

/*
===================
CompareParticlesEnhanced
===================
Comparison function for qsort
*/
static int CompareParticlesEnhanced(const void *a, const void *b)
{
	const particleEnhanced_t *pa = *(const particleEnhanced_t **)a;
	const particleEnhanced_t *pb = *(const particleEnhanced_t **)b;
	
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
R_RenderParticleSystemEnhanced
===================
*/
void R_RenderParticleSystemEnhanced(void)
{
	particleEnhanced_t *p;
	int i;
	particleEnhanced_t *sortedParticles[MAX_PARTICLES];
	int numSorted = 0;
	vec3_t left, up;
	float radius;
	qhandle_t currentShader = 0;
	qboolean shaderChanged = qfalse;
	vec3_t color;
	float size, alpha;
	
	if (particleSystemEnhanced.particles == NULL) {
		return;
	}
	
	if (!r_particlesEnhanced || r_particlesEnhanced->integer == 0) {
		return;
	}
	
	if (particleSystemEnhanced.numActive == 0) {
		return;
	}
	
	// Collect active particles
	for (i = 0; i < particleSystemEnhanced.maxParticles; i++) {
		p = &particleSystemEnhanced.particles[i];
		if (p->active && p->life > 0.0f) {
			sortedParticles[numSorted++] = p;
		}
	}
	
	if (numSorted == 0) {
		return;
	}
	
	// Sort particles by shader and distance
	qsort(sortedParticles, numSorted, sizeof(particleEnhanced_t *), CompareParticlesEnhanced);
	
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
		
		// Interpolate color
		{
			float t = 1.0f - p->life;
			color[0] = p->color[0] * (1.0f - t) + p->colorEnd[0] * t;
			color[1] = p->color[1] * (1.0f - t) + p->colorEnd[1] * t;
			color[2] = p->color[2] * (1.0f - t) + p->colorEnd[2] * t;
		}
		
		// Interpolate size
		{
			float t = 1.0f - p->life;
			size = p->size * (1.0f - t) + p->sizeEnd * t;
		}
		
		// Calculate billboard vectors
		radius = size * p->life;
		
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
		color4ub_t color4ub;
		alpha = p->life;
		color4ub.rgba[0] = (byte)(color[0] * 255.0f);
		color4ub.rgba[1] = (byte)(color[1] * 255.0f);
		color4ub.rgba[2] = (byte)(color[2] * 255.0f);
		color4ub.rgba[3] = (byte)(alpha * 255.0f);
		
		// Add quad to tess
		RB_AddQuadStamp(p->origin, left, up, color4ub);
	}
	
	// End the last surface
	if (shaderChanged) {
		RB_EndSurface();
	}
	
	// Render trails
	for (i = 0; i < MAX_PARTICLE_TRAILS; i++) {
		particleTrail_t *trail = &particleSystemEnhanced.trails[i];
		
		if (!trail->active || trail->numPoints < 2) {
			continue;
		}
		
		// Render trail as connected line segments
		// TODO: Implement trail rendering
	}
	
	// Render ribbons
	for (i = 0; i < MAX_PARTICLE_RIBBONS; i++) {
		particleRibbon_t *ribbon = &particleSystemEnhanced.ribbons[i];
		
		if (!ribbon->active || ribbon->numSegments < 1) {
			continue;
		}
		
		// Render ribbon as connected quads
		// TODO: Implement ribbon rendering
	}
	
	particleSystemEnhanced.particlesRendered = numSorted;
}

/*
===================
R_CreateParticleEmitter
===================
*/
int R_CreateParticleEmitter(emitterType_t type, const vec3_t origin, const vec3_t mins, const vec3_t maxs, float radius, float height)
{
	int i;
	particleEmitter_t *emitter;
	
	for (i = 0; i < MAX_PARTICLE_EMITTERS; i++) {
		if (!particleSystemEnhanced.emitters[i].active) {
			emitter = &particleSystemEnhanced.emitters[i];
			Com_Memset(emitter, 0, sizeof(particleEmitter_t));
			
			emitter->type = type;
			VectorCopy(origin, emitter->origin);
			VectorCopy(mins, emitter->mins);
			VectorCopy(maxs, emitter->maxs);
			emitter->radius = radius;
			emitter->height = height;
			
			// Defaults
			emitter->rate = 10.0f;
			emitter->burstCount = 10.0f;
			emitter->burstInterval = 1.0f;
			emitter->totalParticles = -1;
			emitter->particlesEmitted = 0;
			
			VectorSet(emitter->velocityMin, -10.0f, -10.0f, 10.0f);
			VectorSet(emitter->velocityMax, 10.0f, 10.0f, 50.0f);
			emitter->velocitySpread = 45.0f;
			
			VectorSet(emitter->colorMin, 1.0f, 1.0f, 1.0f);
			VectorSet(emitter->colorMax, 1.0f, 1.0f, 1.0f);
			VectorSet(emitter->colorEndMin, 1.0f, 1.0f, 1.0f);
			VectorSet(emitter->colorEndMax, 1.0f, 1.0f, 1.0f);
			
			emitter->sizeMin = 1.0f;
			emitter->sizeMax = 1.0f;
			emitter->sizeEndMin = 1.0f;
			emitter->sizeEndMax = 1.0f;
			
			emitter->lifeMin = 1.0f;
			emitter->lifeMax = 2.0f;
			
			emitter->rotationSpeedMin = 0.0f;
			emitter->rotationSpeedMax = 180.0f;
			
			emitter->physicsFlags = PARTICLE_PHYSICS_GRAVITY;
			emitter->gravity = 800.0f;
			VectorClear(emitter->wind);
			emitter->turbulence = 0.1f;
			emitter->drag = 0.0f;
			
			emitter->attachEntity = -1;
			VectorClear(emitter->attachOffset);
			
			emitter->shader = 0;
			emitter->active = qtrue;
			emitter->spawnTime = tr.refdef.time;
			emitter->lastEmitTime = tr.refdef.time;
			emitter->startTime = tr.refdef.time;
			emitter->duration = -1;
			emitter->scriptId = -1;
			
			particleSystemEnhanced.numEmitters++;
			return i;
		}
	}
	
	return -1;
}

/*
===================
R_DestroyParticleEmitter
===================
*/
void R_DestroyParticleEmitter(int emitterId)
{
	if (emitterId < 0 || emitterId >= MAX_PARTICLE_EMITTERS) {
		return;
	}
	
	if (particleSystemEnhanced.emitters[emitterId].active) {
		particleSystemEnhanced.emitters[emitterId].active = qfalse;
		particleSystemEnhanced.numEmitters--;
	}
}

/*
===================
R_SetEmitterActive
===================
*/
void R_SetEmitterActive(int emitterId, qboolean active)
{
	if (emitterId < 0 || emitterId >= MAX_PARTICLE_EMITTERS) {
		return;
	}
	
	particleSystemEnhanced.emitters[emitterId].active = active;
}

/*
===================
R_SetEmitterRate
===================
*/
void R_SetEmitterRate(int emitterId, float rate)
{
	if (emitterId < 0 || emitterId >= MAX_PARTICLE_EMITTERS) {
		return;
	}
	
	particleSystemEnhanced.emitters[emitterId].rate = rate;
}

/*
===================
R_SetEmitterVelocity
===================
*/
void R_SetEmitterVelocity(int emitterId, const vec3_t min, const vec3_t max)
{
	if (emitterId < 0 || emitterId >= MAX_PARTICLE_EMITTERS) {
		return;
	}
	
	VectorCopy(min, particleSystemEnhanced.emitters[emitterId].velocityMin);
	VectorCopy(max, particleSystemEnhanced.emitters[emitterId].velocityMax);
}

/*
===================
R_SetEmitterColor
===================
*/
void R_SetEmitterColor(int emitterId, const vec3_t min, const vec3_t max, const vec3_t endMin, const vec3_t endMax)
{
	if (emitterId < 0 || emitterId >= MAX_PARTICLE_EMITTERS) {
		return;
	}
	
	VectorCopy(min, particleSystemEnhanced.emitters[emitterId].colorMin);
	VectorCopy(max, particleSystemEnhanced.emitters[emitterId].colorMax);
	VectorCopy(endMin, particleSystemEnhanced.emitters[emitterId].colorEndMin);
	VectorCopy(endMax, particleSystemEnhanced.emitters[emitterId].colorEndMax);
}

/*
===================
R_SetEmitterSize
===================
*/
void R_SetEmitterSize(int emitterId, float min, float max, float endMin, float endMax)
{
	if (emitterId < 0 || emitterId >= MAX_PARTICLE_EMITTERS) {
		return;
	}
	
	particleSystemEnhanced.emitters[emitterId].sizeMin = min;
	particleSystemEnhanced.emitters[emitterId].sizeMax = max;
	particleSystemEnhanced.emitters[emitterId].sizeEndMin = endMin;
	particleSystemEnhanced.emitters[emitterId].sizeEndMax = endMax;
}

/*
===================
R_SetEmitterLife
===================
*/
void R_SetEmitterLife(int emitterId, float min, float max)
{
	if (emitterId < 0 || emitterId >= MAX_PARTICLE_EMITTERS) {
		return;
	}
	
	particleSystemEnhanced.emitters[emitterId].lifeMin = min;
	particleSystemEnhanced.emitters[emitterId].lifeMax = max;
}

/*
===================
R_SetEmitterPhysics
===================
*/
void R_SetEmitterPhysics(int emitterId, int flags, float gravity, const vec3_t wind, float turbulence, float drag)
{
	if (emitterId < 0 || emitterId >= MAX_PARTICLE_EMITTERS) {
		return;
	}
	
	particleSystemEnhanced.emitters[emitterId].physicsFlags = flags;
	particleSystemEnhanced.emitters[emitterId].gravity = gravity;
	VectorCopy(wind, particleSystemEnhanced.emitters[emitterId].wind);
	particleSystemEnhanced.emitters[emitterId].turbulence = turbulence;
	particleSystemEnhanced.emitters[emitterId].drag = drag;
}

/*
===================
R_SetEmitterAttachment
===================
*/
void R_SetEmitterAttachment(int emitterId, int entityNum, const vec3_t offset)
{
	if (emitterId < 0 || emitterId >= MAX_PARTICLE_EMITTERS) {
		return;
	}
	
	particleSystemEnhanced.emitters[emitterId].attachEntity = entityNum;
	VectorCopy(offset, particleSystemEnhanced.emitters[emitterId].attachOffset);
}

/*
===================
R_SetEmitterShader
===================
*/
void R_SetEmitterShader(int emitterId, qhandle_t shader)
{
	if (emitterId < 0 || emitterId >= MAX_PARTICLE_EMITTERS) {
		return;
	}
	
	particleSystemEnhanced.emitters[emitterId].shader = shader;
}

/*
===================
R_SetEmitterDuration
===================
*/
void R_SetEmitterDuration(int emitterId, int duration)
{
	if (emitterId < 0 || emitterId >= MAX_PARTICLE_EMITTERS) {
		return;
	}
	
	particleSystemEnhanced.emitters[emitterId].duration = duration;
	particleSystemEnhanced.emitters[emitterId].startTime = tr.refdef.time;
}

/*
===================
R_SetEmitterBurst
===================
*/
void R_SetEmitterBurst(int emitterId, float count, float interval)
{
	if (emitterId < 0 || emitterId >= MAX_PARTICLE_EMITTERS) {
		return;
	}
	
	particleSystemEnhanced.emitters[emitterId].burstCount = count;
	particleSystemEnhanced.emitters[emitterId].burstInterval = interval;
	particleSystemEnhanced.emitters[emitterId].type = EMITTER_BURST;
}

/*
===================
R_CreateParticleTrail
===================
*/
int R_CreateParticleTrail(float width, const vec3_t color, qhandle_t shader, int maxPoints, int lifeTime)
{
	int i;
	particleTrail_t *trail;
	
	for (i = 0; i < MAX_PARTICLE_TRAILS; i++) {
		if (!particleSystemEnhanced.trails[i].active) {
			trail = &particleSystemEnhanced.trails[i];
			Com_Memset(trail, 0, sizeof(particleTrail_t));
			
			trail->width = width;
			VectorCopy(color, trail->color);
			trail->shader = shader;
			trail->maxPoints = (maxPoints > MAX_TRAIL_POINTS) ? MAX_TRAIL_POINTS : maxPoints;
			trail->lifeTime = lifeTime;
			trail->numPoints = 0;
			trail->active = qtrue;
			trail->spawnTime = tr.refdef.time;
			
			particleSystemEnhanced.numTrails++;
			return i;
		}
	}
	
	return -1;
}

/*
===================
R_DestroyParticleTrail
===================
*/
void R_DestroyParticleTrail(int trailId)
{
	if (trailId < 0 || trailId >= MAX_PARTICLE_TRAILS) {
		return;
	}
	
	if (particleSystemEnhanced.trails[trailId].active) {
		particleSystemEnhanced.trails[trailId].active = qfalse;
		particleSystemEnhanced.numTrails--;
	}
}

/*
===================
R_AddTrailPoint
===================
*/
void R_AddTrailPoint(int trailId, const vec3_t point)
{
	particleTrail_t *trail;
	
	if (trailId < 0 || trailId >= MAX_PARTICLE_TRAILS) {
		return;
	}
	
	trail = &particleSystemEnhanced.trails[trailId];
	
	if (!trail->active) {
		return;
	}
	
	// Add point
	if (trail->numPoints < trail->maxPoints) {
		VectorCopy(point, trail->points[trail->numPoints]);
		trail->times[trail->numPoints] = (float)tr.refdef.time;
		trail->numPoints++;
	} else {
		// Shift array and add new point at end
		int i;
		for (i = 0; i < trail->maxPoints - 1; i++) {
			VectorCopy(trail->points[i + 1], trail->points[i]);
			trail->times[i] = trail->times[i + 1];
		}
		VectorCopy(point, trail->points[trail->maxPoints - 1]);
		trail->times[trail->maxPoints - 1] = (float)tr.refdef.time;
	}
}

/*
===================
R_SetTrailActive
===================
*/
void R_SetTrailActive(int trailId, qboolean active)
{
	if (trailId < 0 || trailId >= MAX_PARTICLE_TRAILS) {
		return;
	}
	
	particleSystemEnhanced.trails[trailId].active = active;
}

/*
===================
R_SetTrailWidth
===================
*/
void R_SetTrailWidth(int trailId, float width)
{
	if (trailId < 0 || trailId >= MAX_PARTICLE_TRAILS) {
		return;
	}
	
	particleSystemEnhanced.trails[trailId].width = width;
}

/*
===================
R_SetTrailColor
===================
*/
void R_SetTrailColor(int trailId, const vec3_t color)
{
	if (trailId < 0 || trailId >= MAX_PARTICLE_TRAILS) {
		return;
	}
	
	VectorCopy(color, particleSystemEnhanced.trails[trailId].color);
}

/*
===================
R_CreateParticleRibbon
===================
*/
int R_CreateParticleRibbon(qhandle_t shader, int maxSegments, int lifeTime)
{
	(void)maxSegments; // Unused - kept for API compatibility
	int i;
	particleRibbon_t *ribbon;
	
	for (i = 0; i < MAX_PARTICLE_RIBBONS; i++) {
		if (!particleSystemEnhanced.ribbons[i].active) {
			ribbon = &particleSystemEnhanced.ribbons[i];
			Com_Memset(ribbon, 0, sizeof(particleRibbon_t));
			
			ribbon->shader = shader;
			ribbon->numSegments = 0;
			ribbon->lifeTime = lifeTime;
			ribbon->active = qtrue;
			ribbon->spawnTime = tr.refdef.time;
			
			particleSystemEnhanced.numRibbons++;
			return i;
		}
	}
	
	return -1;
}

/*
===================
R_DestroyParticleRibbon
===================
*/
void R_DestroyParticleRibbon(int ribbonId)
{
	if (ribbonId < 0 || ribbonId >= MAX_PARTICLE_RIBBONS) {
		return;
	}
	
	if (particleSystemEnhanced.ribbons[ribbonId].active) {
		particleSystemEnhanced.ribbons[ribbonId].active = qfalse;
		particleSystemEnhanced.numRibbons--;
	}
}

/*
===================
R_AddRibbonSegment
===================
*/
void R_AddRibbonSegment(int ribbonId, const vec3_t start, const vec3_t end, const vec3_t color, float width)
{
	particleRibbon_t *ribbon;
	
	if (ribbonId < 0 || ribbonId >= MAX_PARTICLE_RIBBONS) {
		return;
	}
	
	ribbon = &particleSystemEnhanced.ribbons[ribbonId];
	
	if (!ribbon->active) {
		return;
	}
	
	// Add segment
	if (ribbon->numSegments < MAX_RIBBON_SEGMENTS) {
		VectorCopy(start, ribbon->segments[ribbon->numSegments][0]);
		VectorCopy(end, ribbon->segments[ribbon->numSegments][1]);
		VectorCopy(color, ribbon->colors[ribbon->numSegments]);
		ribbon->widths[ribbon->numSegments] = width;
		ribbon->numSegments++;
	} else {
		// Shift array and add new segment at end
		int i;
		for (i = 0; i < MAX_RIBBON_SEGMENTS - 1; i++) {
			VectorCopy(ribbon->segments[i + 1][0], ribbon->segments[i][0]);
			VectorCopy(ribbon->segments[i + 1][1], ribbon->segments[i][1]);
			VectorCopy(ribbon->colors[i + 1], ribbon->colors[i]);
			ribbon->widths[i] = ribbon->widths[i + 1];
		}
		VectorCopy(start, ribbon->segments[MAX_RIBBON_SEGMENTS - 1][0]);
		VectorCopy(end, ribbon->segments[MAX_RIBBON_SEGMENTS - 1][1]);
		VectorCopy(color, ribbon->colors[MAX_RIBBON_SEGMENTS - 1]);
		ribbon->widths[MAX_RIBBON_SEGMENTS - 1] = width;
	}
}

/*
===================
R_SetRibbonActive
===================
*/
void R_SetRibbonActive(int ribbonId, qboolean active)
{
	if (ribbonId < 0 || ribbonId >= MAX_PARTICLE_RIBBONS) {
		return;
	}
	
	particleSystemEnhanced.ribbons[ribbonId].active = active;
}

/*
===================
R_AddParticleEnhanced
===================
*/
void R_AddParticleEnhanced(const vec3_t origin, const vec3_t velocity, const vec3_t color, const vec3_t colorEnd, 
						   float size, float sizeEnd, float life, qhandle_t shaderHandle, int physicsFlags)
{
	particleEnhanced_t *p;
	int slot;
	
	slot = R_FindFreeParticleSlot();
	if (slot < 0) {
		return;
	}
	
	p = &particleSystemEnhanced.particles[slot];
	
	VectorCopy(origin, p->origin);
	VectorCopy(velocity, p->velocity);
	VectorCopy(color, p->color);
	VectorCopy(colorEnd, p->colorEnd);
	p->size = size;
	p->sizeEnd = sizeEnd;
	p->rotation = RandomFloat(0.0f, 360.0f);
	p->rotationSpeed = RandomFloat(0.0f, 180.0f);
	p->life = 1.0f;
	p->fade = 1.0f / life;
	p->shader = shaderHandle;
	p->spawnTime = tr.refdef.time;
	p->lifeTime = (int)(life * 1000.0f);
	p->active = qtrue;
	
	p->physicsFlags = physicsFlags;
	p->mass = 1.0f;
	VectorClear(p->acceleration);
	VectorClear(p->wind);
	p->turbulence = 0.1f;
	p->drag = 0.0f;
	
	p->attachEntity = -1;
	VectorClear(p->attachOffset);
	
	p->trailId = -1;
	p->ribbonId = -1;
	p->scriptId = -1;
	
	particleSystemEnhanced.numActive++;
}

/*
===================
R_AddParticleWithTrail
===================
*/
void R_AddParticleWithTrail(const vec3_t origin, const vec3_t velocity, const vec3_t color, float size, float life, 
							qhandle_t shaderHandle, int trailId)
{
	vec3_t colorEnd;
	VectorCopy(color, colorEnd);
	R_AddParticleEnhanced(origin, velocity, color, colorEnd, size, size, life, shaderHandle, PARTICLE_PHYSICS_GRAVITY);
	
	if (trailId >= 0 && trailId < MAX_PARTICLE_TRAILS) {
		particleSystemEnhanced.particles[particleSystemEnhanced.nextFree - 1].trailId = trailId;
	}
}

/*
===================
R_AddParticleWithAttachment
===================
*/
void R_AddParticleWithAttachment(const vec3_t origin, const vec3_t velocity, const vec3_t color, float size, float life,
								 qhandle_t shaderHandle, int entityNum, const vec3_t offset)
{
	vec3_t colorEnd;
	VectorCopy(color, colorEnd);
	R_AddParticleEnhanced(origin, velocity, color, colorEnd, size, size, life, shaderHandle, PARTICLE_PHYSICS_GRAVITY);
	
	if (entityNum >= 0) {
		particleSystemEnhanced.particles[particleSystemEnhanced.nextFree - 1].attachEntity = entityNum;
		VectorCopy(offset, particleSystemEnhanced.particles[particleSystemEnhanced.nextFree - 1].attachOffset);
	}
}

/*
===================
R_SetGlobalWind
===================
*/
void R_SetGlobalWind(const vec3_t wind)
{
	VectorCopy(wind, particleSystemEnhanced.globalWind);
}

/*
===================
R_SetGlobalGravity
===================
*/
void R_SetGlobalGravity(float gravity)
{
	particleSystemEnhanced.globalGravity = gravity;
}

/*
===================
R_GetActiveParticleCount
===================
*/
int R_GetActiveParticleCount(void)
{
	return particleSystemEnhanced.numActive;
}

/*
===================
R_GetActiveEmitterCount
===================
*/
int R_GetActiveEmitterCount(void)
{
	return particleSystemEnhanced.numEmitters;
}

