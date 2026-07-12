/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "phys_particles.h"
#include "phys_bullet.h"
#include "phys_debugdraw.h"

#include <math.h>

typedef struct {
	qboolean active;
	vec3_t   position;
	vec3_t   velocity;
	float    radius;
	float    lifetime;
	float    age;
	float    bounce;
	float    friction;
	float    gravityScale;
} physParticle_t;

static physParticle_t s_particles[PHYS_PARTICLE_MAX];
static int s_active;
static cvar_t *phys_particles;

void PhysParticles_Init( void ) {
	phys_particles = Cvar_Get( "phys_particles", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( phys_particles,
		"Enable Box3D-colliding debris particle solver (secondary Soft Step companion)." );
	Com_Memset( s_particles, 0, sizeof( s_particles ) );
	s_active = 0;
	if ( phys_particles->integer ) {
		Com_Printf( "[physics] particles solver ready (collides via Phys_RayCast)\n" );
	}
}

void PhysParticles_Shutdown( void ) {
	Com_Memset( s_particles, 0, sizeof( s_particles ) );
	s_active = 0;
}

void PhysParticles_Clear( void ) {
	PhysParticles_Shutdown();
}

int PhysParticles_GetActiveCount( void ) {
	int i, n = 0;
	for ( i = 0; i < PHYS_PARTICLE_MAX; i++ ) {
		if ( s_particles[i].active ) {
			n++;
		}
	}
	return n;
}

int PhysParticles_Emit( const physParticleSpawn_t *spawn ) {
	int i;
	physParticle_t *p;

	if ( !spawn || !phys_particles || !phys_particles->integer ) {
		return -1;
	}
	for ( i = 0; i < PHYS_PARTICLE_MAX; i++ ) {
		if ( !s_particles[i].active ) {
			break;
		}
	}
	if ( i >= PHYS_PARTICLE_MAX ) {
		return -1;
	}
	p = &s_particles[i];
	Com_Memset( p, 0, sizeof( *p ) );
	p->active = qtrue;
	VectorCopy( spawn->origin, p->position );
	VectorCopy( spawn->velocity, p->velocity );
	p->radius = spawn->radius > 0.0f ? spawn->radius : 2.0f;
	p->lifetime = spawn->lifetime > 0.0f ? spawn->lifetime : 2.0f;
	p->bounce = spawn->bounce >= 0.0f ? spawn->bounce : 0.35f;
	p->friction = spawn->friction >= 0.0f ? spawn->friction : 0.2f;
	p->gravityScale = spawn->gravityScale != 0.0f ? spawn->gravityScale : 1.0f;
	s_active++;
	return i;
}

physParticleSystemHandle_t PhysParticles_CreateBurst( const vec3_t origin, int count,
	float speed, float lifetime ) {
	int i;
	int emitted = 0;
	float spd = speed > 0.0f ? speed : 200.0f;

	if ( count < 1 ) {
		count = 16;
	}
	if ( count > 128 ) {
		count = 128;
	}
	for ( i = 0; i < count; i++ ) {
		physParticleSpawn_t sp;
		float yaw = ( (float)( i * 47 ) ) * 0.1f;
		float pitch = ( (float)( ( i * 19 ) % 50 ) - 25.0f ) * 0.04f;
		Com_Memset( &sp, 0, sizeof( sp ) );
		VectorCopy( origin, sp.origin );
		sp.velocity[0] = cosf( yaw ) * cosf( pitch ) * spd;
		sp.velocity[1] = sinf( yaw ) * cosf( pitch ) * spd;
		sp.velocity[2] = sinf( pitch ) * spd + spd * 0.35f;
		sp.radius = 2.0f;
		sp.lifetime = lifetime > 0.0f ? lifetime : 1.5f;
		sp.bounce = 0.4f;
		sp.friction = 0.25f;
		sp.gravityScale = 1.0f;
		if ( PhysParticles_Emit( &sp ) >= 0 ) {
			emitted++;
		}
	}
	return emitted > 0 ? 0 : -1;
}

void PhysParticles_Step( float dt ) {
	int i;
	physRayResult_t hit;
	vec3_t from, to;
	float g;

	if ( !phys_particles || !phys_particles->integer ) {
		return;
	}
	if ( dt <= 0.0f || dt > 0.1f ) {
		dt = 0.016f;
	}
	g = -800.0f;

	for ( i = 0; i < PHYS_PARTICLE_MAX; i++ ) {
		physParticle_t *p = &s_particles[i];
		float vn;
		if ( !p->active ) {
			continue;
		}
		p->age += dt;
		if ( p->age >= p->lifetime ) {
			p->active = qfalse;
			s_active--;
			continue;
		}

		p->velocity[2] += g * p->gravityScale * dt;
		VectorCopy( p->position, from );
		VectorMA( p->position, dt, p->velocity, to );

		if ( Phys_GetBackend() != PHYS_BACKEND_NONE
			&& Phys_RayCast( from, to, &hit ) && hit.hit && hit.fraction < 1.0f ) {
			VectorMA( hit.hitPoint, p->radius, hit.hitNormal, p->position );
			vn = DotProduct( p->velocity, hit.hitNormal );
			if ( vn < 0.0f ) {
				VectorMA( p->velocity, -( 1.0f + p->bounce ) * vn, hit.hitNormal, p->velocity );
			}
			p->velocity[0] *= ( 1.0f - p->friction );
			p->velocity[1] *= ( 1.0f - p->friction );
		} else {
			VectorCopy( to, p->position );
		}
	}
}

void PhysParticles_DebugDraw( void ) {
	int i;
	vec3_t color, tip;

	VectorSet( color, 1.0f, 0.75f, 0.2f );
	for ( i = 0; i < PHYS_PARTICLE_MAX; i++ ) {
		if ( !s_particles[i].active ) {
			continue;
		}
		VectorCopy( s_particles[i].position, tip );
		tip[2] += s_particles[i].radius * 3.0f;
		PhysDebug_AddLine( s_particles[i].position, tip, color );
	}
}
