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
#include "tr_local.h"
#include "../common/tr_vector_font.h"
#ifdef USE_VULKAN
#include "vk_terrain.h"
#include "vk_biome.h"
#include "vk_ui_blur.h"
#include "vk_vegetation_gpu.h"
#include "vk_temporal.h"
#include "vk_pass_registry.h"
#include "vk_forward_plus.h"
#include "vk_deferred_gbuffer.h"
#include "vk_visibility_buffer.h"
#include "vk_niv.h"
#include "vk_surfel_gi.h"
#include "vk_rcgi.h"
#include "vk_nist.h"
#include "vk_nvc.h"
#include "vk_fsa.h"
#include "vk_vfgi.h"
#include "vk_renderformer.h"
#include "vk_wpt.h"
#include "vk_mgs.h"
#include "vk_squeezeme.h"
#include "vk_vector_font.h"
#include "vk_wsp.h"
#include "vk_vt.h"
#include "vk_fp64_points.h"
#include "vk_scene_pass.h"
#include "vk_render_pass.h"
#include "vk_reactive_mask.h"
#include "vk_temporal_class.h"
#include "vk_object_id.h"
#include "vk_ambient_visibility.h"
#include "vk_raster_gi.h"
#include "vk_gpu_particles.h"
#include "vk_deferred_decals.h"
#include "vk_distortion.h"
#include "vk_transparency_route.h"
#include "vk_gpu_scene.h"
#include "vk_ht_throughput.h"
#include "vk_hiz.h"
#include "vk_selective_sun_shadow.h"
#include "vk_sun_csm.h"
#include "vk_vshadow.h"
#include "vk_capture_pipeline.h"
#include "vk_image_layout.h"
#include "vk_post_fog.h"
#include "vk_postfx_params.h"
#include "vk_view_state.h"
#include <assert.h>
#endif

backEndData_t	*backEndData;
backEndState_t	backEnd;

#ifndef USE_VULKAN
static const float s_flipMatrix[16] = {
	// convert from our coordinate system (looking down X)
	// to OpenGL's coordinate system (looking down -Z)
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};


const float *GL_Ortho( const float left, const float right, const float bottom, const float top, const float znear, const float zfar )
{
	static float m[ 16 ] = { 0 };

	m[0] = 2.0f / (right - left);
	m[5] = 2.0f / (top - bottom);
	m[10] = - 2.0f / (zfar - znear);
	m[12] = - (right + left)/(right - left);
	m[13] = - (top + bottom) / (top - bottom);
	m[14] = - (zfar + znear) / (zfar - znear);
	m[15] = 1.0f;

	return m;
}
#endif


/*
** GL_Bind
*/
void GL_Bind( image_t *image ) {
#ifdef USE_VULKAN
	if ( !image ) {
		ri.Printf( PRINT_WARNING, "GL_Bind: NULL image\n" );
		image = tr.defaultImage;
	}

	if ( r_nobind->integer && tr.dlightImage ) {		// performance evaluation option
		image = tr.dlightImage;
	}

	if ( !image )
		return;

	image->frameUsed = tr.frameCount;
	vk_update_descriptor( glState.currenttmu + VK_DESC_TEXTURE_BASE, image->descriptor );
#else
	GLuint texnum;

	if ( !image ) {
		ri.Printf( PRINT_WARNING, "GL_Bind: NULL image\n" );
		texnum = tr.defaultImage->texnum;
	} else {
		texnum = image->texnum;
	}

	if ( r_nobind->integer && tr.dlightImage ) {		// performance evaluation option
		texnum = tr.dlightImage->texnum;
	}

	if ( glState.currenttextures[glState.currenttmu] != texnum ) {
		if ( image ) {
			image->frameUsed = tr.frameCount;
		}
		glState.currenttextures[glState.currenttmu] = texnum;
		qglBindTexture (GL_TEXTURE_2D, texnum);
	}
#endif
}


/*
** GL_SelectTexture
*/
void GL_SelectTexture( int unit )
{
#ifndef USE_VULKAN
	if ( glState.currenttmu == unit )
	{
		return;
	}
#endif

	if ( unit >= glConfig.numTextureUnits )
	{
		ri.Error( ERR_DROP, "GL_SelectTexture: unit = %i", unit );
	}
#ifndef USE_VULKAN
	qglActiveTextureARB( GL_TEXTURE0_ARB + unit );
#endif
	glState.currenttmu = unit;
}


/*
** GL_SelectClientTexture
*/
#ifndef USE_VULKAN
static void GL_SelectClientTexture( int unit )
{
	if ( glState.currentArray == unit )
	{
		return;
	}

	if ( unit >= glConfig.numTextureUnits )
	{
		ri.Error( ERR_DROP, "GL_SelectClientTexture: unit = %i", unit );
	}

	qglClientActiveTextureARB( GL_TEXTURE0_ARB + unit );

	glState.currentArray = unit;
}
#endif


/*
** GL_Cull
*/
void GL_Cull( cullType_t cullType ) {
	if ( glState.faceCulling == cullType ) {
		return;
	}

	glState.faceCulling = cullType;
#ifndef USE_VULKAN
	if ( cullType == CT_TWO_SIDED )
	{
		qglDisable( GL_CULL_FACE );
	}
	else
	{
		qboolean cullFront;
		qglEnable( GL_CULL_FACE );

		cullFront = (cullType == CT_FRONT_SIDED);
		if ( backEnd.viewParms.portalView == PV_MIRROR )
		{
			cullFront = !cullFront;
		}

		qglCullFace( cullFront ? GL_FRONT : GL_BACK );
	}
#endif
}


/*
** GL_TexEnv
*/
void GL_TexEnv( GLint env )
{
#ifdef USE_VULKAN
	(void)env;
#endif
#ifndef USE_VULKAN
	if ( env == glState.texEnv[ glState.currenttmu ] )
		return;

	glState.texEnv[ glState.currenttmu ] = env;

	switch ( env )
	{
	case GL_MODULATE:
	case GL_REPLACE:
	case GL_DECAL:
	case GL_ADD:
		qglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, env );
		break;
	default:
		ri.Error( ERR_DROP, "GL_TexEnv: invalid env '%d' passed", env );
		break;
	}
#endif
}


/*
** GL_State
**
** This routine is responsible for setting the most commonly changed state
** in Q3.
*/
void GL_State( unsigned stateBits )
{
#ifdef USE_VULKAN
	(void)stateBits;
#endif
#ifndef USE_VULKAN
	unsigned diff = stateBits ^ glState.glStateBits;

	if ( !diff )
	{
		return;
	}

	//
	// check depthFunc bits
	//
	if ( diff & GLS_DEPTHFUNC_EQUAL )
	{
		if ( stateBits & GLS_DEPTHFUNC_EQUAL )
		{
			qglDepthFunc( GL_EQUAL );
		}
		else
		{
			qglDepthFunc( GL_LEQUAL );
		}
	}

	//
	// check blend bits
	//
	if ( diff & GLS_BLEND_BITS )
	{
		GLenum srcFactor = GL_ONE, dstFactor = GL_ONE;

		if ( stateBits & GLS_BLEND_BITS )
		{
			switch ( stateBits & GLS_SRCBLEND_BITS )
			{
			case GLS_SRCBLEND_ZERO:
				srcFactor = GL_ZERO;
				break;
			case GLS_SRCBLEND_ONE:
				srcFactor = GL_ONE;
				break;
			case GLS_SRCBLEND_DST_COLOR:
				srcFactor = GL_DST_COLOR;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:
				srcFactor = GL_ONE_MINUS_DST_COLOR;
				break;
			case GLS_SRCBLEND_SRC_ALPHA:
				srcFactor = GL_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:
				srcFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_SRCBLEND_DST_ALPHA:
				srcFactor = GL_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:
				srcFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			case GLS_SRCBLEND_ALPHA_SATURATE:
				srcFactor = GL_SRC_ALPHA_SATURATE;
				break;
			default:
				ri.Error( ERR_DROP, "GL_State: invalid src blend state bits" );
				break;
			}

			switch ( stateBits & GLS_DSTBLEND_BITS )
			{
			case GLS_DSTBLEND_ZERO:
				dstFactor = GL_ZERO;
				break;
			case GLS_DSTBLEND_ONE:
				dstFactor = GL_ONE;
				break;
			case GLS_DSTBLEND_SRC_COLOR:
				dstFactor = GL_SRC_COLOR;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:
				dstFactor = GL_ONE_MINUS_SRC_COLOR;
				break;
			case GLS_DSTBLEND_SRC_ALPHA:
				dstFactor = GL_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:
				dstFactor = GL_ONE_MINUS_SRC_ALPHA;
				break;
			case GLS_DSTBLEND_DST_ALPHA:
				dstFactor = GL_DST_ALPHA;
				break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:
				dstFactor = GL_ONE_MINUS_DST_ALPHA;
				break;
			default:
				ri.Error( ERR_DROP, "GL_State: invalid dst blend state bits" );
				break;
			}

			qglEnable( GL_BLEND );
			qglBlendFunc( srcFactor, dstFactor );
		}
		else
		{
			qglDisable( GL_BLEND );
		}
	}

	//
	// check depthmask
	//
	if ( diff & GLS_DEPTHMASK_TRUE )
	{
		if ( stateBits & GLS_DEPTHMASK_TRUE )
		{
			qglDepthMask( GL_TRUE );
		}
		else
		{
			qglDepthMask( GL_FALSE );
		}
	}

	//
	// fill/line mode
	//
	if ( diff & GLS_POLYMODE_LINE )
	{
		if ( stateBits & GLS_POLYMODE_LINE )
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		}
		else
		{
			qglPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		}
	}

	//
	// depthtest
	//
	if ( diff & GLS_DEPTHTEST_DISABLE )
	{
		if ( stateBits & GLS_DEPTHTEST_DISABLE )
		{
			qglDisable( GL_DEPTH_TEST );
		}
		else
		{
			qglEnable( GL_DEPTH_TEST );
		}
	}

	//
	// alpha test
	//
	if ( diff & GLS_ATEST_BITS )
	{
		switch ( stateBits & GLS_ATEST_BITS )
		{
		case 0:
			qglDisable( GL_ALPHA_TEST );
			break;
		case GLS_ATEST_GT_0:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GREATER, 0.0f );
			break;
		case GLS_ATEST_LT_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_LESS, 0.5f );
			break;
		case GLS_ATEST_GE_80:
			qglEnable( GL_ALPHA_TEST );
			qglAlphaFunc( GL_GEQUAL, 0.5f );
			break;
		default:
			ri.Error( ERR_DROP, "GL_State: invalid alpha test bits" );
			break;
		}
	}

	glState.glStateBits = stateBits;
#endif // USE_VULKAN
}


#ifndef USE_VULKAN
void GL_ClientState( int unit, unsigned stateBits )
{
	unsigned diff = stateBits ^ glState.glClientStateBits[ unit ];

	if ( diff == 0 )
	{
		if ( stateBits )
		{
			GL_SelectClientTexture( unit );
		}
		return;
	}

	GL_SelectClientTexture( unit );

	if ( diff & CLS_COLOR_ARRAY )
	{
		if ( stateBits & CLS_COLOR_ARRAY )
			qglEnableClientState( GL_COLOR_ARRAY );
		else
			qglDisableClientState( GL_COLOR_ARRAY );
	}

	if ( diff & CLS_NORMAL_ARRAY )
	{
		if ( stateBits & CLS_NORMAL_ARRAY )
			qglEnableClientState( GL_NORMAL_ARRAY );
		else
			qglDisableClientState( GL_NORMAL_ARRAY );
	}

	if ( diff & CLS_TEXCOORD_ARRAY )
	{
		if ( stateBits & CLS_TEXCOORD_ARRAY )
			qglEnableClientState( GL_TEXTURE_COORD_ARRAY );
		else
			qglDisableClientState( GL_TEXTURE_COORD_ARRAY );
	}

	glState.glClientStateBits[ unit ] = stateBits;
}
#endif


static void RB_SetGL2D( void );

/*
================
RB_Hyperspace

A player has predicted a teleport, but hasn't arrived yet
================
*/
static void RB_Hyperspace( void ) {
	color4ub_t c;

	if ( !backEnd.isHyperspace ) {
		// do initialization shit
	}

	RB_SetGL2D();

	if ( tess.shader != tr.whiteShader || backEnd.currentEntity != &backEnd.entity2D ) {
		if ( tess.numIndexes ) {
			RB_EndSurface();
		}
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( tr.whiteShader, 0 );
	}

#ifdef USE_VBO
	VBO_UnBind();
#endif

	if ( r_teleporterFlash->integer == 0 ) {
		c.rgba[0] = c.rgba[1] = c.rgba[2] = 0; // fade to black
	} else {
		c.rgba[0] = c.rgba[1] = c.rgba[2] = (backEnd.refdef.time & 255); // fade to white
	}
	c.rgba[3] = 255;

	RB_AddQuadStamp2( backEnd.refdef.x, backEnd.refdef.y, backEnd.refdef.width, backEnd.refdef.height,
		0.0, 0.0, 0.0, 0.0, c );

	RB_EndSurface();

	tess.numIndexes = 0;
	tess.numVertexes = 0;

	backEnd.isHyperspace = qtrue;
}


static void SetViewportAndScissor( void ) {
#ifdef USE_VULKAN
	//Com_Memcpy( vk_world.modelview_transform, backEnd.or.modelViewMatrix, 64 );
	//vk_update_mvp();
	// force depth range and viewport/scissor updates
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;
#else
	qglMatrixMode(GL_PROJECTION);
	qglLoadMatrixf( backEnd.viewParms.projectionMatrix );
	qglMatrixMode(GL_MODELVIEW);

	// set the window clipping
	qglViewport( backEnd.viewParms.viewportX, backEnd.viewParms.viewportY,
		backEnd.viewParms.viewportWidth, backEnd.viewParms.viewportHeight );
	qglScissor( backEnd.viewParms.scissorX, backEnd.viewParms.scissorY,
		backEnd.viewParms.scissorWidth, backEnd.viewParms.scissorHeight );
#endif
}


/*
=================
RB_BeginDrawingView

Any mirrored or portaled views have already been drawn, so prepare
to actually render the visible surfaces for this view
=================
*/
static void RB_BeginDrawingView( void ) {
#ifndef USE_VULKAN
	int clearBits = 0;
#endif

	// sync with gl if needed
	if ( r_finish->integer == 1 && !glState.finishCalled ) {
#ifdef USE_VULKAN
		vk_queue_wait_idle();
#else
		qglFinish();
#endif
		glState.finishCalled = qtrue;
	} else if ( r_finish->integer == 0 ) {
		glState.finishCalled = qtrue;
	}

	// we will need to change the projection matrix before drawing
	// 2D images again
	backEnd.projection2D = qfalse;

	//
	// set the modelview matrix for the viewer
	//
	SetViewportAndScissor();

#ifdef USE_VULKAN
	vk_clear_depth( r_shadows->integer == 2 ? qtrue : qfalse );
#else
	// ensures that depth writes are enabled for the depth clear
	GL_State( GLS_DEFAULT );

	// clear relevant buffers
	clearBits = GL_DEPTH_BUFFER_BIT;

	if ( r_shadows->integer == 2 )
	{
		clearBits |= GL_STENCIL_BUFFER_BIT;
	}
	qglClear( clearBits );
#endif

	if ( backEnd.refdef.rdflags & RDF_HYPERSPACE ) {
		RB_Hyperspace();
		backEnd.projection2D = qfalse;
		SetViewportAndScissor();
	} else {
		backEnd.isHyperspace = qfalse;
	}

	glState.faceCulling = -1;		// force face culling to set next time

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;
}

static void RB_LightingPass( void );
#ifdef USE_VULKAN
static qboolean s_skipDeferredWeaponSurfaces;
static qboolean s_drawDeferredWeaponSurfacesOnly;
#endif

/*
==================
RB_RenderDrawSurfList
==================
*/
void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	shader_t		*shader, *oldShader;
	int				fogNum;
	int				entityNum, oldEntityNum;
	int				dlighted;
	qboolean		depthRange, isCrosshair;
#ifndef USE_VULKAN
	qboolean		oldDepthRange, wasCrosshair;
#endif
	int				i;
	drawSurf_t		*drawSurf;
	unsigned int	oldSort;
	float			oldShaderSort;
	double			originalTime; // -EC-

	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;

	// draw everything
	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
#ifndef USE_VULKAN
	oldDepthRange = qfalse;
	wasCrosshair = qfalse;
#endif
	oldSort = MAX_UINT;
	oldShaderSort = -1;
	depthRange = qfalse;

	backEnd.pc.c_surfaces += numDrawSurfs;
#ifdef USE_VULKAN
	backEnd.visDrawId = 0;
	vk_vegetation_clear_staging();
#endif

	for (i = 0, drawSurf = drawSurfs ; i < numDrawSurfs ; i++, drawSurf++) {
		if ( drawSurf->sort == oldSort
#ifdef USE_VULKAN
			&& !s_skipDeferredWeaponSurfaces && !s_drawDeferredWeaponSurfacesOnly
#endif
			) {
			// fast path, same as previous sort
			{
				int oldNumVertexes = tess.numVertexes;
				rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
#ifdef USE_VULKAN
				if ( oldShader && ( oldShader->surfaceFlags & SURF_VEGETATION ) )
					vk_vegetation_add_from_tess( oldNumVertexes, tess.numVertexes );
#endif
			}
			continue;
		}

		R_DecomposeSort( drawSurf->sort, &entityNum, &shader, &fogNum, &dlighted );
#ifdef USE_VULKAN
		if ( s_skipDeferredWeaponSurfaces || s_drawDeferredWeaponSurfacesOnly ) {
			qboolean firstPersonSurface = qfalse;
			if ( entityNum != REFENTITYNUM_WORLD && entityNum >= 0 &&
				entityNum < backEnd.refdef.num_entities ) {
				firstPersonSurface =
					( backEnd.refdef.entities[entityNum].e.renderfx & RF_FIRST_PERSON )
						? qtrue : qfalse;
			}
			if ( ( s_skipDeferredWeaponSurfaces && firstPersonSurface ) ||
				( s_drawDeferredWeaponSurfacesOnly && !firstPersonSurface ) ) {
				continue;
			}
		}
		if ( backEnd.depthOnlyWorldPass && entityNum != REFENTITYNUM_WORLD ) {
			continue;  /* occlusion pass: world depth only */
		}
		if ( backEnd.drawSurfFilter ) {
			unsigned stageBits = shader->stages[0] ? shader->stages[0]->stateBits : 0;
			unsigned srcBlend = stageBits & GLS_SRCBLEND_BITS;
			unsigned dstBlend = stageBits & GLS_DSTBLEND_BITS;
			qboolean additive = ( srcBlend == GLS_SRCBLEND_ONE && dstBlend == GLS_DSTBLEND_ONE );
			qboolean transparent = (
				( shader->sort >= SS_BLEND0 && shader->sort <= SS_BLEND6 ) ||
				( srcBlend == GLS_SRCBLEND_SRC_ALPHA && dstBlend == GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA ) ||
				additive
			);
			if ( backEnd.drawSurfFilter == 1 && transparent )
				continue;  /* opaque only: skip transparent */
			if ( backEnd.drawSurfFilter == 2 && !transparent )
				continue;  /* transparent only: skip opaque */
			/* World OIT must not accumulate first-person / depth-hack weapon geometry. */
			if ( backEnd.drawSurfFilter == 2 &&
				( backEnd.oitAccumPass || backEnd.oitMomentsPass ) &&
				entityNum != REFENTITYNUM_WORLD ) {
				const int rfx = backEnd.refdef.entities[ entityNum ].e.renderfx;
				if ( rfx & ( RF_FIRST_PERSON | RF_DEPTHHACK ) )
					continue;
			}
			/* OIT class buckets: 1=alpha-blend, 2=additive */
			if ( backEnd.drawSurfFilter == 2 && backEnd.oitBucketFilter == 1 && additive )
				continue;
			if ( backEnd.drawSurfFilter == 2 && backEnd.oitBucketFilter == 2 && !additive )
				continue;
			/* Raster Ultra 1.4: refractive/screenMap never enter WBOIT/MBOIT. */
			if ( backEnd.drawSurfFilter == 2 &&
				( backEnd.oitAccumPass || backEnd.oitMomentsPass ) &&
				vk_transparency_refractive_exclude_oit() &&
				vk_transparency_is_refractive( shader ) ) {
				continue;
			}
			/* Sorted refractive-only pass after OIT resolve. */
			if ( backEnd.drawSurfFilter == 2 && backEnd.refractiveOnlyPass &&
				!vk_transparency_is_refractive( shader ) ) {
				continue;
			}
			if ( backEnd.drawSurfFilter == 2 && backEnd.skipRefractivePass &&
				vk_transparency_is_refractive( shader ) ) {
				continue;
			}
		}
		if ( ( vk.renderPassIndex == RENDER_PASS_SCREENMAP || vk.renderPassIndex == RENDER_PASS_SUN_SHADOW ) &&
			entityNum != REFENTITYNUM_WORLD && backEnd.refdef.entities[ entityNum ].e.renderfx & RF_DEPTHHACK ) {
			continue;
		}
#endif
		//
		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from separate
		// entities merged into a single batch, like smoke and blood puff sprites
		if ( ( (oldSort ^ drawSurfs->sort ) & ~QSORT_REFENTITYNUM_MASK ) || !shader->entityMergable ) {
			//if ( oldShader != NULL ) {
				RB_EndSurface();
			//}
			#define INSERT_POINT SS_FOG
			if ( backEnd.refdef.numLitSurfs && oldShaderSort < INSERT_POINT && shader->sort >= INSERT_POINT ) {
				//RB_BeginDrawingLitSurfs(); // no need, already setup in RB_BeginDrawingView()
#ifdef USE_VULKAN
				RB_LightingPass();
#else
				if ( depthRange ) {
					qglDepthRange( 0, 1 );
					RB_LightingPass();
					qglDepthRange( 0, 0.3 );
				} else {
					RB_LightingPass();
				}
#endif
				oldEntityNum = -1; // force matrix setup
			}
			oldShaderSort = shader->sort;
			RB_BeginSurface( shader, fogNum );
			oldShader = shader;
		}

		oldSort = drawSurf->sort;

		//
		// change the modelview matrix if needed
		//
		if ( entityNum != oldEntityNum ) {
			depthRange = isCrosshair = qfalse;

			if ( entityNum != REFENTITYNUM_WORLD ) {
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];
				if ( backEnd.currentEntity->intShaderTime )
					backEnd.refdef.floatTime = originalTime - (double)(backEnd.currentEntity->e.shaderTime.i) * 0.001;
				else
					backEnd.refdef.floatTime = originalTime - (double)backEnd.currentEntity->e.shaderTime.f;

				// set up the transformation matrix
				R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.or );
				// set up the dynamic lighting if needed
				if ( !r_dlightMode->integer )
				if ( backEnd.currentEntity->needDlights ) {
					R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.or );
				}
				if ( backEnd.currentEntity->e.renderfx & RF_DEPTHHACK ) {
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;

					if(backEnd.currentEntity->e.renderfx & RF_CROSSHAIR)
						isCrosshair = qtrue;
				}
			} else {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.or = backEnd.viewParms.world;
				if ( !r_dlightMode->integer )
				R_TransformDlights( backEnd.refdef.num_dlights, backEnd.refdef.dlights, &backEnd.or );
			}

			// we have to reset the shaderTime as well otherwise image animations on
			// the world (like water) continue with the wrong frame
			tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

#ifdef USE_VULKAN
			backEnd.useFirstPersonProjection = qfalse;
			if ( depthRange && !isCrosshair ) {
				if ( r_firstPersonFovEnabled->integer && r_firstPersonFov->value > 0.0f ) {
					R_SetupFirstPersonProjection( &backEnd.viewParms, backEnd.firstPersonProjectionMatrix );
					backEnd.useFirstPersonProjection = qtrue;
					vk_temporal_note_first_person_projection();
				}
				if ( r_firstPersonScaleEnabled->integer && r_firstPersonScale->value > 0.0f && r_firstPersonScale->value != 1.0f ) {
					float s = r_firstPersonScale->value;
					float *m = backEnd.or.modelViewMatrix;
					float scaled[16];
					scaled[0]  = m[0]  * s; scaled[1]  = m[1]  * s; scaled[2]  = m[2]  * s; scaled[3]  = m[3];
					scaled[4]  = m[4]  * s; scaled[5]  = m[5]  * s; scaled[6]  = m[6]  * s; scaled[7]  = m[7];
					scaled[8]  = m[8]  * s; scaled[9]  = m[9]  * s; scaled[10] = m[10] * s; scaled[11] = m[11];
					scaled[12] = m[12] * s; scaled[13] = m[13] * s; scaled[14] = m[14] * s; scaled[15] = m[15];
					Com_Memcpy( vk_world.modelview_transform, scaled, 64 );
				} else {
					Com_Memcpy( vk_world.modelview_transform, backEnd.or.modelViewMatrix, 64 );
				}
			} else {
				Com_Memcpy( vk_world.modelview_transform, backEnd.or.modelViewMatrix, 64 );
			}
			tess.depthRange = depthRange ? DEPTH_RANGE_WEAPON : DEPTH_RANGE_NORMAL;
			vk_update_mvp( NULL );
#else
			qglLoadMatrixf( backEnd.or.modelViewMatrix );
#endif

			//
			// change depthrange. Also change projection matrix so first person weapon does not look like coming
			// out of the screen.
			//
#ifndef USE_VULKAN
			if (oldDepthRange != depthRange || wasCrosshair != isCrosshair)
			{
				if (depthRange)
				{
					if(backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						if(isCrosshair)
						{
							if(oldDepthRange)
							{
								// was not a crosshair but now is, change back proj matrix
								qglMatrixMode(GL_PROJECTION);
								qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
								qglMatrixMode(GL_MODELVIEW);
							}
						}
						else
						{
							viewParms_t temp = backEnd.viewParms;

							R_SetupProjection(&temp, r_znear->value, qfalse);

							qglMatrixMode(GL_PROJECTION);
							qglLoadMatrixf(temp.projectionMatrix);
							qglMatrixMode(GL_MODELVIEW);
						}
					}

					if(!oldDepthRange)
						qglDepthRange (0, 0.3);
				}
				else
				{
					if(!wasCrosshair && backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						qglMatrixMode(GL_PROJECTION);
						qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
						qglMatrixMode(GL_MODELVIEW);
					}

					qglDepthRange (0, 1);
				}
				oldDepthRange = depthRange;
				wasCrosshair = isCrosshair;
			}
#endif

			oldEntityNum = entityNum;
		}

		// add the triangles for this surface
		{
			int oldNumVertexes = tess.numVertexes;
			rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
#ifdef USE_VULKAN
			if ( shader && ( shader->surfaceFlags & SURF_VEGETATION ) )
				vk_vegetation_add_from_tess( oldNumVertexes, tess.numVertexes );
#endif
		}
	}

	// draw the contents of the last shader batch
	if ( oldShader != NULL ) {
		RB_EndSurface();
	}

	backEnd.refdef.floatTime = originalTime;

	// go back to the world modelview matrix
#ifdef USE_VULKAN
	Com_Memcpy( vk_world.modelview_transform, backEnd.viewParms.world.modelViewMatrix, 64 );
	tess.depthRange = DEPTH_RANGE_NORMAL;
	backEnd.useFirstPersonProjection = qfalse;
	//vk_update_mvp();
#else
	qglLoadMatrixf( backEnd.viewParms.world.modelViewMatrix );
	if ( depthRange ) {
		qglDepthRange(0, 1);
	}
#endif
}


/*
=================
RB_BeginDrawingLitView
=================
*/
static void RB_BeginDrawingLitSurfs( void )
{
	// we will need to change the projection matrix before drawing
	// 2D images again
	backEnd.projection2D = qfalse;

	// we will only draw a sun if there was sky rendered in this view
	backEnd.skyRenderedThisView = qfalse;

	//
	// set the modelview matrix for the viewer
	//
	SetViewportAndScissor();

	glState.faceCulling = -1;		// force face culling to set next time
}


/*
==================
RB_RenderLitSurfList
==================
*/
static void RB_RenderLitSurfList( dlight_t* dl ) {
	shader_t		*shader, *oldShader;
	int				fogNum;
	int				entityNum, oldEntityNum;
#ifndef USE_VULKAN
	qboolean		oldDepthRange, wasCrosshair;
#endif
	qboolean		depthRange, isCrosshair;
	const litSurf_t	*litSurf;
	unsigned int	oldSort;
	double			originalTime; // -EC-

	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;

	// draw everything
	oldEntityNum = -1;
	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
#ifndef USE_VULKAN
	oldDepthRange = qfalse;
	wasCrosshair = qfalse;
#endif
	oldSort = MAX_UINT;
	depthRange = qfalse;

	tess.dlightUpdateParams = qtrue;

	for ( litSurf = dl->head; litSurf; litSurf = litSurf->next ) {
		//if ( litSurf->sort == sort ) {
		if ( litSurf->sort == oldSort ) {
			// fast path, same as previous sort
			rb_surfaceTable[ *litSurf->surface ]( litSurf->surface );
			continue;
		}

		R_DecomposeLitSort( litSurf->sort, &entityNum, &shader, &fogNum );
#ifdef USE_VULKAN
		if ( ( vk.renderPassIndex == RENDER_PASS_SCREENMAP || vk.renderPassIndex == RENDER_PASS_SUN_SHADOW ) &&
			entityNum != REFENTITYNUM_WORLD && backEnd.refdef.entities[ entityNum ].e.renderfx & RF_DEPTHHACK ) {
			continue;
		}
#endif
		// anything BEFORE opaque is sky/portal, anything AFTER it should never have been added
		//assert( shader->sort == SS_OPAQUE );
		// !!! but MIRRORS can trip that assert, so just do this for now
		//if ( shader->sort < SS_OPAQUE )
		//	continue;

		//
		// change the tess parameters if needed
		// a "entityMergable" shader is a shader that can have surfaces from separate
		// entities merged into a single batch, like smoke and blood puff sprites
		if ( ( (oldSort ^ litSurf->sort) & ~QSORT_REFENTITYNUM_MASK ) || !shader->entityMergable ) {
			if ( oldShader != NULL ) {
				RB_EndSurface();
			}
			RB_BeginSurface( shader, fogNum );
			oldShader = shader;
		}

		oldSort = litSurf->sort;

		//
		// change the modelview matrix if needed
		//
		if ( entityNum != oldEntityNum ) {
			depthRange = isCrosshair = qfalse;

			if ( entityNum != REFENTITYNUM_WORLD ) {
				backEnd.currentEntity = &backEnd.refdef.entities[entityNum];

				if ( backEnd.currentEntity->intShaderTime )
					backEnd.refdef.floatTime = originalTime - (double)(backEnd.currentEntity->e.shaderTime.i) * 0.001;
				else
					backEnd.refdef.floatTime = originalTime - (double)backEnd.currentEntity->e.shaderTime.f;

				// set up the transformation matrix
				R_RotateForEntity( backEnd.currentEntity, &backEnd.viewParms, &backEnd.or );

				if ( backEnd.currentEntity->e.renderfx & RF_DEPTHHACK ) {
					// hack the depth range to prevent view model from poking into walls
					depthRange = qtrue;

					if(backEnd.currentEntity->e.renderfx & RF_CROSSHAIR)
						isCrosshair = qtrue;
				}
			} else {
				backEnd.currentEntity = &tr.worldEntity;
				backEnd.refdef.floatTime = originalTime;
				backEnd.or = backEnd.viewParms.world;
			}

			// we have to reset the shaderTime as well otherwise image animations on
			// the world (like water) continue with the wrong frame
			tess.shaderTime = backEnd.refdef.floatTime - tess.shader->timeOffset;

			// set up the dynamic lighting
			R_TransformDlights( 1, dl, &backEnd.or );
			tess.dlightUpdateParams = qtrue;

#ifdef USE_VULKAN
			backEnd.useFirstPersonProjection = qfalse;
			if ( depthRange && !isCrosshair ) {
				if ( r_firstPersonFovEnabled->integer && r_firstPersonFov->value > 0.0f ) {
					R_SetupFirstPersonProjection( &backEnd.viewParms, backEnd.firstPersonProjectionMatrix );
					backEnd.useFirstPersonProjection = qtrue;
					vk_temporal_note_first_person_projection();
				}
				if ( r_firstPersonScaleEnabled->integer && r_firstPersonScale->value > 0.0f && r_firstPersonScale->value != 1.0f ) {
					float s = r_firstPersonScale->value;
					float *m = backEnd.or.modelViewMatrix;
					float scaled[16];
					scaled[0]  = m[0]  * s; scaled[1]  = m[1]  * s; scaled[2]  = m[2]  * s; scaled[3]  = m[3];
					scaled[4]  = m[4]  * s; scaled[5]  = m[5]  * s; scaled[6]  = m[6]  * s; scaled[7]  = m[7];
					scaled[8]  = m[8]  * s; scaled[9]  = m[9]  * s; scaled[10] = m[10] * s; scaled[11] = m[11];
					scaled[12] = m[12] * s; scaled[13] = m[13] * s; scaled[14] = m[14] * s; scaled[15] = m[15];
					Com_Memcpy( vk_world.modelview_transform, scaled, 64 );
				} else {
					Com_Memcpy( vk_world.modelview_transform, backEnd.or.modelViewMatrix, 64 );
				}
			} else {
				Com_Memcpy( vk_world.modelview_transform, backEnd.or.modelViewMatrix, 64 );
			}
			tess.depthRange = depthRange ? DEPTH_RANGE_WEAPON : DEPTH_RANGE_NORMAL;
			vk_update_mvp( NULL );
#else
			qglLoadMatrixf( backEnd.or.modelViewMatrix );

			//
			// change depthrange. Also change projection matrix so first person weapon does not look like coming
			// out of the screen.
			//

			if (oldDepthRange != depthRange || wasCrosshair != isCrosshair)
			{
				if (depthRange)
				{
					if(backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						if(isCrosshair)
						{
							if(oldDepthRange)
							{
								// was not a crosshair but now is, change back proj matrix
								qglMatrixMode(GL_PROJECTION);
								qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
								qglMatrixMode(GL_MODELVIEW);
							}
						}
						else
						{
							viewParms_t temp = backEnd.viewParms;

							R_SetupProjection(&temp, r_znear->value, qfalse);

							qglMatrixMode(GL_PROJECTION);
							qglLoadMatrixf(temp.projectionMatrix);
							qglMatrixMode(GL_MODELVIEW);
						}
					}

					if(!oldDepthRange)
						qglDepthRange (0, 0.3);
				}
				else
				{
					if(!wasCrosshair && backEnd.viewParms.stereoFrame != STEREO_CENTER)
					{
						qglMatrixMode(GL_PROJECTION);
						qglLoadMatrixf(backEnd.viewParms.projectionMatrix);
						qglMatrixMode(GL_MODELVIEW);
					}

					qglDepthRange (0, 1);
				}
				oldDepthRange = depthRange;
				wasCrosshair = isCrosshair;
			}
#endif

			oldEntityNum = entityNum;
		}

		// add the triangles for this surface
		rb_surfaceTable[ *litSurf->surface ]( litSurf->surface );
	}

	// draw the contents of the last shader batch
	if ( oldShader != NULL ) {
		RB_EndSurface();
	}

	backEnd.refdef.floatTime = originalTime;

	// go back to the world modelview matrix
#ifdef USE_VULKAN
	Com_Memcpy( vk_world.modelview_transform, backEnd.viewParms.world.modelViewMatrix, 64 );
	tess.depthRange = DEPTH_RANGE_NORMAL;
	backEnd.useFirstPersonProjection = qfalse;
	//vk_update_mvp();
#else
	qglLoadMatrixf( backEnd.viewParms.world.modelViewMatrix );
	if ( depthRange ) {
		qglDepthRange (0, 1);
	}
#endif // !USE_VULKAN
}


/*
============================================================================

RENDER BACK END FUNCTIONS

============================================================================
*/

/*
================
RB_SetGL2D
================
*/
static void RB_SetGL2D( void ) {
#ifdef USE_VULKAN
	// Finalize world fog right at the 3D -> 2D boundary.
	vk_prepare_2d();
#endif

	backEnd.projection2D = qtrue;
	backEnd.currentEntity = &backEnd.entity2D;

#ifdef USE_VULKAN
	vk_update_mvp( NULL );

	// force depth range and viewport/scissor updates
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;

	/*
	 * Match the legacy 2D contract even though Vulkan does not drive fixed-function
	 * GL state directly: 2D expects no depth test, standard alpha blending, and
	 * two-sided culling semantics.
	 */
	glState.glStateBits = GLS_DEPTHTEST_DISABLE |
		GLS_SRCBLEND_SRC_ALPHA |
		GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	GL_Cull( CT_TWO_SIDED );
#else
	// set 2D virtual screen size
	qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglMatrixMode( GL_PROJECTION );
	qglLoadMatrixf( GL_Ortho( 0, glConfig.vidWidth, glConfig.vidHeight, 0, 0, 1 ) );
	qglMatrixMode( GL_MODELVIEW );
	qglLoadIdentity();

	GL_State( GLS_DEPTHTEST_DISABLE |
		GLS_SRCBLEND_SRC_ALPHA |
		GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA );

	GL_Cull( CT_TWO_SIDED );
	qglDisable( GL_CLIP_PLANE0 );
#endif

	// set time for 2D shaders
	backEnd.refdef.time = ri.Milliseconds();
	backEnd.refdef.floatTime = (double)backEnd.refdef.time * 0.001; // -EC-: cast to double
}


/*
=============
RE_StretchRaw

Note: Function is screen/bitmap related, not strictly backend.
Stretches a raw 32 bit bitmap image over the given screen rectangle.
Used for cinematics. Modern video decoders can produce non-power-of-two frames.
=============
*/
void RE_StretchRaw( int x, int y, int w, int h, int cols, int rows, byte *data, int client, qboolean dirty ) {
	int			start, end;

	if ( !tr.registered ) {
		return;
	}

	start = 0;
	if ( r_speeds->integer ) {
		start = ri.Milliseconds();
	}

	RE_UploadCinematic( w, h, cols, rows, data, client, dirty );

	if ( r_speeds->integer ) {
		end = ri.Milliseconds();
		ri.Printf( PRINT_ALL, "RE_UploadCinematic( %i, %i ): %i msec\n", cols, rows, end - start );
	}

	tr.cinematicShader->stages[0]->bundle[0].image[0] = tr.scratchImage[client];
	RE_StretchPic( x, y, w, h, 0.5f / cols, 0.5f / rows, 1.0f - 0.5f / cols, 1.0f - 0.5 / rows, tr.cinematicShader->index );
}


void RE_UploadCinematic( int w, int h, int cols, int rows, byte *data, int client, qboolean dirty ) {

	image_t *image;
	(void)w;
	(void)h;

	if ( !tr.scratchImage[ client ] ) {
		tr.scratchImage[ client ] = R_CreateImage( va( "*scratch%i", client ), NULL, data, cols, rows, IMGFLAG_CLAMPTOEDGE | IMGFLAG_RGB | IMGFLAG_NOSCALE, 0, 0 );
		return;
	}

	image = tr.scratchImage[ client ];

#ifndef USE_VULKAN
	GL_Bind( image );
#endif

	// if the scratchImage isn't in the format we want, specify it as a new texture
	if ( cols != image->width || rows != image->height ) {
		image->width = image->uploadWidth = cols;
		image->height = image->uploadHeight = rows;
#ifdef USE_VULKAN
		vk_create_image( image, cols, rows, 1 );
		vk_upload_image_data( image, 0, 0, cols, rows, 1, data, cols * rows * 4, qfalse );
#else
		qglTexImage2D( GL_TEXTURE_2D, 0, image->internalFormat, cols, rows, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, gl_clamp_mode );
		qglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, gl_clamp_mode );
#endif
	} else if ( dirty ) {
		// otherwise, just subimage upload it so that drivers can tell we are going to be changing
		// it and don't try and do a texture compression
#ifdef USE_VULKAN
		vk_upload_image_data( image, 0, 0, cols, rows, 1, data, cols * rows * 4, qtrue );
#else
		qglTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, cols, rows, GL_RGBA, GL_UNSIGNED_BYTE, data );
#endif
	}
}


/*
=============
RB_SetColor
=============
*/
static const void *RB_SetColor( const void *data ) {
	const setColorCommand_t	*cmd;

	cmd = (const setColorCommand_t *)data;

	backEnd.color2D.rgba[0] = cmd->color[0] * 255;
	backEnd.color2D.rgba[1] = cmd->color[1] * 255;
	backEnd.color2D.rgba[2] = cmd->color[2] * 255;
	backEnd.color2D.rgba[3] = cmd->color[3] * 255;

	return (const void *)(cmd + 1);
}


/*
=============
RB_StretchPic
=============
*/
static const void *RB_StretchPic( const void *data ) {
	const stretchPicCommand_t	*cmd;
	shader_t *shader;

	cmd = (const stretchPicCommand_t *)data;

	if ( !backEnd.projection2D ) {
		if ( tess.numIndexes ) {
			RB_EndSurface();
		}
		RB_SetGL2D();
	}

	shader = cmd->shader;
	if ( shader != tess.shader || backEnd.currentEntity != &backEnd.entity2D ) {
		if ( tess.numIndexes ) {
			RB_EndSurface();
		}
		backEnd.currentEntity = &backEnd.entity2D;
		RB_BeginSurface( shader, 0 );
	}

#ifdef USE_VBO
	VBO_UnBind();
#endif

	RB_AddQuadStamp2( cmd->x, cmd->y, cmd->w, cmd->h, cmd->s1, cmd->t1, cmd->s2, cmd->t2, backEnd.color2D );
	if ( cmd->vectorCurveCount > 0 ) {
		tess.vectorCurveStart = cmd->vectorCurveStart;
		tess.vectorCurveCount = cmd->vectorCurveCount;
		tess.vectorCurveTexWidth = R_VectorFont_CurveTexWidth();
		tess.sdfUiEdge = -1.0f;
		tess.subpixelShift = -1.0f;
	} else if ( cmd->subpixelShift >= 0.0f ) {
		tess.vectorCurveStart = 0;
		tess.vectorCurveCount = 0;
		tess.sdfUiEdge = -1.0f;
		tess.subpixelShift = cmd->subpixelShift;
		tess.subpixelInvTexWidth = cmd->subpixelInvTexWidth;
	} else {
		tess.vectorCurveStart = 0;
		tess.vectorCurveCount = 0;
		tess.sdfUiEdge = cmd->sdfSmoothing;
		tess.subpixelShift = -1.0f;
	}

	return (const void *)(cmd + 1);
}

/*
=============
RB_VectorFontString
=============
*/
static const void *RB_VectorFontString( const void *data ) {
	const vectorFontStringCommand_t *cmd;

	cmd = (const vectorFontStringCommand_t *)data;

	if ( !backEnd.projection2D ) {
		if ( tess.numIndexes ) {
			RB_EndSurface();
		}
		RB_SetGL2D();
	}

#ifdef USE_VBO
	VBO_UnBind();
#endif

	if ( cmd->color[3] > 0.0f ) {
		RE_SetColor( cmd->color );
	}
	VK_VectorFont_DrawString( cmd->x, cmd->y, cmd->scale, cmd->text, cmd->color, cmd->shadowOff );
	RE_SetColor( NULL );
	return (const void *)(cmd + 1);
}


static void RB_LightingPass( void )
{
	dlight_t	*dl;
	int	i;

#ifdef USE_VBO
	//VBO_Flush();
	//tess.allowVBO = qfalse; // for now
#endif

	tess.dlightPass = qtrue;

	for ( i = 0; i < (int)backEnd.viewParms.num_dlights; i++ )
	{
		dl = &backEnd.viewParms.dlights[i];
		if ( dl->head )
		{
			tess.light = dl;
			RB_RenderLitSurfList( dl );
		}
	}

	tess.dlightPass = qfalse;

	backEnd.viewParms.num_dlights = 0;
}


static void transform_to_eye_space( const vec3_t v, vec3_t v_eye )
{
	const float *m = backEnd.viewParms.world.modelViewMatrix;
	v_eye[0] = m[0]*v[0] + m[4]*v[1] + m[8 ]*v[2] + m[12];
	v_eye[1] = m[1]*v[0] + m[5]*v[1] + m[9 ]*v[2] + m[13];
	v_eye[2] = m[2]*v[0] + m[6]*v[1] + m[10]*v[2] + m[14];
}


/*
================
RB_DebugPolygon
================
*/
static void RB_DebugPolygon( int color, int numPoints, float *points ) {
	vec3_t pa;
	vec3_t pb;
	vec3_t p;
	vec3_t q;
	vec3_t n;
	int i;

	if ( numPoints < 3 ) {
		return;
	}

	transform_to_eye_space( &points[0], pa );
	transform_to_eye_space( &points[3], pb );
	VectorSubtract( pb, pa, p );

	for ( i = 2; i < numPoints; i++ ) {
		transform_to_eye_space( &points[3*i], pb );
		VectorSubtract( pb, pa, q );
		CrossProduct( q, p, n );
		if ( VectorLength( n ) > 1e-5 ) {
			break;
		}
	}

	if ( DotProduct( n, pa ) >= 0 ) {
		return; // discard backfacing polygon
	}

#ifdef USE_VULKAN
	// Solid shade.
	for (i = 0; i < numPoints; i++) {
		VectorCopy(&points[3*i], tess.xyz[i]);

		tess.svars.colors[0][i].rgba[0] = (color&1) ? 255 : 0;
		tess.svars.colors[0][i].rgba[1] = (color&2) ? 255 : 0;
		tess.svars.colors[0][i].rgba[2] = (color&4) ? 255 : 0;
		tess.svars.colors[0][i].rgba[3] = 255;
	}
	tess.numVertexes = numPoints;

	tess.numIndexes = 0;
	for (i = 1; i < numPoints - 1; i++) {
		tess.indexes[tess.numIndexes + 0] = 0;
		tess.indexes[tess.numIndexes + 1] = i;
		tess.indexes[tess.numIndexes + 2] = i + 1;
		tess.numIndexes += 3;
	}

	vk_bind_index();
	vk_bind_pipeline( vk.surface_debug_pipeline_solid );
	vk_bind_geometry( TESS_XYZ | TESS_RGBA0 | TESS_ST0 );
	vk_draw_geometry( DEPTH_RANGE_NORMAL, qtrue );

	// Outline.
	Com_Memset( tess.svars.colors[0], tr.identityLightByte, numPoints * 2 * sizeof( color4ub_t ) );

	for ( i = 0; i < numPoints; i++ ) {
		VectorCopy( &points[3*i], tess.xyz[2*i] );
		VectorCopy( &points[3*((i + 1) % numPoints)], tess.xyz[2*i + 1] );
	}
	tess.numVertexes = numPoints * 2;
	tess.numIndexes = 0;

	vk_bind_pipeline( vk.surface_debug_pipeline_outline );
	vk_bind_geometry( TESS_XYZ | TESS_RGBA0 );
	vk_draw_geometry( DEPTH_RANGE_ZERO, qfalse );
	tess.numVertexes = 0;
#else
	GL_SelectTexture( 0 );
	qglDisable( GL_TEXTURE_2D );

	GL_ClientState( 0, CLS_NONE );
	qglVertexPointer( 3, GL_FLOAT, 0, points );

	// draw solid shade
	GL_State( GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE );
	qglColor4f( color&1, (color>>1)&1, (color>>2)&1, 1 );
	qglDrawArrays( GL_TRIANGLE_FAN, 0, numPoints );

	// draw wireframe outline
	qglDepthRange( 0, 0 );
	qglColor4f( 1, 1, 1, 1 );
	qglDrawArrays( GL_LINE_LOOP, 0, numPoints );
	qglDepthRange( 0, 1 );

	qglEnable( GL_TEXTURE_2D );
#endif
}


/*
====================
RB_DebugGraphics

Visualization aid for movement clipping debugging
====================
*/
static void RB_DebugGraphics( void ) {

	if ( !r_debugSurface->integer ) {
		return;
	}

	GL_Bind( tr.whiteImage );
#ifdef USE_VULKAN
	vk_update_mvp( NULL );
#else
	GL_Cull( CT_FRONT_SIDED );
#endif
	ri.CM_DrawDebugSurface( RB_DebugPolygon );
}

#ifdef USE_VULKAN
static const char *RB_RenderPassName( renderPass_t pass )
{
	switch ( pass ) {
	case RENDER_PASS_MAIN:
		return "main";
	case RENDER_PASS_SCREENMAP:
		return "screenmap";
	case RENDER_PASS_SUN_SHADOW:
		return "sun_shadow";
	case RENDER_PASS_POST_BLOOM:
		return "post_bloom";
	case RENDER_PASS_UI_OVERLAY:
		return "ui_overlay";
	case RENDER_PASS_CUBEMAP:
		return "cubemap";
	default:
		return "unknown_pass";
	}
}

static void RB_ValidateUnifiedClusteredTransparentHandoff( qboolean usingOit )
{
	if ( !vk_unified_clustered_active() || !r_fboDebug || r_fboDebug->integer < 1 ) {
		return;
	}

	if ( !vk_deferred_lighting_active() ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][unified] transparent handoff: deferred lighting inactive before %s path\n",
			usingOit ? "OIT" : "Forward+" );
	}

	if ( backEnd.drawSurfFilter != 2 ) {
		ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
			"[VK][unified] transparent handoff: expected drawSurfFilter=2 before %s path, got %d\n",
			usingOit ? "OIT" : "Forward+", backEnd.drawSurfFilter );
	}

	if ( usingOit ) {
		if ( vk.inRenderPass ) {
			ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
				"[VK][unified] OIT handoff: expected no active render pass before vk_oit_pass, still in %s\n",
				RB_RenderPassName( vk.renderPassIndex ) );
		}
	} else {
		if ( !vk.inRenderPass || vk.renderPassIndex != RENDER_PASS_MAIN ) {
			ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
				"[VK][unified] transparent Forward+ handoff: expected active main render pass, got inRenderPass=%d active=%s\n",
				vk.inRenderPass ? 1 : 0, RB_RenderPassName( vk.renderPassIndex ) );
		}
	}
}

#ifdef USE_VULKAN
/*
===============
RB_DrawRefractiveAfterOit

Raster Ultra 1.4: sorted refractive/screenMap after WBOIT resolve.
Does not sample unresolved OIT, UI, weapon, or tonemap.
===============
*/
static void RB_DrawRefractiveAfterOit( const drawSurfsCommand_t *cmd )
{
	if ( !vk_transparency_refractive_exclude_oit() ) {
		return;
	}
	if ( !r_oit || !r_oit->integer ) {
		return;
	}
	if ( !cmd || ( cmd->refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		return;
	}
	backEnd.drawSurfFilter = 2;
	backEnd.refractiveOnlyPass = qtrue;
	backEnd.oitBucketFilter = 0;
	RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );
	backEnd.refractiveOnlyPass = qfalse;
	backEnd.drawSurfFilter = 0;
}
#endif

static void RB_RepairUnifiedClusteredTransparentHandoff( qboolean usingOit )
{
	if ( !vk_unified_clustered_active() ) {
		return;
	}

	if ( backEnd.drawSurfFilter != 2 ) {
		if ( r_fboDebug && r_fboDebug->integer >= 1 ) {
			ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
				"[VK][unified] transparent handoff self-heal: restoring drawSurfFilter=2 before %s path (was %d)\n",
				usingOit ? "OIT" : "Forward+", backEnd.drawSurfFilter );
		}
		backEnd.drawSurfFilter = 2;
	}

	if ( usingOit ) {
		if ( vk.inRenderPass ) {
			if ( r_fboDebug && r_fboDebug->integer >= 1 ) {
				ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
					"[VK][unified] OIT handoff self-heal: ending lingering %s render pass before vk_oit_pass\n",
					RB_RenderPassName( vk.renderPassIndex ) );
			}
			vk_end_render_pass();
		}
		return;
	}

	if ( !vk.inRenderPass || vk.renderPassIndex != RENDER_PASS_MAIN ) {
		if ( vk.inRenderPass ) {
			if ( r_fboDebug && r_fboDebug->integer >= 1 ) {
				ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
					"[VK][unified] transparent Forward+ handoff self-heal: ending %s before main-pass resume\n",
					RB_RenderPassName( vk.renderPassIndex ) );
			}
			vk_end_render_pass();
		}
		if ( r_fboDebug && r_fboDebug->integer >= 1 ) {
			ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW
				"[VK][unified] transparent Forward+ handoff self-heal: resuming main render pass before transparent shade\n" );
		}
		vk_resume_main_render_pass();
	}
}

static const float s_shadow_flipMatrix[16] = {
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};

static qboolean RB_ShouldRenderSunShadowMap( const drawSurfsCommand_t *cmd )
{
	const qboolean pbrSun = ( r_pbrSunShadow && r_pbrSunShadow->integer && !R_ClassicLightingActive() );
	const qboolean fogSun = ( r_fog_shadows && r_fog_shadows->integer &&
		r_volumetricFog && r_volumetricFog->integer );

	if ( !vk.fboActive ) {
		return qfalse;
	}
	/* Selective Hybrid Shadows: RT owns sun visibility — never also raster-cascade. */
	if ( vk_shs_rt_owns_sun() && !fogSun ) {
		return qfalse;
	}
	if ( !pbrSun && !fogSun ) {
		return qfalse;
	}
	if ( !cmd || cmd->numDrawSurfs <= 0 ) {
		return qfalse;
	}
	if ( cmd->refdef.rdflags & RDF_NOWORLDMODEL ) {
		return qfalse;
	}
	if ( cmd->viewParms.portalView != PV_NONE ) {
		return qfalse;
	}
	if ( cmd->viewParms.targetCube != NULL ) {
		return qfalse;
	}
	if ( cmd->refdef.needScreenMap || cmd->refdef.switchRenderPass ) {
		return qfalse;
	}
	if ( vk.renderPassIndex != RENDER_PASS_MAIN ) {
		return qfalse;
	}
	return qtrue;
}

static qboolean RB_BuildSunShadowViewRange( const drawSurfsCommand_t *cmd, float nearPlane, float farPlane,
	int viewportX, int viewportY, int viewportW, int viewportH,
	viewParms_t *shadowParms, float *outViewProj )
{
	float tanHalfX;
	float tanHalfY;
	float nearW, nearH, farW, farH;
	vec3_t camPos;
	vec3_t camForward, camRight, camUp;
	vec3_t nearCenter, farCenter;
	vec3_t frustumCorners[8];
	vec3_t cascadeCenter = { 0.0f, 0.0f, 0.0f };
	vec3_t lightForward, lightRight, lightUp;
	vec3_t upRef = { 0.0f, 0.0f, 1.0f };
	float padding = ( r_fogShadowPadding ) ? r_fogShadowPadding->value : 64.0f;
	float lightMin[3], lightMax[3];
	vec3_t shadowOrigin;
	float viewerMatrix[16];
	float lightView[16];
	float invW, invH, invD;
	float left, right, bottom, top, zNear, zFar;
	int i;
	uint32_t snapW, snapH;

	if ( !cmd || !shadowParms || !outViewProj ) {
		return qfalse;
	}

	if ( nearPlane < 0.1f ) {
		nearPlane = 0.1f;
	}
	if ( farPlane <= nearPlane + 1.0f ) {
		return qfalse;
	}

	VectorCopy( cmd->viewParms.or.origin, camPos );
	VectorCopy( cmd->viewParms.or.axis[0], camForward );
	VectorCopy( cmd->viewParms.or.axis[1], camRight );
	VectorCopy( cmd->viewParms.or.axis[2], camUp );
	tanHalfX = tanf( DEG2RAD( cmd->viewParms.fovX * 0.5f ) );
	tanHalfY = tanf( DEG2RAD( cmd->viewParms.fovY * 0.5f ) );
	nearW = nearPlane * tanHalfX;
	nearH = nearPlane * tanHalfY;
	farW = farPlane * tanHalfX;
	farH = farPlane * tanHalfY;

	VectorMA( camPos, nearPlane, camForward, nearCenter );
	VectorMA( camPos, farPlane, camForward, farCenter );

	for ( i = 0; i < 8; i++ ) {
		const qboolean farCorner = ( i >= 4 ) ? qtrue : qfalse;
		const float sx = ( ( i & 1 ) ? 1.0f : -1.0f );
		const float sy = ( ( i & 2 ) ? 1.0f : -1.0f );
		const float w = farCorner ? farW : nearW;
		const float h = farCorner ? farH : nearH;
		const vec3_t center = { farCorner ? farCenter[0] : nearCenter[0], farCorner ? farCenter[1] : nearCenter[1], farCorner ? farCenter[2] : nearCenter[2] };
		VectorCopy( center, frustumCorners[i] );
		VectorMA( frustumCorners[i], sx * w, camRight, frustumCorners[i] );
		VectorMA( frustumCorners[i], sy * h, camUp, frustumCorners[i] );
		VectorAdd( cascadeCenter, frustumCorners[i], cascadeCenter );
	}
	VectorScale( cascadeCenter, 1.0f / 8.0f, cascadeCenter );

	VectorScale( tr.sunDirection, -1.0f, lightForward );
	if ( VectorNormalize( lightForward ) <= 0.0f ) {
		return qfalse;
	}
	if ( fabsf( DotProduct( lightForward, upRef ) ) > 0.95f ) {
		VectorSet( upRef, 0.0f, 1.0f, 0.0f );
	}
	CrossProduct( upRef, lightForward, lightRight );
	if ( VectorNormalize( lightRight ) <= 0.0f ) {
		return qfalse;
	}
	CrossProduct( lightForward, lightRight, lightUp );
	VectorNormalize( lightUp );

	for ( i = 0; i < 3; i++ ) {
		lightMin[i] = 1e30f;
		lightMax[i] = -1e30f;
	}
	for ( i = 0; i < 8; i++ ) {
		vec3_t rel;
		float lx, ly, lz;
		VectorSubtract( frustumCorners[i], cascadeCenter, rel );
		lx = DotProduct( rel, lightRight );
		ly = DotProduct( rel, lightUp );
		lz = DotProduct( rel, lightForward );
		lightMin[0] = MIN( lightMin[0], lx );
		lightMax[0] = MAX( lightMax[0], lx );
		lightMin[1] = MIN( lightMin[1], ly );
		lightMax[1] = MAX( lightMax[1], ly );
		lightMin[2] = MIN( lightMin[2], lz );
		lightMax[2] = MAX( lightMax[2], lz );
	}

	left = lightMin[0] - padding;
	right = lightMax[0] + padding;
	bottom = lightMin[1] - padding;
	top = lightMax[1] + padding;

	snapW = ( viewportW > 0 ) ? (uint32_t)viewportW : vk.sun_shadow_tile_size;
	snapH = ( viewportH > 0 ) ? (uint32_t)viewportH : vk.sun_shadow_tile_size;
	if ( VK_SunCSM_Stable() && snapW > 0u && snapH > 0u ) {
		const float texelX = ( right - left ) / (float)snapW;
		const float texelY = ( top - bottom ) / (float)snapH;
		const float halfW = ( right - left ) * 0.5f;
		const float halfH = ( top - bottom ) * 0.5f;
		float centerX = ( left + right ) * 0.5f;
		float centerY = ( bottom + top ) * 0.5f;

		if ( texelX > 1e-6f && texelY > 1e-6f ) {
			centerX = roundf( centerX / texelX ) * texelX;
			centerY = roundf( centerY / texelY ) * texelY;
			left = centerX - halfW;
			right = centerX + halfW;
			bottom = centerY - halfH;
			top = centerY + halfH;
		}
	}

	zNear = -( lightMax[2] + padding );
	zFar = -( lightMin[2] - padding );
	if ( right <= left + 1e-3f || top <= bottom + 1e-3f || zFar <= zNear + 1e-3f ) {
		return qfalse;
	}

	VectorMA( cascadeCenter, -lightMin[2] + padding, lightForward, shadowOrigin );

	Com_Memset( shadowParms, 0, sizeof( *shadowParms ) );
	*shadowParms = cmd->viewParms;
	VectorCopy( shadowOrigin, shadowParms->or.origin );
	VectorCopy( shadowOrigin, shadowParms->pvsOrigin );
	VectorCopy( lightForward, shadowParms->or.axis[0] );
	VectorCopy( lightRight, shadowParms->or.axis[1] );
	VectorCopy( lightUp, shadowParms->or.axis[2] );
	shadowParms->portalView = PV_NONE;
	shadowParms->viewportX = viewportX;
	shadowParms->viewportY = viewportY;
	shadowParms->viewportWidth = viewportW;
	shadowParms->viewportHeight = viewportH;
	shadowParms->scissorX = viewportX;
	shadowParms->scissorY = viewportY;
	shadowParms->scissorWidth = viewportW;
	shadowParms->scissorHeight = viewportH;
	shadowParms->zFar = farPlane;

	Matrix16Identity( shadowParms->projectionMatrix );
	invW = 1.0f / ( right - left );
	invH = 1.0f / ( top - bottom );
	invD = 1.0f / ( zFar - zNear );
	shadowParms->projectionMatrix[0] = 2.0f * invW;
	shadowParms->projectionMatrix[5] = 2.0f * invH;
	shadowParms->projectionMatrix[10] = -2.0f * invD;
	shadowParms->projectionMatrix[12] = -( right + left ) * invW;
	shadowParms->projectionMatrix[13] = -( top + bottom ) * invH;
	shadowParms->projectionMatrix[14] = -( zFar + zNear ) * invD;
	shadowParms->projectionMatrix[15] = 1.0f;

	viewerMatrix[0] = shadowParms->or.axis[0][0];
	viewerMatrix[4] = shadowParms->or.axis[0][1];
	viewerMatrix[8] = shadowParms->or.axis[0][2];
	viewerMatrix[12] = -shadowOrigin[0] * viewerMatrix[0] + -shadowOrigin[1] * viewerMatrix[4] + -shadowOrigin[2] * viewerMatrix[8];
	viewerMatrix[1] = shadowParms->or.axis[1][0];
	viewerMatrix[5] = shadowParms->or.axis[1][1];
	viewerMatrix[9] = shadowParms->or.axis[1][2];
	viewerMatrix[13] = -shadowOrigin[0] * viewerMatrix[1] + -shadowOrigin[1] * viewerMatrix[5] + -shadowOrigin[2] * viewerMatrix[9];
	viewerMatrix[2] = shadowParms->or.axis[2][0];
	viewerMatrix[6] = shadowParms->or.axis[2][1];
	viewerMatrix[10] = shadowParms->or.axis[2][2];
	viewerMatrix[14] = -shadowOrigin[0] * viewerMatrix[2] + -shadowOrigin[1] * viewerMatrix[6] + -shadowOrigin[2] * viewerMatrix[10];
	viewerMatrix[3] = 0.0f;
	viewerMatrix[7] = 0.0f;
	viewerMatrix[11] = 0.0f;
	viewerMatrix[15] = 1.0f;

	myGlMultMatrix( viewerMatrix, s_shadow_flipMatrix, lightView );
	Matrix16Identity( shadowParms->world.modelMatrix );
	Com_Memcpy( shadowParms->world.modelViewMatrix, lightView, sizeof( lightView ) );
	VectorCopy( shadowOrigin, shadowParms->world.viewOrigin );
	VectorClear( shadowParms->world.origin );
	AxisCopy( axisDefault, shadowParms->world.axis );

	myGlMultMatrix( lightView, shadowParms->projectionMatrix, outViewProj );
	return qtrue;
}

static qboolean RB_BuildSunShadowView( const drawSurfsCommand_t *cmd, viewParms_t *shadowParms, float *outViewProj )
{
	float nearPlane = ( r_znear ) ? r_znear->value : 5.0f;
	float farPlane = cmd->viewParms.zFar;
	float maxDist = VK_SunCSM_MaxDistance();

	if ( nearPlane < 0.1f ) {
		nearPlane = 0.1f;
	}
	if ( maxDist > nearPlane && farPlane > maxDist ) {
		farPlane = maxDist;
	}
	return RB_BuildSunShadowViewRange( cmd, nearPlane, farPlane,
		0, 0, (int)vk.sun_shadow_width, (int)vk.sun_shadow_height,
		shadowParms, outViewProj );
}

static void RB_RenderSunShadowMap( const drawSurfsCommand_t *cmd )
{
	viewParms_t savedViewParms;
	viewParms_t shadowViewParms;
	float shadowViewProj[16];
	float splits[VK_SUN_CSM_MAX];
	float nearPlane;
	float farPlane;
	float maxDist;
	float sliceNear;
	int cascades;
	int c;
	int anyOk = 0;

	if ( !RB_ShouldRenderSunShadowMap( cmd ) ) {
		vk.sun_shadow_valid = qfalse;
		Matrix16Identity( vk.sun_shadow_matrix0 );
		return;
	}

	cascades = (int)vk.sun_shadow_cascade_count;
	if ( cascades < 1 ) {
		cascades = VK_SunCSM_CascadeCount();
	}
	if ( cascades < 1 ) {
		cascades = 1;
	}
	if ( cascades > VK_SUN_CSM_MAX ) {
		cascades = VK_SUN_CSM_MAX;
	}

	nearPlane = ( r_znear ) ? r_znear->value : 5.0f;
	if ( nearPlane < 0.1f ) {
		nearPlane = 0.1f;
	}
	farPlane = cmd->viewParms.zFar;
	maxDist = VK_SunCSM_MaxDistance();
	if ( maxDist > nearPlane && farPlane > maxDist ) {
		farPlane = maxDist;
	}
	VK_SunCSM_ComputeSplits( nearPlane, farPlane, cascades, VK_SunCSM_SplitLambda(), splits );
	vk.sun_shadow_near = nearPlane;
	vk.sun_shadow_cascade_count = (uint32_t)cascades;
	for ( c = 0; c < VK_SUN_CSM_MAX; c++ ) {
		vk.sun_shadow_splits[c] = ( c < cascades ) ? splits[c] : splits[cascades - 1];
		Matrix16Identity( vk.sun_shadow_matrix[c] );
	}

	if ( !vk_begin_sun_shadow_render_pass() ) {
		vk.sun_shadow_valid = qfalse;
		Matrix16Identity( vk.sun_shadow_matrix0 );
		return;
	}

	savedViewParms = backEnd.viewParms;
	sliceNear = nearPlane;

	for ( c = 0; c < cascades; c++ ) {
		int vx, vy, tile, atlas;
		float sliceFar = splits[c];

		VK_SunCSM_AtlasTile( c, cascades, (int)vk.sun_shadow_tile_size, &vx, &vy, &tile, &atlas );
		if ( !RB_BuildSunShadowViewRange( cmd, sliceNear, sliceFar, vx, vy, tile, tile,
			&shadowViewParms, shadowViewProj ) ) {
			sliceNear = sliceFar;
			continue;
		}

		Com_Memcpy( vk.sun_shadow_matrix[c], shadowViewProj, sizeof( shadowViewProj ) );
		if ( c == 0 ) {
			Com_Memcpy( vk.sun_shadow_matrix0, shadowViewProj, sizeof( vk.sun_shadow_matrix0 ) );
		}
		anyOk = 1;

		backEnd.viewParms = shadowViewParms;
		SetViewportAndScissor();
		RB_BeginDrawingView();
		RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );
		RB_EndSurface();

		sliceNear = sliceFar;
	}

	backEnd.viewParms = savedViewParms;
	vk_end_sun_shadow_render_pass();

	vk.sun_shadow_valid = anyOk ? qtrue : qfalse;
	if ( !anyOk ) {
		Matrix16Identity( vk.sun_shadow_matrix0 );
	}

	if ( VK_SunCSM_Debug() >= 2 && anyOk ) {
		ri.Printf( PRINT_ALL, "[VK][CSM] cascades=%d atlas=%ux%u tile=%u splits=%.0f/%.0f/%.0f/%.0f\n",
			cascades, vk.sun_shadow_width, vk.sun_shadow_height, vk.sun_shadow_tile_size,
			vk.sun_shadow_splits[0], vk.sun_shadow_splits[1],
			vk.sun_shadow_splits[2], vk.sun_shadow_splits[3] );
	}

	vk_forward_plus_update_sun_shadow_descriptor();
	SetViewportAndScissor();

	/* Raster Ultra 1.9 — virtual page demand / residency after certified CSM fill. */
	if ( vk_vshadow_active() ) {
		float sunDir[3];
		uint32_t dirty[VK_VSHADOW_MAX_DIRTY_QUEUE];
		int n;
		int i;
		int di;

		VectorCopy( tr.sunDirection, sunDir );
		vk_vshadow_update( cmd->viewParms.or.origin, sunDir, nearPlane, farPlane );

		for ( di = 0; di < (int)backEnd.refdef.num_dlights && di < 16; di++ ) {
			const dlight_t *dl = &backEnd.refdef.dlights[di];
			float importance;
			float dist;
			vec3_t delta;
			vkVShadowLightKind_t kind;

			VectorSubtract( dl->origin, cmd->viewParms.or.origin, delta );
			dist = VectorLength( delta );
			importance = Com_Clamp( 0.0f, 1.0f, dl->radius / ( 400.0f + dist * 0.25f ) );
			kind = ( dl->radius > 0.0f && importance > 0.2f ) ? VK_VSHADOW_LIGHT_POINT : VK_VSHADOW_LIGHT_SPOT;
			(void)vk_vshadow_request_local( kind, di, importance, dist );
		}

		n = vk_vshadow_claim_dirty_pages( dirty, VK_VSHADOW_MAX_DIRTY_QUEUE );
		for ( i = 0; i < n; i++ ) {
			const vkVShadowPageMeta_t *meta = vk_vshadow_page_meta( dirty[i] );
			/*
			 * Page render: after CSM (certified fallback) depth exists for receivers.
			 * Mark page initialized for residency/cache. Alpha-caster policy is recorded
			 * on meta for foliage/fences — not solid rectangles when alphaCasters=1.
			 * Dedicated per-page frustum GPU draw expands when sample path leaves CSM.
			 */
			if ( meta && meta->alphaCasters && vk_vshadow_alpha_casters() ) {
				/* Policy active: alpha-tested casters eligible on this page. */
			}
			if ( anyOk || vk_vshadow_fallback_csm() ) {
				vk_vshadow_mark_page_rendered( dirty[i] );
			}
		}
	}
}

void RB_RenderVolumetricShadowView( const viewParms_t *shadowViewParms, drawSurf_t *drawSurfs, int numDrawSurfs )
{
	viewParms_t savedViewParms;

	if ( !shadowViewParms || !drawSurfs || numDrawSurfs <= 0 ) {
		return;
	}

	savedViewParms = backEnd.viewParms;
	backEnd.viewParms = *shadowViewParms;
	RB_BeginDrawingView();
	RB_RenderDrawSurfList( drawSurfs, numDrawSurfs );
	RB_EndSurface();
	backEnd.viewParms = savedViewParms;
	SetViewportAndScissor();
}
#endif


#ifdef USE_VULKAN
#define MAX_DEFERRED_WEAPON_COMMANDS 16
static drawSurfsCommand_t s_deferredWeaponCmds[MAX_DEFERRED_WEAPON_COMMANDS];
static qboolean s_deferredWeaponMixed[MAX_DEFERRED_WEAPON_COMMANDS];
static int s_deferredWeaponCmdCount;
static const char *s_weaponTemporalFailureReason;
static qboolean s_weaponTemporalFailureWarned;

/*
================
RB_TryDeferWeaponDrawSurfs

RDF_NOWORLDMODEL weapon/view-model draws are deferred when world temporal
reconstruction or SSR must complete without weapon color/depth contamination.
================
*/
qboolean RB_TryDeferWeaponDrawSurfs( const drawSurfsCommand_t *cmd )
{
	qboolean mixedWorldWeapon = qfalse;
	int i;

	if ( !cmd ) {
		return qfalse;
	}
	if ( !( cmd->refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		for ( i = 0; i < cmd->numDrawSurfs; i++ ) {
			int entityNum;
			shader_t *shader;
			int fogNum;
			int dlighted;

			R_DecomposeSort( cmd->drawSurfs[i].sort, &entityNum, &shader, &fogNum, &dlighted );
			(void)shader;
			(void)fogNum;
			(void)dlighted;
			if ( entityNum != REFENTITYNUM_WORLD && entityNum >= 0 &&
				entityNum < cmd->refdef.num_entities &&
				( cmd->refdef.entities[entityNum].e.renderfx & RF_FIRST_PERSON ) ) {
				mixedWorldWeapon = qtrue;
				break;
			}
		}
		if ( !mixedWorldWeapon ) {
			return qfalse;
		}
	}
	if ( !mixedWorldWeapon && !backEnd.doneWorldScene ) {
		return qfalse;
	}
	if ( !vk_temporal_want_weapon_after_world_post() ) {
		return qfalse;
	}
#ifdef VK_CUBEMAP
	if ( cmd->viewParms.targetCube != NULL ) {
		return qfalse;
	}
#endif

	if ( s_deferredWeaponCmdCount >= MAX_DEFERRED_WEAPON_COMMANDS ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][weapon] deferred command queue full (%d); drawing command in legacy order\n"
			S_COLOR_WHITE, MAX_DEFERRED_WEAPON_COMMANDS );
		return qfalse;
	}
	s_deferredWeaponCmds[s_deferredWeaponCmdCount] = *cmd;
	s_deferredWeaponMixed[s_deferredWeaponCmdCount] = mixedWorldWeapon;
	s_deferredWeaponCmdCount++;
	backEnd.doneSurfaces = qtrue;
	if ( mixedWorldWeapon ) {
		s_skipDeferredWeaponSurfaces = qtrue;
	}
	if ( r_temporalDebug && r_temporalDebug->integer ) {
		ri.Printf( PRINT_DEVELOPER,
			"[VK][temporal] deferred weapon/view-model until after world post "
			"(command=%d surfs=%d)\n",
			s_deferredWeaponCmdCount, cmd->numDrawSurfs );
	}
	return mixedWorldWeapon ? qfalse : qtrue;
}

static void RB_CopyHdrViewToColor( VkImageView srcView, uint32_t width, uint32_t height )
{
	VkImage srcImage;
	VkImageCopy region;

	if ( srcView == VK_NULL_HANDLE || srcView == vk.color_image_view ) {
		return;
	}
	srcImage = vk_post_fog_source_image( srcView );
	if ( srcImage == VK_NULL_HANDLE || srcImage == vk.color_image || vk.color_image == VK_NULL_HANDLE ) {
		return;
	}

	Com_Memset( &region, 0, sizeof( region ) );
	region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.srcSubresource.layerCount = 1;
	region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.dstSubresource.layerCount = 1;
	region.extent.width = width > 0 ? width : 1;
	region.extent.height = height > 0 ? height : 1;
	region.extent.depth = 1;

	record_image_layout_transition( vk.cmd->command_buffer, srcImage, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT );
	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT );
	qvkCmdCopyImage( vk.cmd->command_buffer,
		srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		vk.color_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &region );
	record_image_layout_transition( vk.cmd->command_buffer, srcImage, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT );
}

static void RB_UpdateWeaponWorldDescriptor( VkImageView worldView )
{
	VkDescriptorImageInfo info;
	VkWriteDescriptorSet write;
	Vk_Sampler_Def sd;

	if ( worldView == VK_NULL_HANDLE || vk.cmd_index >= NUM_COMMAND_BUFFERS ||
		vk.weapon_world_descriptor[vk.cmd_index] == VK_NULL_HANDLE ) {
		return;
	}
	Com_Memset( &sd, 0, sizeof( sd ) );
	sd.gl_mag_filter = sd.gl_min_filter = GL_LINEAR;
	sd.address_mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	sd.max_lod_1_0 = qtrue;
	sd.noAnisotropy = qtrue;
	Com_Memset( &info, 0, sizeof( info ) );
	info.sampler = vk_find_sampler( &sd );
	info.imageView = worldView;
	info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	Com_Memset( &write, 0, sizeof( write ) );
	write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	write.dstSet = vk.weapon_world_descriptor[vk.cmd_index];
	write.dstBinding = 0;
	write.descriptorCount = 1;
	write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	write.pImageInfo = &info;
	qvkUpdateDescriptorSets( vk.device, 1, &write, 0, NULL );
}

static void RB_DrawTemporalFullscreen( uint32_t width, uint32_t height )
{
	VkViewport viewport;
	VkRect2D scissor;

	Com_Memset( &viewport, 0, sizeof( viewport ) );
	viewport.width = (float)width;
	viewport.height = (float)height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	Com_Memset( &scissor, 0, sizeof( scissor ) );
	scissor.extent.width = width;
	scissor.extent.height = height;
	qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
	qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
}

static qboolean RB_ResolveIndependentWeaponHistory( VkImageView worldView,
	uint32_t width, uint32_t height )
{
	VkDescriptorSet sets[9];
	uint32_t readIndex = vk.temporal.weaponHistoryIndex & 1u;
	uint32_t writeIndex = 1u - readIndex;
	s_weaponTemporalFailureReason = NULL;

#ifndef NDEBUG
	if ( vk.temporal.weaponHistoryValid ) {
		assert( vk.temporal.weaponHistoryFrameId[readIndex] + 1u == vk.temporal.frameIndex );
		assert( vk.temporal.weaponDepthFrameId[readIndex] + 1u == vk.temporal.frameIndex );
	}
#endif
	if ( worldView == VK_NULL_HANDLE || worldView == vk.color_image_view ||
		vk.weapon_taa_pipeline == VK_NULL_HANDLE ||
		vk.weapon_taa_composite_pipeline == VK_NULL_HANDLE ||
		vk.pipeline_layout_weapon_taa == VK_NULL_HANDLE ||
		vk.pipeline_layout_weapon_composite == VK_NULL_HANDLE ||
		vk.framebuffers.weapon_taa[writeIndex] == VK_NULL_HANDLE ||
		vk.weapon_history_descriptor[readIndex] == VK_NULL_HANDLE ||
		vk.weapon_prev_depth_descriptor[readIndex] == VK_NULL_HANDLE ||
		vk.weapon_current_class_descriptor[vk.cmd_index] == VK_NULL_HANDLE ) {
		s_weaponTemporalFailureReason =
			( worldView == VK_NULL_HANDLE ) ? "missing world view" :
			( worldView == vk.color_image_view ) ? "world/weapon HDR are not isolated" :
			( vk.weapon_taa_pipeline == VK_NULL_HANDLE ) ? "missing weapon TAA pipeline" :
			( vk.weapon_taa_composite_pipeline == VK_NULL_HANDLE ) ? "missing weapon composite pipeline" :
			( vk.framebuffers.weapon_taa[writeIndex] == VK_NULL_HANDLE ) ? "missing weapon history framebuffer" :
			( vk.weapon_history_descriptor[readIndex] == VK_NULL_HANDLE ) ? "missing weapon history descriptor" :
			( vk.weapon_prev_depth_descriptor[readIndex] == VK_NULL_HANDLE ) ? "missing weapon depth descriptor" :
			"missing current weapon class descriptor";
		vk_reset_weapon_history();
		return qfalse;
	}
	if ( !vk_temporal_prepare_current_depth() ) {
		s_weaponTemporalFailureReason = "current depth preparation failed";
		vk_reset_weapon_history();
		return qfalse;
	}

	RB_UpdateWeaponWorldDescriptor( worldView );
	vk_barrier_post_fog_source_for_sampling( vk.color_image_view, "weapon mode2 current combined" );
	vk_barrier_post_fog_source_for_sampling( worldView, "weapon mode2 world" );
	vk_barrier_temporal_class_for_sampling( "weapon mode2 class" );
	vk_barrier_reactive_mask_for_sampling( "weapon mode2 reactive" );
	vk_update_postfx_params( vk.cmd_index );

	sets[0] = vk.color_descriptor[vk.cmd_index];
	sets[1] = vk.taa_depth_descriptor[vk.cmd_index];
	sets[2] = vk.postfx_params_descriptor[vk.cmd_index];
	sets[3] = vk.weapon_history_descriptor[readIndex];
	sets[4] = vk.taa_motion_descriptor[vk.cmd_index];
	sets[5] = vk.taa_reactive_descriptor[vk.cmd_index];
	sets[6] = vk.taa_class_descriptor[vk.cmd_index];
	sets[7] = vk.weapon_prev_depth_descriptor[readIndex];
	sets[8] = vk.weapon_current_class_descriptor[vk.cmd_index];
	{
		int i;
		for ( i = 0; i < 9; i++ ) {
			if ( sets[i] == VK_NULL_HANDLE ) {
				s_weaponTemporalFailureReason = "required weapon resolve descriptor is null";
				vk_reset_weapon_history();
				return qfalse;
			}
		}
	}

	vk_pass_diag_stage( "weapon_temporal_resolve_begin" );
	vk_temporal_marker_begin( "TemporalResolveWeapon" );
	if ( vk.weapon_temporal_query_pool != VK_NULL_HANDLE && qvkCmdWriteTimestamp ) {
		const uint32_t queryBase = vk.cmd_index * VK_WEAPON_TEMPORAL_QUERY_SLOTS;
		qvkCmdWriteTimestamp( vk.cmd->command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
			vk.weapon_temporal_query_pool, queryBase );
	}
	vk_begin_render_pass_tracked( vk.render_pass.taa, vk.framebuffers.weapon_taa[writeIndex],
		width, height, qfalse );
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.weapon_taa_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.pipeline_layout_weapon_taa, 0, 9, sets, 0, NULL );
	RB_DrawTemporalFullscreen( width, height );
	vk_end_render_pass();
	vk_temporal_marker_end();
	vk_temporal_note_weapon_resolve();

	if ( !vk_temporal_store_weapon_depth( writeIndex ) ) {
		s_weaponTemporalFailureReason = "weapon previous-depth copy failed";
		vk_reset_weapon_history();
		return qfalse;
	}
	{
		VkImageAspectFlags depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		if ( glConfig.stencilBits > 0 ) {
			depthAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
		record_depth_image_layout_transition( vk.cmd->command_buffer, depthAspect,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );
	}

	vk_begin_post_bloom_render_pass();
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.weapon_taa_composite_pipeline );
	{
		VkDescriptorSet compositeSets[2] = {
			vk.weapon_world_descriptor[vk.cmd_index],
			vk.weapon_history_descriptor[writeIndex]
		};
		qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
			vk.pipeline_layout_weapon_composite, 0, 2, compositeSets, 0, NULL );
	}
	RB_DrawTemporalFullscreen( width, height );
	vk_end_render_pass();

	vk.temporal.weaponHistoryIndex = writeIndex;
	vk.temporal.weaponHistoryValid = qtrue;
	vk.temporal.weaponHistoryFrameId[writeIndex] = vk.temporal.frameIndex;
	vk.temporal.weaponDepthFrameId[writeIndex] = vk.temporal.frameIndex;
	if ( vk.weapon_temporal_query_pool != VK_NULL_HANDLE && qvkCmdWriteTimestamp ) {
		const uint32_t queryBase = vk.cmd_index * VK_WEAPON_TEMPORAL_QUERY_SLOTS;
		qvkCmdWriteTimestamp( vk.cmd->command_buffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
			vk.weapon_temporal_query_pool, queryBase + 1u );
	}
	vk_pass_diag_stage( "weapon_temporal_resolve_end" );
	return qtrue;
}

/*
================
RB_FlushDeferredWeaponAfterTaa

Copies the resolved world HDR into color_image (if needed), then draws the
deferred weapon/view-model after incompatible world post passes. The legacy
function name is retained for ABI stability.
================
*/
void RB_FlushDeferredWeaponAfterTaa( VkImageView *post_fog_src, VkImageView *luminance_src )
{
	uint32_t width = 0;
	uint32_t height = 0;
	VkImageView src;
	int commandCount;
	int i;

	if ( s_deferredWeaponCmdCount == 0 ) {
		return;
	}
	commandCount = s_deferredWeaponCmdCount;
	s_deferredWeaponCmdCount = 0;

	if ( !vk.fboActive || !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	vk.temporal.weaponRenderedThisFrame = qtrue;

	src = ( post_fog_src && *post_fog_src != VK_NULL_HANDLE ) ? *post_fog_src : vk.color_image_view;
	vk_get_active_render_extent( &width, &height );
	if ( width == 0 || height == 0 ) {
		width = vk_get_render_target_width();
		height = vk_get_render_target_height();
	}

	if ( vk.inRenderPass ) {
		vk_end_render_pass();
	}

	RB_CopyHdrViewToColor( src, width, height );

	vk_spine_pass_begin( VK_SPINE_PASS_WEAPON );
	backEnd.drawSurfFilter = 0;
	backEnd.oitMomentsPass = qfalse;
	backEnd.oitAccumPass = qfalse;
	backEnd.reactiveMaskStamp = qfalse;
	backEnd.projection2D = qfalse;

	/* Weapon must not inherit world OIT blend/descriptor state. */
	if ( backEnd.oitAccumPass || backEnd.oitMomentsPass || backEnd.oitBucketFilter ) {
		ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
			"[VK][OIT] weapon flush: OIT flags still set — forcing clear\n" S_COLOR_WHITE );
		backEnd.oitAccumPass = qfalse;
		backEnd.oitMomentsPass = qfalse;
		backEnd.oitBucketFilter = 0;
	}
	if ( r_fboDebug && r_fboDebug->integer >= 1 ) {
		ri.Printf( PRINT_DEVELOPER,
			"[VK][OIT] weapon isolation: oitAccum=%d oitMoments=%d frameState=%u unhealthy=%d\n",
			backEnd.oitAccumPass ? 1 : 0, backEnd.oitMomentsPass ? 1 : 0,
			vk.oitFrameState, vk.oitUnhealthy ? 1 : 0 );
	}

	vk_begin_post_bloom_render_pass();
	for ( i = 0; i < commandCount; i++ ) {
		const drawSurfsCommand_t *weaponCmd = &s_deferredWeaponCmds[i];

		backEnd.refdef = weaponCmd->refdef;
		backEnd.viewParms = weaponCmd->viewParms;
		s_drawDeferredWeaponSurfacesOnly = s_deferredWeaponMixed[i];
		RB_BeginDrawingView();
		RB_RenderDrawSurfList( weaponCmd->drawSurfs, weaponCmd->numDrawSurfs );
		RB_EndSurface();
		s_drawDeferredWeaponSurfacesOnly = qfalse;
	}
	if ( vk.inRenderPass ) {
		vk_end_render_pass();
	}

	/* Classify weapon pixels + stamp reactive for next-frame TAA / upscale. */
	if ( !vk_temporal_prepare_current_depth() ) {
		vk_reset_weapon_history();
	}
	vk_temporal_class_stamp_weapon_from_depth();
	vk_reactive_mask_stamp_weapon_from_depth();
	if ( ( r_weaponTemporalMode && r_weaponTemporalMode->integer == 2 ) ||
		( r_temporalDebug && r_temporalDebug->integer >= 16 ) ) {
		if ( !RB_ResolveIndependentWeaponHistory( src, width, height ) ) {
			if ( !s_weaponTemporalFailureWarned ) {
				ri.Printf( PRINT_WARNING, S_COLOR_YELLOW
					"[VK][weapon] independent temporal resolve unavailable (%s); "
					"using current-frame weapon and rejecting weapon history\n" S_COLOR_WHITE,
					s_weaponTemporalFailureReason ? s_weaponTemporalFailureReason : "unknown reason" );
				s_weaponTemporalFailureWarned = qtrue;
			}
		} else {
			s_weaponTemporalFailureWarned = qfalse;
		}
	} else {
		s_weaponTemporalFailureWarned = qfalse;
		vk_reset_weapon_history();
	}

	if ( post_fog_src ) {
		*post_fog_src = vk.color_image_view;
	}
	if ( luminance_src ) {
		*luminance_src = vk.color_image_view;
	}
	vk_set_scene_post_fog_source( vk.color_image_view );
	vk_set_post_chain_last_writer( r_weaponTemporalMode && r_weaponTemporalMode->integer == 2 ?
		"weapon_temporal_mode2" : "weapon_after_world_post" );

	if ( r_temporalDebug && r_temporalDebug->integer ) {
		ri.Printf( PRINT_DEVELOPER,
			"[VK][temporal] flushed %d deferred weapon commands after world post\n",
			commandCount );
	}
	vk_spine_pass_end( VK_SPINE_PASS_WEAPON );
}
#endif

/*
=============
RB_DrawSurfs
=============
*/
static const void *RB_DrawSurfs( const void *data ) {
	const drawSurfsCommand_t *cmd;

	// finish any 2D drawing if needed
	RB_EndSurface();

	cmd = (const drawSurfsCommand_t *)data;

#ifdef USE_VULKAN
	/* First-person weapon: defer until after world Temporal Reconstruction so
	 * weapon pixels never enter TAA history (dark offset silhouettes / trails). */
	if ( RB_TryDeferWeaponDrawSurfs( cmd ) ) {
		return (const void *)( cmd + 1 );
	}
#endif

	backEnd.refdef = cmd->refdef;
	backEnd.viewParms = cmd->viewParms;
	backEnd.drawSurfFilter = 0;
	backEnd.oitMomentsPass = qfalse;
	backEnd.oitAccumPass = qfalse;

#ifdef USE_VBO
	VBO_UnBind();
#endif

#ifdef USE_VULKAN
	vk_prepare_frame_temporal_state();
	vk_forward_plus_ensure_render_resolution();
	vk_forward_plus_update_for_refdef();
	RB_RenderSunShadowMap( cmd );
#endif

	// clear the z buffer, set the modelview, etc
	RB_BeginDrawingView();

#ifdef USE_VULKAN
	vk_forward_plus_upload_refdef();
	if ( !r_forwardPlusDepthCull || !r_forwardPlusDepthCull->integer ) {
		vk_forward_plus_dispatch_tile_cull();
	}
#endif

#ifdef USE_VULKAN
	if ( r_occlusionCulling && r_occlusionCulling->integer && vk.occlusion_query_pool != VK_NULL_HANDLE ) {
		backEnd.depthOnlyWorldPass = qtrue;
		RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );
		backEnd.depthOnlyWorldPass = qfalse;
		vk_occlusion_draw_entity_bboxes( cmd );
	}
#endif

#ifdef USE_VULKAN
	/* Depth-aware tile lists must be ready before opaque PBR reads the tile SSBO (not after opaque). */
	if ( r_forwardPlus && r_forwardPlus->integer &&
		r_forwardPlusDepthCull && r_forwardPlusDepthCull->integer ) {
		backEnd.forwardPlusDepthPrepass = qtrue;
		RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );
		backEnd.forwardPlusDepthPrepass = qfalse;
		vk_forward_plus_dispatch_tile_cull_after_opaque();
	}
	/* Raster Ultra 1.6: frustum + conservative Hi-Z cull → indirect lists (no CPU readback). */
	if ( vk_gpu_scene_active() ) {
		vk_gpu_scene_begin_frame();
		vk_gpu_scene_cull_and_build_indirect();
		/* HT Slice A: coalesce compatible indirect draws before any consumer. */
		vk_ht_merge_gpu_scene_draws();
	}
#endif

#ifdef USE_VULKAN
	/* Clear + bind the dynamic-object identity buffer before opaque world draws so
	 * gen_frag can stamp per-entity ids. First-person weapon (NOWORLDMODEL) is excluded. */
	if ( !( cmd->refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		vk_object_id_begin_frame();
	}
#endif

#ifdef VK_CUBEMAP
	if ( backEnd.viewParms.targetCube != NULL ) 
	{
		vk_end_render_pass();
		vk_begin_cubemap_render_pass();
		RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );
		backEnd.doneSurfaces = qtrue; // for bloom
		return (const void*)(cmd + 1);
	}
#endif

#ifdef USE_VULKAN
	if ( vk_deferred_opaque_transparent_split() ) {
		/* Mode 1 deferred or mode 3 Unified Clustered:
		 * opaque → G-buffer + deferred → transparent Forward+.
		 * Weapon/UI (RDF_NOWORLDMODEL): Forward+ only — no second deferred composite. */
		backEnd.drawSurfFilter = 1; /* opaque only */
		backEnd.reactiveMaskStamp = qfalse;
		if ( vk_reactive_mask_stamp_enabled() && r_stochasticAlpha && r_stochasticAlpha->integer > 0 ) {
			vk_reactive_mask_clear();
			backEnd.reactiveMaskStamp = qtrue;
			vk_barrier_reactive_mask_for_storage( "deferred-split opaque stochastic" );
		}
		RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );
		backEnd.reactiveMaskStamp = qfalse;
		if ( !( cmd->refdef.rdflags & RDF_NOWORLDMODEL ) ) {
			vk_deferred_gbuffer_capture_after_geometry();
			vk_deferred_decals_apply_to_gbuffer();
			vk_visibility_buffer_capture_after_geometry();
			vk_ambient_visibility_apply_after_geometry();
			if ( vk_visibility_late_shade_wanted() ) {
				vk_visibility_late_shade_apply_after_geometry();
			} else {
				vk_deferred_lighting_apply_after_geometry();
			}
		}
		backEnd.drawSurfFilter = 2; /* transparent only (Forward+ shade) */
		backEnd.reactiveMaskStamp = qfalse;
		if ( r_oit && r_oit->integer && r_fbo && r_fbo->integer ) {
			/* OIT replaces Forward+ transparent shade; r_oitForwardPlus samples the shared tile/Z grid. */
			{
				static qboolean s_oit_mode3_logged;
				if ( !s_oit_mode3_logged ) {
					ri.Printf( PRINT_ALL,
						"[VK][deferred-split] r_oit=%d: OIT pass after deferred (skips Forward+ transparent shade)%s\n",
						r_oit->integer,
						( r_oitForwardPlus && r_oitForwardPlus->integer )
							? ( r_oit->integer == 2
								? "; MBOIT accum uses Forward+ tile lights (r_oitForwardPlus 1)"
								: "; WBOIT uses Forward+ tile lights (r_oitForwardPlus 1)" )
							: "" );
					s_oit_mode3_logged = qtrue;
				}
			}
			RB_RepairUnifiedClusteredTransparentHandoff( qtrue );
			RB_ValidateUnifiedClusteredTransparentHandoff( qtrue );
			vk_oit_pass( cmd );
			RB_DrawRefractiveAfterOit( cmd );
		} else {
			RB_RepairUnifiedClusteredTransparentHandoff( qfalse );
			RB_ValidateUnifiedClusteredTransparentHandoff( qfalse );
			vk_reactive_mask_clear();
			backEnd.reactiveMaskStamp = vk_reactive_mask_stamp_enabled() ? qtrue : qfalse;
			if ( backEnd.reactiveMaskStamp ) {
				vk_barrier_reactive_mask_for_storage( "forward+ transparent" );
			}
			RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );
			backEnd.reactiveMaskStamp = qfalse;
		}
		backEnd.drawSurfFilter = 0;
	} else if ( r_oit && r_oit->integer && r_fbo && r_fbo->integer ) {
		backEnd.drawSurfFilter = 1; /* opaque only */
		/* Stochastic foliage may stamp during opaque when r_stochasticAlpha is on. */
		backEnd.reactiveMaskStamp = qfalse;
		if ( vk_reactive_mask_stamp_enabled() && r_stochasticAlpha && r_stochasticAlpha->integer > 0 ) {
			vk_reactive_mask_clear();
			backEnd.reactiveMaskStamp = qtrue;
			vk_barrier_reactive_mask_for_storage( "opaque stochastic" );
		}
		RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );
		backEnd.reactiveMaskStamp = qfalse;
		backEnd.drawSurfFilter = 0;
		vk_oit_pass( cmd );
		RB_DrawRefractiveAfterOit( cmd );
	} else
#endif
	{
#ifdef USE_VULKAN
		backEnd.reactiveMaskStamp = qfalse;
		if ( vk_reactive_mask_stamp_enabled() &&
			( ( r_stochasticAlpha && r_stochasticAlpha->integer > 0 ) ||
			  !( vk_deferred_opaque_transparent_split() ) ) ) {
			/* Non-split path: stamp stochastic + any blended draws in the single list. */
			vk_reactive_mask_clear();
			backEnd.reactiveMaskStamp = qtrue;
			vk_barrier_reactive_mask_for_storage( "single-pass draws" );
		}
#endif
	RB_RenderDrawSurfList( cmd->drawSurfs, cmd->numDrawSurfs );
#ifdef USE_VULKAN
		backEnd.reactiveMaskStamp = qfalse;
#endif
	}

#ifdef USE_VULKAN
	if ( CBTerrain_IsEnabled() ) {
		CBTerrain_Frame();
	}
	VK_Biome_Frame();
	VK_VegGpu_Frame();
	if ( !vk_deferred_opaque_transparent_split() ) {
		/* Mode 2 sidecar / non-split: never refill G-buffer after weapon or UI. */
		if ( !( cmd->refdef.rdflags & RDF_NOWORLDMODEL ) ) {
			vk_deferred_gbuffer_capture_after_geometry();
			vk_deferred_decals_apply_to_gbuffer();
			vk_visibility_buffer_capture_after_geometry();
			vk_ambient_visibility_apply_after_geometry();
			if ( vk_visibility_late_shade_wanted() ) {
				vk_visibility_late_shade_apply_after_geometry();
			} else {
				vk_deferred_lighting_apply_after_geometry();
			}
		}
	}
	vk_niv_apply_after_geometry();
	vk_raster_gi_apply_after_geometry();
	vk_gpu_particles_apply_after_geometry();
	vk_surfel_gi_apply_after_geometry();
	vk_rcgi_apply_after_geometry();
	vk_nist_apply_after_geometry();
	vk_nvc_apply_after_geometry();
	vk_vfgi_apply_after_geometry();
	vk_renderformer_apply_after_geometry();
	vk_wpt_apply_after_geometry();
	vk_fsa_build_importance_after_geometry();
	if ( R_SQZ_Active() ) {
		vk_sqz_apply_after_geometry();
	} else if ( R_WSP_Active() ) {
		vk_wsp_apply_after_geometry();
	} else {
		vk_mgs_apply_after_geometry();
	}
	vk_distortion_apply();
#endif

#ifdef USE_VBO
	VBO_UnBind();
#endif

	// darken down any stencil shadows
	RB_ShadowFinish();

	// add light flares on lights that aren't obscured
	RB_RenderFlares();

	if ( backEnd.refdef.numLitSurfs && !vk_deferred_lighting_active() ) {
		RB_BeginDrawingLitSurfs();
		RB_LightingPass();
	}

	// draw main system development information (surface outlines, etc)
	RB_DebugGraphics();

#ifdef USE_VULKAN
	VK_FP64_PointsDraw();
#endif

#ifdef USE_VULKAN
	if ( cmd->refdef.switchRenderPass ) {
		vk_end_render_pass();
		vk_begin_main_render_pass();
		backEnd.screenMapDone = qtrue;
	}
#endif

	/* Could check rdf_noworld; q3mme uses full 3d ui. */
	backEnd.doneSurfaces = qtrue; // for bloom
	/*
	 * Track a real world view for frame-end HDR resolve. Later HUD/weapon
	 * RE_RenderScene calls use RDF_NOWORLDMODEL and overwrite tr.refdef; gamma
	 * must not treat those as "no world" or HDR stays untone-mapped (black).
	 */
	if ( !( cmd->refdef.rdflags & RDF_NOWORLDMODEL )
#ifdef VK_CUBEMAP
		&& cmd->viewParms.targetCube == NULL
#endif
		) {
		backEnd.doneWorldScene = qtrue;
		vk_temporal_capture_world_viewparms();
	}
#ifdef USE_VULKAN
	s_skipDeferredWeaponSurfaces = qfalse;
#endif

	return (const void *)(cmd + 1);
}


/*
=============
RB_DrawBuffer
=============
*/
static const void *RB_DrawBuffer( const void *data ) {
	const drawBufferCommand_t	*cmd;

	cmd = (const drawBufferCommand_t *)data;

#ifdef USE_VULKAN
	vk_begin_frame();
	vk_ui_blur_begin_frame();

	tess.depthRange = DEPTH_RANGE_NORMAL;

	// force depth range and viewport/scissor updates
	vk.cmd->depth_range = DEPTH_RANGE_COUNT;

	if ( r_clear->integer && vk.clearAttachment ) {
		const vec4_t color = {1, 0, 0.5, 1};
		backEnd.projection2D = qtrue; // to ensure we have viewport that occupies entire window
		vk_clear_color( color );
		backEnd.projection2D = qfalse;
	}
#else
	qglDrawBuffer( cmd->buffer );

	// clear screen for debugging
	if ( r_clear->integer ) {
		qglClearColor( 1, 0, 0.5, 1 );
		qglClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
	}
#endif

	return (const void *)(cmd + 1);
}


/*
===============
RB_ShowImages

Draw all the images to the screen, on top of whatever
was there.  This is used to test for texture thrashing.

Also called by RE_EndRegistration
===============
*/
#ifdef USE_VULKAN
void RB_ShowImages( void )
{
	int i;

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}

	// draw full-screen quad
	tess.numVertexes = 4;

	tess.svars.colors[0][0].u32 = ~0U; // 255-255-255-255
	tess.svars.colors[0][1].u32 = ~0U;
	tess.svars.colors[0][2].u32 = ~0U;
	tess.svars.colors[0][3].u32 = ~0U;

	tess.svars.texcoords[0][0][0] = 0.0f;
	tess.svars.texcoords[0][0][1] = 0.0f;

	tess.svars.texcoords[0][1][0] = 1.0f;
	tess.svars.texcoords[0][1][1] = 0.0f;

	tess.svars.texcoords[0][2][0] = 0.0f;
	tess.svars.texcoords[0][2][1] = 1.0f;

	tess.svars.texcoords[0][3][0] = 1.0f;
	tess.svars.texcoords[0][3][1] = 1.0f;

	tess.svars.texcoordPtr[0] = tess.svars.texcoords[0];

	tess.xyz[0][0] = 0.0f;
	tess.xyz[0][1] = 0.0f;

	tess.xyz[1][0] = (float)glConfig.vidWidth;
	tess.xyz[1][1] = 0.0f;

	tess.xyz[2][0] = 0.0f;
	tess.xyz[2][1] = (float)glConfig.vidHeight;

	tess.xyz[3][0] = (float)glConfig.vidWidth;
	tess.xyz[3][1] = (float)glConfig.vidHeight;

	vk_bind_pipeline( vk.images_debug_pipeline2 );
	vk_bind_geometry( TESS_XYZ | TESS_RGBA0 | TESS_ST0 );
	vk_draw_geometry( DEPTH_RANGE_NORMAL, qfalse );

	for ( i = 0; i < tr.numImages; i++ ) {
		image_t* image = tr.images[i];

		float w = glConfig.vidWidth / 20;
		float h = glConfig.vidHeight / 15;
		float x = i % 20 * w;
		float y = i / 20 * h;

		// show in proportional size in mode 2
		if ( r_showImages->integer == 2 ) {
			w *= image->uploadWidth / 512.0f;
			h *= image->uploadHeight / 512.0f;
		}

		tess.xyz[0][0] = x;
		tess.xyz[0][1] = y;

		tess.xyz[1][0] = x + w;
		tess.xyz[1][1] = y;

		tess.xyz[2][0] = x;
		tess.xyz[2][1] = y + h;

		tess.xyz[3][0] = x + w;
		tess.xyz[3][1] = y + h;

		GL_Bind( image );
		vk_bind_pipeline( vk.images_debug_pipeline );
		vk_bind_geometry( TESS_XYZ );
		vk_draw_geometry( DEPTH_RANGE_NORMAL, qfalse );
	}

	tess.numIndexes = 0;
	tess.numVertexes = 0;
}
#else
void RB_ShowImages( void ) {
	int		i;
	image_t	*image;
	float	x, y, w, h;
	int		start, end;
	const vec2_t t[4] = { {0,0}, {1,0}, {0,1}, {1,1} };
	vec3_t v[4];

	if ( !backEnd.projection2D ) {
		RB_SetGL2D();
	}

	qglClear( GL_COLOR_BUFFER_BIT );

	qglFinish();

	GL_ClientState( 0, CLS_TEXCOORD_ARRAY );
	qglTexCoordPointer( 2, GL_FLOAT, 0, t );

	start = ri.Milliseconds();

	for ( i = 0; i < tr.numImages; i++ ) {
		image = tr.images[ i ];
		w = glConfig.vidWidth / 20;
		h = glConfig.vidHeight / 15;
		x = i % 20 * w;
		y = i / 20 * h;

		// show in proportional size in mode 2
		if ( r_showImages->integer == 2 ) {
			w *= image->uploadWidth / 512.0f;
			h *= image->uploadHeight / 512.0f;
		}

		GL_Bind( image );

		VectorSet(v[0],x,y,0);
		VectorSet(v[1],x+w,y,0);
		VectorSet(v[2],x,y+h,0);
		VectorSet(v[3],x+w,y+h,0);

		qglVertexPointer( 3, GL_FLOAT, 0, v );
		qglDrawArrays( GL_TRIANGLE_STRIP, 0, 4 );
	}

	qglFinish();

	end = ri.Milliseconds();
	ri.Printf( PRINT_ALL, "%i msec to draw all images\n", end - start );
}
#endif


/*
=============
RB_ColorMask
=============
* Uses VK_EXT_extended_dynamic_state3 (vkCmdSetColorWriteMaskEXT) when available.
*/
static const void *RB_ColorMask( const void *data )
{
	const colorMaskCommand_t *cmd = data;
#ifdef USE_VULKAN
	vk_set_color_write_mask( cmd->rgba[0], cmd->rgba[1], cmd->rgba[2], cmd->rgba[3] );
#else
	qglColorMask( cmd->rgba[0], cmd->rgba[1], cmd->rgba[2], cmd->rgba[3] );
#endif

	return (const void *)(cmd + 1);
}


/*
=============
RB_ClearDepth
=============
*/
static const void *RB_ClearDepth( const void *data )
{
	const clearDepthCommand_t *cmd = data;

	RB_EndSurface();

#ifdef USE_VULKAN
	vk_clear_depth( r_shadows->integer == 2 ? qtrue : qfalse );
#else
	qglClear( GL_DEPTH_BUFFER_BIT );
#endif

	return (const void *)(cmd + 1);
}


/*
=============
RB_ClearColor
=============
*/
static const void *RB_ClearColor( const void *data )
{
	const clearColorCommand_t *cmd = data;

#ifdef USE_VULKAN
	RB_SetGL2D();
	vk_clear_color( colorBlack );
	backEnd.projection2D = qfalse;
#else
	qglViewport( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglScissor( 0, 0, glConfig.vidWidth, glConfig.vidHeight );
	qglClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
	qglClear( GL_COLOR_BUFFER_BIT );
#endif

	return (const void *)(cmd + 1);
}


/*
=============
RB_FinishBloom
=============
*/
static const void *RB_FinishBloom( const void *data )
{
	const finishBloomCommand_t *cmd = data;

	RB_EndSurface();

#ifdef USE_VULKAN
	/* Bloom owns color_image; end UI overlay recording so bloom can use the scene.
	 * Keep uiOverlayContentValid — gamma still needs to compose this frame's HUD. */
	if ( vk.uiOverlayActive ) {
		if ( vk.inRenderPass ) {
			vk_end_render_pass();
		}
		vk.uiOverlayActive = qfalse;
		vk_pass_diag_stage( "finish_bloom_leave_ui_overlay" );
	}
	vk_assert_ui_pass_consistency( "RB_FinishBloom" );
	/* SSAO before bloom so AO darkens the scene before bloom extraction. */
	if ( r_ssao && r_ssao->integer ) {
		vk_ssao_pass();
	}
	if ( r_bloom->integer && !vk_temporal_defer_bloom_for_weapon() ) {
		vk_bloom();
	}
	if ( vk.lensFlareActive ) {
		vk_lens_flare();
	}
#endif

	// texture swapping test
	if ( r_showImages->integer ) {
		RB_ShowImages();
	}

#ifdef USE_VULKAN
	R_VT_DebugDraw();
#endif

	backEnd.drawConsole = qtrue;
	/* Console StretchPics follow FinishBloom in the same client frame. If
	 * projection2D stays true, RB_StretchPic skips SetGL2D/prepare_2d and
	 * records into whatever pass bloom left open (usually post_bloom HDR).
	 * That never reaches overlay_compose, so the console looks empty. Force a
	 * 2D re-entry so prepare_2d resumes the UI overlay (or post_bloom fallback). */
	backEnd.projection2D = qfalse;

	return (const void *)(cmd + 1);
}


static const void *RB_SwapBuffers( const void *data ) {

	const swapBuffersCommand_t	*cmd;

	// finish any 2D drawing if needed
	RB_EndSurface();

	// texture swapping test
	if ( r_showImages->integer && !backEnd.drawConsole ) {
		RB_ShowImages();
	}

	cmd = (const swapBuffersCommand_t *)data;

	tr.needScreenMap = 0;

#ifdef USE_VULKAN
	R_VT_Feedback_EndFrame();
	vk_end_frame();

	if ( backEnd.doneSurfaces && !glState.finishCalled ) {
		vk_queue_wait_idle();
	}
#else
	if ( backEnd.doneSurfaces && !glState.finishCalled ) {
		qglFinish();
	}
#endif

#ifdef USE_VULKAN
	if ( backEnd.screenshotMask && vk.cmd->waitForFence ) {
#else
	if ( backEnd.screenshotMask && tr.frameCount > 1 ) {
#endif
#ifdef USE_VULKAN
		/* EXR is float export — do not apply SDR encode block. */
		if ( backEnd.screenshotMask & SCREENSHOT_EXR && backEnd.screenshotEXR[0] ) {
			vk_capture_pipeline_note_capture();
			RB_TakeScreenshotEXR( 0, 0, gls.captureWidth, gls.captureHeight, backEnd.screenshotEXR );
			if ( !backEnd.screenShotEXRsilent ) {
				ri.Printf( PRINT_ALL, "Wrote %s (display-linearized EXR)\n", backEnd.screenshotEXR );
			}
			backEnd.screenshotEXR[0] = '\0';
			backEnd.screenshotMask &= ~SCREENSHOT_EXR;
		}
		if ( !backEnd.screenshotMask ) {
			/* only EXR was requested */
		} else if ( !vk_capture_pipeline_allow_sdr_encode() ) {
			backEnd.screenshotJPG[0] = '\0';
			backEnd.screenshotTGA[0] = '\0';
			backEnd.screenshotBMP[0] = '\0';
			backEnd.screenshotMask = 0;
		} else
#endif
		{
#ifdef USE_VULKAN
		vk_capture_pipeline_note_capture();
#endif
		if ( backEnd.screenshotMask & SCREENSHOT_TGA && backEnd.screenshotTGA[0] ) {
			RB_TakeScreenshot( 0, 0, gls.captureWidth, gls.captureHeight, backEnd.screenshotTGA );
			if ( !backEnd.screenShotTGAsilent ) {
				ri.Printf( PRINT_ALL, "Wrote %s\n", backEnd.screenshotTGA );
			}
		}
		if ( backEnd.screenshotMask & SCREENSHOT_JPG && backEnd.screenshotJPG[0] ) {
			RB_TakeScreenshotJPEG( 0, 0, gls.captureWidth, gls.captureHeight, backEnd.screenshotJPG );
			if ( !backEnd.screenShotJPGsilent ) {
				ri.Printf( PRINT_ALL, "Wrote %s\n", backEnd.screenshotJPG );
			}
		}
		if ( backEnd.screenshotMask & SCREENSHOT_BMP && ( backEnd.screenshotBMP[0] || ( backEnd.screenshotMask & SCREENSHOT_BMP_CLIPBOARD ) ) ) {
			RB_TakeScreenshotBMP( 0, 0, gls.captureWidth, gls.captureHeight, backEnd.screenshotBMP, backEnd.screenshotMask & SCREENSHOT_BMP_CLIPBOARD );
			if ( !backEnd.screenShotBMPsilent ) {
				ri.Printf( PRINT_ALL, "Wrote %s\n", backEnd.screenshotBMP );
			}
		}
		if ( backEnd.screenshotMask & SCREENSHOT_AVI ) {
			RB_TakeVideoFrameCmd( &backEnd.vcmd );
		}

		backEnd.screenshotJPG[0] = '\0';
		backEnd.screenshotTGA[0] = '\0';
		backEnd.screenshotBMP[0] = '\0';
		backEnd.screenshotEXR[0] = '\0';
		backEnd.screenshotMask = 0;
		}
	}

#ifdef USE_VULKAN
	vk_present_frame();
#else
	ri.GLimp_EndFrame();
#endif

	backEnd.projection2D = qfalse;
	backEnd.doneSurfaces = qfalse;
	backEnd.doneWorldScene = qfalse;
	backEnd.drawConsole = qfalse;
#ifdef USE_VULKAN
	backEnd.doneBloom = qfalse;
	backEnd.doneSSAO = qfalse;
	backEnd.doneLensFlare = qfalse;
	s_deferredWeaponCmdCount = 0;
	s_skipDeferredWeaponSurfaces = qfalse;
	s_drawDeferredWeaponSurfacesOnly = qfalse;
#endif

	return (const void *)(cmd + 1);
}

#ifdef VK_CUBEMAP
/*
=============
RB_PrefilterEnvMap
=============
*/
static const void *RB_PrefilterEnvMap( const void *data )
{
	const convolveCubemapCommand_t *cmd = (const convolveCubemapCommand_t *)data;
	if ( !cmd->cubemap ) {
		ri.Printf( PRINT_WARNING, "RB_PrefilterEnvMap: missing cubemap command data\n" );
		return (const void *)(cmd + 1);
	}

	// finish any 2D drawing if needed
	if ( tess.numIndexes )
		RB_EndSurface();
	if ( !cmd->cubemap->prefiltered_image )
		cmd->cubemap->prefiltered_image = R_CreateImage( 
			va("cubemap prefiltered - %s", cmd->cubemap->name ), NULL,
			NULL, REF_CUBEMAP_SIZE, REF_CUBEMAP_SIZE, 
			IMGFLAG_CUBEMAP | IMGFLAG_MIPMAP, 
			VK_FORMAT_R16G16B16A16_SFLOAT, 0 );
	
	if ( !cmd->cubemap->irradiance_image )
		cmd->cubemap->irradiance_image = R_CreateImage( 
			va("cubemap irradiance - %s", cmd->cubemap->name ), NULL, 
			NULL, REF_CUBEMAP_IRRADIANCE_SIZE, REF_CUBEMAP_IRRADIANCE_SIZE, 
			IMGFLAG_CUBEMAP | IMGFLAG_MIPMAP, 
			VK_FORMAT_R32G32B32A32_SFLOAT, 0 );

	if ( !cmd->cubemap->prefiltered_image || !cmd->cubemap->irradiance_image ) {
		ri.Printf( PRINT_WARNING, "RB_PrefilterEnvMap: failed to allocate prefilter targets for '%s'\n", cmd->cubemap->name );
		return (const void *)(cmd + 1);
	}

	vk_generate_cubemaps( cmd->cubemap );

	{
		const qboolean wantPostLog =
			( r_pbr_bindlog && r_pbr_bindlog->integer ) ||
			( r_ibl_forceCapture && r_ibl_forceCapture->integer ) ||
			( r_pbr_debug && r_pbr_debug->integer >= 17 );
		image_t *preImg = cmd->cubemap->prefiltered_image;
		image_t *irrImg = cmd->cubemap->irradiance_image;
		VkImageView preView = ( preImg ) ? preImg->view : VK_NULL_HANDLE;
		VkImageView irrView = ( irrImg ) ? irrImg->view : VK_NULL_HANDLE;

		if ( wantPostLog ) {
			vk_wait_idle();
			ri.Printf( PRINT_ALL,
				"PBR IBL post: idx=%d preImg=%p preView=%p irrImg=%p irrView=%p name=%s\n",
				cmd->cubemapId,
				(void *)preImg, (void *)preView,
				(void *)irrImg, (void *)irrView,
				cmd->cubemap->name[0] ? cmd->cubemap->name : "<unnamed>" );
		}
		if ( preView == VK_NULL_HANDLE || irrView == VK_NULL_HANDLE ) {
			ri.Printf( PRINT_WARNING,
				"PBR IBL post: cubemap '%s' (idx=%d) has null view(s) after convolution (pre=%p irr=%p)\n",
				cmd->cubemap->name[0] ? cmd->cubemap->name : "<unnamed>",
				cmd->cubemapId, (void *)preView, (void *)irrView );
		}
	}

	return (const void *)(cmd + 1);
}
#endif

#ifdef USE_VULKAN
/*
====================
RB_UIFilter

Forward a queued CSS backdrop-filter / filter blur op to the UI blur compositor.
====================
*/
static const void *RB_UIFilter( const void *data ) {
	const uiFilterCommand_t *cmd = data;
	if ( cmd->kind == 0 ) {
		vk_ui_blur_enqueue_backdrop( &cmd->backdrop );
	} else {
		vk_ui_blur_enqueue_layer( &cmd->layer );
	}
	return (const void *)(cmd + 1);
}
#endif

/*
====================
RB_ExecuteRenderCommands
====================
*/
void RB_ExecuteRenderCommands( const void *data ) {

	backEnd.pc.msec = ri.Milliseconds();

	while ( 1 ) {
		data = PADP(data, sizeof(void *));

		switch ( *(const int *)data ) {
		case RC_SET_COLOR:
			data = RB_SetColor( data );
			break;
		case RC_STRETCH_PIC:
			data = RB_StretchPic( data );
			break;
		case RC_VECTOR_FONT_STRING:
			data = RB_VectorFontString( data );
			break;
		case RC_DRAW_SURFS:
			data = RB_DrawSurfs( data );
			break;
		case RC_DRAW_BUFFER:
			data = RB_DrawBuffer( data );
			break;
		case RC_SWAP_BUFFERS:
			data = RB_SwapBuffers( data );
			break;
		case RC_FINISHBLOOM:
			data = RB_FinishBloom(data);
			break;
		case RC_COLORMASK:
			data = RB_ColorMask(data);
			break;
		case RC_CLEARDEPTH:
			data = RB_ClearDepth(data);
			break;
#ifdef VK_CUBEMAP
		case RC_CONVOLVECUBEMAP:
			data = RB_PrefilterEnvMap( data );
			break;
#endif
		case RC_CLEARCOLOR:
			data = RB_ClearColor(data);
			break;
#ifdef USE_VULKAN
		case RC_UI_FILTER:
			data = RB_UIFilter(data);
			break;
#endif
		case RC_END_OF_LIST:
		default:
			// stop rendering
#ifdef USE_VULKAN
			vk_end_frame();
//			if (com_errorEntered && (begin_frame_called && !end_frame_called)) {
//				vk_end_frame();
//			}
#else
			backEnd.pc.msec = ri.Milliseconds() - backEnd.pc.msec;
#endif
			return;
		}
	}
}
