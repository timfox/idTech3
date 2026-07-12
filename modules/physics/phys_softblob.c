/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "q_shared.h"
#include "qcommon.h"
#include "phys_softblob.h"
#include "phys_bullet.h"
#include "phys_debugdraw.h"

#include <math.h>

typedef struct {
	vec3_t position;
	vec3_t predicted;
	vec3_t velocity;
	float  invMass;
	int    pinned;
} softBlobParticle_t;

typedef struct {
	int   p0, p1;
	float rest;
	float compliance;
	float lambda;
} softBlobConstraint_t;

typedef struct {
	qboolean              active;
	softBlobParticle_t    particles[PHYS_SOFTBLOB_PARTICLES_MAX];
	int                   numParticles;
	softBlobConstraint_t  constraints[PHYS_SOFTBLOB_CONSTRAINTS_MAX];
	int                   numConstraints;
	physSoftBlobConfig_t  config;
	int                   res;
} softBlob_t;

static softBlob_t s_blobs[PHYS_SOFTBLOB_MAX];
static int s_blobCount;
static cvar_t *phys_softblob;

void SoftBlob_DefaultConfig( physSoftBlobConfig_t *cfg ) {
	if ( !cfg ) {
		return;
	}
	cfg->gravity = 800.0f;
	cfg->damping = 0.98f;
	cfg->compliance = 0.0001f;
	cfg->thickness = 1.0f;
	cfg->particleMass = 1.0f;
	cfg->solverIterations = 6;
}

void SoftBlob_Init( void ) {
	phys_softblob = Cvar_Get( "phys_softblob", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( phys_softblob,
		"Enable soft-blob XPBD lattice solver (collides against Box3D Soft Step world)." );
	Com_Memset( s_blobs, 0, sizeof( s_blobs ) );
	s_blobCount = 0;
	if ( phys_softblob->integer ) {
		Com_Printf( "[physics] softblob solver ready (XPBD lattice + Phys_RayCast)\n" );
	}
}

void SoftBlob_Shutdown( void ) {
	Com_Memset( s_blobs, 0, sizeof( s_blobs ) );
	s_blobCount = 0;
}

int SoftBlob_GetActiveCount( void ) {
	int i, n = 0;
	for ( i = 0; i < PHYS_SOFTBLOB_MAX; i++ ) {
		if ( s_blobs[i].active ) {
			n++;
		}
	}
	return n;
}

static void SoftBlob_AddConstraint( softBlob_t *b, int p0, int p1, float compliance ) {
	softBlobConstraint_t *c;
	if ( b->numConstraints >= PHYS_SOFTBLOB_CONSTRAINTS_MAX || p0 == p1 ) {
		return;
	}
	c = &b->constraints[b->numConstraints++];
	c->p0 = p0;
	c->p1 = p1;
	c->rest = Distance( b->particles[p0].position, b->particles[p1].position );
	c->compliance = compliance;
	c->lambda = 0.0f;
}

physSoftBlobHandle_t SoftBlob_CreateLattice( const vec3_t origin, int res, float spacing,
	const physSoftBlobConfig_t *cfg ) {
	softBlob_t *b;
	int x, y, z, idx;
	int h;
	float invMass;

	if ( !phys_softblob || !phys_softblob->integer ) {
		return -1;
	}
	if ( res < 2 ) {
		res = 2;
	}
	if ( res > 8 ) {
		res = 8;
	}
	if ( spacing <= 0.0f ) {
		spacing = 8.0f;
	}
	if ( res * res * res > PHYS_SOFTBLOB_PARTICLES_MAX ) {
		return -1;
	}

	for ( h = 0; h < PHYS_SOFTBLOB_MAX; h++ ) {
		if ( !s_blobs[h].active ) {
			break;
		}
	}
	if ( h >= PHYS_SOFTBLOB_MAX ) {
		return -1;
	}

	b = &s_blobs[h];
	Com_Memset( b, 0, sizeof( *b ) );
	b->active = qtrue;
	b->res = res;
	if ( cfg ) {
		b->config = *cfg;
	} else {
		SoftBlob_DefaultConfig( &b->config );
	}
	invMass = b->config.particleMass > 0.0f ? ( 1.0f / b->config.particleMass ) : 1.0f;

	b->numParticles = res * res * res;
	for ( z = 0; z < res; z++ ) {
		for ( y = 0; y < res; y++ ) {
			for ( x = 0; x < res; x++ ) {
				idx = ( z * res + y ) * res + x;
				b->particles[idx].position[0] = origin[0] + x * spacing;
				b->particles[idx].position[1] = origin[1] + y * spacing;
				b->particles[idx].position[2] = origin[2] + z * spacing;
				VectorCopy( b->particles[idx].position, b->particles[idx].predicted );
				VectorClear( b->particles[idx].velocity );
				b->particles[idx].invMass = invMass;
				b->particles[idx].pinned = 0;
			}
		}
	}

	/* Structural + shear edges on the lattice. */
	for ( z = 0; z < res; z++ ) {
		for ( y = 0; y < res; y++ ) {
			for ( x = 0; x < res; x++ ) {
				idx = ( z * res + y ) * res + x;
				if ( x + 1 < res ) {
					SoftBlob_AddConstraint( b, idx, idx + 1, b->config.compliance );
				}
				if ( y + 1 < res ) {
					SoftBlob_AddConstraint( b, idx, idx + res, b->config.compliance );
				}
				if ( z + 1 < res ) {
					SoftBlob_AddConstraint( b, idx, idx + res * res, b->config.compliance );
				}
				if ( x + 1 < res && y + 1 < res ) {
					SoftBlob_AddConstraint( b, idx, idx + 1 + res, b->config.compliance * 2.0f );
				}
				if ( x + 1 < res && z + 1 < res ) {
					SoftBlob_AddConstraint( b, idx, idx + 1 + res * res, b->config.compliance * 2.0f );
				}
				if ( y + 1 < res && z + 1 < res ) {
					SoftBlob_AddConstraint( b, idx, idx + res + res * res, b->config.compliance * 2.0f );
				}
			}
		}
	}

	if ( h >= s_blobCount ) {
		s_blobCount = h + 1;
	}
	return h;
}

void SoftBlob_Destroy( physSoftBlobHandle_t handle ) {
	if ( handle < 0 || handle >= PHYS_SOFTBLOB_MAX ) {
		return;
	}
	Com_Memset( &s_blobs[handle], 0, sizeof( s_blobs[handle] ) );
}

void SoftBlob_PinCorner( physSoftBlobHandle_t handle, int cornerIndex ) {
	softBlob_t *b;
	int corners[8];
	int r, c;

	if ( handle < 0 || handle >= PHYS_SOFTBLOB_MAX || !s_blobs[handle].active ) {
		return;
	}
	b = &s_blobs[handle];
	r = b->res;
	corners[0] = 0;
	corners[1] = r - 1;
	corners[2] = ( r - 1 ) * r;
	corners[3] = ( r - 1 ) * r + ( r - 1 );
	corners[4] = ( r - 1 ) * r * r;
	corners[5] = ( r - 1 ) * r * r + ( r - 1 );
	corners[6] = ( r - 1 ) * r * r + ( r - 1 ) * r;
	corners[7] = ( r - 1 ) * r * r + ( r - 1 ) * r + ( r - 1 );
	if ( cornerIndex < 0 || cornerIndex > 7 ) {
		cornerIndex = 0;
	}
	c = corners[cornerIndex];
	if ( c >= 0 && c < b->numParticles ) {
		b->particles[c].pinned = 1;
		b->particles[c].invMass = 0.0f;
	}
}

void SoftBlob_ApplyImpulse( physSoftBlobHandle_t handle, const vec3_t point,
	const vec3_t impulse, float radius ) {
	softBlob_t *b;
	int i;
	float r2;

	if ( handle < 0 || handle >= PHYS_SOFTBLOB_MAX || !s_blobs[handle].active || !impulse ) {
		return;
	}
	b = &s_blobs[handle];
	r2 = radius > 0.0f ? radius * radius : 256.0f;
	for ( i = 0; i < b->numParticles; i++ ) {
		vec3_t d;
		float dist2;
		if ( b->particles[i].pinned ) {
			continue;
		}
		VectorSubtract( b->particles[i].position, point, d );
		dist2 = DotProduct( d, d );
		if ( dist2 > r2 ) {
			continue;
		}
		VectorMA( b->particles[i].velocity, 1.0f, impulse, b->particles[i].velocity );
	}
}

static void SoftBlob_CollideParticle( softBlob_t *b, softBlobParticle_t *p ) {
	physRayResult_t hit;
	vec3_t from, to, push;
	float vn;

	if ( p->pinned || Phys_GetBackend() == PHYS_BACKEND_NONE ) {
		return;
	}
	VectorCopy( p->position, from );
	VectorCopy( p->predicted, to );
	if ( Distance( from, to ) < 0.01f ) {
		return;
	}
	if ( !Phys_RayCast( from, to, &hit ) || !hit.hit || hit.fraction >= 1.0f ) {
		return;
	}
	VectorMA( hit.hitPoint, b->config.thickness, hit.hitNormal, push );
	VectorCopy( push, p->predicted );
	vn = DotProduct( p->velocity, hit.hitNormal );
	if ( vn < 0.0f ) {
		VectorMA( p->velocity, -vn * 1.2f, hit.hitNormal, p->velocity );
	}
}

void SoftBlob_Step( float dt ) {
	int bi, i, iter;
	float subDt, invDt;

	if ( !phys_softblob || !phys_softblob->integer ) {
		return;
	}
	if ( dt <= 0.0f || dt > 0.1f ) {
		dt = 0.016f;
	}

	for ( bi = 0; bi < PHYS_SOFTBLOB_MAX; bi++ ) {
		softBlob_t *b = &s_blobs[bi];
		if ( !b->active ) {
			continue;
		}
		subDt = dt / (float)( b->config.solverIterations > 0 ? b->config.solverIterations : 1 );
		invDt = 1.0f / dt;

		for ( i = 0; i < b->numParticles; i++ ) {
			softBlobParticle_t *p = &b->particles[i];
			if ( p->pinned ) {
				VectorCopy( p->position, p->predicted );
				continue;
			}
			p->velocity[2] -= b->config.gravity * dt;
			VectorScale( p->velocity, b->config.damping, p->velocity );
			VectorMA( p->position, dt, p->velocity, p->predicted );
		}

		for ( i = 0; i < b->numConstraints; i++ ) {
			b->constraints[i].lambda = 0.0f;
		}

		for ( iter = 0; iter < b->config.solverIterations; iter++ ) {
			for ( i = 0; i < b->numConstraints; i++ ) {
				softBlobConstraint_t *c = &b->constraints[i];
				softBlobParticle_t *a = &b->particles[c->p0];
				softBlobParticle_t *bp = &b->particles[c->p1];
				vec3_t diff, corr;
				float dist, C, alpha, w, dLambda;

				VectorSubtract( bp->predicted, a->predicted, diff );
				dist = VectorLength( diff );
				if ( dist < 1e-6f ) {
					continue;
				}
				C = dist - c->rest;
				alpha = c->compliance / ( subDt * subDt );
				w = a->invMass + bp->invMass;
				if ( w <= 0.0f ) {
					continue;
				}
				dLambda = ( -C - alpha * c->lambda ) / ( w + alpha );
				c->lambda += dLambda;
				VectorScale( diff, dLambda / dist, corr );
				if ( !a->pinned ) {
					VectorMA( a->predicted, -a->invMass, corr, a->predicted );
				}
				if ( !bp->pinned ) {
					VectorMA( bp->predicted, bp->invMass, corr, bp->predicted );
				}
			}
		}

		for ( i = 0; i < b->numParticles; i++ ) {
			SoftBlob_CollideParticle( b, &b->particles[i] );
		}

		for ( i = 0; i < b->numParticles; i++ ) {
			softBlobParticle_t *p = &b->particles[i];
			vec3_t delta;
			if ( p->pinned ) {
				continue;
			}
			VectorSubtract( p->predicted, p->position, delta );
			VectorScale( delta, invDt, p->velocity );
			VectorCopy( p->predicted, p->position );
		}
	}
}

void SoftBlob_DebugDraw( void ) {
	int bi, i;
	vec3_t color;

	VectorSet( color, 0.9f, 0.4f, 0.95f );
	for ( bi = 0; bi < PHYS_SOFTBLOB_MAX; bi++ ) {
		softBlob_t *b = &s_blobs[bi];
		if ( !b->active ) {
			continue;
		}
		for ( i = 0; i < b->numConstraints; i++ ) {
			softBlobConstraint_t *c = &b->constraints[i];
			PhysDebug_AddLine( b->particles[c->p0].position, b->particles[c->p1].position, color );
		}
	}
}
