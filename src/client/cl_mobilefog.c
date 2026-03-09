/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Mobile fog implementation.

Exponential height fog:
  Applied as a post-vertex color modulation. The fog factor is
  computed as: fogFactor = exp(-density * distance * heightFalloff)
  where heightFalloff increases fog below a reference height.
  This is essentially free — no extra draw calls, just a color blend.

Sprite-based volumetric fog:
  Spawns billboard particles around the camera that rotate slowly,
  creating the illusion of volumetric fog. Each sprite uses a soft
  cloud texture with alpha blending. The sprites orbit the camera
  at varying distances and heights.
  Cost: ~20-40 transparent quads, acceptable on most mobile GPUs.
===========================================================================
*/

#include "client.h"
#include "cl_mobilefog.h"
#include "cl_particles.h"
#include <math.h>
#include <stdio.h>

static cvar_t *r_mobileFog;
static cvar_t *r_volumetricFogTier;
static cvar_t *r_volumetricFogDensity;
static cvar_t *r_volumetricFogTint;
static cvar_t *r_volumetricFogHeightFalloff;
static cvar_t *r_mobileFogDensity;
static cvar_t *r_mobileFogHeight;
static cvar_t *r_mobileFogHeightFalloff;
static cvar_t *r_mobileFogColorR;
static cvar_t *r_mobileFogColorG;
static cvar_t *r_mobileFogColorB;
static cvar_t *r_mobileFogSpriteCount;
static cvar_t *r_mobileFogSpriteRadius;
static cvar_t *r_mobileFogSpriteSize;
static cvar_t *r_mobileFogSpriteSpeed;

static qboolean fogInitialized = qfalse;
static float fogTime = 0.0f;

void MobileFog_Init( void ) {
	r_mobileFog = Cvar_Get( "r_mobileFog", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_mobileFog,
		"Mobile fog mode: 0=off, 1=exponential height fog (free), "
		"2=sprite-based volumetric (medium), 3=full volumetric (expensive)." );

	r_volumetricFogTier = Cvar_Get( "r_volumetricFogTier", "0", CVAR_ARCHIVE );
	Cvar_SetDescription( r_volumetricFogTier,
		"Volumetric fog quality tier: 0=full froxel, 1=reduced, 2=mobile height fog, 3=mobile sprites, 4=off." );

	r_volumetricFogDensity = Cvar_Get( "r_volumetricFogDensity", "0.6", CVAR_ARCHIVE );
	r_volumetricFogTint = Cvar_Get( "r_volumetricFogTint", "1 1 1", CVAR_ARCHIVE );
	r_volumetricFogHeightFalloff = Cvar_Get( "r_volumetricFogHeightFalloff", "0.015", CVAR_ARCHIVE );
	r_mobileFogDensity = Cvar_Get( "r_mobileFogDensity", "0.015", CVAR_ARCHIVE );
	r_mobileFogHeight = Cvar_Get( "r_mobileFogHeight", "0", CVAR_ARCHIVE );
	r_mobileFogHeightFalloff = Cvar_Get( "r_mobileFogHeightFalloff", "0.5", CVAR_ARCHIVE );
	r_mobileFogColorR = Cvar_Get( "r_mobileFogColorR", "0.7", CVAR_ARCHIVE );
	r_mobileFogColorG = Cvar_Get( "r_mobileFogColorG", "0.75", CVAR_ARCHIVE );
	r_mobileFogColorB = Cvar_Get( "r_mobileFogColorB", "0.85", CVAR_ARCHIVE );
	r_mobileFogSpriteCount = Cvar_Get( "r_mobileFogSpriteCount", "24", CVAR_ARCHIVE );
	r_mobileFogSpriteRadius = Cvar_Get( "r_mobileFogSpriteRadius", "400", CVAR_ARCHIVE );
	r_mobileFogSpriteSize = Cvar_Get( "r_mobileFogSpriteSize", "200", CVAR_ARCHIVE );
	r_mobileFogSpriteSpeed = Cvar_Get( "r_mobileFogSpriteSpeed", "0.1", CVAR_ARCHIVE );

	fogInitialized = qtrue;
	fogTime = 0.0f;

#ifdef __ANDROID__
	if ( !r_mobileFog->integer ) {
		Cvar_Set( "r_mobileFog", "1" );
		Com_Printf( "Mobile fog: auto-enabled on Android (mode 1: height fog)\n" );
	}
#endif

	Com_Printf( "Mobile fog: initialized (mode %d)\n", r_mobileFog->integer );
}

void MobileFog_Shutdown( void ) {
	fogInitialized = qfalse;
}

mobileFogMode_t MobileFog_GetMode( void ) {
	if ( !fogInitialized ) return MOBILE_FOG_NONE;
	if ( r_volumetricFogTier && r_volumetricFogTier->integer == 4 ) {
		return MOBILE_FOG_NONE;
	}
	if ( r_volumetricFogTier && r_volumetricFogTier->integer >= 2 && r_volumetricFogTier->integer <= 3 ) {
		return ( r_volumetricFogTier->integer == 2 ) ? MOBILE_FOG_HEIGHT : MOBILE_FOG_SPRITES;
	}
	if ( !r_mobileFog ) return MOBILE_FOG_NONE;
	return (mobileFogMode_t)r_mobileFog->integer;
}

/* ---- Exponential height fog (mode 1) ---- */

static void MobileFog_DrawHeightFog( const vec3_t viewOrigin ) {
	float density, fogHeight, heightFalloff;
	vec4_t fogColor;
	float heightFactor;
	qboolean useVolumetricParams = qfalse;

	if ( !re.SetColor || !re.DrawStretchPic ) return;

	if ( r_volumetricFogTier && r_volumetricFogTier->integer >= 2 && r_volumetricFogTier->integer <= 3 ) {
		useVolumetricParams = qtrue;
	}
	if ( useVolumetricParams && r_volumetricFogDensity && r_volumetricFogTint && r_volumetricFogHeightFalloff ) {
		float tr, tg, tb;
		density = r_volumetricFogDensity->value * 0.025f;
		fogHeight = r_mobileFogHeight->value;
		heightFalloff = r_volumetricFogHeightFalloff->value * 10.0f;
		if ( sscanf( r_volumetricFogTint->string, "%f %f %f", &tr, &tg, &tb ) == 3 ) {
			fogColor[0] = ( tr < 0.0f ) ? 0.0f : ( tr > 4.0f ? 4.0f : tr );
			fogColor[1] = ( tg < 0.0f ) ? 0.0f : ( tg > 4.0f ? 4.0f : tg );
			fogColor[2] = ( tb < 0.0f ) ? 0.0f : ( tb > 4.0f ? 4.0f : tb );
		} else {
			fogColor[0] = fogColor[1] = fogColor[2] = 1.0f;
		}
	} else {
		density = r_mobileFogDensity->value;
		fogHeight = r_mobileFogHeight->value;
		heightFalloff = r_mobileFogHeightFalloff->value;
		fogColor[0] = r_mobileFogColorR->value;
		fogColor[1] = r_mobileFogColorG->value;
		fogColor[2] = r_mobileFogColorB->value;
	}

	heightFactor = 1.0f;
	if ( viewOrigin[2] > fogHeight ) {
		heightFactor = expf( -heightFalloff * ( viewOrigin[2] - fogHeight ) * 0.01f );
	}

	float fogStrength = density * heightFactor;
	if ( fogStrength < 0.001f ) return;
	if ( fogStrength > 0.95f ) fogStrength = 0.95f;

	fogColor[3] = fogStrength;

	re.SetColor( fogColor );
	re.DrawStretchPic( 0, 0, (float)cls.glconfig.vidWidth, (float)cls.glconfig.vidHeight,
		0, 0, 1, 1, cls.whiteShader );
	re.SetColor( NULL );
}

/* ---- Sprite-based volumetric fog (mode 2) ---- */

static float fogHash( float x, float y ) {
	float h = x * 12.9898f + y * 78.233f;
	h = sinf( h ) * 43758.5453f;
	return h - floorf( h );
}

static void MobileFog_DrawSpriteFog( const vec3_t viewOrigin, const vec3_t viewForward,
	const vec3_t viewRight, const vec3_t viewUp, float frametime ) {
	int count, i;
	float radius, spriteSize, speed;

	count = r_mobileFogSpriteCount->integer;
	if ( count < 1 ) count = 1;
	if ( count > 64 ) count = 64;

	radius = r_mobileFogSpriteRadius->value;
	spriteSize = r_mobileFogSpriteSize->value;
	speed = r_mobileFogSpriteSpeed->value;

	fogTime += frametime;

	for ( i = 0; i < count; i++ ) {
		float angle = fogTime * speed * ( 0.5f + fogHash( (float)i, 0.0f ) * 0.5f );
		angle += ( (float)i / (float)count ) * 6.283185f;

		float dist = radius * ( 0.3f + fogHash( (float)i, 1.0f ) * 0.7f );
		float height = ( fogHash( (float)i, 2.0f ) - 0.5f ) * radius * 0.4f;
		float size = spriteSize * ( 0.6f + fogHash( (float)i, 3.0f ) * 0.8f );

		vec3_t pos;
		pos[0] = viewOrigin[0] + cosf( angle ) * dist;
		pos[1] = viewOrigin[1] + sinf( angle ) * dist;
		pos[2] = viewOrigin[2] + height;

		float alpha = r_mobileFogDensity->value * ( 0.3f + fogHash( (float)i, 4.0f ) * 0.7f );
		if ( alpha > 0.5f ) alpha = 0.5f;

		Particles_EmitSmoke( 0, pos, viewUp, 3000.0f, size, size * 1.5f, alpha, PC_CUSTOM );
	}

	(void)viewForward;
	(void)viewRight;
}

void MobileFog_Frame( const vec3_t viewOrigin, const vec3_t viewForward,
	const vec3_t viewRight, const vec3_t viewUp, float frametime ) {
	int mode;

	if ( !fogInitialized ) return;
	if ( r_volumetricFogTier && r_volumetricFogTier->integer == 4 ) {
		return;
	}
	if ( r_volumetricFogTier && r_volumetricFogTier->integer >= 2 && r_volumetricFogTier->integer <= 3 ) {
		mode = r_volumetricFogTier->integer;
	} else if ( r_mobileFog ) {
		mode = r_mobileFog->integer;
	} else {
		return;
	}

	if ( mode <= 0 ) return;

	switch ( mode ) {
		case MOBILE_FOG_HEIGHT:
			MobileFog_DrawHeightFog( viewOrigin );
			break;
		case MOBILE_FOG_SPRITES:
			MobileFog_DrawSpriteFog( viewOrigin, viewForward, viewRight, viewUp, frametime );
			break;
		case MOBILE_FOG_FULL:
			break;
	}
}
