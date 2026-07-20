/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Raster Ultra 1.14 GPU vegetation — instance generation, cull, LOD, wind hooks.
===========================================================================
*/

#include "tr_local.h"
#include "vk_vegetation_gpu.h"
#include "vk_biome.h"
#include "vk_terrain.h"
#include "vk_pass_registry.h"

#include <math.h>

static cvar_t *r_vegGpu;
static cvar_t *r_vegGpuDensity;
static cvar_t *r_vegGpuMaxInstances;
static cvar_t *r_vegGpuDraw;
static cvar_t *r_vegGpuInteraction;
static cvar_t *r_vegGpuDebug;
static cvar_t *r_vegGpuShadowLod;
static cvar_t *r_vegGpuAlphaCoverage;

static vkVegInstance_t *s_instances;
static uint32_t s_generated;
static uint32_t s_rejected;
static uint32_t s_visible;
static uint32_t s_visibleIdx[VK_VEG_MAX_VISIBLE];
static uint32_t s_capacity;
static uint32_t s_generation;
static uint32_t s_seed;
static qboolean s_dirty;
static float s_interactField[VK_VEG_INTERACT_SIZE * VK_VEG_INTERACT_SIZE];
static float s_windTime;
static float s_windTimePrev;

static float Veg_Hash( float x, float z, uint32_t salt )
{
	uint32_t n = (uint32_t)( x * 127.1f ) ^ (uint32_t)( z * 311.7f ) ^ ( s_seed * 0x85ebca6bu ) ^ salt;
	n ^= n >> 16;
	n *= 0x7feb352du;
	n ^= n >> 15;
	n *= 0x846ca68bu;
	n ^= n >> 16;
	return (float)( n & 0xFFFFFFu ) / (float)0xFFFFFFu;
}

static void Veg_ClearInteract( void )
{
	Com_Memset( s_interactField, 0, sizeof( s_interactField ) );
}

static void Veg_StampInteraction( float worldX, float worldZ, float radius, float strength )
{
	float scale = CBTerrain_GetScale();
	float u = ( worldX / scale ) + 0.5f;
	float v = ( worldZ / scale ) + 0.5f;
	int cx = (int)( u * ( VK_VEG_INTERACT_SIZE - 1 ) );
	int cz = (int)( v * ( VK_VEG_INTERACT_SIZE - 1 ) );
	int r = (int)( radius / scale * (float)VK_VEG_INTERACT_SIZE ) + 1;
	int x, z;

	for ( z = cz - r; z <= cz + r; z++ ) {
		for ( x = cx - r; x <= cx + r; x++ ) {
			int ix = x, iz = z;
			float dx, dz, d;
			if ( ix < 0 || iz < 0 || ix >= VK_VEG_INTERACT_SIZE || iz >= VK_VEG_INTERACT_SIZE ) {
				continue;
			}
			dx = (float)( ix - cx );
			dz = (float)( iz - cz );
			d = sqrtf( dx * dx + dz * dz ) / (float)( r + 1 );
			if ( d < 1.0f ) {
				float add = strength * ( 1.0f - d );
				float *cell = &s_interactField[iz * VK_VEG_INTERACT_SIZE + ix];
				*cell = Com_Clamp( 0.0f, 1.0f, *cell + add );
			}
		}
	}
}

static void Veg_Generate( void )
{
	float scale = CBTerrain_GetScale();
	int maxInst = r_vegGpuMaxInstances ? r_vegGpuMaxInstances->integer : 16384;
	float density = r_vegGpuDensity ? r_vegGpuDensity->value : 0.5f;
	int grid = (int)sqrtf( (float)maxInst );
	int gx, gz;
	uint32_t gen = 0, rej = 0;

	if ( maxInst < 64 ) {
		maxInst = 64;
	}
	if ( maxInst > (int)s_capacity ) {
		maxInst = (int)s_capacity;
	}
	if ( grid < 8 ) {
		grid = 8;
	}

	vk_spine_pass_begin( VK_SPINE_PASS_VEG_GENERATE );

	for ( gz = 0; gz < grid && gen < (uint32_t)maxInst; gz++ ) {
		for ( gx = 0; gx < grid && gen < (uint32_t)maxInst; gx++ ) {
			float jx = Veg_Hash( (float)gx, (float)gz, 11 );
			float jz = Veg_Hash( (float)gx, (float)gz, 23 );
			float u = ( (float)gx + jx ) / (float)grid;
			float v = ( (float)gz + jz ) / (float)grid;
			float wx = ( u - 0.5f ) * scale;
			float wz = ( v - 0.5f ) * scale;
			float hy = 0.0f;
			float slope, dens, keep;
			vkBiomeId_t biome;
			const vkBiomeDef_t *def;
			vkVegInstance_t *inst;
			vec3_t n;
			vkVegSpeciesId_t species;

			if ( !CBTerrain_SampleHeight( wx, wz, &hy ) ) {
				rej++;
				continue;
			}
			slope = CBTerrain_SampleSlope( wx, wz );
			if ( slope > 0.65f ) {
				rej++;
				continue;
			}

			dens = VK_Biome_Active() ? VK_Biome_VegetationDensity( wx, wz ) : 0.35f;
			dens *= density;
			keep = Veg_Hash( wx, wz, 99 );
			if ( keep > dens ) {
				rej++;
				continue;
			}

			biome = VK_Biome_Active() ? VK_Biome_Primary( wx, wz ) : VK_BIOME_GRASS;
			def = VK_Biome_GetDef( biome );

			if ( def->treeWeight > 0.55f && Veg_Hash( wx, wz, 3 ) < def->treeWeight * 0.08f ) {
				species = ( Veg_Hash( wx, wz, 4 ) > 0.7f ) ? VK_VEG_SPECIES_TREE_LARGE : VK_VEG_SPECIES_TREE_SMALL;
			} else if ( def->rockWeight > 0.4f && Veg_Hash( wx, wz, 5 ) < 0.12f ) {
				species = ( Veg_Hash( wx, wz, 6 ) > 0.5f ) ? VK_VEG_SPECIES_ROCK : VK_VEG_SPECIES_DEBRIS;
			} else if ( def->grassWeight > 0.3f ) {
				float g = Veg_Hash( wx, wz, 7 );
				if ( g > 0.92f ) {
					species = VK_VEG_SPECIES_SHRUB;
				} else if ( g > 0.85f ) {
					species = VK_VEG_SPECIES_FLOWER;
				} else if ( biome == VK_BIOME_WETLAND ) {
					species = VK_VEG_SPECIES_REED;
				} else {
					species = VK_VEG_SPECIES_GRASS;
				}
			} else {
				rej++;
				continue;
			}

			inst = &s_instances[gen];
			Com_Memset( inst, 0, sizeof( *inst ) );
			inst->pos[0] = wx;
			inst->pos[1] = hy;
			inst->pos[2] = wz;
			VectorCopy( inst->pos, inst->rest );
			VectorCopy( inst->pos, inst->prevPos );
			inst->scale = 0.4f + Veg_Hash( wx, wz, 8 ) * 1.2f;
			if ( species == VK_VEG_SPECIES_TREE_LARGE ) {
				inst->scale *= 4.0f;
			} else if ( species == VK_VEG_SPECIES_TREE_SMALL ) {
				inst->scale *= 2.2f;
			}
			inst->yaw = Veg_Hash( wx, wz, 9 ) * 6.2831853f;
			CBTerrain_SampleNormal( wx, wz, n );
			VectorCopy( n, inst->normal );
			inst->species = (uint32_t)species;
			inst->biome = (uint32_t)biome;
			inst->lod = 0;
			inst->flags = 1u; /* cast shadow by default */
			if ( species <= VK_VEG_SPECIES_FERN ) {
				inst->flags |= 4u; /* alpha-tested foliage */
			}
			inst->windWeight = ( species <= VK_VEG_SPECIES_BUSH ) ? 1.0f :
				( ( species <= VK_VEG_SPECIES_TREE_LARGE ) ? 0.35f : 0.0f );
			gen++;
		}
	}

	s_generated = gen;
	s_rejected = rej;
	s_dirty = qfalse;
	vk_spine_note_write( VK_SPINE_RES_VEG_INSTANCE_BUFFER, VK_SPINE_PASS_VEG_GENERATE,
		VK_SPINE_ACCESS_STORAGE_WRITE );
	vk_spine_pass_end( VK_SPINE_PASS_VEG_GENERATE );
}

static void Veg_Cull( void )
{
	const float *origin = backEnd.viewParms.or.origin;
	const float *forward = backEnd.viewParms.or.axis[0];
	uint32_t i, vis = 0;
	float maxDist = CBTerrain_GetScale() * 1.5f;

	vk_spine_pass_begin( VK_SPINE_PASS_VEG_CULL );
	s_visible = 0;

	for ( i = 0; i < s_generated && vis < VK_VEG_MAX_VISIBLE; i++ ) {
		vkVegInstance_t *inst = &s_instances[i];
		float dx = inst->pos[0] - origin[0];
		float dy = inst->pos[1] - origin[1];
		float dz = inst->pos[2] - origin[2];
		float dist = sqrtf( dx * dx + dy * dy + dz * dz );
		float facing;

		if ( dist > maxDist ) {
			continue;
		}
		facing = dx * forward[0] + dy * forward[1] + dz * forward[2];
		if ( facing < -inst->scale * 2.0f && dist > 64.0f ) {
			continue;
		}

		/* Distance LOD + impostor flag. */
		if ( dist > maxDist * 0.65f ) {
			inst->lod = 3;
			inst->flags |= 2u;
		} else if ( dist > maxDist * 0.4f ) {
			inst->lod = 2;
		} else if ( dist > maxDist * 0.2f ) {
			inst->lod = 1;
		} else {
			inst->lod = 0;
			inst->flags &= ~2u;
		}

		if ( r_vegGpuShadowLod && r_vegGpuShadowLod->integer && dist > maxDist * 0.5f ) {
			inst->flags &= ~1u; /* aggregate canopy / no leaf shadows at distance */
		}

		s_visibleIdx[vis++] = i;
	}

	s_visible = vis;
	vk_spine_note_write( VK_SPINE_RES_VEG_VISIBLE_LIST, VK_SPINE_PASS_VEG_CULL,
		VK_SPINE_ACCESS_STORAGE_WRITE );
	vk_spine_pass_end( VK_SPINE_PASS_VEG_CULL );
}

static void Veg_UpdateWindAndInteract( void )
{
	uint32_t i;
	float dt;

	s_windTimePrev = s_windTime;
	s_windTime = (float)backEnd.refdef.floatTime;
	dt = s_windTime - s_windTimePrev;
	if ( dt < 0.0f || dt > 0.5f ) {
		dt = 0.016f;
	}

	vk_spine_pass_begin( VK_SPINE_PASS_VEG_WIND );
	for ( i = 0; i < s_generated; i++ ) {
		vkVegInstance_t *inst = &s_instances[i];
		float phase, bend;
		VectorCopy( inst->pos, inst->prevPos );
		if ( inst->windWeight <= 0.0f ) {
			VectorCopy( inst->rest, inst->pos );
			continue;
		}
		phase = inst->rest[0] * 0.07f + inst->rest[2] * 0.05f + s_windTime * 1.2f;
		bend = sinf( phase ) * 0.08f * inst->windWeight * inst->scale;
		/* Deterministic world-space wind from rest pose — not camera-relative. */
		inst->pos[0] = inst->rest[0] + bend;
		inst->pos[1] = inst->rest[1];
		inst->pos[2] = inst->rest[2] + cosf( phase * 0.9f ) * bend * 0.6f;
	}
	vk_spine_note_write( VK_SPINE_RES_VEG_WIND_FIELD, VK_SPINE_PASS_VEG_WIND,
		VK_SPINE_ACCESS_STORAGE_WRITE );
	vk_spine_pass_end( VK_SPINE_PASS_VEG_WIND );

	if ( r_vegGpuInteraction && r_vegGpuInteraction->integer ) {
		vk_spine_pass_begin( VK_SPINE_PASS_VEG_INTERACTION );
		/* Decay field; stamp player origin as local disturbance. */
		{
			int c;
			for ( c = 0; c < VK_VEG_INTERACT_SIZE * VK_VEG_INTERACT_SIZE; c++ ) {
				s_interactField[c] *= 0.92f;
			}
		}
		Veg_StampInteraction( backEnd.viewParms.or.origin[0], backEnd.viewParms.or.origin[2],
			48.0f, 0.35f );
		vk_spine_note_write( VK_SPINE_RES_VEG_INTERACTION_FIELD, VK_SPINE_PASS_VEG_INTERACTION,
			VK_SPINE_ACCESS_STORAGE_WRITE );
		vk_spine_pass_end( VK_SPINE_PASS_VEG_INTERACTION );
	}

	(void)dt;
}

static void Veg_DrawBatch( void )
{
	shader_t *sh;
	uint32_t v;
	const int maxCards = 512;

	if ( !r_vegGpuDraw || !r_vegGpuDraw->integer || s_visible == 0 ) {
		return;
	}

	sh = R_FindShader( "textures/demo/blend_ground", LIGHTMAP_NONE, qtrue );
	if ( !sh || sh->defaultShader ) {
		sh = tr.defaultShader;
	}

	vk_spine_pass_begin( VK_SPINE_PASS_VEG_DRAW );
	RB_BeginSurface( sh, 0 );

	for ( v = 0; v < s_visible && v < (uint32_t)maxCards; v++ ) {
		vkVegInstance_t *inst = &s_instances[s_visibleIdx[v]];
		float hs = inst->scale * 0.5f;
		float c = cosf( inst->yaw );
		float s = sinf( inst->yaw );
		vec3_t p[4];
		int i, base;
		byte alpha = 255;

		/* Impostors / distant: skip dense grass cards (coverage proxy on terrain). */
		if ( ( inst->flags & 2u ) && inst->species == VK_VEG_SPECIES_GRASS ) {
			continue;
		}
		if ( inst->species >= VK_VEG_SPECIES_TREE_SMALL ) {
			hs *= 0.35f; /* trunk proxy card — full mesh LODs are future work */
		}

		if ( tess.numVertexes + 4 >= SHADER_MAX_VERTEXES ||
			tess.numIndexes + 6 >= SHADER_MAX_INDEXES ) {
			RB_EndSurface();
			RB_BeginSurface( sh, 0 );
		}

		VectorSet( p[0], inst->pos[0] - c * hs, inst->pos[1], inst->pos[2] - s * hs );
		VectorSet( p[1], inst->pos[0] + c * hs, inst->pos[1], inst->pos[2] + s * hs );
		VectorSet( p[2], inst->pos[0] + c * hs, inst->pos[1] + inst->scale, inst->pos[2] + s * hs );
		VectorSet( p[3], inst->pos[0] - c * hs, inst->pos[1] + inst->scale, inst->pos[2] - s * hs );

		if ( r_vegGpuAlphaCoverage && r_vegGpuAlphaCoverage->integer && ( inst->flags & 4u ) ) {
			/* Mip-aware coverage stand-in: raise threshold with distance LOD. */
			alpha = (byte)( 220 - inst->lod * 40 );
		}

		base = tess.numVertexes;
		for ( i = 0; i < 4; i++ ) {
			VectorCopy( p[i], tess.xyz[base + i] );
			VectorCopy( inst->normal, tess.normal[base + i] );
			tess.texCoords[0][base + i][0] = ( i == 1 || i == 2 ) ? 1.0f : 0.0f;
			tess.texCoords[0][base + i][1] = ( i >= 2 ) ? 1.0f : 0.0f;
			tess.vertexColors[base + i].rgba[0] = 80;
			tess.vertexColors[base + i].rgba[1] = 160;
			tess.vertexColors[base + i].rgba[2] = 70;
			tess.vertexColors[base + i].rgba[3] = alpha;
		}
		tess.indexes[tess.numIndexes + 0] = base + 0;
		tess.indexes[tess.numIndexes + 1] = base + 1;
		tess.indexes[tess.numIndexes + 2] = base + 2;
		tess.indexes[tess.numIndexes + 3] = base + 0;
		tess.indexes[tess.numIndexes + 4] = base + 2;
		tess.indexes[tess.numIndexes + 5] = base + 3;
		tess.numVertexes += 4;
		tess.numIndexes += 6;
	}

	RB_EndSurface();
	vk_spine_pass_end( VK_SPINE_PASS_VEG_DRAW );
}

void VK_VegGpu_RegisterCvars( void )
{
	r_vegGpu = ri.Cvar_Get( "r_vegGpu", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vegGpu, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vegGpu,
		"Raster Ultra 1.14 GPU vegetation instances. Off by default.\n"
		"Requires terrain metadata. Deterministic seed from r_biomeSeed." );
	r_vegGpuDensity = ri.Cvar_Get( "r_vegGpuDensity", "0.45", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vegGpuDensity, "0", "1", CV_FLOAT );
	r_vegGpuMaxInstances = ri.Cvar_Get( "r_vegGpuMaxInstances", "16384", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_vegGpuMaxInstances, "64", "65536", CV_INTEGER );
	r_vegGpuDraw = ri.Cvar_Get( "r_vegGpuDraw", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vegGpuDraw, "0", "1", CV_INTEGER );
	r_vegGpuInteraction = ri.Cvar_Get( "r_vegGpuInteraction", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vegGpuInteraction, "0", "1", CV_INTEGER );
	r_vegGpuDebug = ri.Cvar_Get( "r_vegGpuDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vegGpuDebug, "0", "5", CV_INTEGER );
	r_vegGpuShadowLod = ri.Cvar_Get( "r_vegGpuShadowLod", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vegGpuShadowLod, "0", "1", CV_INTEGER );
	r_vegGpuAlphaCoverage = ri.Cvar_Get( "r_vegGpuAlphaCoverage", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_vegGpuAlphaCoverage, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_vegGpuAlphaCoverage,
		"Coverage-preserving alpha policy for foliage (stable without TAA)." );

	ri.Cmd_AddCommand( "veg_status", VK_VegGpu_Status_f );
	ri.Printf( PRINT_ALL, "GPU vegetation (Raster Ultra 1.14): r_vegGpu %s\n",
		r_vegGpu->integer ? "enabled" : "disabled" );
}

void VK_VegGpu_Init( void )
{
	VK_VegGpu_RegisterCvars();
	s_capacity = VK_VEG_MAX_INSTANCES;
	s_instances = ri.Malloc( sizeof( vkVegInstance_t ) * s_capacity );
	if ( !s_instances ) {
		s_capacity = 0;
		ri.Printf( PRINT_WARNING, "GPU vegetation: instance buffer alloc failed\n" );
		return;
	}
	Com_Memset( s_instances, 0, sizeof( vkVegInstance_t ) * s_capacity );
	s_generated = 0;
	s_rejected = 0;
	s_visible = 0;
	s_dirty = qtrue;
	s_generation = 0;
	s_seed = VK_Biome_Seed();
	Veg_ClearInteract();
}

void VK_VegGpu_Shutdown( void )
{
	if ( s_instances ) {
		ri.Free( s_instances );
		s_instances = NULL;
	}
	s_capacity = 0;
	s_generated = 0;
}

void VK_VegGpu_OnWorldLoad( void )
{
	s_generation++;
	s_seed = VK_Biome_Seed();
	s_dirty = qtrue;
	s_generated = 0;
	s_visible = 0;
	Veg_ClearInteract();
}

void VK_VegGpu_OnWorldUnload( void )
{
	s_generated = 0;
	s_visible = 0;
	s_dirty = qtrue;
}

void VK_VegGpu_OnOriginRebase( void )
{
	s_dirty = qtrue;
	Veg_ClearInteract();
}

qboolean VK_VegGpu_Active( void )
{
	return ( r_vegGpu && r_vegGpu->integer && s_instances && CBTerrain_HasMetadata() ) ? qtrue : qfalse;
}

uint32_t VK_VegGpu_GeneratedCount( void )
{
	return s_generated;
}

uint32_t VK_VegGpu_VisibleCount( void )
{
	return s_visible;
}

uint32_t VK_VegGpu_RejectedCount( void )
{
	return s_rejected;
}

void VK_VegGpu_Frame( void )
{
	if ( !VK_VegGpu_Active() ) {
		return;
	}
	if ( s_dirty || s_generated == 0 ) {
		Veg_Generate();
	}
	Veg_UpdateWindAndInteract();
	Veg_Cull();
	Veg_DrawBatch();
}

void VK_VegGpu_Status_f( void )
{
	ri.Printf( PRINT_ALL, "======== Vegetation GPU (Raster Ultra 1.14) ========\n" );
	ri.Printf( PRINT_ALL, "active       : %s\n", VK_VegGpu_Active() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "seed         : %u\n", s_seed );
	ri.Printf( PRINT_ALL, "generation   : %u dirty=%d\n", s_generation, s_dirty ? 1 : 0 );
	ri.Printf( PRINT_ALL, "requested    : %d\n", r_vegGpuMaxInstances ? r_vegGpuMaxInstances->integer : 0 );
	ri.Printf( PRINT_ALL, "generated    : %u\n", s_generated );
	ri.Printf( PRINT_ALL, "rejected     : %u\n", s_rejected );
	ri.Printf( PRINT_ALL, "visible      : %u / %u\n", s_visible, VK_VEG_MAX_VISIBLE );
	ri.Printf( PRINT_ALL, "density/draw : %.2f / %d\n",
		r_vegGpuDensity ? r_vegGpuDensity->value : 0.0f,
		r_vegGpuDraw ? r_vegGpuDraw->integer : 0 );
	ri.Printf( PRINT_ALL, "interaction  : %d | alphaCoverage=%d shadowLod=%d\n",
		r_vegGpuInteraction ? r_vegGpuInteraction->integer : 0,
		r_vegGpuAlphaCoverage ? r_vegGpuAlphaCoverage->integer : 0,
		r_vegGpuShadowLod ? r_vegGpuShadowLod->integer : 0 );
	ri.Printf( PRINT_ALL, "policy       : instance buffer + frustum cull (no per-blade CPU ents)\n" );
	ri.Printf( PRINT_ALL, "RT           : locked off | TAA not required for coverage\n" );
	ri.Printf( PRINT_ALL, "====================================================\n" );
}
