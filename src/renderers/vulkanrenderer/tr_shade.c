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
// tr_shade.c

#include <stdint.h>
#include "tr_local.h"
#include "glints.h"

extern cvar_t *r_shLighting;
extern cvar_t *r_shWorldLighting;
extern cvar_t *r_shDebugView;

static void R_EvalSH9Color( const vec3_t shCoeffs[SH_COEFF_COUNT], const vec3_t normal, vec3_t out ) {
	const float x = normal[0];
	const float y = normal[1];
	const float z = normal[2];
	const float basis[SH_COEFF_COUNT] = {
		1.0f,
		y,
		z,
		x,
		x * y,
		y * z,
		3.0f * z * z - 1.0f,
		x * z,
		x * x - y * y
	};
	int i;

	VectorClear( out );
	for ( i = 0; i < SH_COEFF_COUNT; i++ ) {
		out[0] += shCoeffs[i][0] * basis[i];
		out[1] += shCoeffs[i][1] * basis[i];
		out[2] += shCoeffs[i][2] * basis[i];
	}
}

static qboolean R_StageHasLightmap( const shaderStage_t *pStage ) {
	return ( pStage->bundle[0].lightmap != LIGHTMAP_INDEX_NONE || pStage->bundle[1].lightmap != LIGHTMAP_INDEX_NONE );
}

static const textureBundle_t *R_FindLightmapBundle( const shader_t *shader ) {
	int i;

	if ( !shader ) {
		return NULL;
	}

	for ( i = 0; i < shader->numUnfoggedPasses && i < MAX_SHADER_STAGES; i++ ) {
		const shaderStage_t *stage = shader->stages[ i ];
		if ( !stage ) {
			break;
		}
		if ( stage->bundle[0].lightmap != LIGHTMAP_INDEX_NONE ) {
			return &stage->bundle[0];
		}
		if ( stage->bundle[1].lightmap != LIGHTMAP_INDEX_NONE ) {
			return &stage->bundle[1];
		}
	}

	return NULL;
}

#define GLINT_LOG_MAX_ENTRIES	256
#define GLINT_LOG_MAX_STAGE_LIST	64
#define GLINT_LOG_MAX_FLAGS	16
#define GLINT_DESCRIPTOR_MAP_SIZE	64
#define GLINT_DICT_MAP_SIZE		16

typedef struct {
	const char *stageName;
	uint32_t flags;
	uint32_t brdfId;
} glintStageKey_t;

typedef struct {
	uint32_t flags;
	uint32_t count;
} glintFlagCounter_t;

typedef struct {
	VkDescriptorSet descriptor;
	uint32_t id;
} glintDescriptorEntry_t;

typedef struct {
	VkImageView view;
	uint32_t id;
} glintDictEntry_t;

typedef struct {
	glintStageKey_t seen[GLINT_LOG_MAX_ENTRIES];
	int seenCount;
	uint32_t totalAttempt;
	uint32_t duplicates;
	uint32_t uniqueStages;
	glintFlagCounter_t flagCounters[GLINT_LOG_MAX_FLAGS];
	int flagCounterCount;
	const char *stageNames[GLINT_LOG_MAX_STAGE_LIST];
	int stageNameCount;
	uint32_t uniqueBrdfIds[GLINT_LOG_MAX_ENTRIES];
	int uniqueBrdfCount;
} glintLogContext_t;

static glintDescriptorEntry_t glintDescriptorMap[GLINT_DESCRIPTOR_MAP_SIZE];
static int glintDescriptorMapCount;
static uint32_t glintDescriptorNextId = 1;
static glintDictEntry_t glintDictMap[GLINT_DICT_MAP_SIZE];
static int glintDictMapCount;
static uint32_t glintDictNextId = 1;

static const struct {
	uint32_t mask;
	const char *name;
} glintFlagNames[] = {
	{ PBR_HAS_NORMALMAP,		"NORMALMAP" },
	{ PBR_HAS_PHYSICALMAP,		"PHYSICALMAP" },
	{ PBR_HAS_SPECULARMAP,		"SPECULARMAP" },
	{ PBR_HAS_LIGHTMAP,			"LIGHTMAP" },
	{ PBR_HAS_EMISSIVE,			"EMISSIVE" },
	{ PBR_HAS_CLEARCOAT,		"CLEARCOAT" },
	{ PBR_HAS_SHEEN,			"SHEEN" },
	{ PBR_HAS_ANISOTROPY,		"ANISOTROPY" },
	{ PBR_HAS_TRANSMISSION,		"TRANSMISSION" },
	{ PBR_HAS_SUBSURFACE,		"SUBSURFACE" },
	{ PBR_HAS_IRRADIANCE,		"IRRADIANCE" },
};

static qboolean pbrDebugPermutationLogged = qfalse;
static int pbrDebugLastMode = 0;
static uint32_t pbrDebugLastFlags = 0;
qboolean tr_pbr_bindLogPrinted = qfalse;


static uint32_t glint_descriptor_id( VkDescriptorSet descriptor )
{
	if ( descriptor == VK_NULL_HANDLE ) {
		return 0;
	}
	for ( int i = 0; i < glintDescriptorMapCount; i++ ) {
		if ( glintDescriptorMap[i].descriptor == descriptor ) {
			return glintDescriptorMap[i].id;
		}
	}
	if ( glintDescriptorMapCount >= GLINT_DESCRIPTOR_MAP_SIZE ) {
		return 0;
	}
	glintDescriptorMap[glintDescriptorMapCount].descriptor = descriptor;
	glintDescriptorMap[glintDescriptorMapCount].id = glintDescriptorNextId;
	glintDescriptorMapCount++;
	return glintDescriptorNextId++;
}

static uint32_t glint_dict_id( VkImageView view )
{
	if ( view == VK_NULL_HANDLE ) {
		return 0;
	}
	for ( int i = 0; i < glintDictMapCount; i++ ) {
		if ( glintDictMap[i].view == view ) {
			return glintDictMap[i].id;
		}
	}
	if ( glintDictMapCount >= GLINT_DICT_MAP_SIZE ) {
		return 0;
	}
	glintDictMap[glintDictMapCount].view = view;
	glintDictMap[glintDictMapCount].id = glintDictNextId;
	glintDictMapCount++;
	return glintDictNextId++;
}

static void glint_flags_to_string( uint32_t flags, char *buf, size_t bufSize )
{
	buf[0] = '\0';
	int added = 0;
	for ( size_t i = 0; i < ARRAY_LEN( glintFlagNames ); i++ ) {
		if ( ( flags & glintFlagNames[i].mask ) != 0 ) {
			if ( added > 0 ) {
				Q_strcat( buf, bufSize, "|" );
			}
			Q_strcat( buf, bufSize, glintFlagNames[i].name );
			added++;
		}
	}
	if ( added == 0 ) {
		Q_strncpyz( buf, "NONE", bufSize );
	}
}

static void glint_log_context_reset( glintLogContext_t *ctx )
{
	ctx->seenCount = 0;
	ctx->totalAttempt = 0;
	ctx->duplicates = 0;
	ctx->uniqueStages = 0;
	ctx->flagCounterCount = 0;
	ctx->stageNameCount = 0;
	ctx->uniqueBrdfCount = 0;
}

static qboolean glint_stage_key_found( glintLogContext_t *ctx, const char *stageName, uint32_t flags, uint32_t brdfId )
{
	for ( int i = 0; i < ctx->seenCount; i++ ) {
		if ( ctx->seen[i].flags == flags && ctx->seen[i].brdfId == brdfId &&
			!Q_stricmp( ctx->seen[i].stageName, stageName ) ) {
			return qtrue;
		}
	}
	return qfalse;
}

static void glint_stage_key_add( glintLogContext_t *ctx, const char *stageName, uint32_t flags, uint32_t brdfId )
{
	if ( ctx->seenCount >= GLINT_LOG_MAX_ENTRIES ) {
		return;
	}
	ctx->seen[ctx->seenCount].stageName = stageName;
	ctx->seen[ctx->seenCount].flags = flags;
	ctx->seen[ctx->seenCount].brdfId = brdfId;
	ctx->seenCount++;
}

static void glint_flag_counter_add( glintLogContext_t *ctx, uint32_t flags )
{
	for ( int i = 0; i < ctx->flagCounterCount; i++ ) {
		if ( ctx->flagCounters[i].flags == flags ) {
			ctx->flagCounters[i].count++;
			return;
		}
	}
	if ( ctx->flagCounterCount >= GLINT_LOG_MAX_FLAGS ) {
		return;
	}
	ctx->flagCounters[ctx->flagCounterCount].flags = flags;
	ctx->flagCounters[ctx->flagCounterCount].count = 1;
	ctx->flagCounterCount++;
}

static void glint_unique_brdf_add( glintLogContext_t *ctx, uint32_t brdfId )
{
	if ( brdfId == 0 ) {
		return;
	}
	for ( int i = 0; i < ctx->uniqueBrdfCount; i++ ) {
		if ( ctx->uniqueBrdfIds[i] == brdfId ) {
			return;
		}
	}
	if ( ctx->uniqueBrdfCount >= GLINT_LOG_MAX_ENTRIES ) {
		return;
	}
	ctx->uniqueBrdfIds[ctx->uniqueBrdfCount++] = brdfId;
}

static void glint_stage_name_add( glintLogContext_t *ctx, const char *stageName )
{
	for ( int i = 0; i < ctx->stageNameCount; i++ ) {
		if ( !Q_stricmp( ctx->stageNames[i], stageName ) ) {
			return;
		}
	}
	if ( ctx->stageNameCount >= GLINT_LOG_MAX_STAGE_LIST ) {
		return;
	}
	ctx->stageNames[ctx->stageNameCount++] = stageName;
}

static void glint_log_summary( glintLogContext_t *ctx, int logLevel )
{
	if ( ctx->totalAttempt == 0 && ctx->uniqueStages == 0 ) {
		return;
	}
	ri.Printf( PRINT_ALL, "[glints] scanned=%u unique=%u duplicates=%u uniqueBrdf=%u\n",
		ctx->totalAttempt, ctx->uniqueStages, ctx->duplicates, ctx->uniqueBrdfCount );
	if ( ctx->flagCounterCount > 0 ) {
		char flagBuf[64];
		ri.Printf( PRINT_ALL, "[glints] flags:" );
		for ( int i = 0; i < ctx->flagCounterCount; i++ ) {
			glint_flags_to_string( ctx->flagCounters[i].flags, flagBuf, sizeof( flagBuf ) );
			ri.Printf( PRINT_ALL, " 0x%x[%s]=%u", ctx->flagCounters[i].flags,
				flagBuf, ctx->flagCounters[i].count );
		}
		ri.Printf( PRINT_ALL, "\n" );
	}
	if ( logLevel >= 2 && ctx->stageNameCount > 0 ) {
		ri.Printf( PRINT_ALL, "[glints] stage list:\n" );
		for ( int i = 0; i < ctx->stageNameCount; i++ ) {
			ri.Printf( PRINT_ALL, "  %s\n", ctx->stageNames[i] );
		}
	}
}

static void glint_log_stage_line( glintLogContext_t *ctx, const char *stageName, const char *shaderName,
	int32_t flags, uint32_t brdfId, uint32_t dictId, VkDescriptorSet descriptor, VkImageView dictView,
	int logLevel )
{
	ctx->totalAttempt++;
	if ( glint_stage_key_found( ctx, stageName, flags, brdfId ) ) {
		ctx->duplicates++;
		return;
	}
	glint_stage_key_add( ctx, stageName, flags, brdfId );
	ctx->uniqueStages++;
	glint_flag_counter_add( ctx, flags );
	glint_unique_brdf_add( ctx, brdfId );
	glint_stage_name_add( ctx, stageName );

	char flagBuf[64];
	glint_flags_to_string( flags, flagBuf, sizeof( flagBuf ) );

	ri.Printf( PRINT_ALL, "[glints] enable stage=%-40s flags=%s brdf=%u dict=%u shader=%s",
		stageName, flagBuf, brdfId, dictId, shaderName );

	if ( logLevel >= 3 ) {
		ri.Printf( PRINT_ALL, " brdf_desc=%p dict_view=%p", (void *)(uintptr_t)descriptor, (void *)(uintptr_t)dictView );
	}
	ri.Printf( PRINT_ALL, "\n" );
}

static void RB_DrawWorldSHDebugOverride( void ) {
	if ( !r_shDebugView || r_shDebugView->integer != 3 ) {
		return;
	}

	if ( backEnd.currentEntity != &tr.worldEntity ) {
		return;
	}

	{
		int v;
		for ( v = 0; v < tess.numVertexes; v++ ) {
			vec3_t shCoeffs[SH_COEFF_COUNT];
			vec3_t shLight;
			qboolean hasSH = R_SampleLightGridSH( tr.world, tess.xyz[v], shCoeffs );

			if ( hasSH ) {
				R_EvalSH9Color( shCoeffs, tess.normal[v], shLight );
				tess.svars.colors[0][v].rgba[0] = (byte)Com_Clamp( 0.0f, 255.0f, shLight[0] );
				tess.svars.colors[0][v].rgba[1] = (byte)Com_Clamp( 0.0f, 255.0f, shLight[1] );
				tess.svars.colors[0][v].rgba[2] = (byte)Com_Clamp( 0.0f, 255.0f, shLight[2] );
			} else {
				tess.svars.colors[0][v].rgba[0] = 255;
				tess.svars.colors[0][v].rgba[1] = 0;
				tess.svars.colors[0][v].rgba[2] = 255;
			}
			tess.svars.colors[0][v].rgba[3] = 255;
		}
	}

	vk_bind_pipeline( vk.surface_debug_pipeline_solid );
	vk_bind_index();
	vk_bind_geometry( TESS_XYZ | TESS_RGBA0 );
	vk_draw_geometry( DEPTH_RANGE_NORMAL, qtrue );
}

/*

  THIS ENTIRE FILE IS BACK END

  This file deals with applying shaders to surface data in the tess struct.
*/


/*
==================
R_DrawElements
==================
*/
#ifndef USE_VULKAN
void R_DrawElements( int numIndexes, const glIndex_t *indexes ) {
	qglDrawElements( GL_TRIANGLES, numIndexes, GL_INDEX_TYPE, indexes );
}
#endif


/*
=============================================================

SURFACE SHADERS

=============================================================
*/

shaderCommands_t	tess;
#ifndef USE_VULKAN
static qboolean	setArraysOnce;
#endif

/*
=================
R_BindAnimatedImage
=================
*/
static void R_BindAnimatedImage( const textureBundle_t *bundle ) {
	int64_t index;
	double	v;

	if ( bundle->isVideoMap ) {
		ri.CIN_RunCinematic(bundle->videoMapHandle);
		ri.CIN_UploadCinematic(bundle->videoMapHandle);
		return;
	}

	if ( bundle->isScreenMap /*&& backEnd.viewParms.frameSceneNum == 1*/ ) {
		if ( !backEnd.screenMapDone )
			GL_Bind( tr.blackImage );
		else
			vk_update_descriptor( glState.currenttmu + VK_DESC_TEXTURE_BASE, vk.screenMap.color_descriptor );
		return;
	}

	if ( bundle->numImageAnimations <= 1 ) {
		GL_Bind( bundle->image[0] );
		return;
	}

	// it is necessary to do this messy calc to make sure animations line up
	// exactly with waveforms of the same frequency
	//v = tess.shaderTime * bundle->imageAnimationSpeed * FUNCTABLE_SIZE;
	//index = v;
	//index >>= FUNCTABLE_SIZE2;

	v = tess.shaderTime * bundle->imageAnimationSpeed; // fix for frameloss bug -EC-
	index = v;

	if ( index < 0 ) {
		index = 0;	// may happen with shader time offsets
	}
	index %= bundle->numImageAnimations;

	GL_Bind( bundle->image[ index ] );
}

static const image_t *R_GetAnimatedImage( const textureBundle_t *bundle ) {
	int64_t index;
	double v;

	if ( !bundle ) {
		return tr.whiteImage;
	}

	if ( bundle->isVideoMap ) {
		return tr.blackImage ? tr.blackImage : tr.whiteImage;
	}

	if ( bundle->isScreenMap ) {
		return tr.blackImage ? tr.blackImage : tr.whiteImage;
	}

	if ( bundle->numImageAnimations <= 1 ) {
		return bundle->image[0] ? bundle->image[0] : tr.whiteImage;
	}

	v = tess.shaderTime * bundle->imageAnimationSpeed;
	index = v;

	if ( index < 0 ) {
		index = 0;
	}
	index %= bundle->numImageAnimations;

	return bundle->image[index] ? bundle->image[index] : tr.whiteImage;
}


/*
================
DrawTris

Draws triangle outlines for debugging
================
*/
static void DrawTris( const shaderCommands_t *input ) {
#ifdef USE_VULKAN
	uint32_t pipeline;

	if ( r_showtris->integer == 1 && backEnd.drawConsole )
		return;

	if ( tess.numIndexes == 0 )
		return;

	if ( r_fastsky->integer && input->shader->isSky )
		return;

#ifdef USE_VBO
	if ( tess.vboIndex ) {
#ifdef USE_PMLIGHT
		if ( tess.dlightPass )
			pipeline = backEnd.viewParms.portalView == PV_MIRROR ? vk.tris_mirror_debug_red_pipeline : vk.tris_debug_red_pipeline;
		else
#endif
			pipeline = backEnd.viewParms.portalView == PV_MIRROR ? vk.tris_mirror_debug_green_pipeline : vk.tris_debug_green_pipeline;
	} else
#endif
	{
#ifdef USE_PMLIGHT
		if ( tess.dlightPass )
			pipeline = backEnd.viewParms.portalView == PV_MIRROR ? vk.tris_mirror_debug_red_pipeline : vk.tris_debug_red_pipeline;
		else
#endif
			pipeline = backEnd.viewParms.portalView == PV_MIRROR ? vk.tris_mirror_debug_pipeline : vk.tris_debug_pipeline;
	}

	vk_bind_pipeline( pipeline );
	vk_draw_geometry( DEPTH_RANGE_ZERO, qtrue );

#else
	if ( r_showtris->integer == 1 && backEnd.drawConsole )
		return;

	GL_ClientState( 0, CLS_NONE );
	qglDisable( GL_TEXTURE_2D );

	qglColor4f( 1, 1, 1, 1 );

	GL_State( GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE );
	qglDepthRange( 0, 0 );

	qglVertexPointer( 3, GL_FLOAT, sizeof( input->xyz[0] ), input->xyz );

	if ( qglLockArraysEXT ) {
		qglLockArraysEXT( 0, input->numVertexes );
	}

	R_DrawElements( input->numIndexes, input->indexes );

	if ( qglUnlockArraysEXT ) {
		qglUnlockArraysEXT();
	}

	qglEnable( GL_TEXTURE_2D );

	qglDepthRange( 0, 1 );
#endif
}


/*
================
DrawNormals

Draws vertex normals for debugging
================
*/
static void DrawNormals( const shaderCommands_t *input ) {
	int		i;
#if defined(USE_VULKAN)
	(void)input;
#endif
#ifdef USE_VULKAN
#ifdef USE_VBO
	if ( tess.vboIndex )
		return; // must be handled specially
#endif

	GL_Bind( tr.whiteImage );

	tess.numIndexes = 0;
	for ( i = 0; i < tess.numVertexes; i++ ) {
		VectorMA( tess.xyz[i], 2.0, tess.normal[i], tess.xyz[i + tess.numVertexes] );
		tess.indexes[  tess.numIndexes + 0 ] = i;
		tess.indexes[  tess.numIndexes + 1 ] = i + tess.numVertexes;
		tess.numIndexes += 2;
	}
	tess.numVertexes *= 2;
	Com_Memset( tess.svars.colors[0][0].rgba, tr.identityLightByte, tess.numVertexes * sizeof( color4ub_t ) );

	vk_bind_pipeline( vk.normals_debug_pipeline );
	vk_bind_index();
	vk_bind_geometry( TESS_XYZ | TESS_ST0 | TESS_RGBA0 );
	vk_draw_geometry( DEPTH_RANGE_ZERO, qtrue );
#else
	GL_ClientState( 0, CLS_NONE );

	qglDisable( GL_TEXTURE_2D );
	qglColor4f( 1, 1, 1, 1 );

	qglDepthRange( 0, 0 );	// never occluded

	GL_State( GLS_DEPTHMASK_TRUE );

	for ( i = tess.numVertexes-1; i >= 0; i-- ) {
		VectorMA( tess.xyz[i], 2.0, tess.normal[i], tess.xyz[i*2 + 1] );
		VectorCopy( tess.xyz[i], tess.xyz[i*2] );
	}

	qglVertexPointer( 3, GL_FLOAT, sizeof( tess.xyz[0] ), tess.xyz );

	if ( qglLockArraysEXT ) {
		qglLockArraysEXT( 0, tess.numVertexes * 2 );
	}

	qglDrawArrays( GL_LINES, 0, tess.numVertexes * 2 );

	if ( qglUnlockArraysEXT ) {
		qglUnlockArraysEXT();
	}

	qglEnable( GL_TEXTURE_2D );

	qglDepthRange( 0, 1 );
#endif
}


/*
==============
RB_BeginSurface

We must set some things up before beginning any tesselation,
because a surface may be forced to perform a RB_End due
to overflow.
==============
*/
void RB_BeginSurface( shader_t *shader, int fogNum ) {

	shader_t *state;

#ifdef USE_VBO
	if ( shader->isStaticShader && !shader->remappedShader ) {
		tess.allowVBO = qtrue;
	} else {
		tess.allowVBO = qfalse;
	}
#endif

	if ( backEnd.currentEntity == &tr.worldEntity &&
		( ( r_shWorldLighting && r_shWorldLighting->integer ) ||
		( r_shDebugView && r_shDebugView->integer ) ) ) {
		tess.allowVBO = qfalse;
	}

	if ( shader->remappedShader ) {
		state = shader->remappedShader;
	} else {
		state = shader;
	}

#ifdef USE_PMLIGHT
	if ( tess.fogNum != fogNum ) {
		tess.dlightUpdateParams = qtrue;
	}
#endif

#ifdef USE_TESS_NEEDS_NORMAL
#ifdef USE_PMLIGHT
	tess.needsNormal = state->needsNormal || tess.dlightPass || r_shownormals->integer ||
		( backEnd.currentEntity == &tr.worldEntity &&
		( ( r_shDebugView && r_shDebugView->integer ) ||
		( r_shWorldLighting && r_shWorldLighting->integer && r_shLighting && r_shLighting->integer ) ) );
#else
	tess.needsNormal = state->needsNormal || r_shownormals->integer ||
		( backEnd.currentEntity == &tr.worldEntity &&
		( ( r_shDebugView && r_shDebugView->integer ) ||
		( r_shWorldLighting && r_shWorldLighting->integer && r_shLighting && r_shLighting->integer ) ) );
#endif
#endif

#ifdef USE_TESS_NEEDS_ST2
	tess.needsST2 = state->needsST2;
#endif

	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.shader = state;
	tess.fogNum = fogNum;

#ifdef USE_LEGACY_DLIGHTS
	tess.dlightBits = 0;		// will be OR'd in by surface functions
#endif
	tess.xstages = state->stages;
	tess.numPasses = state->numUnfoggedPasses;

	tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;
	if ( tess.shader->clampTime && tess.shaderTime >= tess.shader->clampTime ) {
		tess.shaderTime = tess.shader->clampTime;
	}
}


/*
===================
DrawMultitextured

output = t0 * t1 or t0 + t1

t0 = most upstream according to spec
t1 = most downstream according to spec
===================
*/
#ifndef USE_VULKAN
static void DrawMultitextured( const shaderCommands_t *input, int stage ) {
	shaderStage_t *pStage;

	pStage = tess.xstages[ stage ];

	GL_State( pStage->stateBits );

	if ( !setArraysOnce ) {
		R_ComputeColors( 0, tess.svars.colors[0], pStage );
		R_ComputeTexCoords( 0, &pStage->bundle[0] );
		R_ComputeTexCoords( 1, &pStage->bundle[1] );
		GL_ClientState( 0, CLS_TEXCOORD_ARRAY | CLS_COLOR_ARRAY );

		qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoordPtr[0] );
		qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, input->svars.colors[0].rgba );

		GL_ClientState( 1, CLS_TEXCOORD_ARRAY );
		qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoordPtr[1] );
	}

	//
	// base
	//
	GL_SelectTexture( 0 );
	R_BindAnimatedImage( &pStage->bundle[0] );

	//
	// lightmap/secondary pass
	//
	GL_SelectTexture( 1 );
	qglEnable( GL_TEXTURE_2D );
	R_BindAnimatedImage( &pStage->bundle[1] );

	if ( r_lightmap->integer ) {
		GL_TexEnv( GL_REPLACE );
	} else {
		GL_TexEnv( pStage->mtEnv );
	}

	R_DrawElements( input->numIndexes, input->indexes );

	//
	// disable texturing on TEXTURE1, then select TEXTURE0
	//
	//GL_ClientState( 1, CLS_NONE );

	qglDisable( GL_TEXTURE_2D );
	GL_SelectTexture( 0 );
}
#endif


#ifdef USE_LEGACY_DLIGHTS
/*
===================
ProjectDlightTexture

Perform dynamic lighting with another rendering pass
===================
*/
#ifdef USE_VULKAN
static qboolean ProjectDlightTexture( void ) {
#else
static void ProjectDlightTexture( void ) {
#endif
	int		i, l;
	vec3_t	origin;
	float	*texCoords;
	byte	*colors;
	byte	clipBits[SHADER_MAX_VERTEXES];
#ifdef USE_VULKAN
	uint32_t pipeline;
	qboolean rebindIndex = qfalse;
#else
	float	texCoordsArray[SHADER_MAX_VERTEXES][2];
	byte	colorArray[SHADER_MAX_VERTEXES][4];
#endif
	glIndex_t hitIndexes[SHADER_MAX_INDEXES];
	int		numIndexes;
	float	scale;
	float	radius;
	float	modulate = 0.0f;
	const dlight_t *dl;

	if ( !backEnd.refdef.num_dlights ) {
#ifdef USE_VULKAN
		return rebindIndex;
#else
		return;
#endif
	}

	for ( l = 0 ; l < (int)backEnd.refdef.num_dlights ; l++ ) {

		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;	// this surface definitely doesn't have any of this light
		}

#ifdef USE_VULKAN
		texCoords = (float*)&tess.svars.texcoords[0][0];
		tess.svars.texcoordPtr[0] = tess.svars.texcoords[0];
		colors = tess.svars.colors[0][0].rgba;
#else
		texCoords = texCoordsArray[0];
		colors = colorArray[0];
#endif

		dl = &backEnd.refdef.dlights[l];
		VectorCopy( dl->transformed, origin );
		radius = dl->radius;
		scale = 1.0f / radius;

		for ( i = 0 ; i < tess.numVertexes ; i++, texCoords += 2, colors += 4 ) {
			int		clip = 0;
			vec3_t	dist;

			VectorSubtract( origin, tess.xyz[i], dist );

			backEnd.pc.c_dlightVertexes++;

			texCoords[0] = 0.5f + dist[0] * scale;
			texCoords[1] = 0.5f + dist[1] * scale;

			if ( !r_dlightBacks->integer &&
					// dist . tess.normal[i]
					( dist[0] * tess.normal[i][0] +
					dist[1] * tess.normal[i][1] +
					dist[2] * tess.normal[i][2] ) < 0.0f ) {
				clip = 63;
			} else {
				if ( texCoords[0] < 0.0f ) {
					clip |= 1;
				} else if ( texCoords[0] > 1.0f ) {
					clip |= 2;
				}
				if ( texCoords[1] < 0.0f ) {
					clip |= 4;
				} else if ( texCoords[1] > 1.0f ) {
					clip |= 8;
				}

				// modulate the strength based on the height and color
				if ( dist[2] > radius ) {
					clip |= 16;
					modulate = 0.0f;
				} else if ( dist[2] < -radius ) {
					clip |= 32;
					modulate = 0.0f;
				} else {
					//*((int*)&dist[2]) &= 0x7FFFFFFF;
					dist[2] = fabsf( dist[2] );
					if ( dist[2] < radius * 0.5f ) {
						modulate = 1.0 * 255.0;
					} else {
						modulate = 2.0f * (radius - dist[2]) * scale * 255.0;
					}
				}
			}
			clipBits[i] = clip;
			colors[0] = dl->color[0] * modulate;
			colors[1] = dl->color[1] * modulate;
			colors[2] = dl->color[2] * modulate;
			colors[3] = 255;
		}

		// build a list of triangles that need light
		numIndexes = 0;
		for ( i = 0 ; i < tess.numIndexes ; i += 3 ) {
			glIndex_t a, b, c;

			a = tess.indexes[i];
			b = tess.indexes[i+1];
			c = tess.indexes[i+2];
			if ( clipBits[a] & clipBits[b] & clipBits[c] ) {
				continue;	// not lighted
			}
			hitIndexes[numIndexes] = a;
			hitIndexes[numIndexes+1] = b;
			hitIndexes[numIndexes+2] = c;
			numIndexes += 3;
		}

		if ( numIndexes == 0 ) {
			continue;
		}

#ifndef USE_VULKAN
		GL_ClientState( 1, CLS_NONE );
		GL_ClientState( 0, CLS_TEXCOORD_ARRAY | CLS_COLOR_ARRAY );

		qglTexCoordPointer( 2, GL_FLOAT, 0, texCoordsArray[0] );
		qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, colorArray );
#endif

		GL_Bind( tr.dlightImage );

#ifdef USE_VULKAN
		if ( numIndexes != tess.numIndexes ) {
			// re-bind index buffer for later fog pass
			rebindIndex = qtrue;
		}
		pipeline = vk.dlight_pipelines[dl->additive > 0 ? 1 : 0][tess.shader->cullType][tess.shader->polygonOffset];
		vk_bind_pipeline( pipeline );
		vk_bind_index_ext( numIndexes, hitIndexes );
		vk_bind_geometry( TESS_RGBA0 | TESS_ST0 );
		vk_draw_geometry( DEPTH_RANGE_NORMAL, qtrue );
#else
		// include GLS_DEPTHFUNC_EQUAL so alpha tested surfaces don't add light
		// where they aren't rendered

		if ( dl->additive ) {
			GL_State( GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
		} else {
			GL_State( GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL );
		}

		R_DrawElements( numIndexes, hitIndexes );
#endif
		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}

#ifdef USE_VULKAN
	return rebindIndex;
#endif
}

#endif // USE_LEGACY_DLIGHTS

uint32_t vk_append_uniform( const void *uniform, size_t size, uint32_t min_offset );
uint32_t vk_push_uniform( const vkUniform_t *uniform );
static uint32_t vk_push_uniform_cached( const vkUniform_t *u );
void VK_SetFogParams( vkUniform_t *uniform, int *fogStage );
static vkUniform_t uniform;
static vkUniformCamera_t uniform_camera;

#ifdef USE_VK_PBR
typedef struct vkPbrUniformBlock_s {
	vec4_t emissiveScale;
	vec4_t clearcoatScale;
	vec4_t sheenScale;
	vec4_t anisotropyScale;
	vec4_t transmissionScale;
	vec4_t subsurfaceColor;
	vec4_t subsurfaceParams;
	vec4_t shCoeffs[9];
	vec4_t texIndex0;
	vec4_t texIndex1;
	vec4_t texIndex2;
	vec4_t featureFlags;
} vkPbrUniformBlock_t;
#endif

#ifdef USE_VULKAN
static inline void vk_update_descriptor_if_changed( int index, VkDescriptorSet descriptor )
{
	// vk_update_descriptor() already no-ops if the descriptor matches; this avoids the call entirely
	// in hot loops when nothing changes.
	if ( vk.cmd && vk.cmd->descriptor_set.current[index] != descriptor ) {
		vk_update_descriptor( index, descriptor );
	}
}
#endif

/*
===================
RB_FogPass

Blends a fog texture on top of everything else
===================
*/
#ifdef USE_VULKAN
static void RB_FogPass( qboolean rebindIndex ) {
	uint32_t pipeline = vk.fog_pipelines[tess.shader->fogPass - 1][tess.shader->cullType][tess.shader->polygonOffset];
#ifdef USE_FOG_ONLY
	int fog_stage;

	// fog parameters
	vk_bind_pipeline( pipeline );
	if ( rebindIndex ) {
		vk_bind_index();
	}
	VK_SetFogParams( &uniform, &fog_stage );
	vk_push_uniform( &uniform );
	vk_update_descriptor( VK_DESC_FOG_ONLY, tr.fogImage->descriptor );
	vk_draw_geometry( DEPTH_RANGE_NORMAL, qtrue );
#else
	const fog_t	*fog = tr.world->fogs + tess.fogNum;
	int	i;

	for ( i = 0; i < tess.numVertexes; i++ ) {
		tess.svars.colors[0][i] = fog->colorInt;
	}

	RB_CalcFogTexCoords( ( float * ) tess.svars.texcoords[0] );
	tess.svars.texcoordPtr[ 0 ] = tess.svars.texcoords[ 0 ];
	GL_Bind( tr.fogImage );

	vk_bind_pipeline( pipeline );
	if ( rebindIndex ) {
		vk_bind_index();
	}
	vk_bind_geometry( TESS_ST0 | TESS_RGBA0 );
	vk_draw_geometry( DEPTH_RANGE_NORMAL, qtrue );
#endif
#else
static void RB_FogPass( void ) {
	const fog_t	*fog;
	int			i;

	RB_CalcFogTexCoords( ( float * ) tess.svars.texcoords[0] );

	GL_ClientState( 1, CLS_NONE );
	GL_ClientState( 0, CLS_TEXCOORD_ARRAY | CLS_COLOR_ARRAY );

	qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.svars.colors[0].rgba );
	qglTexCoordPointer( 2, GL_FLOAT, 0, tess.svars.texcoords[0] );

	GL_SelectTexture( 0 );
	GL_Bind( tr.fogImage );

	if ( tess.shader->fogPass == FP_EQUAL ) {
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL );
	} else {
		GL_State( GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );
	}

	R_DrawElements( tess.numIndexes, tess.indexes );
#endif
}


/*
===============
R_ComputeColors
===============
*/
void R_ComputeColors( const int b, color4ub_t *dest, const shaderStage_t *pStage )
{
	int		i;

	if ( tess.numVertexes == 0 )
		return;

	if ( backEnd.currentEntity == &tr.worldEntity && R_StageHasLightmap( pStage ) &&
		( ( r_shDebugView && r_shDebugView->integer ) ||
		( r_shWorldLighting && r_shWorldLighting->integer && r_shLighting && r_shLighting->integer ) ) ) {
		int v;
		for ( v = 0; v < tess.numVertexes; v++ ) {
			vec3_t shCoeffs[SH_COEFF_COUNT];
			vec3_t shLight;
			qboolean hasSH = R_SampleLightGridSH( tr.world, tess.xyz[v], shCoeffs );

			if ( r_shDebugView && r_shDebugView->integer ) {
				if ( hasSH ) {
					if ( r_shDebugView->integer == 2 ) {
						float value = shCoeffs[0][0];
						if ( value < 0.0f ) {
							value = 0.0f;
						} else if ( value > 255.0f ) {
							value = 255.0f;
						}
						dest[v].rgba[0] = myftol( value );
						dest[v].rgba[1] = dest[v].rgba[0];
						dest[v].rgba[2] = dest[v].rgba[0];
					} else {
						R_EvalSH9Color( shCoeffs, tess.normal[v], shLight );
						dest[v].rgba[0] = (byte)Com_Clamp( 0.0f, 255.0f, shLight[0] );
						dest[v].rgba[1] = (byte)Com_Clamp( 0.0f, 255.0f, shLight[1] );
						dest[v].rgba[2] = (byte)Com_Clamp( 0.0f, 255.0f, shLight[2] );
					}
				} else {
					dest[v].rgba[0] = 255;
					dest[v].rgba[1] = 0;
					dest[v].rgba[2] = 255;
				}
				dest[v].rgba[3] = 255;
			} else if ( hasSH ) {
				R_EvalSH9Color( shCoeffs, tess.normal[v], shLight );
				dest[v].rgba[0] = (byte)Com_Clamp( 0.0f, 255.0f, shLight[0] );
				dest[v].rgba[1] = (byte)Com_Clamp( 0.0f, 255.0f, shLight[1] );
				dest[v].rgba[2] = (byte)Com_Clamp( 0.0f, 255.0f, shLight[2] );
				dest[v].rgba[3] = 255;
			} else {
				dest[v].rgba[0] = 255;
				dest[v].rgba[1] = 255;
				dest[v].rgba[2] = 255;
				dest[v].rgba[3] = 255;
			}
		}
		return;
	}

	//
	// rgbGen
	//
	switch ( pStage->bundle[b].rgbGen )
	{
		case CGEN_IDENTITY:
			Com_Memset( dest, 0xff, tess.numVertexes * 4 );
			break;
		default:
		case CGEN_IDENTITY_LIGHTING:
			Com_Memset( dest, tr.identityLightByte, tess.numVertexes * 4 );
			break;
		case CGEN_LIGHTING_DIFFUSE:
			RB_CalcDiffuseColor( ( unsigned char * ) dest );
			break;
		case CGEN_EXACT_VERTEX:
			Com_Memcpy( dest, tess.vertexColors, tess.numVertexes * sizeof( tess.vertexColors[0] ) );
			break;
		case CGEN_CONST:
			for ( i = 0; i < tess.numVertexes; i++ ) {
				dest[i] = pStage->bundle[b].constantColor;
			}
			break;
		case CGEN_VERTEX:
			if ( tr.identityLight == 1 )
			{
				Com_Memcpy( dest, tess.vertexColors, tess.numVertexes * sizeof( tess.vertexColors[0] ) );
			}
			else
			{
				for ( i = 0; i < tess.numVertexes; i++ )
				{
					dest[i].rgba[0] = tess.vertexColors[i].rgba[0] * tr.identityLight;
					dest[i].rgba[1] = tess.vertexColors[i].rgba[1] * tr.identityLight;
					dest[i].rgba[2] = tess.vertexColors[i].rgba[2] * tr.identityLight;
					dest[i].rgba[3] = tess.vertexColors[i].rgba[3];
				}
			}
			break;
		case CGEN_ONE_MINUS_VERTEX:
			if ( tr.identityLight == 1 )
			{
				for ( i = 0; i < tess.numVertexes; i++ )
				{
					dest[i].rgba[0] = 255 - tess.vertexColors[i].rgba[0];
					dest[i].rgba[1] = 255 - tess.vertexColors[i].rgba[1];
					dest[i].rgba[2] = 255 - tess.vertexColors[i].rgba[2];
				}
			}
			else
			{
				for ( i = 0; i < tess.numVertexes; i++ )
				{
					dest[i].rgba[0] = ( 255 - tess.vertexColors[i].rgba[0] ) * tr.identityLight;
					dest[i].rgba[1] = ( 255 - tess.vertexColors[i].rgba[1] ) * tr.identityLight;
					dest[i].rgba[2] = ( 255 - tess.vertexColors[i].rgba[2] ) * tr.identityLight;
				}
			}
			break;
		case CGEN_FOG:
			{
				const fog_t *fog = tr.world->fogs + tess.fogNum;

				for ( i = 0; i < tess.numVertexes; i++ ) {
					dest[i] = fog->colorInt;
				}
			}
			break;
		case CGEN_WAVEFORM:
			RB_CalcWaveColor( &pStage->bundle[b].rgbWave, dest->rgba );
			break;
		case CGEN_ENTITY:
			RB_CalcColorFromEntity( dest->rgba );
			break;
		case CGEN_ONE_MINUS_ENTITY:
			RB_CalcColorFromOneMinusEntity( dest->rgba );
			break;
	}

	//
	// alphaGen
	//
	switch ( pStage->bundle[b].alphaGen )
	{
	case AGEN_SKIP:
		break;
	case AGEN_IDENTITY:
		if ( ( pStage->bundle[b].rgbGen == CGEN_VERTEX && tr.identityLight != 1 ) ||
			 pStage->bundle[b].rgbGen != CGEN_VERTEX ) {
			for ( i = 0; i < tess.numVertexes; i++ ) {
				dest[i].rgba[3] = 255;
			}
		}
		break;
	case AGEN_CONST:
		for ( i = 0; i < tess.numVertexes; i++ ) {
			dest[i].rgba[3] = pStage->bundle[b].constantColor.rgba[3];
		}
		break;
	case AGEN_WAVEFORM:
		RB_CalcWaveAlpha( &pStage->bundle[b].alphaWave, dest->rgba );
		break;
	case AGEN_LIGHTING_SPECULAR:
		RB_CalcSpecularAlpha( dest->rgba );
		break;
	case AGEN_ENTITY:
		RB_CalcAlphaFromEntity( dest->rgba );
		break;
	case AGEN_ONE_MINUS_ENTITY:
		RB_CalcAlphaFromOneMinusEntity( dest->rgba );
		break;
	case AGEN_VERTEX:
		for ( i = 0; i < tess.numVertexes; i++ ) {
			dest[i].rgba[3] = tess.vertexColors[i].rgba[3];
		}
		break;
	case AGEN_ONE_MINUS_VERTEX:
		for ( i = 0; i < tess.numVertexes; i++ )
		{
			dest[i].rgba[3] = 255 - tess.vertexColors[i].rgba[3];
		}
		break;
	case AGEN_PORTAL:
		{
			for ( i = 0; i < tess.numVertexes; i++ )
			{
				unsigned char alpha;
				float len;
				vec3_t v;

				VectorSubtract( tess.xyz[i], backEnd.viewParms.or.origin, v );
				len = VectorLength( v ) * tess.shader->portalRangeR;

				if ( len > 1 )
				{
					alpha = 0xff;
				}
				else
				{
					alpha = len * 0xff;
				}

				dest[i].rgba[3] = alpha;
			}
		}
		break;
	}

	//
	// fog adjustment for colors to fade out as fog increases
	//
	if ( tess.fogNum )
	{
		switch ( pStage->bundle[b].adjustColorsForFog )
		{
		case ACFF_MODULATE_RGB:
			RB_CalcModulateColorsByFog( dest->rgba );
			break;
		case ACFF_MODULATE_ALPHA:
			RB_CalcModulateAlphasByFog( dest->rgba );
			break;
		case ACFF_MODULATE_RGBA:
			RB_CalcModulateRGBAsByFog( dest->rgba );
			break;
		case ACFF_NONE:
			break;
		}
	}
}


/*
===============
R_ComputeTexCoords
===============
*/
void R_ComputeTexCoords( const int b, const textureBundle_t *bundle ) {
	int	i;
	int tm;
	vec2_t *src, *dst;

	if ( !tess.numVertexes )
		return;

	src = dst = tess.svars.texcoords[b];

	//
	// generate the texture coordinates
	//
	switch ( bundle->tcGen )
	{
	case TCGEN_IDENTITY:
		src = tess.texCoords00;
		break;
	case TCGEN_TEXTURE:
		src = tess.texCoords[0];
		break;
	case TCGEN_LIGHTMAP:
		src = tess.texCoords[1];
		break;
	case TCGEN_VECTOR:
		for ( i = 0 ; i < tess.numVertexes ; i++ ) {
			dst[i][0] = DotProduct( tess.xyz[i], bundle->tcGenVectors[0] );
			dst[i][1] = DotProduct( tess.xyz[i], bundle->tcGenVectors[1] );
		}
		break;
	case TCGEN_FOG:
		RB_CalcFogTexCoords( ( float * ) dst );
		break;
	case TCGEN_ENVIRONMENT_MAPPED:
		RB_CalcEnvironmentTexCoords( ( float * ) dst );
		break;
	case TCGEN_ENVIRONMENT_MAPPED_FP:
		RB_CalcEnvironmentTexCoordsFP( ( float * ) dst, bundle->isScreenMap );
		break;
	case TCGEN_BAD:
		return;
	}

	//
	// alter texture coordinates
	//
	for ( tm = 0; tm < bundle->numTexMods ; tm++ ) {
		switch ( bundle->texMods[tm].type )
		{
		case TMOD_NONE:
			tm = TR_MAX_TEXMODS; // break out of for loop
			break;

		case TMOD_TURBULENT:
			RB_CalcTurbulentTexCoords( &bundle->texMods[tm].wave, (float *)src, (float *) dst );
			src = dst;
			break;

		case TMOD_ENTITY_TRANSLATE:
			RB_CalcScrollTexCoords( backEnd.currentEntity->e.shaderTexCoord, (float *)src, (float *) dst );
			src = dst;
			break;

		case TMOD_SCROLL:
			RB_CalcScrollTexCoords( bundle->texMods[tm].scroll, (float *)src, (float *) dst );
			src = dst;
			break;

		case TMOD_SCALE:
			RB_CalcScaleTexCoords( bundle->texMods[tm].scale, (float *) src, (float *) dst );
			src = dst;
			break;

		case TMOD_OFFSET:
			for ( i = 0; i < tess.numVertexes; i++ ) {
				dst[i][0] = src[i][0] + bundle->texMods[tm].offset[0];
				dst[i][1] = src[i][1] + bundle->texMods[tm].offset[1];
			}
			src = dst;
			break;

		case TMOD_SCALE_OFFSET:
			for ( i = 0; i < tess.numVertexes; i++ ) {
				dst[i][0] = (src[i][0] * bundle->texMods[tm].scale[0] ) + bundle->texMods[tm].offset[0];
				dst[i][1] = (src[i][1] * bundle->texMods[tm].scale[1] ) + bundle->texMods[tm].offset[1];
			}
			src = dst;
			break;

		case TMOD_OFFSET_SCALE:
			for ( i = 0; i < tess.numVertexes; i++ ) {
				dst[i][0] = (src[i][0] + bundle->texMods[tm].offset[0]) * bundle->texMods[tm].scale[0];
				dst[i][1] = (src[i][1] + bundle->texMods[tm].offset[1]) * bundle->texMods[tm].scale[1];
			}
			src = dst;
			break;

		case TMOD_STRETCH:
			RB_CalcStretchTexCoords( &bundle->texMods[tm].wave, (float *)src, (float *) dst );
			src = dst;
			break;

		case TMOD_TRANSFORM:
			RB_CalcTransformTexCoords( &bundle->texMods[tm], (float *)src, (float *) dst );
			src = dst;
			break;

		case TMOD_ROTATE:
			RB_CalcRotateTexCoords( bundle->texMods[tm].rotateSpeed, (float *) src, (float *) dst );
			src = dst;
			break;

		default:
			ri.Error( ERR_DROP, "ERROR: unknown texmod '%d' in shader '%s'", bundle->texMods[tm].type, tess.shader->name );
			break;
		}
	}

	tess.svars.texcoordPtr[ b ] = src;
}

static qboolean vk_is_valid_pbr_surface( const qboolean hasPBR ) {
	if( !vk.pbrActive || !hasPBR )
		return qfalse;

	if ( backEnd.projection2D )
		return qfalse;

	if ( backEnd.viewParms.portalView == PV_MIRROR )
		return qfalse;

	// PBR is now supported for both world surfaces and models (entities)
	// The check for worldEntity was removed to allow models to use PBR materials

	return qtrue;
}

// Note: we no longer copy cubemap SH into stage/uniform directly here.
// RB_IterateStagesGeneric builds the final PBR uniform block (including optional cubemap SH)
// and only pushes it when it actually changes.

static void R_GetPBRSurfacePosition( vec3_t outPos ) {
	int i;

	if ( backEnd.currentEntity && backEnd.currentEntity != &tr.worldEntity ) {
		VectorCopy( backEnd.currentEntity->e.origin, outPos );
		return;
	}

	VectorClear( outPos );
	if ( tess.numVertexes <= 0 ) {
		return;
	}

	for ( i = 0; i < tess.numVertexes; i++ ) {
		outPos[0] += tess.xyz[i][0];
		outPos[1] += tess.xyz[i][1];
		outPos[2] += tess.xyz[i][2];
	}

	outPos[0] /= tess.numVertexes;
	outPos[1] /= tess.numVertexes;
	outPos[2] /= tess.numVertexes;
}

static int R_SelectCubemapIndexForPBR( void ) {
	int i;
	int bestIndex = -1;
	int bestInRadius = -1;
	float bestDistSq = 0.0f;
	float bestInRadiusDistSq = 0.0f;
	vec3_t pos;

	if ( tr.numCubemaps <= 0 )
		return -1;

	R_GetPBRSurfacePosition( pos );

	for ( i = 0; i < tr.numCubemaps; i++ ) {
		float distSq;
		vec3_t delta;
		const cubemap_t *cube = &tr.cubemaps[i];

		VectorSubtract( pos, cube->origin, delta );
		distSq = VectorLengthSquared( delta );

		if ( bestIndex == -1 || distSq < bestDistSq ) {
			bestIndex = i;
			bestDistSq = distSq;
		}

		if ( cube->parallaxRadius > 0.0f && distSq > ( cube->parallaxRadius * cube->parallaxRadius ) )
			continue;

		if ( bestInRadius == -1 || distSq < bestInRadiusDistSq ) {
			bestInRadius = i;
			bestInRadiusDistSq = distSq;
		}
	}

	const qboolean logSelect =
		( r_pbr_bindlog && r_pbr_bindlog->integer ) ||
		( r_pbr_debug && r_pbr_debug->integer >= 17 );

	if ( logSelect ) {
		const char *mapName = ( tr.world && tr.world->baseName[0] ) ? tr.world->baseName : "unknown";
		const cubemap_t *bestCube = ( bestIndex >= 0 ) ? &tr.cubemaps[bestIndex] : NULL;
		ri.Printf( PRINT_ALL,
			"PBR cubemap select: map=%s bestIndex=%d bestDistSq=%.2f bestInRadius=%d bestInDistSq=%.2f "
			"pos=(%.0f %.0f %.0f) cubeOrigin=(%.0f %.0f %.0f) radius=%.0f\n",
			mapName,
			bestIndex,
			bestDistSq,
			bestInRadius,
			bestInRadiusDistSq,
			pos[0], pos[1], pos[2],
			bestCube ? bestCube->origin[0] : 0.0f,
			bestCube ? bestCube->origin[1] : 0.0f,
			bestCube ? bestCube->origin[2] : 0.0f,
			bestCube ? bestCube->parallaxRadius : 0.0f );
	}

	return ( bestInRadius != -1 ) ? bestInRadius : bestIndex;
}

static void R_UpdatePBRCubemapDebugCvar( int cubemapIndex, const vec3_t pos )
{
#ifdef VK_CUBEMAP
	static int lastIndex = -9999;
	static vec3_t lastPos = { 0.0f, 0.0f, 0.0f };
	int now = ri.Milliseconds();
	static int lastUpdateMs = 0;

	if ( !r_pbr_showCubemap || !r_pbr_showCubemap->integer ) {
		return;
	}
	if ( !r_pbr_cubemapInfo ) {
		return;
	}

	// Rate-limit updates to avoid spamming cvar system.
	if ( cubemapIndex == lastIndex && VectorCompare( pos, lastPos ) ) {
		return;
	}
	if ( now - lastUpdateMs < 100 ) {
		return;
	}
	lastUpdateMs = now;
	lastIndex = cubemapIndex;
	VectorCopy( pos, lastPos );

	if ( cubemapIndex < 0 || cubemapIndex >= tr.numCubemaps ) {
		ri.Cvar_Set( "r_pbr_cubemapInfo", va( "PBR cubemap: none (pos %.0f %.0f %.0f)", pos[0], pos[1], pos[2] ) );
		return;
	}

	const cubemap_t *cube = &tr.cubemaps[cubemapIndex];
	ri.Cvar_Set( "r_pbr_cubemapInfo", va( "PBR cubemap: %d '%s' (pos %.0f %.0f %.0f r=%.0f)",
		cubemapIndex, cube->name, pos[0], pos[1], pos[2], cube->parallaxRadius ) );
#endif
}

/*
** RB_IterateStagesGeneric
*/
#ifdef USE_VK_PBR
static void VK_SetGlintParams( vkUniform_t *ubo );
#endif
#ifdef USE_VULKAN
static void RB_IterateStagesGeneric( const shaderCommands_t *input, qboolean fogCollapse )
#else
static void RB_IterateStagesGeneric( const shaderCommands_t *input )
#endif
{
	shaderStage_t *pStage;
	int tess_flags;
	int stage, i;

#ifdef USE_VULKAN
#if 1
	if ( r_shDebugView && r_shDebugView->integer == 3 ) {
		RB_DrawWorldSHDebugOverride();
		return;
	}
#endif
#ifdef USE_VK_PBR
	qboolean is_pbr_surface;
#endif
	uint32_t pipeline;
	int fog_stage;
	qboolean pushUniform;

#ifdef USE_VK_PBR
	const int glintsLogLevel = ( r_glintsLog != NULL ) ? r_glintsLog->integer : 0;
	const qboolean glintsLogEnabled = glintsLogLevel > 0 ||
		( r_glints_debug && r_glints_debug->value > 0.0f ) ||
		( r_glints_verbose && r_glints_verbose->integer > 0 );
	glintLogContext_t glintLogCtx;
	if ( glintsLogEnabled ) {
		glint_log_context_reset( &glintLogCtx );
	}
#endif
	vk_bind_index();

	tess_flags = input->shader->tessFlags;

	pushUniform = qfalse;

	is_pbr_surface = qfalse;

#ifdef USE_VK_PBR
	VK_SetGlintParams( &uniform );
#endif

#ifdef USE_FOG_COLLAPSE
	if ( fogCollapse ) {
		VK_SetFogParams( &uniform, &fog_stage );
		VectorCopy( backEnd.or.viewOrigin, uniform.eyePos );
		vk_update_descriptor( VK_DESC_FOG_COLLAPSE, tr.fogImage->descriptor );
		pushUniform = qtrue;
	} else
#endif
	{
		fog_stage = 0;
		if ( tess_flags & TESS_VPOS ) {
			VectorCopy( backEnd.or.viewOrigin, uniform.eyePos );
			tess_flags &= ~TESS_VPOS;
			pushUniform = qtrue;
		}
	}

#ifdef USE_VK_PBR
	is_pbr_surface = vk_is_valid_pbr_surface( tess.shader->hasPBR );

	// Debug view: render a non-PBR pass and optionally override texture0 binding.
	// Keeps runtime inspection simple without requiring extra shader variants.
	const int pbr_debug = ( r_pbr_debug != NULL ) ? r_pbr_debug->integer : 0;
	if ( pbr_debug > 0 && pbr_debug <= 4 ) {
		is_pbr_surface = qfalse;
	} else if ( pbr_debug > 4 ) {
		is_pbr_surface = qtrue;
	}
	const qboolean glintsModeEnabled = ( r_glints_mode && r_glints_mode->integer > 0 );
	qboolean debugDictValid = qfalse;
	const image_t *dictImage = vk_get_glint_dictionary_image();
	const image_t *debugEnvImage = NULL;
	qboolean debugEnvViewUsed = qfalse;
	qboolean debugHasEnv = qfalse;
	qboolean debugHasIrr = qfalse;
	qboolean stageHasLightmap = qfalse;

	if ( is_pbr_surface ) {
		Com_Memcpy( &uniform_camera.modelMatrix, backEnd.or.modelMatrix, sizeof(float) * 16 );
		Com_Memcpy( &uniform_camera.viewOrigin, backEnd.refdef.vieworg, sizeof( vec3_t) );
		uniform_camera.viewOrigin[3] = 0.0;

		vk.cmd->camera_ubo_offset = vk_append_uniform( &uniform_camera, sizeof(uniform_camera), vk.uniform_camera_item_size );

		uniform.pbrDebug[0] = ( pbr_debug > 4 ) ? (float)pbr_debug : 0.0f;
		uniform.pbrDebug[1] = ( r_pbr_normalSwizzle != NULL ) ? r_pbr_normalSwizzle->value : 0.0f;
		uniform.pbrDebug[2] = ( r_pbr_forceLight != NULL ) ? r_pbr_forceLight->value : 0.0f;
		uniform.pbrDebug[3] = ( r_pbr_forceGlints != NULL ) ? r_pbr_forceGlints->value : 0.0f;

		pushUniform = qtrue;
	}
#endif
#endif // USE_VULKAN

	if ( pbr_debug == 0 ) {
		pbrDebugPermutationLogged = qfalse;
		pbrDebugLastMode = 0;
		pbrDebugLastFlags = 0;
	}

#ifdef USE_VK_PBR
		stageHasLightmap = qfalse;
#endif
	for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ )
	{
#ifdef USE_VK_PBR
		stageHasLightmap = qfalse;
		qboolean wantsLightmap = qfalse;
		const char *lightmapSource = "none";
		const image_t *selectedLightmap = NULL;
		qboolean stageHasEnv = qfalse;
#endif
		pStage = tess.xstages[ stage ];
		if ( !pStage )
			break;

#ifdef USE_VBO
		tess.vboStage = stage;
#endif

#ifdef USE_VULKAN
		tess_flags |= pStage->tessFlags;
#ifdef USE_VK_PBR
		const textureBundle_t *pbrLightmapBundle = NULL;
#endif

		for ( i = 0;  i < (int)pStage->numTexBundles; i++ ) {
			if ( pStage->bundle[i].image[0] != NULL ) {
				GL_SelectTexture( i );
				R_BindAnimatedImage( &pStage->bundle[i] );
				if ( tess_flags & ( TESS_ST0 << i ) ) {
					R_ComputeTexCoords( i, &pStage->bundle[i] );
				}
				if ( tess_flags & ( TESS_RGBA0 << i ) ) {
					R_ComputeColors( i, tess.svars.colors[i], pStage );
				}
				if ( tess_flags & (TESS_ENT0 << i) && backEnd.currentEntity ) {
					uniform.ent.color[i][0] = backEnd.currentEntity->e.shader.rgba[0] / 255.0;
					uniform.ent.color[i][1] = backEnd.currentEntity->e.shader.rgba[1] / 255.0;
					uniform.ent.color[i][2] = backEnd.currentEntity->e.shader.rgba[2] / 255.0;
					uniform.ent.color[i][3] = pStage->bundle[i].alphaGen == AGEN_IDENTITY ? 1.0 : (backEnd.currentEntity->e.shader.rgba[3] / 255.0);
					pushUniform = qtrue;
				}
			}
		}

#ifdef USE_VK_PBR
		// PBR uses the lightmap as a lighting source; ensure it is bound even when the
		// original shader uses a separate lightmap stage (legacy multi-pass).
		if ( is_pbr_surface ) {
			// Ensure base texture is always bound
			if ( pStage->bundle[0].image[0] == NULL ) {
				GL_SelectTexture( 0 );
				GL_Bind( tr.whiteImage );
				if ( r_pbr_validate && r_pbr_validate->integer > 0 ) {
					static char loggedShaders[1024] = {0};
					if ( !strstr( loggedShaders, tess.shader->name ) ) {
						ri.Printf( PRINT_ALL, "PBR: missing base bundle0 for shader '%s'\n", tess.shader->name );
						strncat( loggedShaders, tess.shader->name, sizeof(loggedShaders) - strlen(loggedShaders) - 1 );
					}
				}
			}

			// Ensure lightmap is bound for BSP-ish passes (when stage expects it)
			wantsLightmap = ( (tess_flags & TESS_ST1) != 0 ) || R_StageHasLightmap( pStage );
			lightmapSource = "none";
			selectedLightmap = NULL;

			if ( wantsLightmap ) {
				if ( pStage->numTexBundles > 1 && pStage->bundle[1].lightmap != LIGHTMAP_INDEX_NONE ) {
					pbrLightmapBundle = &pStage->bundle[1];
					lightmapSource = "bundle1";
				} else {
					pbrLightmapBundle = R_FindLightmapBundle( tess.shader );
					lightmapSource = "surface";
				}

				GL_SelectTexture( 1 );
				if ( pbrLightmapBundle && pbrLightmapBundle->image[0] ) {
					R_BindAnimatedImage( pbrLightmapBundle );
					R_ComputeTexCoords( 1, pbrLightmapBundle );
					tess_flags |= TESS_ST1;
					selectedLightmap = R_GetAnimatedImage( pbrLightmapBundle );
				} else {
					GL_Bind( tr.whiteImage );
					if ( r_pbr_validate && r_pbr_validate->integer > 0 ) {
						ri.Printf( PRINT_ALL, "PBR: missing lightmap for shader '%s'\n", tess.shader->name );
					}
					lightmapSource = "missing";
				}
				GL_SelectTexture( 0 );
			}

		}
#endif

		if ( pushUniform ) {
			pushUniform = qfalse;
			vk_push_uniform_cached( &uniform );
		}

		GL_SelectTexture( 0 );

		if ( r_lightmap->integer && pStage->bundle[1].lightmap != LIGHTMAP_INDEX_NONE ) {
			//GL_SelectTexture( 0 );
			GL_Bind( tr.whiteImage ); // replace diffuse texture with a white one thus effectively render only lightmap
		}

#ifdef USE_VK_PBR
		if ( pbr_debug > 0 && pbr_debug <= 4 && pStage->vk_pbr_flags ) {
			switch ( pbr_debug ) {
				default:
				case 1: // base/albedo (already bound)
					break;
				case 2: // normal map
					if ( pStage->normalMap ) {
						GL_Bind( pStage->normalMap );
					}
					break;
				case 3: // packed physical map
					if ( pStage->physicalMap ) {
						GL_Bind( pStage->physicalMap );
					}
					break;
				case 4: // emissive map
					if ( pStage->emissiveMap ) {
						GL_Bind( pStage->emissiveMap );
					}
					break;
			}
		}
#endif

	#ifdef USE_VK_PBR
		const qboolean useIndexing = vk.descriptorIndexingActive ? qtrue : qfalse;
	#else
		const qboolean useIndexing = qfalse;
	#endif
		VkDescriptorSet pbrDescriptor = VK_NULL_HANDLE;

#ifdef USE_VK_PBR
		if ( is_pbr_surface && ( pStage->vk_pbr_flags || pbr_debug >= 17 ) ) {
			static VkCommandBuffer lastCmdBuf = VK_NULL_HANDLE;
			static qboolean lastValid = qfalse;
			static vkPbrUniformBlock_t lastBlock;

			vkPbrUniformBlock_t block;
			const image_t *fallback_white = tr.whiteImage;
			const image_t *fallback_black = tr.blackImage ? tr.blackImage : tr.whiteImage;
			const image_t *albedoImage = R_GetAnimatedImage( &pStage->bundle[0] );
			const image_t *lightmapImage = fallback_white;
			qboolean hasLightmap = qfalse;
			qboolean hasEnv = qfalse;
			qboolean hasIrr = qfalse;
			VkImageView glintDictView = vk_get_glint_dictionary_view();
			VkImageView envView = VK_NULL_HANDLE;
			VkImageView irrView = VK_NULL_HANDLE;

			Com_Memset( &block, 0, sizeof( block ) );
			Vector4Copy( pStage->emissiveScale, block.emissiveScale );
			Vector4Copy( pStage->clearcoatScale, block.clearcoatScale );
			Vector4Copy( pStage->sheenScale, block.sheenScale );
			Vector4Copy( pStage->anisotropyScale, block.anisotropyScale );
			Vector4Copy( pStage->transmissionScale, block.transmissionScale );
			Vector4Copy( pStage->subsurfaceColor, block.subsurfaceColor );
			Vector4Copy( pStage->subsurfaceParams, block.subsurfaceParams );

			if ( selectedLightmap ) {
				lightmapImage = selectedLightmap;
			}

			hasLightmap = ( lightmapImage != fallback_white );
			stageHasLightmap = hasLightmap;

			// Stage descriptor sets can become invalid after a descriptor pool reset (e.g. swapchain/video restart).
			// Recreate lazily here so we never bind a stale set.
			if ( useIndexing ) {
				pbrDescriptor = vk_get_pbr_indexed_descriptor();
				if ( pbrDescriptor != VK_NULL_HANDLE ) {
					vk_update_descriptor_if_changed( VK_DESC_PBR, pbrDescriptor );
				}
			} else {
				if ( vk_create_pbr_descriptor_set( pStage ) && pStage->pbrDescriptor != VK_NULL_HANDLE ) {
					pbrDescriptor = pStage->pbrDescriptor;
					vk_update_descriptor_if_changed( VK_DESC_PBR, pbrDescriptor );
					vk_update_pbr_descriptor_common( pbrDescriptor );
					vk_update_pbr_descriptor_binding( pbrDescriptor, VK_PBR_BINDING_ALBEDO, albedoImage );
					vk_update_pbr_descriptor_binding( pbrDescriptor, VK_PBR_BINDING_LIGHTMAP, lightmapImage );
				}
			}

			if (!useIndexing && pbrDescriptor != VK_NULL_HANDLE) {
				if (glintDictView != VK_NULL_HANDLE) {
					if (dictImage) {
						vk_update_pbr_descriptor_binding(
							pbrDescriptor,
							VK_PBR_BINDING_GLINT_DICT,
							dictImage
						);
					}
				} else {
					vk_update_pbr_descriptor_binding(
						pbrDescriptor,
						VK_PBR_BINDING_GLINT_DICT,
						tr.blackImage ? tr.blackImage : tr.whiteImage
					);
				}
			}

			if ( glintsLogEnabled ) {
				const char *stageName = tess.shader ? tess.shader->name : "null";
				const char *shaderName = stageName;
				const uint32_t brdfId = glint_descriptor_id( pbrDescriptor );
				const uint32_t dictId = glint_dict_id( glintDictView );
				glint_log_stage_line( &glintLogCtx, stageName, shaderName, pStage->vk_pbr_flags, brdfId, dictId,
					pbrDescriptor, glintDictView, glintsLogLevel );
			}
			
			int cubemapIndex = -1;
			const image_t *envFallback = tr.pbrEnvFallback ? tr.pbrEnvFallback :
				( tr.emptyCubemap ? tr.emptyCubemap : fallback_black );
			const image_t *irrFallback = tr.pbrIrrFallback ? tr.pbrIrrFallback :
				( tr.emptyCubemap ? tr.emptyCubemap : fallback_black );
			const image_t *envImage = envFallback;
			const image_t *irrImage = irrFallback;
			if ( !tr.numCubemaps || backEnd.viewParms.targetCube != NULL ) {
				if ( backEnd.viewParms.targetCube == NULL ) {
					vec3_t dbgPos;
					R_GetPBRSurfacePosition( dbgPos );
					R_UpdatePBRCubemapDebugCvar( -1, dbgPos );
				}

				// Use stage-provided SH when no cubemap is available.
				Com_Memcpy( block.shCoeffs, pStage->shCoeffs, sizeof( block.shCoeffs ) );
			}
			else { 
				vec3_t dbgPos;
				R_GetPBRSurfacePosition( dbgPos );
				cubemapIndex = R_SelectCubemapIndexForPBR();
				R_UpdatePBRCubemapDebugCvar( cubemapIndex, dbgPos );
				if ( cubemapIndex < 0 ) {
					Com_Memcpy( block.shCoeffs, pStage->shCoeffs, sizeof( block.shCoeffs ) );
				} else {
					envImage = tr.cubemaps[cubemapIndex].prefiltered_image ?
						tr.cubemaps[cubemapIndex].prefiltered_image : envImage;
					if ( tr.cubemaps[cubemapIndex].irradiance_image ) {
						irrImage = tr.cubemaps[cubemapIndex].irradiance_image;
					}

					// Prefer cubemap SH coefficients when present, otherwise fall back to stage SH.
					if ( tr.cubemaps[cubemapIndex].hasSHCoeffs ) {
						Com_Memcpy( block.shCoeffs, tr.cubemaps[cubemapIndex].shCoeffs, sizeof( block.shCoeffs ) );
					} else {
						Com_Memcpy( block.shCoeffs, pStage->shCoeffs, sizeof( block.shCoeffs ) );
					}
				}
			}

			envView = envImage ? envImage->view : VK_NULL_HANDLE;
			irrView = irrImage ? irrImage->view : VK_NULL_HANDLE;

			hasEnv = ( envView != VK_NULL_HANDLE );
			hasIrr = ( irrView != VK_NULL_HANDLE );
			stageHasEnv = hasEnv;
			stageHasEnv = hasEnv;

			debugHasEnv = hasEnv;
			debugHasIrr = hasIrr;

			static int lastLoggedCubemapIndex = -2;
			static int lastLoggedNumCubemaps = -1;
			static qboolean lastLoggedHasEnv = qfalse;
			static qboolean lastLoggedHasIrr = qfalse;

			if ( cubemapIndex != lastLoggedCubemapIndex ||
				 tr.numCubemaps != lastLoggedNumCubemaps ||
				 hasEnv != lastLoggedHasEnv ||
				 hasIrr != lastLoggedHasIrr )
			{
				const image_t *dbgPre = ( cubemapIndex >= 0 ) ? tr.cubemaps[cubemapIndex].prefiltered_image : NULL;
				const image_t *dbgIrr = ( cubemapIndex >= 0 ) ? tr.cubemaps[cubemapIndex].irradiance_image : NULL;
				ri.Printf( PRINT_ALL,
					"PBR IBL state: numCubemaps=%d index=%d hasEnv=%d hasIrr=%d prefiltered=%p irradiance=%p\n",
					tr.numCubemaps,
					cubemapIndex,
					hasEnv ? 1 : 0,
					hasIrr ? 1 : 0,
					(const void *)dbgPre,
					(const void *)dbgIrr );
				lastLoggedCubemapIndex = cubemapIndex;
				lastLoggedNumCubemaps = tr.numCubemaps;
				lastLoggedHasEnv = hasEnv;
				lastLoggedHasIrr = hasIrr;
			}

			if ( useIndexing ) {
				vk_update_pbr_indexed_common( envImage, irrImage );
				Vector4Set( block.texIndex0,
					(float)vk_get_image_descriptor_index( albedoImage ),
					(float)vk_get_image_descriptor_index( pStage->normalMap ? pStage->normalMap : fallback_white ),
					(float)vk_get_image_descriptor_index( pStage->physicalMap ? pStage->physicalMap : fallback_white ),
					(float)vk_get_image_descriptor_index( pStage->emissiveMap ? pStage->emissiveMap : fallback_black ) );
				Vector4Set( block.texIndex1,
					(float)vk_get_image_descriptor_index( lightmapImage ),
					(float)vk_get_image_descriptor_index( pStage->clearcoatMap ? pStage->clearcoatMap : fallback_black ),
					(float)vk_get_image_descriptor_index( pStage->sheenMap ? pStage->sheenMap : fallback_black ),
					(float)vk_get_image_descriptor_index( pStage->anisotropyMap ? pStage->anisotropyMap : fallback_black ) );
				Vector4Set( block.texIndex2,
					(float)vk_get_image_descriptor_index( pStage->transmissionMap ? pStage->transmissionMap : fallback_black ),
					(float)vk_get_image_descriptor_index( pStage->subsurfaceMap ? pStage->subsurfaceMap : fallback_black ),
					(float)vk_get_glint_dict_index(),
					0.0f );
			} else {
				if ( pbrDescriptor != VK_NULL_HANDLE ) {
					vk_update_pbr_descriptor_binding( pbrDescriptor, VK_PBR_BINDING_ENV_CUBEMAP, envImage );
					vk_update_pbr_descriptor_binding( pbrDescriptor, VK_PBR_BINDING_IRRADIANCE, irrImage );
#ifdef VK_CUBEMAP
					if ( cubemapIndex < 0 ) {
						VkImageView sceneView = vk_get_scene_cubemap_view();
						if ( sceneView != VK_NULL_HANDLE ) {
							vk_update_pbr_descriptor_binding_from_view( pbrDescriptor, VK_PBR_BINDING_ENV_CUBEMAP, sceneView );
							vk_update_pbr_descriptor_binding_from_view( pbrDescriptor, VK_PBR_BINDING_IRRADIANCE, sceneView );
							hasEnv = hasIrr = qtrue;
					envView = sceneView;
					irrView = sceneView;
							debugEnvViewUsed = qtrue;
						}
					}
#endif
				}
				Vector4Set( block.texIndex0, 0.0f, 0.0f, 0.0f, 0.0f );
				Vector4Set( block.texIndex1, 0.0f, 0.0f, 0.0f, 0.0f );
				Vector4Set( block.texIndex2, 0.0f, 0.0f, 0.0f, 0.0f );
			}

			debugEnvImage = debugEnvViewUsed ? NULL : envImage;

			const qboolean shouldLogBindings =
				!tr_pbr_bindLogPrinted &&
				( ( r_pbr_bindlog && r_pbr_bindlog->integer ) ||
				  ( r_pbr_debug && r_pbr_debug->integer >= 17 ) );
			if ( shouldLogBindings ) {
			const char *mapName = ( tr.world && tr.world->baseName[0] ) ? tr.world->baseName : "unknown";
			ri.Printf( PRINT_ALL,
				"PBR IBL bind: map=%s numCubemaps=%d cubemapIndex=%d envImg=%p envView=%p irrImg=%p irrView=%p hasEnv=%d hasIrr=%d stateBits=0x%08x\n",
				mapName,
				tr.numCubemaps,
				cubemapIndex,
				(const void *)envImage,
				(const void *)(uintptr_t)envView,
				(const void *)irrImage,
				(const void *)(uintptr_t)irrView,
				envView != VK_NULL_HANDLE ? 1 : 0,
				irrView != VK_NULL_HANDLE ? 1 : 0,
				pStage->stateBits );
			if ( r_glints && r_glints->integer ) {
				const int dictW = dictImage ? ( dictImage->uploadWidth ? dictImage->uploadWidth : dictImage->width ) : 0;
				const int dictH = dictImage ? ( dictImage->uploadHeight ? dictImage->uploadHeight : dictImage->height ) : 0;
				ri.Printf( PRINT_ALL,
					"PBR glints bind: dictImg=%p dictView=%p dictValid=%d dictW=%d dictH=%d\n",
					(const void *)dictImage,
					(const void *)(uintptr_t)glintDictView,
					glintDictView != VK_NULL_HANDLE ? 1 : 0,
					dictW,
					dictH );
			}
			tr_pbr_bindLogPrinted = qtrue;
		}

			Vector4Set( block.featureFlags,
				hasEnv ? 1.0f : 0.0f,
				hasIrr ? 1.0f : 0.0f,
				hasLightmap ? 1.0f : 0.0f,
				glintDictView != VK_NULL_HANDLE ? 1.0f : 0.0f );

			const float iblCompiledFlag = ( envImage != NULL && irrImage != NULL ) ? 1.0f : 0.0f;
			const float hasEnvFlag = hasEnv ? 1.0f : 0.0f;
			const float hasIrrFlag = hasIrr ? 1.0f : 0.0f;
			const qboolean dictValidState = ( glintDictView != VK_NULL_HANDLE );
			const float dictValidFlag = dictValidState ? 1.0f : 0.0f;
			debugDictValid = dictValidState;
			const float debugForceLod = r_ibl_forceLod ? (float)r_ibl_forceLod->integer : -1.0f;
			const float debugEps = r_pbr_debug_eps ? r_pbr_debug_eps->value : 1e-4f;
			const float glintsEnabledFlag = glintsModeEnabled ? 1.0f : 0.0f;

			static const world_t *lastLoggedWorld = NULL;
			static qboolean loggedBindings = qfalse;
			if ( tr.world != lastLoggedWorld ) {
				lastLoggedWorld = tr.world;
				loggedBindings = qfalse;
			}
			if ( !loggedBindings ) {
				loggedBindings = qtrue;
				ri.Printf( PRINT_ALL,
					"PBR IBL bind: numCubemaps=%d index=%d envImg=%p envView=%p irrImg=%p irrView=%p hasEnv=%d hasIrr=%d\n",
					tr.numCubemaps,
					cubemapIndex,
					(const void *)envImage,
					(const void *)(envImage ? (uintptr_t)envImage->view : 0),
					(const void *)irrImage,
					(const void *)(irrImage ? (uintptr_t)irrImage->view : 0),
					hasEnv ? 1 : 0,
					hasIrr ? 1 : 0 );
				if ( r_glints && r_glints->integer ) {
					ri.Printf( PRINT_ALL,
						"PBR glints bind: dictImg=%p dictView=%p valid=%d\n",
						(const void *)dictImage,
						(const void *)(uintptr_t)glintDictView,
						dictValidState ? 1 : 0 );
				}
			}

			if ( pbr_debug != 0 && (
					!pbrDebugPermutationLogged ||
					pbrDebugLastMode != pbr_debug ||
					pbrDebugLastFlags != pStage->vk_pbr_flags ) )
			{
				char flagBuf[64];
				glint_flags_to_string( pStage->vk_pbr_flags, flagBuf, sizeof( flagBuf ) );
				ri.Printf( PRINT_ALL,
					"PBR debug mode %d permutation: flags=0x%08x[%s] wantsLM=%d hasLM=%d src=%s IBL=%d lightmap=%d glints=%d\n",
					pbr_debug,
					pStage->vk_pbr_flags,
					flagBuf,
					wantsLightmap ? 1 : 0,
					hasLightmap ? 1 : 0,
					lightmapSource,
					( pStage->vk_pbr_flags & PBR_HAS_IRRADIANCE ) ? 1 : 0,
					( pStage->vk_pbr_flags & PBR_HAS_LIGHTMAP ) ? 1 : 0,
					( glintsModeEnabled && dictValidState ) ? 1 : 0 );
				pbrDebugPermutationLogged = qtrue;
				pbrDebugLastMode = pbr_debug;
				pbrDebugLastFlags = pStage->vk_pbr_flags;
			}

			Vector4Set( uniform.pbrDebugFlags, iblCompiledFlag, hasEnvFlag, hasIrrFlag, dictValidFlag );
			float cubemapStateValue = CUBEMAP_STATE_NONE;
			if ( tr.numCubemaps > 0 ) {
				cubemapStateValue = CUBEMAP_STATE_HAVE_DEFS_NOT_RENDERED;
			}
			if ( cubemapIndex >= 0 && cubemapIndex < tr.numCubemaps ) {
				cubemapStateValue = tr.cubemaps[cubemapIndex].state;
			}
			if ( debugEnvViewUsed ) {
				cubemapStateValue = CUBEMAP_STATE_READY;
			}
			Vector4Set( uniform.pbrDebugParams, debugForceLod, debugEps, glintsEnabledFlag, cubemapStateValue );

			// Only push uniforms when the PBR block or debug flags actually changed.
			if ( vk.cmd && vk.cmd->command_buffer != lastCmdBuf ) {
				lastCmdBuf = vk.cmd->command_buffer;
				lastValid = qfalse;
			}
			static vec4_t lastDebugFlags;
			static vec4_t lastDebugParams;
			static qboolean lastDebugValid = qfalse;
			const qboolean debugChanged =
				( !lastDebugValid ) ||
				memcmp( lastDebugFlags, uniform.pbrDebugFlags, sizeof( uniform.pbrDebugFlags ) ) != 0 ||
				memcmp( lastDebugParams, uniform.pbrDebugParams, sizeof( uniform.pbrDebugParams ) ) != 0;

			if ( !lastValid || memcmp( &lastBlock, &block, sizeof( block ) ) != 0 || debugChanged ) {
				lastBlock = block;
				lastValid = qtrue;
				lastDebugValid = qtrue;
				Vector4Copy( uniform.pbrDebugFlags, lastDebugFlags );
				Vector4Copy( uniform.pbrDebugParams, lastDebugParams );

				Vector4Copy( block.emissiveScale, uniform.pbrEmissiveScale );
				Vector4Copy( block.clearcoatScale, uniform.pbrClearcoatScale );
				Vector4Copy( block.sheenScale, uniform.pbrSheenScale );
				Vector4Copy( block.anisotropyScale, uniform.pbrAnisotropyScale );
				Vector4Copy( block.transmissionScale, uniform.pbrTransmissionScale );
				Vector4Copy( block.subsurfaceColor, uniform.pbrSubsurfaceColor );
				Vector4Copy( block.subsurfaceParams, uniform.pbrSubsurfaceParams );
				Com_Memcpy( uniform.pbrShCoeffs, block.shCoeffs, sizeof( uniform.pbrShCoeffs ) );
				Vector4Copy( block.texIndex0, uniform.pbrTexIndex0 );
				Vector4Copy( block.texIndex1, uniform.pbrTexIndex1 );
				Vector4Copy( block.texIndex2, uniform.pbrTexIndex2 );
				Vector4Copy( block.featureFlags, uniform.pbrFeatureFlags );

				vk_push_uniform_cached( &uniform );
			}
		}
#endif

		if ( backEnd.viewParms.portalView == PV_MIRROR ) {
			pipeline = pStage->vk_mirror_pipeline[fog_stage];
		} else {
			pipeline = pStage->vk_pipeline[fog_stage];
		}

	#ifdef USE_VK_PBR
			if ( pipeline != 0 ) {
			Vk_Pipeline_Def def;
			vk_get_pipeline_def( pipeline, &def );

		if ( is_pbr_surface && ( pStage->vk_pbr_flags || pbr_debug >= 17 ) ) {
				const uint32_t baseFlags = pStage->vk_pbr_flags & ~( PBR_HAS_LIGHTMAP | PBR_HAS_IRRADIANCE );
				uint32_t desiredFlags = baseFlags;
				if ( stageHasLightmap ) {
					desiredFlags |= PBR_HAS_LIGHTMAP;
				}
				if ( stageHasEnv ) {
					desiredFlags |= PBR_HAS_IRRADIANCE;
				}

				if ( def.vk_pbr_flags != desiredFlags ) {
					def.vk_pbr_flags = desiredFlags;
					pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
					vk_get_pipeline_def( pipeline, &def );
				}
			}

			if ( !is_pbr_surface && pStage->vk_pbr_flags ) {
				def.vk_pbr_flags = 0;
				pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
				vk_get_pipeline_def( pipeline, &def );
			}

			if ( ( pbr_debug == 18 || pbr_debug == 19 ) && pipeline != 0 ) {
				int envBindingIndex = -1;
				if ( !debugEnvViewUsed ) {
					if ( useIndexing && debugEnvImage ) {
						envBindingIndex = (int)vk_get_image_descriptor_index( debugEnvImage );
					} else if ( !useIndexing ) {
						envBindingIndex = VK_PBR_BINDING_ENV_CUBEMAP;
					}
				}

				const int hasEnvLog = debugHasEnv ? 1 : 0;
				const int hasIrrLog = debugHasIrr ? 1 : 0;
				const int glintsEnabledLog = glintsModeEnabled ? 1 : 0;
				const int dictValidLog = debugDictValid ? 1 : 0;
				const int dictWidthLog = dictImage ? dictImage->uploadWidth : 0;
				const int dictHeightLog = dictImage ? dictImage->uploadHeight : 0;

				static struct {
					qboolean valid;
					int hasEnv;
					int hasIrr;
					int glintsEnabled;
					int dictValid;
					int dictW;
					int dictH;
					int envIndex;
					uint32_t stateBits;
					uint32_t pbrFlags;
					uint32_t descriptorIndexing;
				} last_log = { qfalse, 0, 0, 0, 0, 0, 0, -1, 0, 0, 0 };

				if ( !last_log.valid ||
					last_log.hasEnv != hasEnvLog ||
					last_log.hasIrr != hasIrrLog ||
					last_log.glintsEnabled != glintsEnabledLog ||
					last_log.dictValid != dictValidLog ||
					last_log.dictW != dictWidthLog ||
					last_log.dictH != dictHeightLog ||
					last_log.envIndex != envBindingIndex ||
					last_log.stateBits != def.state_bits ||
					last_log.pbrFlags != def.vk_pbr_flags ||
					last_log.descriptorIndexing != def.descriptorIndexing ) {

					ri.Printf( PRINT_ALL,
						"PBR debug %d: hasEnv=%d hasIrr=%d glintsEnabled=%d dictValid=%d dictW=%d dictH=%d envBinding=%d stateBits=0x%08x pbrFlags=0x%08x descIndexing=%u\n",
						pbr_debug,
						hasEnvLog,
						hasIrrLog,
						glintsEnabledLog,
						dictValidLog,
						dictWidthLog,
						dictHeightLog,
						envBindingIndex,
						def.state_bits,
						def.vk_pbr_flags,
						def.descriptorIndexing );

					last_log.valid = qtrue;
					last_log.hasEnv = hasEnvLog;
					last_log.hasIrr = hasIrrLog;
					last_log.glintsEnabled = glintsEnabledLog;
					last_log.dictValid = dictValidLog;
					last_log.dictW = dictWidthLog;
					last_log.dictH = dictHeightLog;
					last_log.envIndex = envBindingIndex;
					last_log.stateBits = def.state_bits;
					last_log.pbrFlags = def.vk_pbr_flags;
					last_log.descriptorIndexing = def.descriptorIndexing;
				}
			}
		}
#endif


		vk_bind_pipeline( pipeline );
		vk_bind_geometry( tess_flags );
		vk_draw_geometry( tess.depthRange, qtrue );

		if ( pStage->depthFragment ) {
			if ( backEnd.viewParms.portalView == PV_MIRROR )
				pipeline = pStage->vk_mirror_pipeline_df;
			else
				pipeline = pStage->vk_pipeline_df;
			vk_bind_pipeline( pipeline );
			vk_draw_geometry( tess.depthRange, qtrue );
		}
#else
		R_ComputeColors( 0, tess.svars.colors[0].rgba, pStage );

		R_ComputeTexCoords( 0, &pStage->bundle[0] );

		//
		// do multitexture
		//
		if ( pStage->bundle[1].image[0] != NULL )
		{
			DrawMultitextured( input, stage );
		}
		else
		{
			if ( !setArraysOnce )
			{
				R_ComputeTexCoords( 0, &pStage->bundle[0] );
				R_ComputeColors( 0, tess.svars.colors[0], pStage );

				GL_ClientState( 1, CLS_NONE );
				GL_ClientState( 0, CLS_TEXCOORD_ARRAY | CLS_COLOR_ARRAY );

				qglTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoordPtr[0] );
				qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, input->svars.colors[0].rgba );
			}

			//
			// set state
			//
			R_BindAnimatedImage( &pStage->bundle[0] );

			GL_State( pStage->stateBits );

			//
			// draw
			//
			R_DrawElements( input->numIndexes, input->indexes );
		}
#endif

		// allow skipping out to show just lightmaps during development
		if ( r_lightmap->integer && ( pStage->bundle[0].lightmap != LIGHTMAP_INDEX_NONE || pStage->bundle[1].lightmap != LIGHTMAP_INDEX_NONE ) )
			break;

		tess_flags = 0;
	}

#ifdef USE_VULKAN
	if ( pushUniform ) {
		vk_push_uniform_cached( &uniform );
	}
	if ( tess_flags ) // fog-only shaders?
		vk_bind_geometry( tess_flags );
#endif

#ifdef USE_VK_PBR
	if ( glintsLogEnabled ) {
		glint_log_summary( &glintLogCtx, glintsLogLevel );
	}
#endif
}


#ifdef USE_VULKAN

void VK_SetFogParams( vkUniform_t *ubo, int *fogStage )
{
	if ( tess.fogNum && tess.shader->fogPass ) {
		const fogProgramParms_t *fp = RB_CalcFogProgramParms();
		// vertex data
		Vector4Copy( fp->fogDistanceVector, ubo->fogDistanceVector );
		Vector4Copy( fp->fogDepthVector, ubo->fogDepthVector );
		ubo->fogEyeT[0] = fp->eyeT;
		if ( fp->eyeOutside ) {
			ubo->fogEyeT[1] = 0.0; // fog eye out
		} else {
			ubo->fogEyeT[1] = 1.0; // fog eye in
		}
		// fragment data
		Vector4Copy( fp->fogColor, ubo->fogColor );
		*fogStage = 1;
	} else {
		*fogStage = 0;
	}
}

#ifdef USE_VK_PBR
static void VK_SetGlintParams( vkUniform_t *ubo )
{
	if ( !r_glints )
	{
		return;
	}

	if ( r_glints->integer <= 0 )
	{
		const size_t glintBytes = (size_t)((char *)&ubo->glintColor + sizeof( ubo->glintColor ) - (char *)&ubo->glintCore);
		Com_Memset( &ubo->glintCore, 0, glintBytes );
		return;
	}

	// Update the glint dictionary once per frame if params changed or reload requested.
	{
		static int lastFrameChecked = -1;
		if ( tr.frameCount != lastFrameChecked ) {
			glint_dict_params_t params;
			qboolean forceReload = qfalse;

			lastFrameChecked = tr.frameCount;

			params.entries = r_glints_dict_count ? r_glints_dict_count->integer : GLINT_DICT_MAX_ENTRIES;
			params.levels = r_glints_dict_levels ? r_glints_dict_levels->integer : GLINT_DICT_MAX_LEVELS;
			params.size = r_glints_dict_size ? r_glints_dict_size->integer : GLINT_DICT_MAX_SIZE;
			params.alpha = r_glints_dict_alpha ? r_glints_dict_alpha->value : 0.5f;
			params.lobeSigma = r_glints_dict_lobeSigma ? r_glints_dict_lobeSigma->value : 0.02f;
			params.mode = r_glints_mode ? r_glints_mode->integer : 0;
			params.seed = r_glints_seed ? (uint32_t)r_glints_seed->integer : 0u;

			if ( r_glints_dict_reload && r_glints_dict_reload->integer != 0 ) {
				forceReload = qtrue;
				ri.Cvar_Set( "r_glints_dict_reload", "0" );
			}

			vk_update_glint_dictionary_if_needed( &params, forceReload );
		}
	}

	// Log debug level changes once to confirm the shader override path.
	if ( r_glints_debug ) {
		static float lastGlintDebug = -9999.0f;
		if ( r_glints_debug->value != lastGlintDebug ) {
			lastGlintDebug = r_glints_debug->value;
			ri.Printf( PRINT_ALL, "glint debug level = %.2f (r_glints=%d)\n",
				r_glints_debug->value, r_glints->integer );
		}
	}

	if ( r_glints_verbose && r_glints_verbose->integer > 0 ) {
		static int lastMode = -9999;
		static int lastLod = -9999;
		static int lastSeed = -9999;
		const int mode = r_glints_mode ? r_glints_mode->integer : 0;
		const int lod = r_glints_lod ? r_glints_lod->integer : -1;
		const int seed = r_glints_seed ? r_glints_seed->integer : 0;

		if ( mode != lastMode ) {
			lastMode = mode;
			ri.Printf( PRINT_ALL, "glint mode = %d (%s)\n",
				mode, ( mode >= 2 ) ? "Chermain 2020" : "legacy" );
		}
		if ( lod != lastLod ) {
			lastLod = lod;
			ri.Printf( PRINT_ALL, "glint forced LOD = %d\n", lod );
		}
		if ( seed != lastSeed ) {
			lastSeed = seed;
			ri.Printf( PRINT_ALL, "glint seed = %d\n", seed );
		}
	}

	ubo->glintCore[0] = r_glints->value;
	ubo->glintCore[1] = r_glints_mode->value;
	ubo->glintCore[2] = r_glints_debug->value;
	ubo->glintCore[3] = r_glints_seed->value;

	static qboolean lastGlintValid = qfalse;
	static size_t lastGlintSize = 0;
	static VkImageView lastGlintView = VK_NULL_HANDLE;
	const VkImageView currentGlintView = vk_get_glint_dictionary_view();

	if ( vk.glint.valid != lastGlintValid || vk.glint.size != lastGlintSize || currentGlintView != lastGlintView ) {
		lastGlintValid = vk.glint.valid;
		lastGlintSize = vk.glint.size;
		lastGlintView = currentGlintView;
		ri.Printf( PRINT_ALL,
			"PBR glints: r_glints=%d valid=%d dict=%p size=%zu view=%p\n",
			r_glints->integer,
			vk.glint.valid ? 1 : 0,
			(void *)(uintptr_t)vk.glint.dictionary,
			vk.glint.size,
			(void *)(uintptr_t)currentGlintView );
	}

	ubo->glintMaterial[0] = r_glints_strength->value;
	ubo->glintMaterial[1] = r_glints_minRoughness->value;
	ubo->glintMaterial[2] = r_glints_maxRoughness->value;
	ubo->glintMaterial[3] = r_glints_materialMask->value;

	ubo->glintMicro[0] = r_glints_density->value;
	ubo->glintMicro[1] = r_glints_scale->value;
	ubo->glintMicro[2] = r_glints_anisotropy->value;
	ubo->glintMicro[3] = r_glints_slopeVariance->value;

	ubo->glintSampling[0] = r_glints_samples->value;
	ubo->glintSampling[1] = r_glints_filter->value;
	ubo->glintSampling[2] = r_glints_tileSize->value;
	ubo->glintSampling[3] = r_glints_mipBias->value;

	ubo->glintTemporal[0] = r_glints_temporal->value;
	ubo->glintTemporal[1] = r_glints_taaWeight->value;
	ubo->glintTemporal[2] = r_glints_historyClamp->value;
	ubo->glintTemporal[3] = r_glints_jitter->value;

	ubo->glintEnergy[0] = r_glints_energyComp->value;
	ubo->glintEnergy[1] = r_glints_fresnel->value;
	ubo->glintEnergy[2] = r_glints_f0Override->value;
	ubo->glintEnergy[3] = r_glints_fireflyFilter->value;

	ubo->glintBudget[0] = r_glints_maxLuminance->value;
	ubo->glintBudget[1] = r_glints_budgetMs->value;
	ubo->glintBudget[2] = r_glints_maxDistance->value;
	ubo->glintBudget[3] = r_glints_maxScreenSlope->value;

	ubo->glintRouting[0] = r_glints_affectIBL->value;
	ubo->glintRouting[1] = r_glints_affectDirect->value;
	ubo->glintRouting[2] = r_glints_shadowed->value;
	ubo->glintRouting[3] = r_glints_halfRes->value;

	ubo->glintModel[0] = r_glints_beta->value;
	ubo->glintModel[1] = r_glints_ax->value;
	ubo->glintModel[2] = r_glints_ay->value;
	ubo->glintModel[3] = r_glints_gamma->value;

	ubo->glintDensity[0] = r_glints_density->value;
	ubo->glintDensity[1] = r_glints_lnrho->value;
	ubo->glintDensity[2] = r_glints_zeta->value;
	ubo->glintDensity[3] = r_glints_maxCells->value;

	{
		int levels = r_glints_dict_levels ? r_glints_dict_levels->integer : GLINT_DICT_MAX_LEVELS;
		int size = r_glints_dict_size ? r_glints_dict_size->integer : GLINT_DICT_MAX_SIZE;
		int entries = r_glints_dict_count ? r_glints_dict_count->integer : GLINT_DICT_MAX_ENTRIES;
		float alpha = r_glints_dict_alpha ? r_glints_dict_alpha->value : 0.5f;

		if ( levels < 1 ) levels = 1;
		if ( levels > GLINT_DICT_MAX_LEVELS ) levels = GLINT_DICT_MAX_LEVELS;
		if ( size < 2 ) size = 2;
		if ( size > GLINT_DICT_MAX_SIZE ) size = GLINT_DICT_MAX_SIZE;
		if ( entries < 1 ) entries = 1;
		if ( entries > GLINT_DICT_MAX_ENTRIES ) entries = GLINT_DICT_MAX_ENTRIES;
		if ( alpha < 1e-4f ) alpha = 1e-4f;

		ubo->glintDict[0] = (float)levels;
		ubo->glintDict[1] = (float)size;
		ubo->glintDict[2] = (float)entries;
		ubo->glintDict[3] = alpha;
	}

	ubo->glintDictExtras[0] = r_glints_dict_lobeSigma->value;
	ubo->glintDictExtras[1] = r_glints_lod->value;
	ubo->glintDictExtras[2] = r_glints_masking->value;
	ubo->glintDictExtras[3] = r_glints_energy_debug->value;

	ubo->glintPerformance[0] = r_glints_lodBias->value;
	ubo->glintPerformance[1] = r_glints_colored->value;
	ubo->glintPerformance[2] = r_glints_colorCount->value;
	ubo->glintPerformance[3] = r_glints_colorStrength->value;

	ubo->glintColor[0] = r_glints_anisoRotation ? r_glints_anisoRotation->value : 0.0f;
	{
		float glintIntensityValue = ( r_glints_intensity ) ? r_glints_intensity->value : 1.0f;
		if ( glintIntensityValue < 0.0f )
			glintIntensityValue = 0.0f;
		ubo->glintColor[1] = glintIntensityValue;
	}
	ubo->glintColor[2] = 0.0f;
	ubo->glintColor[3] = 0.0f;

	// Log glint UBO values when they change (sanity check for cvar plumbing).
	if ( r_glints_verbose && r_glints_verbose->integer > 0 ) {
		static vkUniform_t lastGlintUbo;
		static qboolean lastValid = qfalse;
		const size_t glintBytes = (size_t)((char *)&ubo->glintColor + sizeof( ubo->glintColor ) - (char *)&ubo->glintCore);

		if ( !lastValid || memcmp( &lastGlintUbo.glintCore, &ubo->glintCore, glintBytes ) != 0 ) {
			ri.Printf( PRINT_ALL,
				"glint UBO:\n"
				"  core=(%.3f %.3f %.3f %.3f) material=(%.3f %.3f %.3f %.3f)\n"
				"  micro=(%.3f %.3f %.3f %.3f) sampling=(%.3f %.3f %.3f %.3f)\n"
				"  temporal=(%.3f %.3f %.3f %.3f) energy=(%.3f %.3f %.3f %.3f)\n"
				"  budget=(%.3f %.3f %.3f %.3f) routing=(%.3f %.3f %.3f %.3f)\n"
				"  model=(%.3f %.3f %.3f %.3f) density=(%.3f %.3f %.3f %.3f)\n"
				"  dict=(%.3f %.3f %.3f %.3f) dictX=(%.3f %.3f %.3f %.3f)\n"
				"  perf=(%.3f %.3f %.3f %.3f) color=(%.3f %.3f %.3f %.3f)\n",
				ubo->glintCore[0], ubo->glintCore[1], ubo->glintCore[2], ubo->glintCore[3],
				ubo->glintMaterial[0], ubo->glintMaterial[1], ubo->glintMaterial[2], ubo->glintMaterial[3],
				ubo->glintMicro[0], ubo->glintMicro[1], ubo->glintMicro[2], ubo->glintMicro[3],
				ubo->glintSampling[0], ubo->glintSampling[1], ubo->glintSampling[2], ubo->glintSampling[3],
				ubo->glintTemporal[0], ubo->glintTemporal[1], ubo->glintTemporal[2], ubo->glintTemporal[3],
				ubo->glintEnergy[0], ubo->glintEnergy[1], ubo->glintEnergy[2], ubo->glintEnergy[3],
				ubo->glintBudget[0], ubo->glintBudget[1], ubo->glintBudget[2], ubo->glintBudget[3],
				ubo->glintRouting[0], ubo->glintRouting[1], ubo->glintRouting[2], ubo->glintRouting[3],
				ubo->glintModel[0], ubo->glintModel[1], ubo->glintModel[2], ubo->glintModel[3],
				ubo->glintDensity[0], ubo->glintDensity[1], ubo->glintDensity[2], ubo->glintDensity[3],
				ubo->glintDict[0], ubo->glintDict[1], ubo->glintDict[2], ubo->glintDict[3],
				ubo->glintDictExtras[0], ubo->glintDictExtras[1], ubo->glintDictExtras[2], ubo->glintDictExtras[3],
				ubo->glintPerformance[0], ubo->glintPerformance[1], ubo->glintPerformance[2], ubo->glintPerformance[3],
				ubo->glintColor[0], ubo->glintColor[1], ubo->glintColor[2], ubo->glintColor[3] );

			Com_Memcpy( &lastGlintUbo.glintCore, &ubo->glintCore, glintBytes );
			lastValid = qtrue;
		}
	}
}
#endif


#ifdef USE_PMLIGHT
static void VK_SetLightParams( vkUniform_t *ubo, const dlight_t *dl ) {
	float radius;

#ifdef USE_VULKAN
	if ( !glConfig.deviceSupportsGamma && !vk.fboActive )
#else
	if ( !glConfig.deviceSupportsGamma )
#endif
		VectorScale( dl->color, 2 * powf( r_intensity->value, r_gamma->value ), ubo->light.color);
	else
		VectorCopy( dl->color, ubo->light.color );

	radius = dl->radius;

	// vertex data
	VectorCopy( backEnd.or.viewOrigin, ubo->eyePos ); ubo->eyePos[3] = 0.0f;
	VectorCopy( dl->transformed, ubo->light.pos ); ubo->light.pos[3] = 0.0f;

	// fragment data
	ubo->light.color[3] = 1.0f / Square( radius );

	if ( dl->linear )
	{
		vec4_t ab;
		VectorSubtract( dl->transformed2, dl->transformed, ab );
		ab[3] = 1.0f / DotProduct( ab, ab );
		Vector4Copy( ab, ubo->light.vector );
	}
}
#endif

uint32_t vk_append_uniform( const void *uniform_data, size_t size, uint32_t min_offset ) {
	const uint32_t offset = PAD(vk.cmd->vertex_buffer_offset, (VkDeviceSize)vk.uniform_alignment);

	if ( offset + min_offset > vk.geometry_buffer_size )
		return ~0U;

	Com_Memcpy( vk.cmd->vertex_buffer_ptr + offset, uniform_data, size );
	vk.cmd->vertex_buffer_offset = offset + min_offset;

	return offset;
}

static uint32_t vk_push_uniform_cached( const vkUniform_t *u )
{
	static VkCommandBuffer last_cmd_buf = VK_NULL_HANDLE;
	static uint32_t last_camera_offset = ~0U;
	static uint32_t last_uniform_offset = ~0U;
	static vkUniform_t last_uniform;

	// Reset cache when we move to a new command buffer.
	if ( vk.cmd == NULL || vk.cmd->command_buffer != last_cmd_buf ) {
		last_cmd_buf = ( vk.cmd != NULL ) ? vk.cmd->command_buffer : VK_NULL_HANDLE;
		last_camera_offset = ~0U;
		last_uniform_offset = ~0U;
		Com_Memset( &last_uniform, 0, sizeof( last_uniform ) );
	}

	if ( last_uniform_offset != ~0U &&
		last_camera_offset == vk.cmd->camera_ubo_offset &&
		memcmp( &last_uniform, u, sizeof( *u ) ) == 0 ) {
		return last_uniform_offset;
	}

	Com_Memcpy( &last_uniform, u, sizeof( *u ) );
	last_camera_offset = vk.cmd->camera_ubo_offset;
	last_uniform_offset = vk_push_uniform( u );

	return last_uniform_offset;
}

uint32_t vk_push_uniform( const vkUniform_t *ubo ) {
	const uint32_t offset = vk_append_uniform( ubo, sizeof(*ubo), (VkDeviceSize)vk.uniform_item_size );

	vk_reset_descriptor( VK_DESC_UNIFORM );
	vk_update_descriptor( VK_DESC_UNIFORM, vk.cmd->uniform_descriptor );
	vk_update_descriptor_offset( VK_DESC_UNIFORM_MAIN_BINDING, offset );
	vk_update_descriptor_offset( VK_DESC_UNIFORM_CAMERA_BINDING, vk.cmd->camera_ubo_offset );

	return offset;
}

#ifdef USE_PMLIGHT
void VK_LightingPass( void )
{
	static uint32_t uniform_offset;
	static int fog_stage;
	uint32_t pipeline;
	const shaderStage_t *pStage;
	cullType_t cull;
	int abs_light;

	if ( tess.shader->lightingStage < 0 )
		return;

	pStage = tess.xstages[ tess.shader->lightingStage ];

	// we may need to update programs for fog transitions
	if ( tess.dlightUpdateParams ) {

		// fog parameters
		VK_SetFogParams( &uniform, &fog_stage );
		// light parameters
		VK_SetLightParams( &uniform, tess.light );

		uniform_offset = vk_push_uniform( &uniform );

		tess.dlightUpdateParams = qfalse;
	}

	if ( uniform_offset == ~0U )
		return; // no space left...

	cull = tess.shader->cullType;
	if ( backEnd.viewParms.portalView == PV_MIRROR ) {
		switch ( cull ) {
			case CT_FRONT_SIDED: cull = CT_BACK_SIDED; break;
			case CT_BACK_SIDED: cull = CT_FRONT_SIDED; break;
			default: break;
		}
	}

	abs_light = /* (pStage->stateBits & GLS_ATEST_BITS) && */ (cull == CT_TWO_SIDED) ? 1 : 0;

	if ( fog_stage )
		vk_update_descriptor( VK_DESC_FOG_DLIGHT, tr.fogImage->descriptor );

	if ( tess.light->linear )
		pipeline = vk.dlight1_pipelines_x[cull][tess.shader->polygonOffset][fog_stage][abs_light];
	else
		pipeline = vk.dlight_pipelines_x[cull][tess.shader->polygonOffset][fog_stage][abs_light];

	GL_SelectTexture( 0 );
	R_BindAnimatedImage( &pStage->bundle[ tess.shader->lightingBundle ] );

#ifdef USE_VBO
	if ( tess.vboIndex == 0 )
#endif
	{
		R_ComputeTexCoords( tess.shader->lightingBundle, &pStage->bundle[ tess.shader->lightingBundle ] );
	}

	vk_bind_pipeline( pipeline );
	vk_bind_index();
	vk_bind_lighting( tess.shader->lightingStage, tess.shader->lightingBundle );
	vk_draw_geometry( tess.depthRange, qtrue );
}
#endif // USE_PMLIGHT

void RB_StageIteratorGeneric( void )
{
#ifdef USE_VULKAN
	qboolean rebindIndex = qfalse;
#endif
	qboolean fogCollapse = qfalse;
	qboolean worldShOverride;

#ifdef USE_VBO
	if ( tess.vboIndex != 0 ) {
		VBO_PrepareQueues();
		tess.vboStage = 0;
	} else
#endif
	RB_DeformTessGeometry();

#ifdef USE_PMLIGHT
	if ( tess.dlightPass ) {
		VK_LightingPass();
		return;
	}
#endif

#ifdef USE_FOG_COLLAPSE
	fogCollapse = tess.fogNum && tess.shader->fogPass && tess.shader->fogCollapse;
#endif
	worldShOverride = ( r_shDebugView && r_shDebugView->integer == 3 );

	// call shader function
	RB_IterateStagesGeneric( &tess, fogCollapse );

	// now do any dynamic lighting needed
#ifdef USE_LEGACY_DLIGHTS
#ifdef USE_PMLIGHT
	if ( r_dlightMode->integer == 0 )
#endif
	if ( !worldShOverride && tess.dlightBits && tess.shader->sort <= SS_OPAQUE && !(tess.shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY) ) ) {
		if ( !fogCollapse ) {
#ifdef USE_VULKAN
			rebindIndex = ProjectDlightTexture();
#else	
			ProjectDlightTexture();
#endif
		}
	}
#endif // USE_LEGACY_DLIGHTS

	// now do fog
	if ( !worldShOverride && tess.fogNum && tess.shader->fogPass && !fogCollapse ) {
#ifdef USE_VULKAN
		RB_FogPass( rebindIndex );
#else
		RB_FogPass();
#endif
	}
}

#else

/*
** RB_StageIteratorGeneric
*/
void RB_StageIteratorGeneric( void )
{
	const shaderCommands_t *input;
	shader_t		*shader;

	RB_DeformTessGeometry();

	input = &tess;
	shader = input->shader;

	//
	// set face culling appropriately
	//
	GL_Cull( shader->cullType );

	// set polygon offset if necessary
	if ( shader->polygonOffset )
	{
		qglEnable( GL_POLYGON_OFFSET_FILL );
		qglPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );
	}

	//
	// if there is only a single pass then we can enable color
	// and texture arrays before we compile, otherwise we need
	// to avoid compiling those arrays since they will change
	// during multipass rendering
	//
	if ( tess.numPasses > 1 )
	{
		setArraysOnce = qfalse;

		GL_ClientState( 1, CLS_NONE );
		GL_ClientState( 0, CLS_NONE );
	}
	else
	{
		// FIXME: we can't do that if going to lighting/fog later?
		setArraysOnce = qtrue;

		GL_ClientState( 0, CLS_COLOR_ARRAY | CLS_TEXCOORD_ARRAY );

		if ( tess.xstages[0] )
		{
			R_ComputeColors( 0, tess.svars.colors, tess.xstages[0] );
			qglColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.svars.colors[0].rgba );
			R_ComputeTexCoords( 0, &tess.xstages[0]->bundle[0] );
			qglTexCoordPointer( 2, GL_FLOAT, 0, tess.svars.texcoordPtr[0] );
			if ( shader->multitextureEnv )
			{
				GL_ClientState( 1, CLS_TEXCOORD_ARRAY );
				R_ComputeTexCoords( 1, &tess.xstages[0]->bundle[1] );
				qglTexCoordPointer( 2, GL_FLOAT, 0, tess.svars.texcoordPtr[1] );
			}
			else
			{
				GL_ClientState( 1, CLS_NONE );
			}
		}
	}

	qglVertexPointer( 3, GL_FLOAT, sizeof( input->xyz[0] ), input->xyz ); // padded for SIMD

	//
	// lock XYZ
	//
	if ( qglLockArraysEXT )
	{
		qglLockArraysEXT( 0, input->numVertexes );
	}

	//
	// call shader function
	//
	if ( r_shDebugView && r_shDebugView->integer == 3 ) {
		RB_DrawWorldSHDebugOverride();
	} else {
		RB_IterateStagesGeneric( input );
	}

	//
	// now do any dynamic lighting needed
	//
	if ( tess.dlightBits && tess.shader->sort <= SS_OPAQUE && !(tess.shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY) ) )
	{
		ProjectDlightTexture();
	}

	//
	// now do fog
	//
	if ( tess.fogNum && tess.shader->fogPass )
	{
		RB_FogPass();
	}

	//
	// unlock arrays
	//
	if ( qglUnlockArraysEXT )
	{
		qglUnlockArraysEXT();
	}

	GL_ClientState( 1, CLS_NONE );

	//
	// reset polygon offset
	//
	if ( shader->polygonOffset )
	{
		qglDisable( GL_POLYGON_OFFSET_FILL );
	}
}
#endif // !USE_VULKAN


/*
** RB_EndSurface
*/
void RB_EndSurface( void ) {
	const shaderCommands_t *input;

	input = &tess;

	if ( input->numIndexes == 0 ) {
		//VBO_UnBind();
		return;
	}

	if ( input->numIndexes > SHADER_MAX_INDEXES ) {
		ri.Error( ERR_DROP, "RB_EndSurface() - SHADER_MAX_INDEXES hit" );
	}

	if ( input->numVertexes > SHADER_MAX_VERTEXES ) {
		ri.Error( ERR_DROP, "RB_EndSurface() - SHADER_MAX_VERTEXES hit" );
	}

	if ( tess.shader == tr.shadowShader ) {
		RB_ShadowTessEnd();
		return;
	}

	// for debugging of sort order issues, stop rendering after a given sort value
	if ( r_debugSort->integer && r_debugSort->integer < tess.shader->sort && !backEnd.doneSurfaces ) {
#ifdef USE_VBO
		tess.vboIndex = 0; //VBO_UnBind();
#endif
		return;
	}

	//
	// update performance counters
	//
#ifdef USE_PMLIGHT
	if ( tess.dlightPass ) {
		backEnd.pc.c_lit_batches++;
		backEnd.pc.c_lit_vertices += tess.numVertexes;
		backEnd.pc.c_lit_indices += tess.numIndexes;
	} else
#endif
	{
		backEnd.pc.c_shaders++;
		backEnd.pc.c_vertexes += tess.numVertexes;
		backEnd.pc.c_indexes += tess.numIndexes;
	}
	backEnd.pc.c_totalIndexes += tess.numIndexes * tess.numPasses;

	//
	// call off to shader specific tess end function
	//
	tess.shader->optimalStageIteratorFunc();

	//
	// draw debugging stuff
	//
	if ( r_showtris->integer ) {
		DrawTris( input );
	}
	if ( r_shownormals->integer ) {
		DrawNormals( input );
	}

	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;
	tess.numVertexes = 0;

#ifdef USE_VBO
	tess.vboIndex = 0;
	//VBO_ClearQueue();
#endif
}
