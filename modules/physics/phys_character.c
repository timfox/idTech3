/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "phys_bullet.h"
#include "phys_character.h"

#define MAX_PHYS_CHARACTERS 64

typedef struct {
	qboolean	active;
	physBodyHandle_t body;
	vec3_t		origin;
	vec3_t		velocity;
	qboolean	grounded;
	float		radius;
	float		height;
	float		stepHeight;
} physCharacter_t;

static physCharacter_t s_chars[MAX_PHYS_CHARACTERS];
static cvar_t *phys_character;

void Phys_CharacterInit( void ) {
	phys_character = Cvar_Get( "phys_character", "1", CVAR_ARCHIVE );
	Cvar_SetDescription( phys_character,
		"Enable kinematic character controller traps (G_PHYS_CHARACTER_*). Default on." );
	if ( phys_character->integer ) {
		Com_Printf( "[physics] phys_character=1 (kinematic capsule controller)\n" );
	}
}

int Phys_CharacterCreate( float radius, float height, float stepHeight ) {
	int i;
	physBodyDef_t def;

	if ( !phys_character || !phys_character->integer ) {
		return -1;
	}

	for ( i = 0; i < MAX_PHYS_CHARACTERS; i++ ) {
		if ( !s_chars[i].active ) {
			break;
		}
	}
	if ( i >= MAX_PHYS_CHARACTERS ) {
		return -1;
	}

	Com_Memset( &def, 0, sizeof( def ) );
	def.type = PHYS_BODY_KINEMATIC;
	def.shape = PHYS_SHAPE_CAPSULE;
	def.mass = 1.0f;
	def.radius = radius > 0.0f ? radius : 16.0f;
	def.height = height > 0.0f ? height : 56.0f;
	def.friction = 0.8f;
	def.restitution = 0.0f;

	s_chars[i].active = qtrue;
	s_chars[i].body = Phys_CreateBody( &def );
	s_chars[i].radius = def.radius;
	s_chars[i].height = def.height;
	s_chars[i].stepHeight = stepHeight > 0.0f ? stepHeight : 18.0f;
	VectorClear( s_chars[i].origin );
	VectorClear( s_chars[i].velocity );
	s_chars[i].grounded = qtrue;

	return i;
}

int Phys_CharacterMove( int handle, const float *wishDir, float wishSpeed, qboolean jump ) {
	physCharacter_t *ch;
	vec3_t move;
	float speed;

	if ( handle < 0 || handle >= MAX_PHYS_CHARACTERS || !s_chars[handle].active ) {
		return -1;
	}
	ch = &s_chars[handle];

	if ( wishDir ) {
		VectorCopy( wishDir, move );
	} else {
		VectorClear( move );
	}
	speed = wishSpeed > 0.0f ? wishSpeed : 320.0f;
	VectorNormalize( move );
	VectorScale( move, speed * 0.016f, move );
	VectorAdd( ch->origin, move, ch->origin );

	if ( jump && ch->grounded ) {
		ch->velocity[2] = 280.0f;
		ch->grounded = qfalse;
	}

	ch->velocity[2] -= 800.0f * 0.016f;
	ch->origin[2] += ch->velocity[2] * 0.016f;
	if ( ch->origin[2] < 0.0f ) {
		ch->origin[2] = 0.0f;
		ch->velocity[2] = 0.0f;
		ch->grounded = qtrue;
	}

	if ( ch->body ) {
		Phys_SetBodyTransform( ch->body, ch->origin, NULL );
		Phys_SetBodyVelocity( ch->body, ch->velocity, NULL );
	}

	return 0;
}

void Phys_CharacterDestroy( int handle ) {
	if ( handle < 0 || handle >= MAX_PHYS_CHARACTERS || !s_chars[handle].active ) {
		return;
	}
	if ( s_chars[handle].body ) {
		Phys_DestroyBody( s_chars[handle].body );
	}
	Com_Memset( &s_chars[handle], 0, sizeof( s_chars[handle] ) );
}

void Phys_CharacterGetState( int handle, vec3_t origin, vec3_t velocity, qboolean *grounded ) {
	if ( handle < 0 || handle >= MAX_PHYS_CHARACTERS || !s_chars[handle].active ) {
		return;
	}
	if ( origin ) {
		VectorCopy( s_chars[handle].origin, origin );
	}
	if ( velocity ) {
		VectorCopy( s_chars[handle].velocity, velocity );
	}
	if ( grounded ) {
		*grounded = s_chars[handle].grounded;
	}
}
