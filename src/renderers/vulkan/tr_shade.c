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

#include "tr_local.h"

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
	const shaderStage_t *pStage;

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
	vec4_t advancedParams;
	vec4_t glintParams0;
	vec4_t glintParams1;
	vec4_t glintFlags;
	vec4_t shCoeffs[9];
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

static inline void vk_update_descriptor_if_changed_with_image( int index, VkDescriptorSet descriptor, const image_t *image )
{
	vk_update_descriptor_if_changed( index, descriptor );
	if ( vk.cmd ) {
		vk.cmd->descriptor_set.image[ index ] = image;
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
static inline float LerpClamp( float value, float minValue, float maxValue )
{
	if ( value < minValue ) {
		return minValue;
	}
	if ( value > maxValue ) {
		return maxValue;
	}
	return value;
}

#ifdef USE_VULKAN
static void RB_IterateStagesGeneric( const shaderCommands_t *input, qboolean fogCollapse )
#else
static void RB_IterateStagesGeneric( const shaderCommands_t *input )
#endif
{
	const shaderStage_t *pStage;
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

	vk_bind_index();

	tess_flags = input->shader->tessFlags;

	pushUniform = qfalse;

	is_pbr_surface = qfalse;

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
	uniform.pbrDebugMode[0] = pbr_debug;
	uniform.pbrDebugMode[1] = 0.0f;
	uniform.pbrDebugMode[2] = 0.0f;
	uniform.pbrDebugMode[3] = 0.0f;

	if ( is_pbr_surface ) {
		Com_Memcpy( &uniform_camera.modelMatrix, backEnd.or.modelMatrix, sizeof(float) * 16 );
		Com_Memcpy( &uniform_camera.viewOrigin, backEnd.refdef.vieworg, sizeof( vec3_t) );
		uniform_camera.viewOrigin[3] = 0.0;

		vk.cmd->camera_ubo_offset = vk_append_uniform( &uniform_camera, sizeof(uniform_camera), vk.uniform_camera_item_size );

		pushUniform = qtrue;
	}
#endif
#endif // USE_VULKAN

	for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ )
	{
		pStage = tess.xstages[ stage ];
		if ( !pStage )
			break;

#ifdef USE_VBO
		tess.vboStage = stage;
#endif

#ifdef USE_VULKAN
		tess_flags |= pStage->tessFlags;

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

		if ( pushUniform ) {
			pushUniform = qfalse;
			vk_push_uniform_cached( &uniform );
		}

		GL_SelectTexture( 0 );

		if ( r_lightmap->integer && pStage->bundle[1].lightmap != LIGHTMAP_INDEX_NONE ) {
			//GL_SelectTexture( 0 );
			GL_Bind( tr.whiteImage ); // replace diffuse texture with a white one thus effectively render only lightmap
		}

		if ( backEnd.viewParms.portalView == PV_MIRROR ) {
			pipeline = pStage->vk_mirror_pipeline[fog_stage];
		} else {
			pipeline = pStage->vk_pipeline[fog_stage];
		}

#ifdef USE_VK_PBR
		Vk_Pipeline_Def	def;
		vk_get_pipeline_def( pipeline, &def );

		if ( is_pbr_surface && pStage->vk_pbr_flags ) {
			static VkCommandBuffer lastCmdBuf = VK_NULL_HANDLE;
			static qboolean lastValid = qfalse;
			static vkPbrUniformBlock_t lastBlock;
			static qboolean warnedMissingBrdfLut = qfalse;
			static qboolean warnedMissingEmptyCubemap = qfalse;
			static qboolean warnedMissingMapCubemapData = qfalse;

			vkPbrUniformBlock_t block;
			Vector4Copy( pStage->emissiveScale, block.emissiveScale );
			Vector4Copy( pStage->clearcoatScale, block.clearcoatScale );
			Vector4Copy( pStage->sheenScale, block.sheenScale );
			Vector4Copy( pStage->anisotropyScale, block.anisotropyScale );
			Vector4Copy( pStage->transmissionScale, block.transmissionScale );
			Vector4Copy( pStage->subsurfaceColor, block.subsurfaceColor );
			Vector4Copy( pStage->subsurfaceParams, block.subsurfaceParams );
			Vector4Set( block.advancedParams,
				( r_pbr_multiScatter && r_pbr_multiScatter->integer ) ? 1.0f : 0.0f,
				( r_pbr_multiScatterStrength ? r_pbr_multiScatterStrength->value : 1.0f ),
				0.0f, 0.0f );

			float glintDensityExp = r_glintDensity ? LerpClamp( r_glintDensity->value, -4.0f, 6.0f ) : 3.0f;
			float glintDensity = 1000.0f * powf( 10.0f, glintDensityExp );
			float glintMicro = r_glintMicrofacetRoughness ? LerpClamp( r_glintMicrofacetRoughness->value, 0.001f, 0.1f ) : 0.01f;
			float glintPixel = r_glintPixelFilterSize ? LerpClamp( r_glintPixelFilterSize->value, 0.5f, 1.2f ) : 0.7f;
			int glintBudget = r_glintSampleBudget ? (int) LerpClamp( (float)r_glintSampleBudget->integer, 0.0f, 2.0f ) : 1;
			float glintMaxLod = r_glintMaxLodClamp ? LerpClamp( r_glintMaxLodClamp->value, 0.0f, 16.0f ) : 12.0f;
			float glintDMax = r_glintDMax ? LerpClamp( r_glintDMax->value, 1.0f, 1000000.0f ) : 1000.0f;
			float glintLo = r_glintRoughnessLo ? LerpClamp( r_glintRoughnessLo->value, 0.0f, 0.5f ) : 0.02f;
			float glintHi = r_glintRoughnessHi ? LerpClamp( r_glintRoughnessHi->value, 0.0f, 0.6f ) : 0.15f;
			if ( glintHi < glintLo + 0.001f ) {
				glintHi = glintLo + 0.001f;
			}

			Vector4Set( block.glintParams0, glintDensity, glintMicro, glintPixel, (float)glintBudget );
			Vector4Set( block.glintParams1, glintMaxLod, glintDMax, glintLo, glintHi );
			Vector4Set( block.glintFlags,
				( r_glint && r_glint->integer ) ? 1.0f : 0.0f,
				( r_glintMode && r_glintMode->integer ) ? 1.0f : 0.0f,
				0.0f, 0.0f );

			{
				const VkDescriptorSet fallback2D = ( tr.whiteImage ) ? tr.whiteImage->descriptor : VK_NULL_HANDLE;
				const VkDescriptorSet fallbackCube = ( tr.emptyCubemap ) ? tr.emptyCubemap->descriptor : VK_NULL_HANDLE;
				VkDescriptorSet brdfDescriptor = vk.brdflut_image_descriptor ? vk.brdflut_image_descriptor : fallback2D;
				VkDescriptorSet envDescriptor = fallbackCube;
				VkDescriptorSet irradianceDescriptor = fallbackCube;

				if ( !vk.brdflut_image_descriptor && !warnedMissingBrdfLut ) {
					ri.Printf( PRINT_WARNING, "PBR IBL: BRDF LUT descriptor unavailable, using fallback texture\n" );
					warnedMissingBrdfLut = qtrue;
				}
				if ( !fallbackCube && !warnedMissingEmptyCubemap ) {
					ri.Printf( PRINT_WARNING, "PBR IBL: empty cubemap fallback missing\n" );
					warnedMissingEmptyCubemap = qtrue;
				}

				const cubemap_t *cube = NULL;
				int cubemapIndex = -1;
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
					if ( cubemapIndex >= 0 ) {
						cube = &tr.cubemaps[cubemapIndex];
					}

					if ( cube ) {
						if ( cube->prefiltered_image ) {
							envDescriptor = cube->prefiltered_image->descriptor;
						}
						if ( cube->irradiance_image ) {
							irradianceDescriptor = cube->irradiance_image->descriptor;
						}
						if ( !cube->prefiltered_image || !cube->irradiance_image ) {
							if ( !warnedMissingMapCubemapData ) {
								ri.Printf( PRINT_WARNING, "PBR IBL: cubemap '%s' missing prefiltered/irradiance image, using fallback where needed\n",
									cube->name[0] ? cube->name : "<unnamed>" );
								warnedMissingMapCubemapData = qtrue;
							}
						}

						// Prefer cubemap SH coefficients when present, otherwise fall back to stage SH.
						if ( cube->hasSHCoeffs ) {
							Com_Memcpy( block.shCoeffs, cube->shCoeffs, sizeof( block.shCoeffs ) );
						} else {
							Com_Memcpy( block.shCoeffs, pStage->shCoeffs, sizeof( block.shCoeffs ) );
						}
					} else {
						Com_Memcpy( block.shCoeffs, pStage->shCoeffs, sizeof( block.shCoeffs ) );
					}
				}

				if ( brdfDescriptor ) {
					vk_update_descriptor_if_changed( VK_DESC_PBR_BRDFLUT, brdfDescriptor );
				}
				if ( envDescriptor ) {
					vk_update_descriptor_if_changed_with_image( VK_DESC_PBR_CUBEMAP, envDescriptor, cube ? cube->prefiltered_image : NULL );
				}
				if ( irradianceDescriptor ) {
					vk_update_descriptor_if_changed_with_image( VK_DESC_PBR_IRRADIANCE, irradianceDescriptor, cube ? cube->irradiance_image : NULL );
				}
			}
				
			if ( pStage->vk_pbr_flags & PBR_HAS_NORMALMAP )
				vk_update_descriptor_if_changed_with_image( VK_DESC_PBR_NORMAL, pStage->normalMap->descriptor, pStage->normalMap );

			if ( pStage->vk_pbr_flags & PBR_HAS_PHYSICALMAP || pStage->vk_pbr_flags & PBR_HAS_SPECULARMAP )
				vk_update_descriptor_if_changed_with_image( VK_DESC_PBR_PHYSICAL, pStage->physicalMap->descriptor, pStage->physicalMap );
			
			// Commented out descriptor updates for removed PBR features
			// if ( pStage->vk_pbr_flags & PBR_HAS_EMISSIVE )
			// 	vk_update_descriptor_if_changed( VK_DESC_PBR_EMISSIVE, pStage->emissiveMap->descriptor );

			// if ( pStage->vk_pbr_flags & PBR_HAS_CLEARCOAT )
			// 	vk_update_descriptor_if_changed( VK_DESC_PBR_CLEARCOAT, pStage->clearcoatMap->descriptor );

			// if ( pStage->vk_pbr_flags & PBR_HAS_SHEEN )
			// 	vk_update_descriptor_if_changed( VK_DESC_PBR_SHEEN, pStage->sheenMap->descriptor );

			// if ( pStage->vk_pbr_flags & PBR_HAS_ANISOTROPY )
			// 	vk_update_descriptor_if_changed( VK_DESC_PBR_ANISOTROPY, pStage->anisotropyMap->descriptor );

			// if ( pStage->vk_pbr_flags & PBR_HAS_TRANSMISSION )
			// 	vk_update_descriptor_if_changed( VK_DESC_PBR_TRANSMISSION, pStage->transmissionMap->descriptor );

			// if ( pStage->vk_pbr_flags & PBR_HAS_SUBSURFACE )
			// 	vk_update_descriptor_if_changed( VK_DESC_PBR_SUBSURFACE, pStage->subsurfaceMap->descriptor );

			// Only push uniforms when the PBR block has actually changed for this command buffer.
			if ( vk.cmd && vk.cmd->command_buffer != lastCmdBuf ) {
				lastCmdBuf = vk.cmd->command_buffer;
				lastValid = qfalse;
			}
			if ( !lastValid || memcmp( &lastBlock, &block, sizeof( block ) ) != 0 ) {
				lastBlock = block;
				lastValid = qtrue;

				Vector4Copy( block.emissiveScale, uniform.pbrEmissiveScale );
				Vector4Copy( block.clearcoatScale, uniform.pbrClearcoatScale );
				Vector4Copy( block.sheenScale, uniform.pbrSheenScale );
				Vector4Copy( block.anisotropyScale, uniform.pbrAnisotropyScale );
				Vector4Copy( block.transmissionScale, uniform.pbrTransmissionScale );
				Vector4Copy( block.subsurfaceColor, uniform.pbrSubsurfaceColor );
				Vector4Copy( block.subsurfaceParams, uniform.pbrSubsurfaceParams );
				Vector4Copy( block.advancedParams, uniform.pbrAdvancedParams );
				Vector4Copy( block.glintParams0, uniform.pbrGlintParams0 );
				Vector4Copy( block.glintParams1, uniform.pbrGlintParams1 );
				Vector4Copy( block.glintFlags, uniform.pbrGlintFlags );
				Com_Memcpy( uniform.pbrShCoeffs, block.shCoeffs, sizeof( uniform.pbrShCoeffs ) );

				vk_push_uniform_cached( &uniform );
			}

			#ifdef HDR_DELUXE_LIGHTMAP
				// aparently lightmap is not always in bundle 1 ..
				// should probably fix this in collapseMuklitexture
				if ( def.vk_pbr_flags & PBR_HAS_DELUXEMAP0 )
					vk_update_descriptor(  VK_DESC_PBR_DELUXE, pStage->bundle[0].deluxeMap->descriptor );

				if ( def.vk_pbr_flags & PBR_HAS_DELUXEMAP1 )
					vk_update_descriptor(  VK_DESC_PBR_DELUXE, pStage->bundle[1].deluxeMap->descriptor );

				else if ( !(def.vk_pbr_flags & PBR_HAS_DELUXEMAP0) )
					vk_update_descriptor(  VK_DESC_PBR_DELUXE, tr.whiteImage->descriptor );
				}
			#endif
#endif

#ifdef USE_VK_PBR
		if ( !is_pbr_surface && pStage->vk_pbr_flags ) {
			def.vk_pbr_flags = 0;
			pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
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
