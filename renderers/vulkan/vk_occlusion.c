/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Occlusion queries + visibility buffer for GPU entity culling (split from vk.c).
===========================================================================
*/

#include "tr_local.h"
#include "vk_occlusion.h"
#include "vk_view_state.h"
#include "vk_util.h"

/* GPU occlusion culling: visibility from previous frame (1=visible, 0=occluded) */
uint64_t vk_entity_occlusion_visibility[MAX_REFENTITIES];
static uint32_t vk_occlusion_last_entity_count;

void vk_occlusion_seed_visibility_all_visible( void )
{
	Com_Memset( vk_entity_occlusion_visibility, 0xFF, sizeof( vk_entity_occlusion_visibility ) );
}

/* Unit cube for occlusion bbox: 8 vertices, 36 indices (12 triangles) */
static const float s_occlusion_cube_verts[8][3] = {
	{ 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { 1, 1, 0 },
	{ 0, 0, 1 }, { 1, 0, 1 }, { 0, 1, 1 }, { 1, 1, 1 }
};
static const uint16_t s_occlusion_cube_indices[36] = {
	0,2,1, 1,2,3, 4,5,6, 5,7,6, 0,1,4, 1,5,4,
	2,6,3, 3,6,7, 0,4,2, 2,4,6, 1,3,5, 3,7,5
};

void vk_occlusion_draw_entity_bboxes( const struct drawSurfsCommand_s *cmd )
{
	const trRefEntity_t *ent;
	vec3_t mins, maxs;
	float model[16], mvp[16];
	int i, n;
	const drawSurfsCommand_t *c = (const drawSurfsCommand_t *)cmd;

	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE )
		return;
	if ( vk.occlusion_query_pool == VK_NULL_HANDLE || vk.occlusion_bbox_pipeline == 0 ||
		!qvkCmdBeginQuery || !qvkCmdEndQuery || !qvkCmdResetQueryPool )
		return;

	n = c->refdef.num_entities;
	vk_occlusion_last_entity_count = (uint32_t)n;
	if ( n <= 0 || n > MAX_REFENTITIES )
		return;

	qvkCmdResetQueryPool( vk.cmd->command_buffer, vk.occlusion_query_pool, 0, (uint32_t)n );
	vk_bind_pipeline( vk.occlusion_bbox_pipeline );

	for ( i = 0; i < n; i++ ) {
		ent = &c->refdef.entities[i];
		if ( ent->e.reType != RT_MODEL || !ent->e.hModel )
			continue;

		R_ModelBounds( ent->e.hModel, mins, maxs );
		R_RotateForEntity( ent, &c->viewParms, &backEnd.or );

		/* Build model matrix: origin + axis * (mins + v * (maxs-mins)) for v in [0,1]^3 */
		{
			float *m = model;
			float sx = maxs[0] - mins[0], sy = maxs[1] - mins[1], sz = maxs[2] - mins[2];
			const float *ax0 = backEnd.or.axis[0], *ax1 = backEnd.or.axis[1], *ax2 = backEnd.or.axis[2];
			const float *o = backEnd.or.origin;

			m[0]  = ax0[0]*sx; m[1]  = ax0[1]*sx; m[2]  = ax0[2]*sx; m[3]  = 0;
			m[4]  = ax1[0]*sy; m[5]  = ax1[1]*sy; m[6]  = ax1[2]*sy; m[7]  = 0;
			m[8]  = ax2[0]*sz; m[9]  = ax2[1]*sz; m[10] = ax2[2]*sz; m[11] = 0;
			m[12] = o[0] + ax0[0]*mins[0] + ax1[0]*mins[1] + ax2[0]*mins[2];
			m[13] = o[1] + ax0[1]*mins[0] + ax1[1]*mins[1] + ax2[1]*mins[2];
			m[14] = o[2] + ax0[2]*mins[0] + ax1[2]*mins[1] + ax2[2]*mins[2];
			m[15] = 1;
		}
		myGlMultMatrix( c->viewParms.world.modelViewMatrix, model, mvp );
		myGlMultMatrix( c->viewParms.projectionMatrix, mvp, mvp );

		qvkCmdBeginQuery( vk.cmd->command_buffer, vk.occlusion_query_pool, (uint32_t)i, 0 );
		vk_update_mvp( mvp );
		RB_CheckOverflow( 8, 36 );
		for ( n = 0; n < 8; n++ ) {
			tess.xyz[n][0] = s_occlusion_cube_verts[n][0];
			tess.xyz[n][1] = s_occlusion_cube_verts[n][1];
			tess.xyz[n][2] = s_occlusion_cube_verts[n][2];
			tess.xyz[n][3] = 1.0f;
		}
		tess.numVertexes = 8;
		{
			glIndex_t idx_buf[36];
			uint32_t idx_off;
			for ( n = 0; n < 36; n++ )
				idx_buf[n] = s_occlusion_cube_indices[n];
			vk_bind_geometry( TESS_XYZ );
			idx_off = vk_tess_index( 36, idx_buf );
			if ( idx_off != ~0U ) {
				vk_bind_index_buffer( vk.cmd->vertex_buffer, idx_off );
				vk.cmd->num_indexes = 36;
				vk_draw_geometry( DEPTH_RANGE_NORMAL, qtrue );
			}
		}
		qvkCmdEndQuery( vk.cmd->command_buffer, vk.occlusion_query_pool, (uint32_t)i );
	}
}

void vk_occlusion_readback( void )
{
	VkResult res;
	uint32_t n;

	if ( vk.occlusion_query_pool == VK_NULL_HANDLE || !qvkGetQueryPoolResults )
		return;

	n = vk_occlusion_last_entity_count;
	if ( n <= 0 || n > MAX_REFENTITIES )
		return;

	/* VUID-09401: queries must be reset before use. vk_occlusion_draw_entity_bboxes resets
	 * before begin/end. If we never drew this frame, skip readback (queries may be uninitialized). */
	res = qvkGetQueryPoolResults( vk.device, vk.occlusion_query_pool, 0, n,
		sizeof( vk_entity_occlusion_visibility ), vk_entity_occlusion_visibility,
		sizeof( uint64_t ), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT );
	if ( res != VK_SUCCESS ) {
		/*
		 * Raster Ultra 1.6: never treat failed/stale query readback as "all occluded".
		 * Zeroing hid every entity for a frame (one-frame disappearance). Prefer
		 * conservative all-visible until the next successful query — same as camera-cut reset.
		 */
		ri.Printf( PRINT_WARNING, "Occlusion readback failed: %s (keeping entities visible)\n",
			vk_result_string( res ) );
		Com_Memset( vk_entity_occlusion_visibility, 0xFF, sizeof( vk_entity_occlusion_visibility ) );
		return;
	}
}

void vk_occlusion_pass( const struct drawSurfsCommand_s *cmd )
{
	/* Called from RB_DrawSurfs. World depth + entity bboxes are done there. */
	(void)cmd;
}

void vk_reset_occlusion_visibility( void )
{
	/* Stale visibility after camera cut / world change; treat all visible until next query */
	Com_Memset( vk_entity_occlusion_visibility, 0xFF, sizeof( vk_entity_occlusion_visibility ) );
}
