/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "phys_fluid.h"
#include "phys_bullet.h"
#include "phys_debugdraw.h"

#include <math.h>

typedef struct {
	qboolean active;
	vec3_t   position;
	vec3_t   velocity;
	float    density;
	float    pressure;
	int      emitter;
} fluidParticle_t;

typedef struct {
	qboolean          active;
	physFluidConfig_t config;
	int               first;
	int               count;
} fluidEmitter_t;

static fluidParticle_t s_particles[PHYS_FLUID_MAX_PARTICLES];
static fluidEmitter_t  s_emitters[PHYS_FLUID_MAX_EMITTERS];
static int             s_activeParticles;
static cvar_t         *phys_fluid;

void PhysFluid_DefaultConfig( physFluidConfig_t *cfg ) {
	if ( !cfg ) {
		return;
	}
	cfg->restDensity = 1.0f;
	cfg->gasConstant = 2000.0f;
	cfg->viscosity = 0.08f;
	cfg->smoothingRadius = 18.0f;
	cfg->particleMass = 1.0f;
	cfg->gravity = -800.0f;
	cfg->damping = 0.995f;
	cfg->worldBounce = 0.2f;
	cfg->rigidCoupling = 2.0f;
}

void PhysFluid_Init( void ) {
	phys_fluid = Cvar_Get( "phys_fluid", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( phys_fluid,
		"Enable SPH fluid companion solver (collides / couples via Phys_* to Soft Step)." );
	Com_Memset( s_particles, 0, sizeof( s_particles ) );
	Com_Memset( s_emitters, 0, sizeof( s_emitters ) );
	s_activeParticles = 0;
	if ( phys_fluid->integer ) {
		Com_Printf( "[physics] fluid solver ready (SPH-ish + Phys_RayCast / overlap)\n" );
	}
}

void PhysFluid_Shutdown( void ) {
	Com_Memset( s_particles, 0, sizeof( s_particles ) );
	Com_Memset( s_emitters, 0, sizeof( s_emitters ) );
	s_activeParticles = 0;
}

void PhysFluid_Clear( void ) {
	PhysFluid_Shutdown();
}

int PhysFluid_GetActiveCount( void ) {
	return s_activeParticles;
}

static int PhysFluid_AllocParticle( void ) {
	int i;
	for ( i = 0; i < PHYS_FLUID_MAX_PARTICLES; i++ ) {
		if ( !s_particles[i].active ) {
			return i;
		}
	}
	return -1;
}

physFluidHandle_t PhysFluid_CreateBlob( const vec3_t origin, int count, float spacing,
	const physFluidConfig_t *cfg ) {
	physFluidConfig_t local;
	int e, i, side, emitted;
	float half;

	if ( !phys_fluid || !phys_fluid->integer || !origin ) {
		return -1;
	}
	if ( count < 8 ) {
		count = 8;
	}
	if ( count > 256 ) {
		count = 256;
	}
	if ( spacing <= 0.0f ) {
		spacing = 8.0f;
	}

	for ( e = 0; e < PHYS_FLUID_MAX_EMITTERS; e++ ) {
		if ( !s_emitters[e].active ) {
			break;
		}
	}
	if ( e >= PHYS_FLUID_MAX_EMITTERS ) {
		return -1;
	}

	if ( cfg ) {
		local = *cfg;
	} else {
		PhysFluid_DefaultConfig( &local );
	}

	side = (int)ceilf( cbrtf( (float)count ) );
	if ( side < 2 ) {
		side = 2;
	}
	half = ( side - 1 ) * spacing * 0.5f;
	emitted = 0;

	for ( i = 0; i < side * side * side && emitted < count; i++ ) {
		int idx = PhysFluid_AllocParticle();
		int x, y, z;
		fluidParticle_t *p;
		if ( idx < 0 ) {
			break;
		}
		x = i % side;
		y = ( i / side ) % side;
		z = i / ( side * side );
		p = &s_particles[idx];
		Com_Memset( p, 0, sizeof( *p ) );
		p->active = qtrue;
		p->emitter = e;
		p->position[0] = origin[0] + x * spacing - half;
		p->position[1] = origin[1] + y * spacing - half;
		p->position[2] = origin[2] + z * spacing - half;
		emitted++;
		s_activeParticles++;
	}

	if ( emitted <= 0 ) {
		return -1;
	}

	s_emitters[e].active = qtrue;
	s_emitters[e].config = local;
	s_emitters[e].first = 0;
	s_emitters[e].count = emitted;
	return e;
}

void PhysFluid_Destroy( physFluidHandle_t handle ) {
	int i;
	if ( handle < 0 || handle >= PHYS_FLUID_MAX_EMITTERS || !s_emitters[handle].active ) {
		return;
	}
	for ( i = 0; i < PHYS_FLUID_MAX_PARTICLES; i++ ) {
		if ( s_particles[i].active && s_particles[i].emitter == handle ) {
			s_particles[i].active = qfalse;
			s_activeParticles--;
		}
	}
	Com_Memset( &s_emitters[handle], 0, sizeof( s_emitters[handle] ) );
}

static const physFluidConfig_t *PhysFluid_ConfigFor( int emitter ) {
	static physFluidConfig_t fallback;
	if ( emitter >= 0 && emitter < PHYS_FLUID_MAX_EMITTERS && s_emitters[emitter].active ) {
		return &s_emitters[emitter].config;
	}
	PhysFluid_DefaultConfig( &fallback );
	return &fallback;
}

static float PhysFluid_KernelPoly6( float r2, float h, float h2 ) {
	float diff;
	if ( r2 >= h2 ) {
		return 0.0f;
	}
	diff = h2 - r2;
	return diff * diff * diff;
}

static float PhysFluid_KernelSpikyGrad( float r, float h ) {
	float diff;
	if ( r <= 0.0f || r >= h ) {
		return 0.0f;
	}
	diff = h - r;
	return -diff * diff;
}

void PhysFluid_Step( float dt ) {
	int i, j;
	float h, h2;
	physBodyHandle_t hits[8];

	if ( !phys_fluid || !phys_fluid->integer || s_activeParticles <= 0 ) {
		return;
	}
	if ( dt <= 0.0f || dt > 0.05f ) {
		dt = 0.016f;
	}

	/* density / pressure */
	for ( i = 0; i < PHYS_FLUID_MAX_PARTICLES; i++ ) {
		fluidParticle_t *a;
		const physFluidConfig_t *cfg;
		float density;
		if ( !s_particles[i].active ) {
			continue;
		}
		a = &s_particles[i];
		cfg = PhysFluid_ConfigFor( a->emitter );
		h = cfg->smoothingRadius;
		h2 = h * h;
		density = cfg->particleMass * PhysFluid_KernelPoly6( 0.0f, h, h2 );
		for ( j = 0; j < PHYS_FLUID_MAX_PARTICLES; j++ ) {
			vec3_t d;
			float r2;
			if ( i == j || !s_particles[j].active || s_particles[j].emitter != a->emitter ) {
				continue;
			}
			VectorSubtract( a->position, s_particles[j].position, d );
			r2 = DotProduct( d, d );
			if ( r2 < h2 ) {
				density += cfg->particleMass * PhysFluid_KernelPoly6( r2, h, h2 );
			}
		}
		a->density = density > 1e-4f ? density : 1e-4f;
		a->pressure = cfg->gasConstant * ( a->density - cfg->restDensity );
		if ( a->pressure < 0.0f ) {
			a->pressure = 0.0f;
		}
	}

	/* forces + integrate + Soft Step collide / couple */
	for ( i = 0; i < PHYS_FLUID_MAX_PARTICLES; i++ ) {
		fluidParticle_t *a;
		const physFluidConfig_t *cfg;
		vec3_t force, from, to;
		physRayResult_t hit;
		int nHits, k;

		if ( !s_particles[i].active ) {
			continue;
		}
		a = &s_particles[i];
		cfg = PhysFluid_ConfigFor( a->emitter );
		h = cfg->smoothingRadius;
		h2 = h * h;
		VectorSet( force, 0.0f, 0.0f, cfg->gravity * cfg->particleMass );

		for ( j = 0; j < PHYS_FLUID_MAX_PARTICLES; j++ ) {
			fluidParticle_t *b;
			vec3_t d, dir, relV;
			float r, r2, press, visc, w;
			if ( i == j || !s_particles[j].active || s_particles[j].emitter != a->emitter ) {
				continue;
			}
			b = &s_particles[j];
			VectorSubtract( a->position, b->position, d );
			r2 = DotProduct( d, d );
			if ( r2 <= 1e-6f || r2 >= h2 ) {
				continue;
			}
			r = sqrtf( r2 );
			dir[0] = d[0] / r;
			dir[1] = d[1] / r;
			dir[2] = d[2] / r;
			w = PhysFluid_KernelSpikyGrad( r, h );
			press = -cfg->particleMass * ( a->pressure + b->pressure ) / ( 2.0f * b->density ) * w;
			VectorMA( force, press, dir, force );
			VectorSubtract( b->velocity, a->velocity, relV );
			visc = cfg->viscosity * cfg->particleMass / b->density * ( h - r );
			VectorMA( force, visc, relV, force );
		}

		VectorMA( a->velocity, dt / cfg->particleMass, force, a->velocity );
		VectorScale( a->velocity, cfg->damping, a->velocity );

		VectorCopy( a->position, from );
		VectorMA( a->position, dt, a->velocity, to );

		if ( Phys_GetBackend() != PHYS_BACKEND_NONE
			&& Phys_RayCast( from, to, &hit ) && hit.hit && hit.fraction < 1.0f ) {
			float vn;
			VectorMA( hit.hitPoint, 1.5f, hit.hitNormal, a->position );
			vn = DotProduct( a->velocity, hit.hitNormal );
			if ( vn < 0.0f ) {
				VectorMA( a->velocity, -( 1.0f + cfg->worldBounce ) * vn, hit.hitNormal, a->velocity );
			}
			if ( hit.body >= 0 && Phys_IsBodyDynamic( hit.body ) && cfg->rigidCoupling > 0.0f ) {
				vec3_t impulse;
				VectorScale( hit.hitNormal, -vn * cfg->particleMass * cfg->rigidCoupling, impulse );
				Phys_ApplyImpulse( hit.body, impulse, hit.hitPoint );
			}
		} else {
			VectorCopy( to, a->position );
		}

		/* splash coupling onto nearby Soft Step dynamics */
		if ( Phys_GetBackend() != PHYS_BACKEND_NONE && cfg->rigidCoupling > 0.0f ) {
			nHits = Phys_OverlapSphere( a->position, cfg->smoothingRadius * 0.5f, hits, 8 );
			for ( k = 0; k < nHits; k++ ) {
				vec3_t impulse, push;
				if ( !Phys_IsBodyDynamic( hits[k] ) ) {
					continue;
				}
				VectorScale( a->velocity, cfg->particleMass * cfg->rigidCoupling * 0.02f, impulse );
				VectorCopy( a->position, push );
				Phys_ApplyImpulse( hits[k], impulse, push );
			}
		}
	}
}

void PhysFluid_DebugDraw( void ) {
	int i;
	vec3_t color, tip;

	VectorSet( color, 0.2f, 0.65f, 1.0f );
	for ( i = 0; i < PHYS_FLUID_MAX_PARTICLES; i++ ) {
		if ( !s_particles[i].active ) {
			continue;
		}
		VectorCopy( s_particles[i].position, tip );
		tip[2] += 3.0f;
		PhysDebug_AddLine( s_particles[i].position, tip, color );
	}
}
