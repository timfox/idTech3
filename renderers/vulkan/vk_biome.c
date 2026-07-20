/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Raster Ultra 1.14 biome evaluation — deterministic, metadata-gated.
===========================================================================
*/

#include "tr_local.h"
#include "vk_biome.h"
#include "vk_terrain.h"
#include "vk_pass_registry.h"

#include <math.h>

static cvar_t *r_biome;
static cvar_t *r_biomeSeed;
static cvar_t *r_biomeDebug;
static cvar_t *r_biomeSeason; /* 0 spring .. 3 winter */
static cvar_t *r_biomeWetness;

static vkBiomeDef_t s_defs[VK_BIOME_COUNT];
static uint32_t s_seed;
static uint32_t s_generation;
static qboolean s_ready;

static float Biome_Hash( float x, float z, uint32_t salt )
{
	uint32_t n = (uint32_t)( x * 374761.0f ) ^ (uint32_t)( z * 668265.0f ) ^ ( s_seed * 0x9E3779B9u ) ^ salt;
	n = ( n ^ ( n >> 16 ) ) * 0x7feb352du;
	n = ( n ^ ( n >> 15 ) ) * 0x846ca68bu;
	n ^= n >> 16;
	return (float)( n & 0xFFFFFFu ) / (float)0xFFFFFFu;
}

static float Biome_Noise( float x, float z )
{
	float fx = floorf( x );
	float fz = floorf( z );
	float tx = x - fx;
	float tz = z - fz;
	float a = Biome_Hash( fx, fz, 1 );
	float b = Biome_Hash( fx + 1.0f, fz, 1 );
	float c = Biome_Hash( fx, fz + 1.0f, 1 );
	float d = Biome_Hash( fx + 1.0f, fz + 1.0f, 1 );
	float ux = tx * tx * ( 3.0f - 2.0f * tx );
	float uz = tz * tz * ( 3.0f - 2.0f * tz );
	float ab = a + ( b - a ) * ux;
	float cd = c + ( d - c ) * ux;
	return ab + ( cd - ab ) * uz;
}

static void Biome_InitDefs( void )
{
	Com_Memset( s_defs, 0, sizeof( s_defs ) );

	s_defs[VK_BIOME_SOIL] = (vkBiomeDef_t){
		.id = VK_BIOME_SOIL, .elevMin = 0.0f, .elevMax = 1.0f, .slopeMax = 0.55f,
		.moisture = 0.4f, .temperature = 0.5f, .vegDensity = 0.2f,
		.grassWeight = 0.3f, .treeWeight = 0.05f, .rockWeight = 0.1f, .layerMask = 1u
	};
	s_defs[VK_BIOME_GRASS] = (vkBiomeDef_t){
		.id = VK_BIOME_GRASS, .elevMin = 0.05f, .elevMax = 0.55f, .slopeMax = 0.35f,
		.moisture = 0.55f, .temperature = 0.55f, .vegDensity = 0.85f,
		.grassWeight = 1.0f, .treeWeight = 0.15f, .rockWeight = 0.05f, .layerMask = 1u << 1
	};
	s_defs[VK_BIOME_ROCK] = (vkBiomeDef_t){
		.id = VK_BIOME_ROCK, .elevMin = 0.2f, .elevMax = 1.0f, .slopeMax = 1.0f,
		.moisture = 0.2f, .temperature = 0.4f, .vegDensity = 0.05f,
		.grassWeight = 0.05f, .treeWeight = 0.0f, .rockWeight = 1.0f, .layerMask = 1u << 2
	};
	s_defs[VK_BIOME_SAND] = (vkBiomeDef_t){
		.id = VK_BIOME_SAND, .elevMin = 0.0f, .elevMax = 0.25f, .slopeMax = 0.25f,
		.moisture = 0.15f, .temperature = 0.85f, .vegDensity = 0.08f,
		.grassWeight = 0.1f, .rockWeight = 0.2f, .layerMask = 1u << 3
	};
	s_defs[VK_BIOME_MUD] = (vkBiomeDef_t){
		.id = VK_BIOME_MUD, .elevMin = 0.0f, .elevMax = 0.2f, .slopeMax = 0.2f,
		.moisture = 0.9f, .temperature = 0.5f, .vegDensity = 0.35f,
		.grassWeight = 0.4f, .wetness = 0.8f, .layerMask = 1u << 4
	};
	s_defs[VK_BIOME_SNOW] = (vkBiomeDef_t){
		.id = VK_BIOME_SNOW, .elevMin = 0.55f, .elevMax = 1.0f, .slopeMax = 0.7f,
		.moisture = 0.5f, .temperature = 0.1f, .vegDensity = 0.12f,
		.grassWeight = 0.05f, .treeWeight = 0.2f, .snowWeight = 1.0f, .layerMask = 1u << 5
	};
	s_defs[VK_BIOME_WETLAND] = (vkBiomeDef_t){
		.id = VK_BIOME_WETLAND, .elevMin = 0.0f, .elevMax = 0.15f, .slopeMax = 0.15f,
		.moisture = 1.0f, .temperature = 0.45f, .vegDensity = 0.7f,
		.grassWeight = 0.9f, .wetness = 1.0f, .layerMask = 1u << 6
	};
	s_defs[VK_BIOME_FOREST] = (vkBiomeDef_t){
		.id = VK_BIOME_FOREST, .elevMin = 0.1f, .elevMax = 0.65f, .slopeMax = 0.4f,
		.moisture = 0.65f, .temperature = 0.5f, .vegDensity = 0.95f,
		.grassWeight = 0.5f, .treeWeight = 1.0f, .rockWeight = 0.1f, .layerMask = 1u << 7
	};
	s_defs[VK_BIOME_DESERT] = (vkBiomeDef_t){
		.id = VK_BIOME_DESERT, .elevMin = 0.0f, .elevMax = 0.4f, .slopeMax = 0.3f,
		.moisture = 0.05f, .temperature = 0.95f, .vegDensity = 0.04f,
		.rockWeight = 0.3f, .layerMask = 1u << 8
	};
	s_defs[VK_BIOME_ASH] = (vkBiomeDef_t){
		.id = VK_BIOME_ASH, .elevMin = 0.3f, .elevMax = 0.9f, .slopeMax = 0.6f,
		.moisture = 0.1f, .temperature = 0.7f, .vegDensity = 0.02f,
		.rockWeight = 0.5f, .layerMask = 1u << 9
	};
}

void VK_Biome_RegisterCvars( void )
{
	r_biome = ri.Cvar_Get( "r_biome", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_biome, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_biome,
		"Raster Ultra 1.14 biome evaluation (latched via archive). Off by default.\n"
		"Requires terrain metadata for height/slope inputs; inactive otherwise." );
	r_biomeSeed = ri.Cvar_Get( "r_biomeSeed", "1", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_SetDescription( r_biomeSeed, "Deterministic biome placement seed (stable across launches)." );
	r_biomeDebug = ri.Cvar_Get( "r_biomeDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_biomeDebug, "0", "3", CV_INTEGER );
	r_biomeSeason = ri.Cvar_Get( "r_biomeSeason", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_biomeSeason, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_biomeSeason, "Season: 0 spring, 1 summer, 2 autumn, 3 winter" );
	r_biomeWetness = ri.Cvar_Get( "r_biomeWetness", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_biomeWetness, "0", "1", CV_FLOAT );

	ri.Cmd_AddCommand( "biome_status", VK_Biome_Status_f );
	ri.Printf( PRINT_ALL, "Biome system (Raster Ultra 1.14): r_biome %s seed=%s\n",
		r_biome->integer ? "enabled" : "disabled", r_biomeSeed->string );
}

void VK_Biome_Init( void )
{
	VK_Biome_RegisterCvars();
	Biome_InitDefs();
	s_seed = (uint32_t)( r_biomeSeed ? r_biomeSeed->integer : 1 );
	if ( s_seed == 0 ) {
		s_seed = 1;
	}
	s_ready = qtrue;
	s_generation = 0;
}

void VK_Biome_Shutdown( void )
{
	s_ready = qfalse;
}

void VK_Biome_OnWorldLoad( void )
{
	s_generation++;
	s_seed = (uint32_t)( r_biomeSeed ? r_biomeSeed->integer : 1 );
	if ( s_seed == 0 ) {
		s_seed = 1;
	}
}

void VK_Biome_OnWorldUnload( void )
{
}

qboolean VK_Biome_Active( void )
{
	return ( r_biome && r_biome->integer && s_ready ) ? qtrue : qfalse;
}

uint32_t VK_Biome_Seed( void )
{
	return s_seed;
}

void VK_Biome_Evaluate( float worldX, float worldZ, float weights[VK_BIOME_COUNT] )
{
	float height = 0.0f, slope = 0.0f, elevNorm;
	float moisture, temp, n;
	float season = r_biomeSeason ? r_biomeSeason->value : 0.0f;
	float wetBoost = r_biomeWetness ? r_biomeWetness->value : 0.0f;
	float sum = 0.0f;
	int i;
	float scale = CBTerrain_GetScale();

	Com_Memset( weights, 0, sizeof( float ) * VK_BIOME_COUNT );

	if ( CBTerrain_HasMetadata() ) {
		CBTerrain_SampleHeight( worldX, worldZ, &height );
		slope = CBTerrain_SampleSlope( worldX, worldZ );
		elevNorm = Com_Clamp( 0.0f, 1.0f, ( height / ( scale * 0.35f + 1.0f ) ) * 0.5f + 0.25f );
	} else {
		elevNorm = 0.35f;
		slope = 0.1f;
	}

	n = Biome_Noise( worldX * 0.01f, worldZ * 0.01f );
	moisture = Com_Clamp( 0.0f, 1.0f, 0.35f + 0.4f * n + wetBoost );
	temp = Com_Clamp( 0.0f, 1.0f, 0.7f - elevNorm * 0.6f - season * 0.12f );

	for ( i = 0; i < VK_BIOME_COUNT; i++ ) {
		const vkBiomeDef_t *d = &s_defs[i];
		float w = 1.0f;
		if ( elevNorm < d->elevMin || elevNorm > d->elevMax ) {
			w *= 0.05f;
		} else {
			w *= 1.0f - fabsf( elevNorm - 0.5f * ( d->elevMin + d->elevMax ) );
		}
		if ( slope > d->slopeMax ) {
			w *= 0.15f;
		}
		w *= 1.0f - fabsf( moisture - d->moisture );
		w *= 1.0f - 0.5f * fabsf( temp - d->temperature );
		w *= 0.5f + Biome_Hash( worldX, worldZ, (uint32_t)( i + 7 ) );
		if ( w < 0.0f ) {
			w = 0.0f;
		}
		weights[i] = w;
		sum += w;
	}

	if ( sum > 1e-5f ) {
		for ( i = 0; i < VK_BIOME_COUNT; i++ ) {
			weights[i] /= sum;
		}
	} else {
		weights[VK_BIOME_GRASS] = 1.0f;
	}
}

vkBiomeId_t VK_Biome_Primary( float worldX, float worldZ )
{
	float weights[VK_BIOME_COUNT];
	int i, best = 0;
	float bestW = -1.0f;
	VK_Biome_Evaluate( worldX, worldZ, weights );
	for ( i = 0; i < VK_BIOME_COUNT; i++ ) {
		if ( weights[i] > bestW ) {
			bestW = weights[i];
			best = i;
		}
	}
	return (vkBiomeId_t)best;
}

float VK_Biome_VegetationDensity( float worldX, float worldZ )
{
	float weights[VK_BIOME_COUNT];
	float d = 0.0f;
	int i;
	VK_Biome_Evaluate( worldX, worldZ, weights );
	for ( i = 0; i < VK_BIOME_COUNT; i++ ) {
		d += weights[i] * s_defs[i].vegDensity;
	}
	return Com_Clamp( 0.0f, 1.0f, d );
}

const vkBiomeDef_t *VK_Biome_GetDef( vkBiomeId_t id )
{
	if ( id < 0 || id >= VK_BIOME_COUNT ) {
		return &s_defs[VK_BIOME_SOIL];
	}
	return &s_defs[id];
}

void VK_Biome_Frame( void )
{
	if ( !VK_Biome_Active() ) {
		return;
	}
	if ( !CBTerrain_HasMetadata() ) {
		return;
	}
	vk_spine_pass_begin( VK_SPINE_PASS_BIOME_EVAL );
	/* Evaluation is query-driven; stamp pass for registry ownership. */
	vk_spine_note_write( VK_SPINE_RES_BIOME_MAP, VK_SPINE_PASS_BIOME_EVAL, VK_SPINE_ACCESS_STORAGE_WRITE );
	vk_spine_pass_end( VK_SPINE_PASS_BIOME_EVAL );
}

void VK_Biome_Status_f( void )
{
	static const char *names[] = {
		"soil", "grass", "rock", "sand", "mud", "snow", "wetland", "forest", "desert", "ash"
	};
	ri.Printf( PRINT_ALL, "======== Biomes (Raster Ultra 1.14) ========\n" );
	ri.Printf( PRINT_ALL, "active       : %s\n", VK_Biome_Active() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "seed         : %u (deterministic)\n", s_seed );
	ri.Printf( PRINT_ALL, "generation   : %u\n", s_generation );
	ri.Printf( PRINT_ALL, "terrain_meta : %s\n", CBTerrain_HasMetadata() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "season/wet   : %d / %.2f\n",
		r_biomeSeason ? r_biomeSeason->integer : 0,
		r_biomeWetness ? r_biomeWetness->value : 0.0f );
	ri.Printf( PRINT_ALL, "types        : %d (", VK_BIOME_COUNT );
	{
		int i;
		for ( i = 0; i < VK_BIOME_COUNT; i++ ) {
			ri.Printf( PRINT_ALL, "%s%s", names[i], ( i + 1 < VK_BIOME_COUNT ) ? "," : "" );
		}
	}
	ri.Printf( PRINT_ALL, ")\n" );
	ri.Printf( PRINT_ALL, "============================================\n" );
}
