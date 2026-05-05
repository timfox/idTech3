#include "tr_local.h"
#include "vk.h"
#include "vk_temporal.h"
#include "vk_view_state.h"

typedef struct vkMvpPushConstants_s {
	float mvp[16];
	float prev_mvp[16];
	float reserved[8]; /* padding / future push data; size must match VkPushConstantRange in vk_init_device.c */
} vkMvpPushConstants_t;

static VkRect2D vk_scene_src_rect;
static qboolean vk_scene_src_rect_valid;

static float vk_prev_entity_model_matrices[MAX_REFENTITIES][16];
static float vk_curr_entity_model_matrices[MAX_REFENTITIES][16];
static int vk_prev_entity_model_handles[MAX_REFENTITIES];
static int vk_curr_entity_model_handles[MAX_REFENTITIES];
static int vk_prev_entity_types[MAX_REFENTITIES];
static int vk_curr_entity_types[MAX_REFENTITIES];
static qboolean vk_prev_entity_model_valid[MAX_REFENTITIES];
static qboolean vk_curr_entity_model_valid[MAX_REFENTITIES];

uint32_t vk_get_render_target_width( void )
{
	if ( vk.fboActive && vk.mainColorWidth > 0u ) {
		return vk.mainColorWidth;
	}
	if ( vk.renderWidth > 0 ) {
		return vk.renderWidth;
	}
	if ( glConfig.vidWidth > 0 ) {
		return (uint32_t)glConfig.vidWidth;
	}
	return 1u;
}

uint32_t vk_get_render_target_height( void )
{
	if ( vk.fboActive && vk.mainColorHeight > 0u ) {
		return vk.mainColorHeight;
	}
	if ( vk.renderHeight > 0 ) {
		return vk.renderHeight;
	}
	if ( glConfig.vidHeight > 0 ) {
		return (uint32_t)glConfig.vidHeight;
	}
	return 1u;
}

static float vk_get_2d_logical_width( void )
{
	if ( glConfig.vidWidth > 0 ) {
		return (float)glConfig.vidWidth;
	}
	return (float)vk_get_render_target_width();
}

static float vk_get_2d_logical_height( void )
{
	if ( glConfig.vidHeight > 0 ) {
		return (float)glConfig.vidHeight;
	}
	return (float)vk_get_render_target_height();
}

void vk_reset_scene_src_rect_tracking( void )
{
	vk_scene_src_rect_valid = qfalse;
}

qboolean vk_get_scene_src_rect( VkRect2D *out_rect )
{
	if ( !out_rect || !vk_scene_src_rect_valid ) {
		return qfalse;
	}

	*out_rect = vk_scene_src_rect;
	return qtrue;
}

static void vk_get_viewport_rect( VkRect2D *r )
{
	if ( backEnd.projection2D ) {
		r->offset.x = 0;
		r->offset.y = 0;
		r->extent.width = vk.renderWidth;
		r->extent.height = vk.renderHeight;
	} else {
		r->offset.x = backEnd.viewParms.viewportX * vk.renderScaleX;
		r->offset.y = vk.renderHeight - ( backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight ) * vk.renderScaleY;
		r->extent.width = (float)backEnd.viewParms.viewportWidth * vk.renderScaleX;
		r->extent.height = (float)backEnd.viewParms.viewportHeight * vk.renderScaleY;
	}
}

static void vk_get_viewport( VkViewport *viewport, Vk_Depth_Range depth_range )
{
	VkRect2D r;

	vk_get_viewport_rect( &r );

	viewport->x = (float)r.offset.x;
	viewport->y = (float)r.offset.y;
	viewport->width = (float)r.extent.width;
	viewport->height = (float)r.extent.height;

	switch ( depth_range ) {
		default:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ZERO:
			viewport->minDepth = 1.0f;
			viewport->maxDepth = 1.0f;
			break;
		case DEPTH_RANGE_ONE:
			viewport->minDepth = 0.0f;
			viewport->maxDepth = 0.0f;
			break;
		case DEPTH_RANGE_WEAPON:
			viewport->minDepth = 0.6f;
			viewport->maxDepth = 1.0f;
			break;
	}
}

void vk_get_scissor_rect( VkRect2D *r )
{
	if ( backEnd.viewParms.portalView != PV_NONE ) {
		const uint32_t targetWidth = vk_get_render_target_width();
		const uint32_t targetHeight = vk_get_render_target_height();
		r->offset.x = (int32_t)( (float)backEnd.viewParms.scissorX * vk.renderScaleX );
		r->offset.y = (int32_t)( (float)targetHeight -
			(float)( backEnd.viewParms.scissorY + backEnd.viewParms.scissorHeight ) * vk.renderScaleY );
		r->extent.width = (uint32_t)( (float)backEnd.viewParms.scissorWidth * vk.renderScaleX );
		r->extent.height = (uint32_t)( (float)backEnd.viewParms.scissorHeight * vk.renderScaleY );
		if ( r->offset.x < 0 ) {
			r->offset.x = 0;
		}
		if ( r->offset.y < 0 ) {
			r->offset.y = 0;
		}
		if ( (uint32_t)r->offset.x >= targetWidth || (uint32_t)r->offset.y >= targetHeight ) {
			r->extent.width = 0;
			r->extent.height = 0;
			return;
		}
		if ( (uint32_t)r->offset.x + r->extent.width > targetWidth ) {
			r->extent.width = targetWidth - (uint32_t)r->offset.x;
		}
		if ( (uint32_t)r->offset.y + r->extent.height > targetHeight ) {
			r->extent.height = targetHeight - (uint32_t)r->offset.y;
		}
	} else {
		const uint32_t targetWidth = vk_get_render_target_width();
		const uint32_t targetHeight = vk_get_render_target_height();
		vk_get_viewport_rect( r );

		if ( r->offset.x < 0 ) {
			r->offset.x = 0;
		}
		if ( r->offset.y < 0 ) {
			r->offset.y = 0;
		}

		if ( (uint32_t)r->offset.x >= targetWidth || (uint32_t)r->offset.y >= targetHeight ) {
			r->extent.width = 0;
			r->extent.height = 0;
			return;
		}

		if ( (uint32_t)r->offset.x + r->extent.width > targetWidth ) {
			r->extent.width = targetWidth - (uint32_t)r->offset.x;
		}
		if ( (uint32_t)r->offset.y + r->extent.height > targetHeight ) {
			r->extent.height = targetHeight - (uint32_t)r->offset.y;
		}
	}
}

void vk_get_projection_matrix_vk( const float *projection_matrix, float *projection_vk )
{
	Com_Memcpy( projection_vk, projection_matrix, sizeof( float ) * 16 );
	projection_vk[5] = -projection_matrix[5];
}

static void vk_get_mvp_transform( float *mvp )
{
	if ( backEnd.projection2D ) {
		float mvp0 = 2.0f / vk_get_2d_logical_width();
		float mvp5 = 2.0f / vk_get_2d_logical_height();

		mvp[0]  =  mvp0; mvp[1]  =  0.0f; mvp[2]  = 0.0f; mvp[3]  = 0.0f;
		mvp[4]  =  0.0f; mvp[5]  =  mvp5; mvp[6]  = 0.0f; mvp[7]  = 0.0f;
		mvp[8]  =  0.0f; mvp[9]  =  0.0f; mvp[10] = 0.0f; mvp[11] = 0.0f;
		mvp[12] = -1.0f; mvp[13] = -1.0f; mvp[14] = 1.0f; mvp[15] = 1.0f;
	} else {
		float proj[16];
		const float *projection = backEnd.useFirstPersonProjection
			? backEnd.firstPersonProjectionMatrix
			: backEnd.viewParms.projectionMatrix;
		vk_get_projection_matrix_vk( projection, proj );
		myGlMultMatrix( vk_world.modelview_transform, proj, mvp );
	}
}

void vk_prime_gpu_morph_weights_current( void )
{
	unsigned int i;
	trRefEntity_t *ents;

	if ( !backEndData ) {
		return;
	}
	ents = backEndData->entities;
	for ( i = 0; i < (unsigned int)MAX_REFENTITIES; i++ ) {
		ents[i].morphGpuWeightsPrimedSingleUse = qfalse;
	}
}

void vk_snap_gpu_morph_weights_for_motion( void )
{
	unsigned int i, k;
	const unsigned int n = tr.refdef.num_entities;
	trRefEntity_t *ents;

	if ( n == 0 ) {
		return;
	}
	ents = tr.refdef.entities;
	if ( !ents ) {
		return;
	}
	for ( i = 0; i < n; i++ ) {
		trRefEntity_t *re = ents + i;
		int ch;
		for ( k = 0; k < (unsigned int)IQM_MORPH_TOP_K; k++ ) {
			re->morphGpuWeightPrev[k] = re->morphActiveWeight[k];
		}
		for ( ch = 0; ch < re->morphChannelCount && ch < IQM_MORPH_MAX_CHANNELS; ch++ ) {
			re->morphChannelWeightPrev[ch] = re->morphChannelWeights[ch];
		}
	}
}

void vk_begin_motion_frame( void )
{
	for ( int i = 0; i < MAX_REFENTITIES; i++ ) {
		if ( vk_curr_entity_model_valid[i] ) {
			Com_Memcpy( vk_prev_entity_model_matrices[i], vk_curr_entity_model_matrices[i], sizeof( vk_prev_entity_model_matrices[i] ) );
			vk_prev_entity_model_handles[i] = vk_curr_entity_model_handles[i];
			vk_prev_entity_types[i] = vk_curr_entity_types[i];
			vk_prev_entity_model_valid[i] = qtrue;
		} else {
			vk_prev_entity_model_valid[i] = qfalse;
		}
		vk_curr_entity_model_valid[i] = qfalse;
	}
}

void vk_reset_motion_history( void )
{
	Com_Memset( vk_prev_entity_model_matrices, 0, sizeof( vk_prev_entity_model_matrices ) );
	Com_Memset( vk_curr_entity_model_matrices, 0, sizeof( vk_curr_entity_model_matrices ) );
	Com_Memset( vk_prev_entity_model_handles, 0, sizeof( vk_prev_entity_model_handles ) );
	Com_Memset( vk_curr_entity_model_handles, 0, sizeof( vk_curr_entity_model_handles ) );
	Com_Memset( vk_prev_entity_types, 0, sizeof( vk_prev_entity_types ) );
	Com_Memset( vk_curr_entity_types, 0, sizeof( vk_curr_entity_types ) );
	Com_Memset( vk_prev_entity_model_valid, 0, sizeof( vk_prev_entity_model_valid ) );
	Com_Memset( vk_curr_entity_model_valid, 0, sizeof( vk_curr_entity_model_valid ) );
}

static int vk_get_current_entity_motion_index( void )
{
	const trRefEntity_t *ent = backEnd.currentEntity;
	const trRefEntity_t *base = backEnd.refdef.entities;

	if ( !ent || ent == &tr.worldEntity || !base || backEnd.refdef.num_entities <= 0 ) {
		return -1;
	}
	if ( ent < base || ent >= base + backEnd.refdef.num_entities ) {
		return -1;
	}
	return (int)( ent - base );
}

static qboolean vk_entity_requires_no_motion( const trRefEntity_t *ent )
{
	qboolean markUnreliable = qfalse;

	if ( !ent ) {
		return qfalse;
	}
	if ( ent->e.frame != ent->e.oldframe ) {
		markUnreliable = qtrue;
	}
	if ( !markUnreliable && ent->e.backlerp > 0.001f ) {
		markUnreliable = qtrue;
	}
	if ( !markUnreliable && ent->e.customShader ) {
		markUnreliable = qtrue;
	}
	/* View weapon / first-person geometry: model matrix is view-relative; per-entity
	 * prev-model history does not represent prior screen motion. Use current MVP as prev. */
	if ( !markUnreliable && ( ent->e.renderfx & RF_FIRST_PERSON ) ) {
		markUnreliable = qtrue;
	}
	if ( markUnreliable ) {
		if ( !vk.temporal.unreliableMotionThisFrame && r_temporalDebug && r_temporalDebug->integer >= 2 ) {
			ri.Printf( PRINT_DEVELOPER, "[VK][temporal] unreliable motion for entity type=%d customShader=%d frame=%d oldframe=%d backlerp=%.3f rf_fp=%d\n",
				ent->e.reType, ent->e.customShader, ent->e.frame, ent->e.oldframe, ent->e.backlerp,
				( ent->e.renderfx & RF_FIRST_PERSON ) ? 1 : 0 );
		}
		vk.temporal.unreliableMotionThisFrame = qtrue;
		return qtrue;
	}
	return qfalse;
}

static void vk_get_prev_mvp_transform( float *prev_mvp )
{
	float prev_model[16];
	float prev_model_view[16];
	float prev_proj[16];
	int motion_index;

	if ( backEnd.projection2D || !vk_prev_matrices_valid ) {
		vk_get_mvp_transform( prev_mvp );
		return;
	}

	Com_Memcpy( prev_model, backEnd.or.modelMatrix, sizeof( prev_model ) );

	motion_index = vk_get_current_entity_motion_index();
	if ( motion_index >= 0 && backEnd.currentEntity && backEnd.currentEntity->e.reType == RT_MODEL ) {
		if ( !vk_curr_entity_model_valid[motion_index] ) {
			Com_Memcpy( vk_curr_entity_model_matrices[motion_index], backEnd.or.modelMatrix, sizeof( vk_curr_entity_model_matrices[motion_index] ) );
			vk_curr_entity_model_handles[motion_index] = backEnd.currentEntity->e.hModel;
			vk_curr_entity_types[motion_index] = (int)backEnd.currentEntity->e.reType;
			vk_curr_entity_model_valid[motion_index] = qtrue;
		}

		/* Rigid entity motion: previous model matrix. GPU skin deformation uses prev pose in the skin SSBO. */
		if ( !vk_entity_requires_no_motion( backEnd.currentEntity ) &&
			vk_prev_entity_model_valid[motion_index] &&
			vk_prev_entity_model_handles[motion_index] == backEnd.currentEntity->e.hModel &&
			vk_prev_entity_types[motion_index] == (int)backEnd.currentEntity->e.reType ) {
			Com_Memcpy( prev_model, vk_prev_entity_model_matrices[motion_index], sizeof( prev_model ) );
		}
	}

	myGlMultMatrix( prev_model, vk_prev_view_matrix, prev_model_view );
	vk_get_projection_matrix_vk( vk_prev_projection_matrix, prev_proj );
	myGlMultMatrix( prev_model_view, prev_proj, prev_mvp );
}

void vk_update_mvp( const float *m )
{
	vkMvpPushConstants_t push_constants;
	VkPipelineLayout layout;
	VkShaderStageFlags stage_flags;
	uint32_t push_bytes;

	Com_Memset( &push_constants, 0, sizeof( push_constants ) );

	if ( m ) {
		Com_Memcpy( push_constants.mvp, m, sizeof( push_constants.mvp ) );
	} else {
		vk_get_mvp_transform( push_constants.mvp );
	}
	vk_get_prev_mvp_transform( push_constants.prev_mvp );
	push_constants.reserved[0] = ( tess.sdfUiEdge >= 0.0f ) ? tess.sdfUiEdge : 0.0f;

	layout = ( backEnd.oitAccumPass && vk.pipeline_layout_oit_accum != VK_NULL_HANDLE ) ?
		vk.pipeline_layout_oit_accum : vk.pipeline_layout;
	/*
	 * Pipeline layouts declare this push range for VERTEX|FRAGMENT (vk_init_device.c).
	 * Stages omitted from vkCmdPushConstants do not receive the update (Vulkan spec);
	 * frag_ui_sdf_text.frag reads sdfEdgeSmooth from bytes after the two mat4s.
	 */
	stage_flags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	push_bytes = (uint32_t)sizeof( push_constants );
	if ( layout == vk.pipeline_layout_oit_accum && vk.pipeline_layout_oit_accum != VK_NULL_HANDLE ) {
		/* OIT accum layout only reserves two mat4 (128 B); no reserved[] tail. */
		push_bytes = (uint32_t)( sizeof( float ) * 32 );
	}
	qvkCmdPushConstants( vk.cmd->command_buffer, layout, stage_flags, 0, push_bytes, &push_constants );

	vk.stats.push_size += push_bytes;
}

void vk_update_depth_range( Vk_Depth_Range depth_range )
{
	if ( vk.cmd->depth_range != depth_range ) {
		VkRect2D scissor_rect;
		VkViewport viewport;

		vk.cmd->depth_range = depth_range;

		vk_get_scissor_rect( &scissor_rect );

		if ( memcmp( &vk.cmd->scissor_rect, &scissor_rect, sizeof( scissor_rect ) ) != 0 ) {
			qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor_rect );
			vk.cmd->scissor_rect = scissor_rect;
		}

		vk_get_viewport( &viewport, depth_range );
		qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
	}

	if ( !backEnd.projection2D &&
		( vk.renderPassIndex == RENDER_PASS_MAIN || vk.renderPassIndex == RENDER_PASS_POST_BLOOM ) ) {
		VkRect2D r;
		uint32_t maxW = ( glConfig.vidWidth > 0 ) ? (uint32_t)glConfig.vidWidth : 1u;
		uint32_t maxH = ( glConfig.vidHeight > 0 ) ? (uint32_t)glConfig.vidHeight : 1u;
		uint64_t area;
		uint64_t bestArea;

		vk_get_viewport_rect( &r );

		if ( r.offset.x < 0 ) {
			int dx = -r.offset.x;
			r.offset.x = 0;
			r.extent.width = ( r.extent.width > (uint32_t)dx ) ? ( r.extent.width - (uint32_t)dx ) : 0u;
		}
		if ( r.offset.y < 0 ) {
			int dy = -r.offset.y;
			r.offset.y = 0;
			r.extent.height = ( r.extent.height > (uint32_t)dy ) ? ( r.extent.height - (uint32_t)dy ) : 0u;
		}
		if ( (uint32_t)r.offset.x >= maxW || (uint32_t)r.offset.y >= maxH ) {
			return;
		}
		if ( (uint32_t)r.offset.x + r.extent.width > maxW ) {
			r.extent.width = maxW - (uint32_t)r.offset.x;
		}
		if ( (uint32_t)r.offset.y + r.extent.height > maxH ) {
			r.extent.height = maxH - (uint32_t)r.offset.y;
		}

		area = (uint64_t)r.extent.width * (uint64_t)r.extent.height;
		if ( area == 0 ) {
			return;
		}
		bestArea = vk_scene_src_rect_valid ? (uint64_t)vk_scene_src_rect.extent.width * (uint64_t)vk_scene_src_rect.extent.height : 0u;
		if ( !vk_scene_src_rect_valid || area > bestArea ) {
			vk_scene_src_rect = r;
			vk_scene_src_rect_valid = qtrue;
		}
	}
}
