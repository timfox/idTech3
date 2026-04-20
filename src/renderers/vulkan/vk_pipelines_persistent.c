/*
===========================================================================
Long-lived VkPipeline handles allocated once (skybox, fog, debug draws, etc.).
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_pipelines_persistent.h"

void vk_alloc_persistent_pipelines( void )
{
	unsigned int state_bits;
	Vk_Pipeline_Def def;

	// skybox
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.shader_type = TYPE_SIGNLE_TEXTURE_FIXED_COLOR;
		def.color.rgb = tr.identityLightByte;
		def.color.alpha = tr.identityLightByte;
		def.face_culling = CT_FRONT_SIDED;
		def.polygon_offset = qfalse;
		def.mirror = qfalse;
		vk.skybox_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// stencil shadows (polygon offset pushes volume forward to reduce thin black lines)
	{
		cullType_t cull_types[2] = { CT_FRONT_SIDED, CT_BACK_SIDED };
		qboolean mirror_flags[2] = { qfalse, qtrue };
		int i, j;

		Com_Memset( &def, 0, sizeof( def ) );
		def.polygon_offset = qtrue;
		def.state_bits = 0;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.shadow_phase = SHADOW_EDGES;

		for ( i = 0; i < 2; i++ ) {
			def.face_culling = cull_types[i];
			for ( j = 0; j < 2; j++ ) {
				def.mirror = mirror_flags[j];
				vk.shadow_volume_pipelines[i][j] = vk_find_pipeline_ext( 0, &def, r_shadows->integer ? qtrue : qfalse );
			}
		}
	}
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.face_culling = CT_FRONT_SIDED;
		def.polygon_offset = qfalse;
		def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ZERO;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.mirror = qfalse;
		def.shadow_phase = SHADOW_FS_QUAD;
		def.primitives = TRIANGLE_STRIP;
		vk.shadow_finish_pipeline = vk_find_pipeline_ext( 0, &def, r_shadows->integer ? qtrue : qfalse );
	}

	// fog and dlights
	{
		unsigned int fog_state_bits[2] = {
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA | GLS_DEPTHFUNC_EQUAL, // fogPass == FP_EQUAL
			GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA // fogPass == FP_LE
		};
		unsigned int dlight_state_bits[2] = {
			GLS_SRCBLEND_DST_COLOR | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL,	// modulated
			GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL			// additive
		};
		qboolean polygon_offset[2] = { qfalse, qtrue };
		int i, j, k;
		int l;

		Com_Memset( &def, 0, sizeof( def ) );
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.mirror = qfalse;

		for ( i = 0; i < 2; i++ ) {
			unsigned fog_state = fog_state_bits[i];
			unsigned dlight_state = dlight_state_bits[i];

			for ( j = 0; j < 3; j++ ) {
				def.face_culling = j; // cullType_t value

				for ( k = 0; k < 2; k++ ) {
					def.polygon_offset = polygon_offset[k];
#ifdef USE_FOG_ONLY
					def.shader_type = TYPE_FOG_ONLY;
#else
					def.shader_type = TYPE_SIGNLE_TEXTURE;
#endif
					def.state_bits = fog_state;
					vk.fog_pipelines[i][j][k] = vk_find_pipeline_ext( 0, &def, qtrue );

					def.shader_type = TYPE_SIGNLE_TEXTURE;
					def.state_bits = dlight_state;
					vk.dlight_pipelines[i][j][k] = vk_find_pipeline_ext( 0, &def, r_dlightMode->integer == 0 ? qtrue : qfalse );
				}
			}
		}

		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE | GLS_DEPTHFUNC_EQUAL;
		for ( i = 0; i < 3; i++ ) { // cullType
			def.face_culling = i;
			for ( j = 0; j < 2; j++ ) { // polygonOffset
				def.polygon_offset = polygon_offset[j];
				for ( k = 0; k < 2; k++ ) {
					def.fog_stage = k; // fogStage
					for ( l = 0; l < 2; l++ ) {
						def.abs_light = l;
						def.shader_type = TYPE_SIGNLE_TEXTURE_LIGHTING;
						vk.dlight_pipelines_x[i][j][k][l] = vk_find_pipeline_ext( 0, &def, qfalse );
						def.shader_type = TYPE_SIGNLE_TEXTURE_LIGHTING_LINEAR;
						vk.dlight1_pipelines_x[i][j][k][l] = vk_find_pipeline_ext( 0, &def, qfalse );
					}
				}
			}
		}
	}

	// RT_BEAM surface
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.state_bits = GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.face_culling = CT_FRONT_SIDED;
		def.primitives = TRIANGLE_STRIP;
		vk.surface_beam_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// axis for missing models
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.state_bits = GLS_DEFAULT;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.face_culling = CT_TWO_SIDED;
		def.primitives = LINE_LIST;
		if ( vk.wideLines ) {
			def.line_width = 3;
		}
		vk.surface_axis_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// flare visibility test dot
	if ( vk.fragmentStores ) {
		Com_Memset( &def, 0, sizeof( def ) );
		def.face_culling = CT_TWO_SIDED;
		def.shader_type = TYPE_DOT;
		def.primitives = POINT_LIST;
		vk.dot_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// occlusion culling: bbox pipeline (depth test on, depth/color write off)
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.state_bits = 0; /* depth test on, depth write off */
		def.face_culling = CT_FRONT_SIDED;
		def.shader_type = TYPE_OCCLUSION_BBOX;
		def.primitives = TRIANGLE_LIST;
		vk.occlusion_bbox_pipeline = vk_find_pipeline_ext( 0, &def, qtrue );
	}

	// DrawTris()
	state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE;
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_WHITE;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_WHITE;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_GREEN;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_green_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_GREEN;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_green_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_RED;
		def.face_culling = CT_FRONT_SIDED;
		vk.tris_debug_red_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.state_bits = state_bits;
		def.shader_type = TYPE_COLOR_RED;
		def.face_culling = CT_BACK_SIDED;
		vk.tris_mirror_debug_red_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// DrawNormals()
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.state_bits = GLS_DEPTHMASK_TRUE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = LINE_LIST;
		vk.normals_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// RB_DebugPolygon()
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.state_bits = GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		vk.surface_debug_pipeline_solid = vk_find_pipeline_ext( 0, &def, qfalse );
	}
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.state_bits = GLS_POLYMODE_LINE | GLS_DEPTHMASK_TRUE | GLS_SRCBLEND_ONE | GLS_DSTBLEND_ONE;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = LINE_LIST;
		vk.surface_debug_pipeline_outline = vk_find_pipeline_ext( 0, &def, qfalse );
	}

	// RB_ShowImages
	{
		Com_Memset( &def, 0, sizeof( def ) );
		def.state_bits = GLS_DEPTHTEST_DISABLE | GLS_SRCBLEND_SRC_ALPHA | GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
		def.shader_type = TYPE_SIGNLE_TEXTURE;
		def.primitives = TRIANGLE_STRIP;
		vk.images_debug_pipeline = vk_find_pipeline_ext( 0, &def, qfalse );

		def.state_bits = GLS_DEPTHTEST_DISABLE;
		def.shader_type = TYPE_COLOR_BLACK;
		def.primitives = TRIANGLE_STRIP;
		vk.images_debug_pipeline2 = vk_find_pipeline_ext( 0, &def, qfalse );
	}
}
