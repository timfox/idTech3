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
#include "vk_util.h"
#include "tr_material_paint.h"
#include "vk_deferred_gbuffer.h"
#include "vk_deferred_honesty.h"
#include "vk_render_path.h"
#include "vk_meshlets.h"
#include "vk_selective_sun_shadow.h"
#include "vk_selective_reflection.h"
#include "vk_sun_csm.h"
#include "vk_day_night.h"
#include "vk_surface_evolution.h"
#include "vk_frequency_aware.h"
#include "vk_bsp_viz.h"
#include "../common/tr_vector_font.h"
#include "vk_postfx.h"

static color4ub_t RB_TintedFogColor( const fog_t *fog ) {
	color4ub_t color;
	vec3_t tint;
	float alpha;

	color = fog->colorInt;
	if ( !r_fogTint || !r_fogTint->string[0] ) {
		return color;
	}
	if ( !vk_parse_fog_tint_string( r_fogTint->string, tint ) ) {
		return color;
	}

	alpha = (float)color.rgba[3];
	color.rgba[0] = (byte)Com_Clamp( 0, 255, (int)( (float)color.rgba[0] * tint[0] ) );
	color.rgba[1] = (byte)Com_Clamp( 0, 255, (int)( (float)color.rgba[1] * tint[1] ) );
	color.rgba[2] = (byte)Com_Clamp( 0, 255, (int)( (float)color.rgba[2] * tint[2] ) );
	color.rgba[3] = (byte)alpha;

	return color;
}
#include "vk_skybox_hdr.h"
#include "vk_shadow_contract.h"

extern cvar_t *r_shLighting;
extern cvar_t *r_shWorldLighting;
extern cvar_t *r_shDebugView;

static void VK_FillPbrSunShadowUniform( vkUniform_t *ubo ) {
	int i;
	int c;
	int cascades;
	int grid;

	if ( !ubo ) {
		return;
	}

	for ( i = 0; i < 4; i++ ) {
		Vector4Set( ubo->pbrSunShadowRows[i], 0.0f, 0.0f, 0.0f, 0.0f );
	}
	for ( i = 0; i < 12; i++ ) {
		Vector4Set( ubo->pbrSunShadowCascadeRows[i], 0.0f, 0.0f, 0.0f, 0.0f );
	}
	Vector4Set( ubo->pbrSunShadowParams, 0.0f, 0.0f, 0.0f, 0.0f );
	Vector4Set( ubo->pbrSunShadowSplits, 0.0f, 0.0f, 0.0f, 0.0f );
	Vector4Set( ubo->pbrSunShadowMeta, 1.0f, 1.0f, 0.0f, 4.0f );

	if ( !r_pbrSunShadow || !r_pbrSunShadow->integer || !vk.sun_shadow_valid || R_ClassicLightingActive() ) {
		return;
	}
	if ( vk_shs_rt_owns_sun() ) {
		return;
	}

	cascades = (int)vk.sun_shadow_cascade_count;
	if ( cascades < 1 ) {
		cascades = 1;
	}
	if ( cascades > 4 ) {
		cascades = 4;
	}
	grid = ( cascades <= 1 ) ? 1 : 2;

	for ( i = 0; i < 4; i++ ) {
		ubo->pbrSunShadowRows[i][0] = vk.sun_shadow_matrix[0][i * 4 + 0];
		ubo->pbrSunShadowRows[i][1] = vk.sun_shadow_matrix[0][i * 4 + 1];
		ubo->pbrSunShadowRows[i][2] = vk.sun_shadow_matrix[0][i * 4 + 2];
		ubo->pbrSunShadowRows[i][3] = vk.sun_shadow_matrix[0][i * 4 + 3];
	}
	for ( c = 1; c < cascades; c++ ) {
		for ( i = 0; i < 4; i++ ) {
			int dst = ( c - 1 ) * 4 + i;
			ubo->pbrSunShadowCascadeRows[dst][0] = vk.sun_shadow_matrix[c][i * 4 + 0];
			ubo->pbrSunShadowCascadeRows[dst][1] = vk.sun_shadow_matrix[c][i * 4 + 1];
			ubo->pbrSunShadowCascadeRows[dst][2] = vk.sun_shadow_matrix[c][i * 4 + 2];
			ubo->pbrSunShadowCascadeRows[dst][3] = vk.sun_shadow_matrix[c][i * 4 + 3];
		}
	}

	ubo->pbrSunShadowParams[0] = ( r_fogShadowBias ) ? r_fogShadowBias->value : 0.001f;
	ubo->pbrSunShadowParams[1] = ( r_fogShadowPcfRadius ) ? r_fogShadowPcfRadius->value : 1.0f;
	ubo->pbrSunShadowParams[2] = 1.0f;
	ubo->pbrSunShadowParams[3] = ( ( r_pbrSunShadowStrength ) ?
		Com_Clamp( 0.0f, 1.0f, r_pbrSunShadowStrength->value ) : 1.0f ) *
		vk_day_night_shadow_factor();

	Vector4Set( ubo->pbrSunShadowSplits,
		vk.sun_shadow_splits[0],
		vk.sun_shadow_splits[1],
		vk.sun_shadow_splits[2],
		vk.sun_shadow_splits[3] );
	Vector4Set( ubo->pbrSunShadowMeta,
		(float)cascades,
		1.0f / (float)grid,
		VK_SunCSM_CascadeBlend(),
		vk.sun_shadow_near > 0.0f ? vk.sun_shadow_near : 4.0f );
	vk_shadow_contract_note_consumer( 0, "forward_plus" );
}

static qboolean R_StageHasLightmap( const shaderStage_t *pStage ) {
	return ( pStage->bundle[0].lightmap != LIGHTMAP_INDEX_NONE || pStage->bundle[1].lightmap != LIGHTMAP_INDEX_NONE );
}

static qboolean R_StageUsesWorldSH( const shaderStage_t *pStage ) {
	if ( pStage->materialBlend || ( pStage->vk_pbr_flags & PBR_HAS_MATERIAL_BLEND ) ) {
		return qfalse;
	}
	if ( R_StageHasLightmap( pStage ) ) {
		return qtrue;
	}
	if ( pStage->bundle[0].rgbGen == CGEN_LIGHTING_DIFFUSE || pStage->bundle[0].rgbGen == CGEN_IDENTITY_LIGHTING ) {
		return qtrue;
	}
	return qfalse;
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
			(void)R_WorldSHVertexColor( tess.xyz[v], tess.normal[v], tess.svars.colors[0][v].rgba );
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


/*
=============================================================

SURFACE SHADERS

=============================================================
*/

shaderCommands_t	tess;

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

	if ( bundle->isScreenMap ) {
		/*
		 * Match the legacy renderer's screenMap behavior: only sample the captured
		 * screen texture for the primary in-world scene once the dedicated capture
		 * pass has completed. Any other scene/pass falls back to black so we do not
		 * feed stale or recursive screen contents back into reflective materials.
		 */
		if ( backEnd.viewParms.frameSceneNum != 1 ||
			vk.renderPassIndex == RENDER_PASS_SCREENMAP ||
			!backEnd.screenMapDone ) {
			GL_Bind( tr.blackImage );
		} else {
			vk_update_descriptor( glState.currenttmu + VK_DESC_TEXTURE_BASE, vk.screenMap.color_descriptor );
		}
		return;
	}

	if ( bundle->numImageAnimations <= 1 ) {
		if ( tess.vectorCurveCount > 0 ) {
			image_t *curve = R_VectorFont_GetCurveImage();
			GL_Bind( curve ? curve : bundle->image[0] );
		} else {
			GL_Bind( bundle->image[0] );
		}
		return;
	}

	// RF_ANIMFRAME: caller drives the frame via refEntity->frame instead of by time.
	if ( backEnd.currentEntity && ( backEnd.currentEntity->e.renderfx & RF_ANIMFRAME ) ) {
		index = backEnd.currentEntity->e.frame;
		if ( index < 0 ) {
			index = 0;
		}
		index %= bundle->numImageAnimations;
		GL_Bind( bundle->image[ index ] );
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
	uint32_t pipeline;
	Vk_Depth_Range depthRange;
	const int showtrisMode = vk_bsp_viz_effective_showtris();

	if ( showtrisMode == 1 && backEnd.drawConsole )
		return;

	if ( tess.numIndexes == 0 )
		return;

	if ( r_fastsky->integer && input->shader->isSky )
		return;

	if ( tess.vboIndex ) {
		if ( tess.dlightPass )
			pipeline = backEnd.viewParms.portalView == PV_MIRROR ? vk.tris_mirror_debug_red_pipeline : vk.tris_debug_red_pipeline;
		else
			pipeline = backEnd.viewParms.portalView == PV_MIRROR ? vk.tris_mirror_debug_green_pipeline : vk.tris_debug_green_pipeline;
	} else
	{
		if ( tess.dlightPass )
			pipeline = backEnd.viewParms.portalView == PV_MIRROR ? vk.tris_mirror_debug_red_pipeline : vk.tris_debug_red_pipeline;
		else
			pipeline = backEnd.viewParms.portalView == PV_MIRROR ? vk.tris_mirror_debug_pipeline : vk.tris_debug_pipeline;
	}

	vk_bind_pipeline( pipeline );
	/*
	 * Mode 1 is a visible-surface visualization: retain the submitted
	 * geometry's real depth so opaque foreground surfaces occlude it.
	 * DEPTH_RANGE_ZERO maps to 1.0 in the Vulkan reversed-Z viewport and
	 * therefore makes every line pass as nearest geometry.  Preserve that
	 * old behavior only as the explicit through-wall developer mode 2
	 * (r_showtris 2, r_bspViz 4, or r_bspVizThroughWalls 1).
	 *
	 * DrawTris pipelines do not write depth; the final qfalse also prevents
	 * the draw helper from treating this diagnostic as a depth-writing draw.
	 */
	depthRange = ( showtrisMode == 2 ) ? DEPTH_RANGE_ZERO : DEPTH_RANGE_NORMAL;
	vk_draw_geometry( depthRange, qfalse );

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
	if ( tess.vboIndex )
		return; // must be handled specially

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
	/*
	 * Default normals overlay is depth-tested (visible-only). Through-wall
	 * normals remain available via r_shownormals 2 or r_bspVizThroughWalls.
	 */
	{
		const Vk_Depth_Range nr =
			( r_shownormals->integer >= 2 || vk_bsp_viz_want_through_walls() )
				? DEPTH_RANGE_ZERO
				: DEPTH_RANGE_NORMAL;
		vk_draw_geometry( nr, qtrue );
	}
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

	if ( shader->isStaticShader && !shader->remappedShader ) {
		tess.allowVBO = qtrue;
	} else {
		tess.allowVBO = qfalse;
	}

	if ( backEnd.currentEntity == &tr.worldEntity &&
		( ( r_shWorldLighting && r_shWorldLighting->integer ) ||
		( r_shDebugView && r_shDebugView->integer ) ) && !R_ClassicLightingActive() ) {
		tess.allowVBO = qfalse;
	}

	if ( shader->remappedShader ) {
		state = shader->remappedShader;
	} else {
		state = shader;
	}

	if ( tess.fogNum != fogNum ) {
		tess.dlightUpdateParams = qtrue;
	}



	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.sdfUiEdge = -1.0f;
	R_Meshlets_BeginSurface();
	tess.subpixelShift = -1.0f;
	tess.subpixelInvTexWidth = 0.0f;
	tess.vectorCurveStart = 0;
	tess.vectorCurveCount = 0;
	tess.vectorCurveTexWidth = 0;
	tess.shader = state;
	tess.fogNum = fogNum;
	tess.gltfDrawSurface = NULL;
	tess.gltfUseGpuPipeline = qfalse;
	tess.gltfGpuMorphActive = qfalse;
	tess.gltfGpuMorphCount = 0;
	vk_reset_iqm_storage_offsets();
	R_IQMBeginSurfaceBatch();

	tess.dlightBits = 0;		// will be OR'd in by surface functions
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


/*
===================
ProjectDlightTexture

Perform dynamic lighting with another rendering pass
===================
*/
static qboolean ProjectDlightTexture( void ) {
	int		i, l;
	vec3_t	origin;
	float	*texCoords;
	byte	*colors;
	byte	clipBits[SHADER_MAX_VERTEXES];
	uint32_t pipeline;
	qboolean rebindIndex = qfalse;
	glIndex_t hitIndexes[SHADER_MAX_INDEXES];
	int		numIndexes;
	float	scale;
	float	radius;
	float	modulate = 0.0f;
	const dlight_t *dl;

	if ( !backEnd.refdef.num_dlights ) {
		return rebindIndex;
	}

	for ( l = 0 ; l < (int)R_NumSurfaceDlights( backEnd.refdef.num_dlights ); l++ ) {

		if ( !( tess.dlightBits & ( 1 << l ) ) ) {
			continue;	// this surface definitely doesn't have any of this light
		}

		texCoords = (float*)&tess.svars.texcoords[0][0];
		tess.svars.texcoordPtr[0] = tess.svars.texcoords[0];
		colors = tess.svars.colors[0][0].rgba;

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


		GL_Bind( tr.dlightImage );

		if ( numIndexes != tess.numIndexes ) {
			// re-bind index buffer for later fog pass
			rebindIndex = qtrue;
		}
		pipeline = vk.dlight_pipelines[dl->additive > 0 ? 1 : 0][tess.shader->cullType][tess.shader->polygonOffset];
		vk_bind_pipeline( pipeline );
		vk_bind_index_ext( numIndexes, hitIndexes );
		vk_bind_geometry( TESS_RGBA0 | TESS_ST0 );
		vk_draw_geometry( DEPTH_RANGE_NORMAL, qtrue );
		backEnd.pc.c_totalIndexes += numIndexes;
		backEnd.pc.c_dlightIndexes += numIndexes;
	}

	return rebindIndex;
}


uint32_t vk_append_uniform( const void *uniform, size_t size, uint32_t min_offset );
uint32_t vk_push_uniform( const vkUniform_t *uniform );
static uint32_t vk_push_uniform_cached( const vkUniform_t *u );
void VK_SetFogParams( vkUniform_t *uniform, int *fogStage );
static vkUniform_t uniform;
static vkUniformCamera_t uniform_camera;

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
	vec4_t parallaxParams;
	vec4_t materialBlend;
} vkPbrUniformBlock_t;

/* Once-per-map bind diagnostics (reset on world load / vid_restart). */
static qboolean tr_pbr_bindLogPrinted = qfalse;

void R_PBR_ResetBindLog( void )
{
	tr_pbr_bindLogPrinted = qfalse;
}

static qboolean R_PBR_ShouldLogBindings( void )
{
	if ( r_pbr_bindlog && r_pbr_bindlog->integer ) {
		return qtrue;
	}
	if ( r_pbr_debug && r_pbr_debug->integer >= 17 ) {
		return qtrue;
	}
	return qfalse;
}

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

/*
===================
RB_FogPass

Blends a fog texture on top of everything else
===================
*/
static void RB_FogPass( qboolean rebindIndex ) {
	uint32_t pipeline = vk.fog_pipelines[tess.shader->fogPass - 1][tess.shader->cullType][tess.shader->polygonOffset];
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
}


/*
===============
R_ComputeColors
===============
*/
void R_ComputeColors( const int b, color4ub_t *dest, const shaderStage_t *pStage )
{
	int		i;
	const float identityLight = backEnd.projection2D ? 1.0f : tr.identityLight;
	const byte identityLightByte = backEnd.projection2D ? 255 : tr.identityLightByte;

	if ( tess.numVertexes == 0 )
		return;

	if ( backEnd.currentEntity == &tr.worldEntity && R_StageUsesWorldSH( pStage ) &&
		!R_ClassicLightingActive() &&
		( ( r_shDebugView && r_shDebugView->integer ) ||
		( r_shWorldLighting && r_shWorldLighting->integer && r_shLighting && r_shLighting->integer ) ) ) {
		int v;
		for ( v = 0; v < tess.numVertexes; v++ ) {
			vec3_t shCoeffs[SH_COEFF_COUNT];

			if ( r_shDebugView && r_shDebugView->integer == 2 ) {
				qboolean hasSH = R_SampleLightGridSH( tr.world, tess.xyz[v], shCoeffs );
				if ( hasSH ) {
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
					dest[v].rgba[0] = 255;
					dest[v].rgba[1] = 0;
					dest[v].rgba[2] = 255;
				}
				dest[v].rgba[3] = 255;
				continue;
			}

			if ( r_shDebugView && r_shDebugView->integer == 1 ) {
				vec3_t shLight;
				if ( R_SampleLightGridSH( tr.world, tess.xyz[v], shCoeffs ) ) {
					R_EvalSH9_RGB( shCoeffs, tess.normal[v], shLight );
					dest[v].rgba[0] = (byte)Com_Clamp( 0.0f, 255.0f, shLight[0] );
					dest[v].rgba[1] = (byte)Com_Clamp( 0.0f, 255.0f, shLight[1] );
					dest[v].rgba[2] = (byte)Com_Clamp( 0.0f, 255.0f, shLight[2] );
				} else {
					dest[v].rgba[0] = 255;
					dest[v].rgba[1] = 0;
					dest[v].rgba[2] = 255;
				}
				dest[v].rgba[3] = 255;
				continue;
			}

			(void)R_WorldSHVertexColor( tess.xyz[v], tess.normal[v], dest[v].rgba );
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
			Com_Memset( dest, identityLightByte, tess.numVertexes * 4 );
			break;
		case CGEN_LIGHTING_DIFFUSE:
			RB_CalcDiffuseColor( ( unsigned char * ) dest );
			break;
		case CGEN_EXACT_VERTEX:
			if ( b == 1 && R_MaterialPaint_HasStream2() ) {
				Com_Memcpy( dest, tess.vertexColors1, tess.numVertexes * sizeof( tess.vertexColors1[0] ) );
			} else {
				Com_Memcpy( dest, tess.vertexColors, tess.numVertexes * sizeof( tess.vertexColors[0] ) );
			}
			break;
		case CGEN_CONST:
			for ( i = 0; i < tess.numVertexes; i++ ) {
				dest[i] = pStage->bundle[b].constantColor;
			}
			break;
		case CGEN_VERTEX:
			if ( identityLight == 1 )
			{
				Com_Memcpy( dest, tess.vertexColors, tess.numVertexes * sizeof( tess.vertexColors[0] ) );
			}
			else
			{
				for ( i = 0; i < tess.numVertexes; i++ )
				{
					dest[i].rgba[0] = tess.vertexColors[i].rgba[0] * identityLight;
					dest[i].rgba[1] = tess.vertexColors[i].rgba[1] * identityLight;
					dest[i].rgba[2] = tess.vertexColors[i].rgba[2] * identityLight;
					dest[i].rgba[3] = tess.vertexColors[i].rgba[3];
				}
			}
			break;
		case CGEN_ONE_MINUS_VERTEX:
			if ( identityLight == 1 )
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
					dest[i].rgba[0] = ( 255 - tess.vertexColors[i].rgba[0] ) * identityLight;
					dest[i].rgba[1] = ( 255 - tess.vertexColors[i].rgba[1] ) * identityLight;
					dest[i].rgba[2] = ( 255 - tess.vertexColors[i].rgba[2] ) * identityLight;
				}
			}
			break;
		case CGEN_FOG:
			{
				const fog_t *fog = tr.world->fogs + tess.fogNum;
				const color4ub_t fogColor = RB_TintedFogColor( fog );

				for ( i = 0; i < tess.numVertexes; i++ ) {
					dest[i] = fogColor;
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
		if ( ( pStage->bundle[b].rgbGen == CGEN_VERTEX && identityLight != 1 ) ||
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

static qboolean R_CubemapPbrIblReady( const cubemap_t *cube )
{
	if ( !cube || !cube->prefiltered_image || !cube->irradiance_image ) {
		return qfalse;
	}

	if ( cube->prefiltered_image->handle == VK_NULL_HANDLE ||
		cube->prefiltered_image->view == VK_NULL_HANDLE ||
		cube->prefiltered_image->descriptor == VK_NULL_HANDLE ) {
		return qfalse;
	}

	if ( cube->irradiance_image->handle == VK_NULL_HANDLE ||
		cube->irradiance_image->view == VK_NULL_HANDLE ||
		cube->irradiance_image->descriptor == VK_NULL_HANDLE ) {
		return qfalse;
	}

	return qtrue;
}

static int R_SelectReadyCubemapIndexForPBRAt( const vec3_t pos )
{
	int i;
	int bestIndex = -1;
	int bestInRadius = -1;
	float bestDistSq = 0.0f;
	float bestInRadiusDistSq = 0.0f;

	for ( i = 0; i < tr.numCubemaps; i++ ) {
		float distSq;
		vec3_t delta;
		const cubemap_t *cube = &tr.cubemaps[i];

		if ( !R_CubemapPbrIblReady( cube ) ) {
			continue;
		}

		VectorSubtract( pos, cube->origin, delta );
		distSq = VectorLengthSquared( delta );

		if ( bestIndex == -1 || distSq < bestDistSq ) {
			bestIndex = i;
			bestDistSq = distSq;
		}

		if ( cube->parallaxRadius > 0.0f && distSq > ( cube->parallaxRadius * cube->parallaxRadius ) ) {
			continue;
		}

		if ( bestInRadius == -1 || distSq < bestInRadiusDistSq ) {
			bestInRadius = i;
			bestInRadiusDistSq = distSq;
		}
	}

	return ( bestInRadius != -1 ) ? bestInRadius : bestIndex;
}

static qboolean R_HasReadyCubemapForPBR( void )
{
	int i;

	for ( i = 0; i < tr.numCubemaps; i++ ) {
		if ( R_CubemapPbrIblReady( &tr.cubemaps[i] ) ) {
			return qtrue;
		}
	}

	return qfalse;
}

static void R_UpdatePbrIblRuntimeState( qboolean usingHdrSkybox, qboolean hasReadyMapCubemap )
{
	int i;
	int ready = 0;
	int incomplete = 0;

	for ( i = 0; i < tr.numCubemaps; i++ ) {
		if ( R_CubemapPbrIblReady( &tr.cubemaps[i] ) ) {
			ready++;
		} else {
			incomplete++;
		}
	}

	vk.pbr_ibl_using_hdr_fallback = usingHdrSkybox;
	vk.pbr_ibl_has_ready_local_cubemap = hasReadyMapCubemap;
	vk.pbr_ibl_ready_cubemap_count = ready;
	vk.pbr_ibl_incomplete_cubemap_count = incomplete;
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

static void RB_IterateStagesGeneric( const shaderCommands_t *input, qboolean fogCollapse )
{
	const shaderStage_t *pStage;
	int tess_flags;
	int stage, i;

#if 1
	if ( r_shDebugView && r_shDebugView->integer == 3 ) {
		RB_DrawWorldSHDebugOverride();
		return;
	}
	qboolean is_pbr_surface;
	uint32_t pipeline;
	int fog_stage;
	qboolean pushUniform;

	vk_bind_index();

	tess_flags = input->shader->tessFlags;

	pushUniform = qfalse;

	is_pbr_surface = qfalse;

	if ( fogCollapse ) {
		VK_SetFogParams( &uniform, &fog_stage );
		VectorCopy( backEnd.or.viewOrigin, uniform.eyePos );
		vk_update_descriptor( VK_DESC_FOG_COLLAPSE, tr.fogImage->descriptor );
		pushUniform = qtrue;
	} else
	{
		fog_stage = 0;
		if ( tess_flags & TESS_VPOS ) {
			VectorCopy( backEnd.or.viewOrigin, uniform.eyePos );
			tess_flags &= ~TESS_VPOS;
			pushUniform = qtrue;
		}
	}

	is_pbr_surface = vk_is_valid_pbr_surface( tess.shader->hasPBR );

	// Debug view: render a non-PBR pass and optionally override texture0 binding.
	// Keeps runtime inspection simple without requiring extra shader variants.
	const int pbr_debug = ( r_pbr_debug != NULL ) ? r_pbr_debug->integer : 0;
	uniform.pbrDebugMode[0] = pbr_debug;
	uniform.pbrDebugMode[1] = 0.0f;
	uniform.pbrDebugMode[2] = 0.0f;
	uniform.pbrDebugMode[3] = 0.0f;
	{
		unsigned pathFlags = 0u;
		vkViewClass_t viewCls = vk_classify_current_view();
		renderPath_t path;

		if ( backEnd.currentEntity && backEnd.currentEntity != &tr.worldEntity ) {
			const int rfx = backEnd.currentEntity->e.renderfx;
			if ( rfx & ( RF_FIRST_PERSON | RF_DEPTHHACK ) ) {
				pathFlags |= R_PATH_FLAG_WEAPON_CANDIDATE;
			}
		}
		path = R_SelectSurfaceRenderPath( tess.shader, NULL, pathFlags, (int)viewCls );
		R_RenderPath_Note( path );
		/* Opaque deferred handoff from selector (not a second heuristic).
		 * y: 1=hybrid additive, 2=split compare, 3=mixed material (unlit+LM ownership). */
		if ( backEnd.drawSurfFilter == 1 && R_RenderPath_WantsDeferredHandoff( path ) &&
			vk_deferred_lighting_path_ready() ) {
			const qboolean mixed = R_DeferredMixedMaterialWanted();
			if ( ( r_hybridCompare && r_hybridCompare->integer ) ||
				( r_deferredArchitecture && r_deferredArchitecture->integer == DEFERRED_ARCH_COMPARE ) ) {
				uniform.pbrDebugMode[1] = mixed ? 4.0f : 2.0f; /* left deferred / right Forward+ */
			} else if ( mixed ) {
				uniform.pbrDebugMode[1] = 3.0f;
			} else {
				uniform.pbrDebugMode[1] = 1.0f;
			}
		}
		if ( r_renderPathDebug && r_renderPathDebug->integer >= 1 ) {
			uniform.pbrDebugMode[2] = (float)path;
		}
		if ( r_deferredEligibilityDebug && r_deferredEligibilityDebug->integer >= 1 ) {
			DeferredEligibilityResult elig =
				R_GetDeferredEligibility( tess.shader, NULL, pathFlags, (int)viewCls );
			/* Encode eligibility+1 so 0 remains "off" for the shader. */
			uniform.pbrDebugMode[3] = (float)( (int)elig.eligibility + 1 );
		}
	}

	if ( is_pbr_surface ) {
		Com_Memcpy( &uniform_camera.modelMatrix, backEnd.or.modelMatrix, sizeof(float) * 16 );
		Com_Memcpy( &uniform_camera.viewOrigin, backEnd.refdef.vieworg, sizeof( vec3_t) );
		uniform_camera.viewOrigin[3] = 0.0;

		vk.cmd->camera_ubo_offset = vk_append_uniform( &uniform_camera, sizeof(uniform_camera), vk.uniform_camera_item_size );

		pushUniform = qtrue;

		VK_FillPbrSunShadowUniform( &uniform );

		uniform.pbrForwardPlus[0] = -1.0f;
		uniform.pbrForwardPlus[1] = 0.0f;
		uniform.pbrForwardPlus[2] = 0.65f;
		uniform.pbrForwardPlus[3] = 0.0f;
		if ( r_forwardPlus && r_forwardPlus->integer && !R_ClassicLightingActive() ) {
			if ( r_forwardPlusOverflowShade && r_forwardPlusOverflowShade->value > 0.0f ) {
				uniform.pbrForwardPlus[0] = Com_Clamp( 0.0f, 4.0f, r_forwardPlusOverflowShade->value );
			}
			/* Single-path: when Forward+ shade owns dynamics, do not skip via dlightBits
			 * (classic projector is suppressed). Only mask when shade is off or deferred handoff. */
			if ( !( r_forwardPlusShade && r_forwardPlusShade->value > 0.0f ) ||
				( uniform.pbrDebugMode[1] > 0.5f ) || vk_deferred_unlit_base_wanted() ) {
				const uint32_t bits = (uint32_t)tess.dlightBits;
				if ( bits != 0u ) {
					float maskF;
					Com_Memcpy( &maskF, &bits, sizeof( maskF ) );
					uniform.pbrForwardPlus[1] = maskF;
				}
			}
			if ( r_forwardPlusSpecularStrength ) {
				uniform.pbrForwardPlus[2] = Com_Clamp( 0.0f, 4.0f, r_forwardPlusSpecularStrength->value );
			}
			if ( r_forwardPlusEnergyRenorm ) {
				uniform.pbrForwardPlus[3] = Com_Clamp( 0.0f, 2.0f, r_forwardPlusEnergyRenorm->value );
			}
		}
	} else {
		VK_FillPbrSunShadowUniform( &uniform );
		uniform.pbrForwardPlus[0] = -1.0f;
		uniform.pbrForwardPlus[1] = 0.0f;
		uniform.pbrForwardPlus[2] = 0.65f;
		uniform.pbrForwardPlus[3] = 0.0f;
	}
#endif // USE_VULKAN

	for ( stage = 0; stage < MAX_SHADER_STAGES; stage++ )
	{
		pStage = tess.xstages[ stage ];
		if ( !pStage )
			break;

		tess.vboStage = stage;

		tess_flags |= pStage->tessFlags;

		for ( i = 0;  i < (int)pStage->numTexBundles; i++ ) {
			if ( pStage->bundle[i].image[0] != NULL ) {
				/* Pipeline requires set 0 (uniforms); ensure we push for any textured draw (e.g. cinematic). */
				pushUniform = qtrue;
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

		if ( pStage->bundle[0].flowmapImage != NULL ) {
			const float flowTime = (float)tess.shaderTime;
			uniform.flowmapParams[0] = pStage->bundle[0].flowmapSpeed;
			uniform.flowmapParams[1] = flowTime;
			uniform.flowmapParams[2] = fmodf( flowTime, 1.0f );
			uniform.flowmapParams[3] = 0.0f;
			vk_update_descriptor( VK_DESC_TEXTURE1, pStage->bundle[0].flowmapImage->descriptor );
			pushUniform = qtrue;
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

		if ( backEnd.projection2D ) {
			Vk_Pipeline_Def uiDef;
			vk_get_pipeline_def( pipeline, &uiDef );
			uiDef.state_bits |= GLS_DEPTHTEST_DISABLE;
			uiDef.state_bits &= ~GLS_DEPTHMASK_TRUE;
			uiDef.face_culling = CT_TWO_SIDED;
			if ( tess.subpixelShift >= 0.0f ) {
				uiDef.shader_type = TYPE_SIGNLE_TEXTURE_UI_SUBPIXEL;
			}
			pipeline = vk_find_pipeline_ext( 0, &uiDef, qfalse );
		}

		if ( tess.shader && tess.shader->hasPBR && pStage->vk_pbr_flags && tess.gltfUseGpuPipeline ) {
			if ( backEnd.viewParms.portalView == PV_MIRROR ) {
				pipeline = pStage->vk_mirror_pipeline_gltf_gpu[fog_stage];
			} else {
				pipeline = pStage->vk_pipeline_gltf_gpu[fog_stage];
			}
		}

		Vk_Pipeline_Def	def;
		vk_get_pipeline_def( pipeline, &def );

		if ( is_pbr_surface && pStage->vk_pbr_flags ) {
			static VkCommandBuffer lastCmdBuf = VK_NULL_HANDLE;
			static qboolean lastValid = qfalse;
			static vkPbrUniformBlock_t lastBlock;
			static qboolean warnedMissingBrdfLut = qfalse;
			static qboolean warnedMissingEmptyCubemap = qfalse;
			static qboolean warnedMissingMapCubemapData = qfalse;
			static qboolean warnedRemappedMapCubemap = qfalse;
			static qboolean warnedHdrSkyboxFallbackForIncompleteMap = qfalse;

			vkPbrUniformBlock_t block;
			Vector4Copy( pStage->emissiveScale, block.emissiveScale );
			Vector4Copy( pStage->clearcoatScale, block.clearcoatScale );
			Vector4Copy( pStage->sheenScale, block.sheenScale );
			Vector4Copy( pStage->anisotropyScale, block.anisotropyScale );
			Vector4Copy( pStage->transmissionScale, block.transmissionScale );
			Vector4Copy( pStage->subsurfaceColor, block.subsurfaceColor );
			Vector4Copy( pStage->subsurfaceParams, block.subsurfaceParams );
			{
				float aaStrength = 0.0f;
				if ( r_pbr_specularAA && r_pbr_specularAA->integer ) {
					aaStrength = LerpClamp( ( r_pbr_specularAAStrength ? r_pbr_specularAAStrength->value : 0.5f ), 0.0f, 2.0f );
					/* Raster Ultra 1.12: frequency-aware may raise strength (Toksvig path); never a global roughness rewrite. */
					if ( vk_frequency_aware_active() ) {
						float fa = vk_frequency_aware_specular_aa_strength();
						if ( fa > aaStrength ) {
							aaStrength = LerpClamp( fa, 0.0f, 2.0f );
						}
					}
				}
				Vector4Set( block.advancedParams,
					( r_pbr_multiScatter && r_pbr_multiScatter->integer ) ? 1.0f : 0.0f,
					( r_pbr_multiScatterStrength ? r_pbr_multiScatterStrength->value : 1.0f ),
					( r_pbr_fresnelRoughness && r_pbr_fresnelRoughness->integer ) ? 1.0f : 0.0f,
					aaStrength );
			}

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
				( r_pbr_anisotropicSpecular && r_pbr_anisotropicSpecular->integer ) ? 1.0f : 0.0f,
				( r_pbr_iblAnisoStretch ) ? LerpClamp( r_pbr_iblAnisoStretch->value, 0.0f, 1.0f ) : 0.0f );

			Vector4Set( block.parallaxParams,
				( r_pomScale && r_pomScale->value > 0.0f ) ? r_pomScale->value : 0.06f,
				( r_pomShadow && r_pomShadow->value > 0.0f ) ? LerpClamp( r_pomShadow->value, 0.0f, 1.0f ) : 0.0f,
				(float)( r_pomShadowSteps ? Com_Clamp( 2, 16, r_pomShadowSteps->integer ) : 6 ),
				0.0f );

			{
				float sharpness = pStage->blendSharpness > 0.0f ? pStage->blendSharpness : 8.0f;
				if ( r_materialBlendSharpness && r_materialBlendSharpness->value > 0.0f ) {
					sharpness = r_materialBlendSharpness->value;
				}
				Vector4Set( block.materialBlend, sharpness, 0.0f, 0.0f, 0.0f );
			}

			{
				const VkDescriptorSet fallback2D = ( tr.whiteImage ) ? tr.whiteImage->descriptor : VK_NULL_HANDLE;
				const VkDescriptorSet fallbackCube = ( tr.emptyCubemap ) ? tr.emptyCubemap->descriptor : VK_NULL_HANDLE;
				VkDescriptorSet brdfDescriptor = vk.brdflut_image_descriptor ? vk.brdflut_image_descriptor : fallback2D;
				VkDescriptorSet envDescriptor = fallbackCube;
				VkDescriptorSet irradianceDescriptor = fallbackCube;
				const VkDescriptorSet hdrEnvDescriptor = SkyboxHDR_GetPrefilteredDescriptor();
				const VkDescriptorSet hdrIrradianceDescriptor = SkyboxHDR_GetIrradianceDescriptor();
				const qboolean hasReadyMapCubemap = R_HasReadyCubemapForPBR();
				/*
				 * The runtime HDR skybox is a global fallback for maps that do not provide
				 * usable cubemaps. Mixing it into a map that already has ready local IBL
				 * data produces visibly inconsistent lighting/reflections, so only use it
				 * when no local cubemap is actually ready.
				 */
				const qboolean hdrSkyboxReady = ( !hasReadyMapCubemap &&
					hdrEnvDescriptor != VK_NULL_HANDLE &&
					hdrIrradianceDescriptor != VK_NULL_HANDLE );
				qboolean usingHdrSkybox = qfalse;

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

					if ( backEnd.viewParms.targetCube == NULL && hdrSkyboxReady ) {
						envDescriptor = hdrEnvDescriptor;
						irradianceDescriptor = hdrIrradianceDescriptor;
						usingHdrSkybox = qtrue;
						if ( tr.numCubemaps > 0 && !warnedHdrSkyboxFallbackForIncompleteMap ) {
							ri.Printf( PRINT_WARNING,
								"PBR IBL: no ready local cubemap found; using HDR skybox fallback until map probes are complete\n" );
							warnedHdrSkyboxFallbackForIncompleteMap = qtrue;
						}
						if ( !SkyboxHDR_CopySHCoeffs( block.shCoeffs ) ) {
							Com_Memcpy( block.shCoeffs, pStage->shCoeffs, sizeof( block.shCoeffs ) );
						}
					}
					if ( !usingHdrSkybox ) {
						// Use stage-provided SH when no cubemap is available.
						Com_Memcpy( block.shCoeffs, pStage->shCoeffs, sizeof( block.shCoeffs ) );
					}
				}
				else {
					vec3_t dbgPos;
					R_GetPBRSurfacePosition( dbgPos );
					cubemapIndex = R_SelectCubemapIndexForPBR();
					if ( cubemapIndex >= 0 && cubemapIndex < tr.numCubemaps ) {
						const cubemap_t *selectedCube = &tr.cubemaps[cubemapIndex];
						if ( !R_CubemapPbrIblReady( selectedCube ) ) {
							int readyIndex = R_SelectReadyCubemapIndexForPBRAt( dbgPos );
							if ( readyIndex >= 0 ) {
								if ( !warnedRemappedMapCubemap ) {
									ri.Printf( PRINT_WARNING,
										"PBR IBL: cubemap '%s' is incomplete; remapping to nearest ready cubemap '%s'\n",
										selectedCube->name[0] ? selectedCube->name : "<unnamed>",
										tr.cubemaps[readyIndex].name[0] ? tr.cubemaps[readyIndex].name : "<unnamed>" );
									warnedRemappedMapCubemap = qtrue;
								}
								cubemapIndex = readyIndex;
							}
						}
					}

					R_UpdatePBRCubemapDebugCvar( cubemapIndex, dbgPos );
					if ( cubemapIndex >= 0 && cubemapIndex < tr.numCubemaps ) {
						cube = &tr.cubemaps[cubemapIndex];
					}

					if ( cube ) {
						const qboolean cubeReady = R_CubemapPbrIblReady( cube );

						if ( cubeReady ) {
							envDescriptor = cube->prefiltered_image->descriptor;
							irradianceDescriptor = cube->irradiance_image->descriptor;
						}
						if ( !cubeReady ) {
							if ( !warnedMissingMapCubemapData ) {
								ri.Printf( PRINT_WARNING, "PBR IBL: cubemap '%s' missing prefiltered/irradiance image, using fallback where needed\n",
									cube->name[0] ? cube->name : "<unnamed>" );
								warnedMissingMapCubemapData = qtrue;
							}
							if ( hdrSkyboxReady ) {
								envDescriptor = hdrEnvDescriptor;
								irradianceDescriptor = hdrIrradianceDescriptor;
								usingHdrSkybox = qtrue;
							}
						}

						// Prefer cubemap SH coefficients when present, otherwise fall back to stage SH.
						if ( cubeReady && cube->hasSHCoeffs ) {
							Com_Memcpy( block.shCoeffs, cube->shCoeffs, sizeof( block.shCoeffs ) );
						} else {
							if ( usingHdrSkybox && SkyboxHDR_CopySHCoeffs( block.shCoeffs ) ) {
								/* HDR fallback supplied SH successfully. */
							} else {
							Com_Memcpy( block.shCoeffs, pStage->shCoeffs, sizeof( block.shCoeffs ) );
							}
						}

					} else {
						if ( hdrSkyboxReady ) {
							envDescriptor = hdrEnvDescriptor;
							irradianceDescriptor = hdrIrradianceDescriptor;
							usingHdrSkybox = qtrue;
							if ( tr.numCubemaps > 0 && !warnedHdrSkyboxFallbackForIncompleteMap ) {
								ri.Printf( PRINT_WARNING,
									"PBR IBL: no ready local cubemap found; using HDR skybox fallback until map probes are complete\n" );
								warnedHdrSkyboxFallbackForIncompleteMap = qtrue;
							}
							if ( !SkyboxHDR_CopySHCoeffs( block.shCoeffs ) ) {
								Com_Memcpy( block.shCoeffs, pStage->shCoeffs, sizeof( block.shCoeffs ) );
							}
						}
						if ( !usingHdrSkybox ) {
							Com_Memcpy( block.shCoeffs, pStage->shCoeffs, sizeof( block.shCoeffs ) );
						}
					}
				}

				if ( brdfDescriptor ) {
					vk_update_descriptor_if_changed( VK_DESC_PBR_BRDFLUT, brdfDescriptor );
				}
				if ( envDescriptor ) {
					if ( usingHdrSkybox ) {
						vk_update_descriptor_if_changed( VK_DESC_PBR_CUBEMAP, envDescriptor );
					} else {
						vk_update_descriptor_if_changed_with_image( VK_DESC_PBR_CUBEMAP, envDescriptor, cube ? cube->prefiltered_image : NULL );
					}
				}
				if ( irradianceDescriptor ) {
					if ( usingHdrSkybox ) {
						vk_update_descriptor_if_changed( VK_DESC_PBR_IRRADIANCE, irradianceDescriptor );
					} else {
						vk_update_descriptor_if_changed_with_image( VK_DESC_PBR_IRRADIANCE, irradianceDescriptor, cube ? cube->irradiance_image : NULL );
					}
				}

				{
					image_t *envImage = NULL;
					image_t *irrImage = NULL;
					VkImageView envView = VK_NULL_HANDLE;
					VkImageView irrView = VK_NULL_HANDLE;
					qboolean hasEnv = qfalse;
					qboolean hasIrr = qfalse;
					/* Procedural glints have no GPU dictionary texture in this path. */
					const qboolean dictValid = ( r_glint && r_glint->integer && r_glintMode && r_glintMode->integer ) ? qtrue : qfalse;
					int bindFlags;

					if ( usingHdrSkybox ) {
						SkyboxHDR_GetCubemapViews( &envView, &irrView );
						hasEnv = ( envView != VK_NULL_HANDLE ) ? qtrue : qfalse;
						hasIrr = ( irrView != VK_NULL_HANDLE ) ? qtrue : qfalse;
					} else if ( cube ) {
						envImage = cube->prefiltered_image;
						irrImage = cube->irradiance_image;
						if ( envImage ) {
							envView = envImage->view;
						}
						if ( irrImage ) {
							irrView = irrImage->view;
						}
						hasEnv = ( envView != VK_NULL_HANDLE ) ? qtrue : qfalse;
						hasIrr = ( irrView != VK_NULL_HANDLE ) ? qtrue : qfalse;
					} else if ( tr.emptyCubemap ) {
						envImage = tr.emptyCubemap;
						irrImage = tr.emptyCubemap;
						envView = tr.emptyCubemap->view;
						irrView = tr.emptyCubemap->view;
						hasEnv = ( envView != VK_NULL_HANDLE ) ? qtrue : qfalse;
						hasIrr = ( irrView != VK_NULL_HANDLE ) ? qtrue : qfalse;
					}

					bindFlags = ( hasEnv ? 1 : 0 ) | ( hasIrr ? 2 : 0 ) | ( dictValid ? 4 : 0 );
					uniform.pbrDebugMode[2] = (float)bindFlags;

					if ( !tr_pbr_bindLogPrinted && R_PBR_ShouldLogBindings() ) {
						tr_pbr_bindLogPrinted = qtrue;
						ri.Printf( PRINT_ALL,
							"PBR bind: map=%s numCubemaps=%d cubemapIndex=%d usingHdr=%d "
							"envImg=%p envView=%p hasEnv=%d irrImg=%p irrView=%p hasIrr=%d "
							"dictImg=%p dictView=%p dictValid=%d (procedural glints)\n",
							( tr.world && tr.world->baseName[0] ) ? tr.world->baseName : "<none>",
							tr.numCubemaps,
							cubemapIndex,
							usingHdrSkybox ? 1 : 0,
							(void *)envImage, (void *)envView, hasEnv ? 1 : 0,
							(void *)irrImage, (void *)irrView, hasIrr ? 1 : 0,
							(void *)NULL, (void *)VK_NULL_HANDLE, dictValid ? 1 : 0 );
						ri.Printf( PRINT_ALL,
							"PBR IBL descwrite: binding=%d (env/CUBEMAP) view=%p set=%p\n",
							VK_DESC_PBR_CUBEMAP, (void *)envView, (void *)envDescriptor );
						ri.Printf( PRINT_ALL,
							"PBR IBL descwrite: binding=%d (irr/IRRADIANCE) view=%p set=%p\n",
							VK_DESC_PBR_IRRADIANCE, (void *)irrView, (void *)irradianceDescriptor );
					}

					/* Ensure debug 17-19 see current resource flags even if PBR block is unchanged. */
					if ( pbr_debug >= 17 || ( r_pbr_bindlog && r_pbr_bindlog->integer ) ) {
						vk_push_uniform_cached( &uniform );
					}
				}

				R_UpdatePbrIblRuntimeState( usingHdrSkybox, hasReadyMapCubemap );
			}
				
			if ( pStage->vk_pbr_flags & PBR_HAS_NORMALMAP )
				vk_update_descriptor_if_changed_with_image( VK_DESC_PBR_NORMAL, pStage->normalMap->descriptor, pStage->normalMap );

			if ( pStage->vk_pbr_flags & PBR_HAS_PHYSICALMAP || pStage->vk_pbr_flags & PBR_HAS_SPECULARMAP )
				vk_update_descriptor_if_changed_with_image( VK_DESC_PBR_PHYSICAL, pStage->physicalMap->descriptor, pStage->physicalMap );

			if ( pStage->vk_pbr_flags & PBR_HAS_DETAILMAP )
				vk_update_descriptor_if_changed_with_image( VK_DESC_PBR_DETAIL, pStage->detailMap->descriptor, pStage->detailMap );

			/* Multi-material blend: bind unique layer arrays on set 19. */
			if ( vk.set_layout_blend_layers != VK_NULL_HANDLE ) {
				static qboolean blendDescReady;
				static VkDescriptorPool blendDescPool;
				image_t *white = tr.whiteImage;
				image_t *albedos[8];
				image_t *normals[8];
				image_t *orms[8];
				VkDescriptorImageInfo imgInfos[24];
				VkWriteDescriptorSet writes[3];
				int li;

				/* Descriptor pool is recreated on vid_restart / ERR_DROP recovery. */
				if ( blendDescReady && blendDescPool != vk.descriptor_pool ) {
					blendDescReady = qfalse;
					vk.blend_layers_descriptor = VK_NULL_HANDLE;
				}

				if ( !blendDescReady && vk.descriptor_pool != VK_NULL_HANDLE ) {
					VkDescriptorSetAllocateInfo alloc;
					Com_Memset( &alloc, 0, sizeof( alloc ) );
					alloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
					alloc.descriptorPool = vk.descriptor_pool;
					alloc.descriptorSetCount = 1;
					alloc.pSetLayouts = &vk.set_layout_blend_layers;
					if ( qvkAllocateDescriptorSets( vk.device, &alloc, &vk.blend_layers_descriptor ) == VK_SUCCESS ) {
						blendDescReady = qtrue;
						blendDescPool = vk.descriptor_pool;
					}
				}

				albedos[0] = ( pStage->bundle[0].image[0] ) ? pStage->bundle[0].image[0] : white;
				normals[0] = pStage->normalMap ? pStage->normalMap : white;
				orms[0] = pStage->physicalMap ? pStage->physicalMap : white;
				for ( li = 0; li < 7; li++ ) {
					albedos[li + 1] = pStage->layerAlbedo[li] ? pStage->layerAlbedo[li] : albedos[0];
					normals[li + 1] = pStage->layerNormal[li] ? pStage->layerNormal[li] : normals[0];
					orms[li + 1] = pStage->layerPhysical[li] ? pStage->layerPhysical[li] : orms[0];
				}
				if ( !( ( pStage->vk_pbr_flags & PBR_HAS_MATERIAL_BLEND ) && pStage->materialBlend &&
					pStage->materialLayerCount >= 2 &&
					( !r_materialBlend || r_materialBlend->integer ) ) ) {
					for ( li = 0; li < 8; li++ ) {
						albedos[li] = white;
						normals[li] = white;
						orms[li] = white;
					}
				}
				if ( blendDescReady && vk.blend_layers_descriptor != VK_NULL_HANDLE && white && white->view ) {
					for ( li = 0; li < 8; li++ ) {
						image_t *img = albedos[li] && albedos[li]->view ? albedos[li] : white;
						imgInfos[li].sampler = img->vk_sampler ? img->vk_sampler : white->vk_sampler;
						imgInfos[li].imageView = img->view;
						imgInfos[li].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						img = normals[li] && normals[li]->view ? normals[li] : white;
						imgInfos[8 + li].sampler = img->vk_sampler ? img->vk_sampler : white->vk_sampler;
						imgInfos[8 + li].imageView = img->view;
						imgInfos[8 + li].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
						img = orms[li] && orms[li]->view ? orms[li] : white;
						imgInfos[16 + li].sampler = img->vk_sampler ? img->vk_sampler : white->vk_sampler;
						imgInfos[16 + li].imageView = img->view;
						imgInfos[16 + li].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
					}
					for ( li = 0; li < 3; li++ ) {
						Com_Memset( &writes[li], 0, sizeof( writes[li] ) );
						writes[li].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
						writes[li].dstSet = vk.blend_layers_descriptor;
						writes[li].dstBinding = (uint32_t)li;
						writes[li].dstArrayElement = 0;
						writes[li].descriptorCount = 8;
						writes[li].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
						writes[li].pImageInfo = &imgInfos[li * 8];
					}
					qvkUpdateDescriptorSets( vk.device, 3, writes, 0, NULL );
					vk_update_descriptor( VK_DESC_PBR_BLEND_LAYERS, vk.blend_layers_descriptor );
				}
			}
			
			// Commented out descriptor updates for removed PBR features
			if ( ( pStage->vk_pbr_flags & PBR_HAS_EMISSIVE ) && pStage->emissiveMap )
				vk_update_descriptor_if_changed_with_image( VK_DESC_PBR_EMISSIVE,
					pStage->emissiveMap->descriptor, pStage->emissiveMap );

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
				Vector4Copy( block.parallaxParams, uniform.pbrParallaxParams );
				Vector4Copy( block.materialBlend, uniform.pbrMaterialBlend );
				if ( backEnd.reactiveMaskStamp ) {
					uniform.pbrMaterialBlend[3] = 1.0f;
				}
				vk_surface_evolution_fill_ubo( uniform.pbrSurfaceEvolution );

				vk_push_uniform_cached( &uniform );
			}

			// aparently lightmap is not always in bundle 1 ..
			// should probably fix this in collapseMuklitexture
			if ( !( ( pStage->vk_pbr_flags & PBR_HAS_MATERIAL_BLEND ) && pStage->materialLayerCount > 2 ) ) {
				if ( def.vk_pbr_flags & PBR_HAS_DELUXEMAP0 )
					vk_update_descriptor(  VK_DESC_PBR_DELUXE, pStage->bundle[0].deluxeMap->descriptor );

				if ( def.vk_pbr_flags & PBR_HAS_DELUXEMAP1 )
					vk_update_descriptor(  VK_DESC_PBR_DELUXE, pStage->bundle[1].deluxeMap->descriptor );

				else if ( !(def.vk_pbr_flags & PBR_HAS_DELUXEMAP0) && tr.whiteImage )
					vk_update_descriptor(  VK_DESC_PBR_DELUXE, tr.whiteImage->descriptor );
			}
		}

		if ( !is_pbr_surface && pStage->vk_pbr_flags ) {
			def.vk_pbr_flags = 0;
			def.lightmap_bundle = -1;
			pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
		}

		if ( ( backEnd.depthOnlyWorldPass || backEnd.forwardPlusDepthPrepass ) && pStage->depthFragment ) {
			/* Occlusion pass: depth-only draw */
			if ( backEnd.viewParms.portalView == PV_MIRROR )
				pipeline = pStage->vk_mirror_pipeline_df;
			else
				pipeline = pStage->vk_pipeline_df;
			vk_bind_pipeline( pipeline );
			vk_bind_geometry( tess_flags );
			vk_draw_geometry( tess.depthRange, qtrue );
		} else {
			vk_bind_pipeline( pipeline );
			vk_bind_geometry( tess_flags );
			vk_draw_geometry( tess.depthRange, qtrue );

			if ( !backEnd.depthOnlyWorldPass && !backEnd.forwardPlusDepthPrepass && pStage->depthFragment ) {
				if ( backEnd.viewParms.portalView == PV_MIRROR )
					pipeline = pStage->vk_mirror_pipeline_df;
				else
					pipeline = pStage->vk_pipeline_df;
				vk_bind_pipeline( pipeline );
				vk_draw_geometry( tess.depthRange, qtrue );
			}
		}

		// allow skipping out to show just lightmaps during development
		if ( r_lightmap->integer && ( pStage->bundle[0].lightmap != LIGHTMAP_INDEX_NONE || pStage->bundle[1].lightmap != LIGHTMAP_INDEX_NONE ) )
			break;

		tess_flags = 0;
	}

	if ( pushUniform ) {
		vk_push_uniform_cached( &uniform );
	}
	if ( tess_flags ) // fog-only shaders?
		vk_bind_geometry( tess_flags );
}



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


static void VK_SetLightParams( vkUniform_t *ubo, const dlight_t *dl ) {
	float radius;

	R_DynamicLightColor( dl, ubo->light.color );

	radius = dl->radius;
	if ( radius < 0.001f )
		radius = 0.001f;

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

uint32_t vk_append_uniform( const void *uniform_data, size_t size, uint32_t min_offset ) {
	const uint32_t offset = PAD(vk.cmd->vertex_buffer_offset, (VkDeviceSize)vk.uniform_alignment);


	if ( offset + min_offset > vk.geometry_buffer_size ) {
		/* Schedule geometry buffer resize; callers must skip draw when ~0U returned */
		vk.geometry_buffer_size_new = (VkDeviceSize)log2pad( (unsigned int)( offset + min_offset ), 1 );
		return ~0U;
	}

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
	vkUniform_t stamped;

	// Reset cache when we move to a new command buffer.
	if ( vk.cmd == NULL || vk.cmd->command_buffer != last_cmd_buf ) {
		last_cmd_buf = ( vk.cmd != NULL ) ? vk.cmd->command_buffer : VK_NULL_HANDLE;
		last_camera_offset = ~0U;
		last_uniform_offset = ~0U;
		Com_Memset( &last_uniform, 0, sizeof( last_uniform ) );
	}

	if ( backEnd.reactiveMaskStamp && u != NULL ) {
		Com_Memcpy( &stamped, u, sizeof( stamped ) );
		stamped.pbrMaterialBlend[3] = 1.0f;
		if ( backEnd.drawSurfFilter == 2 ) {
			stamped.pbrMaterialBlend[2] = 1.0f; /* transparent filter: stamp all survivors */
		}
		u = &stamped;
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

	if ( offset == ~0U ) {
		/* Uniform buffer overflow; vk_append_uniform already set geometry_buffer_size_new.
		 * Do not update descriptors with invalid offset; callers must skip the draw. */
		return ~0U;
	}

	vk_reset_descriptor( VK_DESC_UNIFORM );
	vk_update_descriptor( VK_DESC_UNIFORM, vk.cmd->uniform_descriptor );
	vk_update_descriptor_offset( VK_DESC_UNIFORM_MAIN_BINDING, offset );
	vk_update_descriptor_offset( VK_DESC_UNIFORM_CAMERA_BINDING, vk.cmd->camera_ubo_offset );
	vk_update_descriptor_offset( VK_DESC_UNIFORM_IQM_SKIN_BINDING, vk.cmd->iqm_skin_offset );
	vk_update_descriptor_offset( VK_DESC_UNIFORM_IQM_MORPH_BINDING, vk.cmd->iqm_morph_offset );
	vk_update_descriptor_offset( VK_DESC_UNIFORM_GLTF_TOPO_BINDING, vk.cmd->gltf_topo_offset );

	return offset;
}

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

	abs_light = ( ( pStage->stateBits & GLS_ATEST_BITS ) && cull == CT_TWO_SIDED ) ? 1 : 0;

	if ( fog_stage )
		vk_update_descriptor( VK_DESC_FOG_DLIGHT, tr.fogImage->descriptor );

	if ( tess.light->linear )
		pipeline = vk.dlight1_pipelines_x[cull][tess.shader->polygonOffset][fog_stage][abs_light];
	else
		pipeline = vk.dlight_pipelines_x[cull][tess.shader->polygonOffset][fog_stage][abs_light];

	GL_SelectTexture( 0 );
	R_BindAnimatedImage( &pStage->bundle[ tess.shader->lightingBundle ] );

	if ( tess.vboIndex == 0 )
	{
		R_ComputeTexCoords( tess.shader->lightingBundle, &pStage->bundle[ tess.shader->lightingBundle ] );
	}

	vk_bind_pipeline( pipeline );
	vk_bind_index();
	vk_bind_lighting( tess.shader->lightingStage, tess.shader->lightingBundle );
	vk_draw_geometry( tess.depthRange, qtrue );
}

void RB_StageIteratorGeneric( void )
{
	qboolean rebindIndex = qfalse;
	qboolean fogCollapse = qfalse;
	qboolean worldShOverride;

	if ( tess.vboIndex != 0 ) {
		if ( !VBO_ItemIsStream( tess.vboIndex ) ) {
			VBO_PrepareQueues();
		}
		tess.vboStage = 0;
	} else
	RB_DeformTessGeometry();

	if ( tess.dlightPass ) {
		VK_LightingPass();
		return;
	}

	fogCollapse = tess.fogNum && tess.shader->fogPass && tess.shader->fogCollapse;
	worldShOverride = ( r_shDebugView && r_shDebugView->integer == 3 );

	// call shader function
	RB_IterateStagesGeneric( &tess, fogCollapse );

	// now do any dynamic lighting needed
	if ( r_dlightMode->integer == 0 )
	if ( !vk_deferred_unlit_base_wanted() )
	/* Single-path Forward+: shade owns dynamics — skip classic projector. */
	if ( !( r_forwardPlus && r_forwardPlus->integer && r_forwardPlusShade &&
			r_forwardPlusShade->value > 0.0f && !vk_unified_clustered_opaque_handoff() &&
			!R_ClassicLightingActive() ) )
	if ( !worldShOverride && tess.dlightBits && tess.shader->sort <= SS_OPAQUE && !(tess.shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY) ) ) {
		if ( !fogCollapse ) {
			rebindIndex = ProjectDlightTexture();
		}
	}

	// now do fog
	if ( !worldShOverride && tess.fogNum && tess.shader->fogPass && !fogCollapse ) {
		RB_FogPass( rebindIndex );
	}
}



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
		tess.vboIndex = 0; //VBO_UnBind();
		return;
	}

	//
	// update performance counters
	//
	if ( tess.dlightPass ) {
		backEnd.pc.c_lit_batches++;
		backEnd.pc.c_lit_vertices += tess.numVertexes;
		backEnd.pc.c_lit_indices += tess.numIndexes;
	} else
	{
		backEnd.pc.c_shaders++;
		backEnd.pc.c_vertexes += tess.numVertexes;
		backEnd.pc.c_indexes += tess.numIndexes;
	}
	backEnd.pc.c_totalIndexes += tess.numIndexes * tess.numPasses;

	//
	// call off to shader specific tess end function
	//
	if ( backEnd.projection2D ) {
		backEnd.currentEntity = &backEnd.entity2D;
		backEnd.useFirstPersonProjection = qfalse;
		tess.depthRange = DEPTH_RANGE_NORMAL;
		vk_update_mvp( NULL );
	}
	/* Deform vegetation verts on GPU before draw (staging filled during RB_DrawSurfs). */
	if ( PostFX_VegWind_IsEnabled() && tess.shader && ( tess.shader->surfaceFlags & SURF_VEGETATION ) ) {
		vk_vegetation_wind_prepare_draw();
	}
	R_IQMCommitSurfaceBatch();
	tess.shader->optimalStageIteratorFunc();

	//
	// draw debugging stuff
	//
	if ( vk_bsp_viz_effective_showtris() ) {
		DrawTris( input );
	}
	if ( r_shownormals->integer ) {
		DrawNormals( input );
	}

	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;
	tess.numVertexes = 0;

	tess.vboIndex = 0;
	tess.vboStreamItem = NULL;
	//VBO_ClearQueue();
}
