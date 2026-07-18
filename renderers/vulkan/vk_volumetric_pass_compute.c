/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Volumetric fog pass: local light shadow atlases, froxel compute, history,
composite fullscreen draw, SMAA subpasses. Split from vk.c.
===========================================================================
*/

#include "tr_local.h"
#include "vk_image_layout.h"
#include "vk_render_pass.h"
#include "vk_atmosphere.h"
#include "vk_volumetric_params.h"
#include "vk_volumetric_pass.h"
#include "vk_post_aa.h"
#include "vk_volumetric_internal.h"
#include "vk_sim_render_debug.h"
#include "vk_post_fog.h"
#include "vk_temporal.h"
#include "vk_nslm.h"

static const float vk_local_shadow_flip_matrix[16] = {
	0, 0, -1, 0,
	-1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 0, 1
};

static qboolean vk_begin_local_spot_shadow_render_pass( void )
{
	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

	if ( !vk.fboActive || vk.render_pass.local_spot_shadow == VK_NULL_HANDLE ||
		vk.framebuffers.local_spot_shadow == VK_NULL_HANDLE ||
		vk.local_spot_shadow_atlas_image == VK_NULL_HANDLE ||
		vk.local_spot_shadow_atlas_size == 0 )
	{
		return qfalse;
	}

	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	vk_end_render_pass();

	record_image_layout_transition( vk.cmd->command_buffer, vk.local_spot_shadow_atlas_image, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );

	vk.renderPassIndex = RENDER_PASS_SUN_SHADOW;
	vk.renderWidth = vk.local_spot_shadow_atlas_size;
	vk.renderHeight = vk.local_spot_shadow_atlas_size;
	vk.renderScaleX = vk.renderScaleY = 1.0f;
	vk_begin_render_pass_tracked( vk.render_pass.local_spot_shadow, vk.framebuffers.local_spot_shadow, qtrue, vk.renderWidth, vk.renderHeight );

	return qtrue;
}

static void vk_end_local_spot_shadow_render_pass( void )
{
	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

	if ( !vk.fboActive || vk.local_spot_shadow_atlas_image == VK_NULL_HANDLE ) {
		return;
	}

	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	vk_end_render_pass();

	record_image_layout_transition( vk.cmd->command_buffer, vk.local_spot_shadow_atlas_image, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
}

static qboolean vk_begin_local_point_shadow_render_pass( int faceLayer )
{
	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

	if ( !vk.fboActive || vk.render_pass.local_point_shadow == VK_NULL_HANDLE ||
		vk.local_point_shadow_array_image == VK_NULL_HANDLE ||
		vk.local_point_shadow_face_size == 0 ||
		faceLayer < 0 || faceLayer >= (int)( vk.local_point_shadow_capacity * 6 ) ||
		vk.framebuffers.local_point_shadow[faceLayer] == VK_NULL_HANDLE )
	{
		return qfalse;
	}

	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	vk_end_render_pass();

	record_image_layout_transition( vk.cmd->command_buffer, vk.local_point_shadow_array_image, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );

	vk.renderPassIndex = RENDER_PASS_SUN_SHADOW;
	vk.renderWidth = vk.local_point_shadow_face_size;
	vk.renderHeight = vk.local_point_shadow_face_size;
	vk.renderScaleX = vk.renderScaleY = 1.0f;
	vk_begin_render_pass_tracked( vk.render_pass.local_point_shadow, vk.framebuffers.local_point_shadow[faceLayer], qtrue, vk.renderWidth, vk.renderHeight );

	return qtrue;
}

static void vk_end_local_point_shadow_render_pass( void )
{
	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;

	if ( !vk.fboActive || vk.local_point_shadow_array_image == VK_NULL_HANDLE ) {
		return;
	}

	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	vk_end_render_pass();

	record_image_layout_transition( vk.cmd->command_buffer, vk.local_point_shadow_array_image, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
}


static void vk_begin_volumetric_render_pass( void )
{
	VkFramebuffer frameBuffer = vk.framebuffers.volumetric[ vk.cmd->swapchain_image_index ];

	vk.renderWidth = glConfig.vidWidth;
	vk.renderHeight = glConfig.vidHeight;
	vk.renderScaleX = vk.renderScaleY = 1.0f;

	vk_begin_render_pass_tracked( vk.render_pass.volumetric, frameBuffer, qfalse, vk.renderWidth, vk.renderHeight );
}

/* vk_resolve_volumetric_depth_msaa, vk_fluid_simulation_pass, perf queries: vk_volumetric_internal.c */


static const dlight_t *vk_get_volumetric_local_light( int local_index )
{
	int current = 0;

	if ( local_index < 0 ) {
		return NULL;
	}

	for ( int i = 0; i < (int)backEnd.viewParms.num_dlights; i++ ) {
		const dlight_t *dl = &backEnd.viewParms.dlights[i];
		if ( dl->radius <= 0.001f ) {
			continue;
		}
		if ( current == local_index ) {
			return dl;
		}
		current++;
	}

	return NULL;
}

static qboolean vk_build_local_shadow_view_axes(
	const vec3_t origin,
	const vec3_t forward_in,
	const vec3_t up_hint,
	float fov_degrees,
	float max_distance,
	int viewportX,
	int viewportY,
	int viewportWidth,
	int viewportHeight,
	viewParms_t *shadowParms,
	float *outViewProj )
{
	vec3_t forward;
	vec3_t upRef;
	vec3_t right;
	vec3_t up;
	float viewerMatrix[16];
	float lightView[16];
	float nearPlane = ( r_znear ) ? r_znear->value : 8.0f;

	if ( !shadowParms || !outViewProj ) {
		return qfalse;
	}
	if ( viewportWidth <= 0 || viewportHeight <= 0 ) {
		return qfalse;
	}

	if ( nearPlane < 1.0f ) {
		nearPlane = 1.0f;
	}
	if ( max_distance <= nearPlane + 1.0f ) {
		return qfalse;
	}

	VectorCopy( forward_in, forward );
	if ( VectorNormalize( forward ) <= 0.0f ) {
		return qfalse;
	}

	VectorCopy( up_hint, upRef );
	if ( VectorLengthSquared( upRef ) < 1e-6f || fabsf( DotProduct( forward, upRef ) ) > 0.95f ) {
		VectorSet( upRef, 0.0f, 0.0f, 1.0f );
		if ( fabsf( DotProduct( forward, upRef ) ) > 0.95f ) {
			VectorSet( upRef, 0.0f, 1.0f, 0.0f );
		}
	}

	CrossProduct( upRef, forward, right );
	if ( VectorNormalize( right ) <= 0.0f ) {
		return qfalse;
	}
	CrossProduct( forward, right, up );
	VectorNormalize( up );

	*shadowParms = backEnd.viewParms;
	VectorCopy( origin, shadowParms->or.origin );
	VectorCopy( origin, shadowParms->pvsOrigin );
	VectorCopy( forward, shadowParms->or.axis[0] );
	VectorCopy( right, shadowParms->or.axis[1] );
	VectorCopy( up, shadowParms->or.axis[2] );
	shadowParms->portalView = PV_NONE;
	shadowParms->targetCube = NULL;
	shadowParms->targetCubeLayer = 0;
	shadowParms->viewportX = viewportX;
	shadowParms->viewportY = viewportY;
	shadowParms->viewportWidth = viewportWidth;
	shadowParms->viewportHeight = viewportHeight;
	shadowParms->scissorX = viewportX;
	shadowParms->scissorY = viewportY;
	shadowParms->scissorWidth = viewportWidth;
	shadowParms->scissorHeight = viewportHeight;
	shadowParms->zFar = max_distance;
	shadowParms->fovX = fov_degrees;
	shadowParms->fovY = fov_degrees;
	R_SetupProjection( shadowParms, nearPlane, qfalse );

	viewerMatrix[0] = shadowParms->or.axis[0][0];
	viewerMatrix[4] = shadowParms->or.axis[0][1];
	viewerMatrix[8] = shadowParms->or.axis[0][2];
	viewerMatrix[12] = -origin[0] * viewerMatrix[0] + -origin[1] * viewerMatrix[4] + -origin[2] * viewerMatrix[8];
	viewerMatrix[1] = shadowParms->or.axis[1][0];
	viewerMatrix[5] = shadowParms->or.axis[1][1];
	viewerMatrix[9] = shadowParms->or.axis[1][2];
	viewerMatrix[13] = -origin[0] * viewerMatrix[1] + -origin[1] * viewerMatrix[5] + -origin[2] * viewerMatrix[9];
	viewerMatrix[2] = shadowParms->or.axis[2][0];
	viewerMatrix[6] = shadowParms->or.axis[2][1];
	viewerMatrix[10] = shadowParms->or.axis[2][2];
	viewerMatrix[14] = -origin[0] * viewerMatrix[2] + -origin[1] * viewerMatrix[6] + -origin[2] * viewerMatrix[10];
	viewerMatrix[3] = 0.0f;
	viewerMatrix[7] = 0.0f;
	viewerMatrix[11] = 0.0f;
	viewerMatrix[15] = 1.0f;

	myGlMultMatrix( viewerMatrix, vk_local_shadow_flip_matrix, lightView );
	Matrix16Identity( shadowParms->world.modelMatrix );
	Com_Memcpy( shadowParms->world.modelViewMatrix, lightView, sizeof( lightView ) );
	VectorCopy( origin, shadowParms->world.viewOrigin );
	VectorClear( shadowParms->world.origin );
	AxisCopy( axisDefault, shadowParms->world.axis );

	myGlMultMatrix( lightView, shadowParms->projectionMatrix, outViewProj );
	return qtrue;
}

static qboolean vk_build_local_spot_shadow_view( const dlight_t *dl, int viewportX, int viewportY, int viewportSize, viewParms_t *shadowParms, float *outViewProj )
{
	vec3_t forward;
	vec3_t up = { 0.0f, 0.0f, 1.0f };

	if ( !dl || !shadowParms || !outViewProj ) {
		return qfalse;
	}

	VectorSubtract( dl->origin2, dl->origin, forward );
	if ( VectorNormalize( forward ) <= 0.001f ) {
		VectorSet( forward, 0.0f, 0.0f, -1.0f );
	}

	return vk_build_local_shadow_view_axes( dl->origin, forward, up, 70.0f, dl->radius,
		viewportX, viewportY, viewportSize, viewportSize, shadowParms, outViewProj );
}

static qboolean vk_build_local_point_shadow_view( const dlight_t *dl, int face, int viewportSize, viewParms_t *shadowParms, float *outViewProj )
{
	vec3_t forward = { 0.0f, 0.0f, -1.0f };
	vec3_t up = { 0.0f, 0.0f, 1.0f };

	if ( !dl || !shadowParms || !outViewProj ) {
		return qfalse;
	}

	switch ( face ) {
		case 0: VectorSet( forward, 1.0f, 0.0f, 0.0f ); VectorSet( up, 0.0f, 0.0f, 1.0f ); break; // +X
		case 1: VectorSet( forward, -1.0f, 0.0f, 0.0f ); VectorSet( up, 0.0f, 0.0f, 1.0f ); break; // -X
		case 2: VectorSet( forward, 0.0f, 1.0f, 0.0f ); VectorSet( up, 0.0f, 0.0f, 1.0f ); break; // +Y
		case 3: VectorSet( forward, 0.0f, -1.0f, 0.0f ); VectorSet( up, 0.0f, 0.0f, 1.0f ); break; // -Y
		case 4: VectorSet( forward, 0.0f, 0.0f, 1.0f ); VectorSet( up, 0.0f, -1.0f, 0.0f ); break; // +Z
		case 5: VectorSet( forward, 0.0f, 0.0f, -1.0f ); VectorSet( up, 0.0f, 1.0f, 0.0f ); break; // -Z
		default: return qfalse;
	}

	return vk_build_local_shadow_view_axes( dl->origin, forward, up, 90.0f, dl->radius,
		0, 0, viewportSize, viewportSize, shadowParms, outViewProj );
}

static qboolean vk_render_local_volumetric_shadow_view( const viewParms_t *shadowParms, qboolean pointShadow, int pointFaceLayer )
{
	renderPass_t saved_pass;
	uint32_t saved_width;
	uint32_t saved_height;
	float saved_scale_x;
	float saved_scale_y;

	if ( !shadowParms || backEnd.refdef.numDrawSurfs <= 0 || !backEnd.refdef.drawSurfs ) {
		return qfalse;
	}
	saved_pass = vk.renderPassIndex;
	saved_width = vk.renderWidth;
	saved_height = vk.renderHeight;
	saved_scale_x = vk.renderScaleX;
	saved_scale_y = vk.renderScaleY;

	if ( pointShadow ) {
		if ( !vk_begin_local_point_shadow_render_pass( pointFaceLayer ) ) {
			vk.renderPassIndex = saved_pass;
			vk.renderWidth = saved_width;
			vk.renderHeight = saved_height;
			vk.renderScaleX = saved_scale_x;
			vk.renderScaleY = saved_scale_y;
			return qfalse;
		}
	} else if ( !vk_begin_local_spot_shadow_render_pass() ) {
		vk.renderPassIndex = saved_pass;
		vk.renderWidth = saved_width;
		vk.renderHeight = saved_height;
		vk.renderScaleX = saved_scale_x;
		vk.renderScaleY = saved_scale_y;
		return qfalse;
	}

	RB_RenderVolumetricShadowView( shadowParms, backEnd.refdef.drawSurfs, backEnd.refdef.numDrawSurfs );
	if ( pointShadow ) {
		vk_end_local_point_shadow_render_pass();
	} else {
		vk_end_local_spot_shadow_render_pass();
	}
	vk.renderPassIndex = saved_pass;
	vk.renderWidth = saved_width;
	vk.renderHeight = saved_height;
	vk.renderScaleX = saved_scale_x;
	vk.renderScaleY = saved_scale_y;
	return qtrue;
}

static void vk_volumetric_compute_pass( void )
{
	enum {
		VK_VOLUMETRIC_STAGE_CLEAR = 0,
		VK_VOLUMETRIC_STAGE_GLOBAL_DENSITY = 1,
		VK_VOLUMETRIC_STAGE_VOLUME_DENSITY = 2,
		VK_VOLUMETRIC_STAGE_LOCAL_LIGHT = 3,
		VK_VOLUMETRIC_STAGE_SUN_LIGHT = 4,
		VK_VOLUMETRIC_STAGE_CLAMP_LEVEL0 = 5,
		VK_VOLUMETRIC_STAGE_CLAMP_LEVEL1 = 6,
		VK_VOLUMETRIC_STAGE_TEMPORAL = 7,
		VK_VOLUMETRIC_STAGE_FLUID_DENSITY = 8
	};

	if ( vk.volumetric_compute_pipeline == VK_NULL_HANDLE || !vk.froxel_width || !vk.froxel_height || !vk.froxel_slices ||
		vk.froxel_extinction_image == VK_NULL_HANDLE || vk.froxel_clamp_image == VK_NULL_HANDLE )
	{
		if ( r_fogDebug && r_fogDebug->integer >= 1 ) {
			ri.Printf( PRINT_WARNING, "[VK][fog] skipping compute pass: pipeline=0x%llx dims=%ux%ux%u ext=0x%llx clamp=0x%llx\n",
				(unsigned long long)(uintptr_t)vk.volumetric_compute_pipeline, vk.froxel_width, vk.froxel_height, vk.froxel_slices,
				(unsigned long long)(uintptr_t)vk.froxel_extinction_image,
				(unsigned long long)(uintptr_t)vk.froxel_clamp_image );
		}
		return;
	}

	const uint32_t groups_x = ( vk.froxel_width + 7 ) / 8;
	const uint32_t groups_y = ( vk.froxel_height + 7 ) / 8;
	const uint32_t groups_z = ( vk.froxel_slices + 3 ) / 4;
	const uint32_t clamp0_width = ( vk.froxel_width + 1 ) / 2;
	const uint32_t clamp0_height = ( vk.froxel_height + 1 ) / 2;
	const uint32_t clamp1_width = ( clamp0_width + 1 ) / 2;
	const uint32_t clamp1_height = ( clamp0_height + 1 ) / 2;
	const uint32_t clamp0_groups_x = ( clamp0_width + 7 ) / 8;
	const uint32_t clamp0_groups_y = ( clamp0_height + 7 ) / 8;
	const uint32_t clamp1_groups_x = ( clamp1_width + 7 ) / 8;
	const uint32_t clamp1_groups_y = ( clamp1_height + 7 ) / 8;
	const uint32_t clamp_groups_z = ( vk.froxel_slices + 3 ) / 4;
	int local_light_count = 0;
	volumetric_params_t *params_rw = (volumetric_params_t *)vk.volumetric_params_ptr;
	qboolean use_local_shadows = ( r_fog_shadows && r_fog_shadows->integer ) ? qtrue : qfalse;
	int spot_shadow_slot = 0;
	int point_shadow_slot = 0;
	int spot_shadow_ready_count = 0;
	int point_shadow_ready_count = 0;

	vk_fluid_simulation_pass( params_rw ? params_rw->fluidParams0[0] : (1.0f / 60.0f) );
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_FLUID_SIM, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.volumetric_compute_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, vk.volumetric_compute_pipeline_layout, 0, 1, &vk.volumetric_compute_descriptor, 0, NULL );

	if ( vk.volumetric_params_ptr ) {
		const volumetric_params_t *params = (const volumetric_params_t *)vk.volumetric_params_ptr;
		local_light_count = (int)( params->volumeCounts[1] + 0.5f );
		if ( local_light_count < 0 ) {
			local_light_count = 0;
		} else if ( local_light_count > VK_VOLUMETRIC_MAX_LIGHTS ) {
			local_light_count = VK_VOLUMETRIC_MAX_LIGHTS;
		}
	}
	vk_volumetric_validation_state.local_light_count = (uint32_t)local_light_count;

	if ( params_rw ) {
		for ( int i = 0; i < VK_VOLUMETRIC_MAX_LIGHTS; i++ ) {
			Matrix16Identity( params_rw->localSpotShadowMatrix[i] );
			for ( int face = 0; face < 6; face++ ) {
				Matrix16Identity( params_rw->localPointShadowMatrix[i][face] );
			}
			params_rw->localShadowAtlasUv[i][0] = 1.0f;
			params_rw->localShadowAtlasUv[i][1] = 1.0f;
			params_rw->localShadowAtlasUv[i][2] = 0.0f;
			params_rw->localShadowAtlasUv[i][3] = 0.0f;
			params_rw->lightExtra[i][3] = -1.0f;
		}
	}

	if ( r_fogDebug && r_fogDebug->integer >= 1 && ( vk.volumetric_frame % 120u ) == 0u ) {
		ri.Printf( PRINT_ALL,
			"[VK][fog] screen=%dx%d froxel=%ux%ux%u groups=%ux%ux%u volImg=0x%llx volView=0x%llx compSet=0x%llx fragSet=0x%llx\n",
			glConfig.vidWidth, glConfig.vidHeight,
			vk.froxel_width, vk.froxel_height, vk.froxel_slices,
			groups_x, groups_y, groups_z,
			(unsigned long long)(uintptr_t)vk.froxel_volume_image,
			(unsigned long long)(uintptr_t)vk.froxel_volume_view,
				(unsigned long long)(uintptr_t)vk.volumetric_compute_descriptor,
				(unsigned long long)(uintptr_t)vk.volumetric_composite_descriptor );
	}

	vk_set_volumetric_pass_params( (float)VK_VOLUMETRIC_STAGE_CLEAR, 0.0f, 0.0f, 0.0f );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, groups_z );
	vk_volumetric_stage_barrier( vk.froxel_volume_image );
	vk_volumetric_stage_barrier( vk.froxel_light_image );
	vk_volumetric_stage_barrier( vk.froxel_extinction_image );
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_CLEAR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	vk_set_volumetric_pass_params( (float)VK_VOLUMETRIC_STAGE_GLOBAL_DENSITY, 0.0f, 0.0f, 0.0f );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, groups_z );
	vk_volumetric_stage_barrier( vk.froxel_light_image );
	vk_volumetric_stage_barrier( vk.froxel_extinction_image );
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_GLOBAL_DENSITY, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	vk_set_volumetric_pass_params( (float)VK_VOLUMETRIC_STAGE_VOLUME_DENSITY, 0.0f, 0.0f, 0.0f );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, groups_z );
	vk_volumetric_stage_barrier( vk.froxel_light_image );
	vk_volumetric_stage_barrier( vk.froxel_extinction_image );
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_VOLUME_DENSITY, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	vk_set_volumetric_pass_params( (float)VK_VOLUMETRIC_STAGE_FLUID_DENSITY, 0.0f, 0.0f, 0.0f );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, groups_z );
	vk_volumetric_stage_barrier( vk.froxel_extinction_image );
	vk_volumetric_stage_barrier( vk.froxel_light_image );
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_FLUID_DENSITY, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	vk_set_volumetric_pass_params( (float)VK_VOLUMETRIC_STAGE_SUN_LIGHT, 0.0f, 0.0f, 0.0f );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, groups_z );
	vk_volumetric_stage_barrier( vk.froxel_volume_image );
	vk_volumetric_stage_barrier( vk.froxel_extinction_image );
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_SUN, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	for ( int i = 0; i < local_light_count; i++ ) {
		const dlight_t *dl = vk_get_volumetric_local_light( i );
		qboolean shadow_ready = qfalse;

		if ( use_local_shadows && params_rw && dl ) {
			if ( dl->linear ) {
				viewParms_t shadowView;
				float shadowViewProj[16];
				const uint32_t atlas_size = MAX( vk.local_spot_shadow_atlas_size, 1u );
				const uint32_t tile_size = MAX( vk.local_spot_shadow_tile_size, 1u );
				const uint32_t grid = MAX( 1u, atlas_size / tile_size );

				if ( spot_shadow_slot < (int)vk.local_spot_shadow_capacity ) {
					const int slot = spot_shadow_slot++;
					const int tile_x = ( slot % (int)grid ) * (int)tile_size;
					const int tile_y = ( slot / (int)grid ) * (int)tile_size;

					if ( vk_build_local_spot_shadow_view( dl, tile_x, tile_y, (int)tile_size, &shadowView, shadowViewProj ) &&
						vk_render_local_volumetric_shadow_view( &shadowView, qfalse, -1 ) )
					{
						Com_Memcpy( params_rw->localSpotShadowMatrix[i], shadowViewProj, sizeof( shadowViewProj ) );
						params_rw->localShadowAtlasUv[i][0] = (float)tile_size / (float)atlas_size;
						params_rw->localShadowAtlasUv[i][1] = (float)tile_size / (float)atlas_size;
						params_rw->localShadowAtlasUv[i][2] = (float)tile_x / (float)atlas_size;
						params_rw->localShadowAtlasUv[i][3] = (float)tile_y / (float)atlas_size;
						params_rw->lightExtra[i][3] = (float)slot;
						shadow_ready = qtrue;
						spot_shadow_ready_count++;
					}
				}
			} else {
				if ( point_shadow_slot < (int)vk.local_point_shadow_capacity ) {
					const int point_slot = point_shadow_slot++;
					qboolean all_faces_rendered = qtrue;

					for ( int face = 0; face < 6; face++ ) {
						viewParms_t shadowView;
						float shadowViewProj[16];
						const int layer = point_slot * 6 + face;

						if ( !vk_build_local_point_shadow_view( dl, face, (int)vk.local_point_shadow_face_size, &shadowView, shadowViewProj ) ||
							!vk_render_local_volumetric_shadow_view( &shadowView, qtrue, layer ) )
						{
							all_faces_rendered = qfalse;
							break;
						}

						Com_Memcpy( params_rw->localPointShadowMatrix[i][face], shadowViewProj, sizeof( shadowViewProj ) );
					}

					if ( all_faces_rendered ) {
						params_rw->lightExtra[i][3] = (float)point_slot;
						shadow_ready = qtrue;
						point_shadow_ready_count++;
					}
				}
			}
		}

		// passParams.y carries local light index for the STAGE_LOCAL_LIGHT dispatch.
		vk_set_volumetric_pass_params( (float)VK_VOLUMETRIC_STAGE_LOCAL_LIGHT, (float)i, shadow_ready ? 1.0f : 0.0f, 0.0f );
		qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, groups_z );
		vk_volumetric_stage_barrier( vk.froxel_volume_image );
	}
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_LOCAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	vk_volumetric_validation_state.local_shadow_ready_spot = (uint32_t)spot_shadow_ready_count;
	vk_volumetric_validation_state.local_shadow_ready_point = (uint32_t)point_shadow_ready_count;

	vk_set_volumetric_pass_params( (float)VK_VOLUMETRIC_STAGE_CLAMP_LEVEL0, 0.0f, 0.0f, 0.0f );
	qvkCmdDispatch( vk.cmd->command_buffer, clamp0_groups_x, clamp0_groups_y, clamp_groups_z );
	vk_volumetric_stage_barrier( vk.froxel_clamp_image );
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_CLAMP0, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	vk_set_volumetric_pass_params( (float)VK_VOLUMETRIC_STAGE_CLAMP_LEVEL1, 0.0f, 0.0f, 0.0f );
	qvkCmdDispatch( vk.cmd->command_buffer, clamp1_groups_x, clamp1_groups_y, clamp_groups_z );
	vk_volumetric_stage_barrier( vk.froxel_light_image );
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_CLAMP1, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	vk_set_volumetric_pass_params( (float)VK_VOLUMETRIC_STAGE_TEMPORAL, 0.0f, 0.0f, 0.0f );
	qvkCmdDispatch( vk.cmd->command_buffer, groups_x, groups_y, groups_z );
	vk_volumetric_stage_barrier( vk.froxel_volume_image );
	vk_nslm_apply_to_froxels( groups_x, groups_y, groups_z );
	vk_volumetric_stage_barrier( vk.froxel_volume_image );
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_TEMPORAL, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	record_image_layout_transition( vk.cmd->command_buffer, vk.froxel_volume_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	record_image_layout_transition( vk.cmd->command_buffer, vk.froxel_extinction_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	record_image_layout_transition( vk.cmd->command_buffer, vk.volumetric_telemetry_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	if ( r_fogDebug && r_fogDebug->integer >= 1 && ( vk.volumetric_frame % 120u ) == 0u ) {
		ri.Printf( PRINT_ALL, "[VK][fog] transition froxelVolume/extinction to SHADER_READ_ONLY_OPTIMAL for composite\n" );
	}
}

static void vk_copy_froxel_history( void )
{
	if ( !vk.froxel_volume_image || !vk.froxel_history_image || !vk.froxel_width || !vk.froxel_height || !vk.froxel_slices ) {
		return;
	}

	VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
	VkCommandBuffer cmd = vk.cmd->command_buffer;

	VkImageCopy copy;
	Com_Memset( &copy, 0, sizeof( copy ) );
	copy.srcSubresource.aspectMask = aspect;
	copy.srcSubresource.mipLevel = 0;
	copy.srcSubresource.baseArrayLayer = 0;
	copy.srcSubresource.layerCount = 1;
	copy.dstSubresource = copy.srcSubresource;
	copy.extent.width = vk.froxel_width;
	copy.extent.height = vk.froxel_height;
	copy.extent.depth = vk.froxel_slices;

	record_image_layout_transition( cmd, vk.froxel_volume_image, aspect,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 0, 0 );
	record_image_layout_transition( cmd, vk.froxel_history_image, aspect,
		VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, 0 );

	qvkCmdCopyImage( cmd,
		vk.froxel_volume_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		vk.froxel_history_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1, &copy );

	record_image_layout_transition( cmd, vk.froxel_volume_image, aspect,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 0, 0 );
	record_image_layout_transition( cmd, vk.froxel_history_image, aspect,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, 0, 0 );
	if ( r_fogDebug && r_fogDebug->integer >= 1 && ( vk.volumetric_frame % 120u ) == 0u ) {
		ri.Printf( PRINT_ALL,
			"[VK][fog] history copy vol=0x%llx READ_ONLY->TRANSFER_SRC->READ_ONLY hist=0x%llx GENERAL->TRANSFER_DST->GENERAL\n",
			(unsigned long long)(uintptr_t)vk.froxel_volume_image,
			(unsigned long long)(uintptr_t)vk.froxel_history_image );
	}

	vk.has_prev_volumetric = qtrue;
}

static void vk_volumetric_composite_pass( void )
{
	if ( vk.volumetric_composite_pipeline == VK_NULL_HANDLE || vk.render_pass.volumetric == VK_NULL_HANDLE ) {
		return;
	}

	vk_begin_volumetric_render_pass();

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.volumetric_composite_pipeline );
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.volumetric_composite_pipeline_layout, 0, 1, &vk.volumetric_composite_descriptor, 0, NULL );

	VkViewport viewport;
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = vk.renderWidth > 0 ? (float) vk.renderWidth : (float)glConfig.vidWidth;
	viewport.height = vk.renderHeight > 0 ? (float) vk.renderHeight : (float)glConfig.vidHeight;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	uint32_t scissorWidth = ( viewport.width > 0.0f ) ? (uint32_t) viewport.width : (uint32_t) glConfig.vidWidth;
	uint32_t scissorHeight = ( viewport.height > 0.0f ) ? (uint32_t) viewport.height : (uint32_t) glConfig.vidHeight;
	scissor.extent.width = scissorWidth;
	scissor.extent.height = scissorHeight;

	qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
	qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor );

	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();
}

void vk_reset_volumetric_history( void )
{
	vk.has_prev_volumetric = qfalse;
	vk.volumetric_frame = 0;
	vk_prev_matrices_valid = qfalse;
	vk_prev_volumetric_time_valid = qfalse;
	vk_volumetric_noise_time = 0.0f;
	vk_near_static_view_frames = 0;
	Com_Memset( &vk_volumetric_validation_state, 0, sizeof( vk_volumetric_validation_state ) );
}

void vk_volumetric_fog_pass( void )
{
	/* Atmosphere: only when we have a 3D world. Skip for menus, videos, RDF_NOWORLDMODEL
	 * (depth is cleared to far; drawing sky over full screen would cover UI). */
	if ( tr.world && !( tr.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		vk_atmosphere_pass();
	}

	if ( backEnd.doneFog ) {
		return;
	}

	{
		int tier = 0;
		cvar_t *tierCvar = ri.Cvar_Get( "r_volumetricFogTier", "0", 0 );
		if ( tierCvar ) tier = tierCvar->integer;
		if ( tier >= 2 || tier == 4 || !r_volumetricFog->integer || !vk.fboActive ||
			!tr.world || ( tr.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
			vk_volumetric_skip_cleanup( "volumetric skipped (tier/off/no-world)",
				VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
			return;
		}
	}
	if ( vk.froxel_volume_image == VK_NULL_HANDLE || vk.froxel_history_image == VK_NULL_HANDLE ||
		vk.froxel_extinction_image == VK_NULL_HANDLE || vk.froxel_clamp_image == VK_NULL_HANDLE ||
		vk.fog_scene_image == VK_NULL_HANDLE || vk.motion_vector_image == VK_NULL_HANDLE )
	{
		if ( r_fogDebug && r_fogDebug->integer >= 1 ) {
			ri.Printf( PRINT_WARNING, "[VK][fog] skipping fog pass: resources missing vol=0x%llx hist=0x%llx ext=0x%llx clamp=0x%llx scene=0x%llx motion=0x%llx\n",
				(unsigned long long)(uintptr_t)vk.froxel_volume_image,
				(unsigned long long)(uintptr_t)vk.froxel_history_image,
				(unsigned long long)(uintptr_t)vk.froxel_extinction_image,
				(unsigned long long)(uintptr_t)vk.froxel_clamp_image,
				(unsigned long long)(uintptr_t)vk.fog_scene_image,
				(unsigned long long)(uintptr_t)vk.motion_vector_image );
		}
		vk_volumetric_skip_cleanup( "volumetric skipped (missing resources)",
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
		return;
	}
	if ( vk.msaaActive &&
		( vk.volumetric_depth_image == VK_NULL_HANDLE ||
		  vk.volumetric_depth_view == VK_NULL_HANDLE ||
		  vk.volumetric_depth_resolve_pipeline == VK_NULL_HANDLE ||
		  vk.volumetric_depth_resolve_descriptor == VK_NULL_HANDLE ) )
	{
		if ( r_fogDebug && r_fogDebug->integer >= 1 ) {
			ri.Printf( PRINT_WARNING, "[VK][fog] skipping fog pass: MSAA depth resolve path incomplete image=0x%llx view=0x%llx pipeline=0x%llx descriptor=0x%llx\n",
				(unsigned long long)(uintptr_t)vk.volumetric_depth_image,
				(unsigned long long)(uintptr_t)vk.volumetric_depth_view,
				(unsigned long long)(uintptr_t)vk.volumetric_depth_resolve_pipeline,
				(unsigned long long)(uintptr_t)vk.volumetric_depth_resolve_descriptor );
		}
		vk_volumetric_skip_cleanup( "volumetric skipped (MSAA depth resolve missing)",
			VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
		return;
	}

	VkImageAspectFlags depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );

	vk_resolve_volumetric_depth_msaa();
	vk_update_volumetric_params();

	/* Skip volumetrics when view is nearly static (death cam) to avoid gradient/streak artifacts */
	if ( vk_temporal_near_static_streak_guard() ) {
		vk_volumetric_skip_cleanup( "volumetric skipped (static view, death cam)",
			VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
		return;
	}

	vk_volumetric_validation_state.telemetry_nan_or_inf = 0;
	vk_volumetric_validation_state.telemetry_extinction_clamp_hits = 0;
	vk_volumetric_validation_state.telemetry_velocity_clamp_hits = 0;
	vk_volumetric_validation_state.telemetry_density_clamp_hits = 0;
	vk_volumetric_validation_state.telemetry_pressure_sanitize_hits = 0;
	vk_volumetric_validation_state.telemetry_temporal_rejects = 0;

	if ( vk.volumetric_query_pool != VK_NULL_HANDLE &&
		vk_volumetric_perf_wanted() &&
		qvkCmdResetQueryPool )
	{
		const uint32_t query_base = vk.cmd_index * VK_VOLUMETRIC_QUERY_SLOTS;
		qvkCmdResetQueryPool( vk.cmd->command_buffer, vk.volumetric_query_pool, query_base, VK_VOLUMETRY_QUERY_USED );
	}
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_FOG_START, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );

	if ( vk.volumetric_telemetry_image != VK_NULL_HANDLE ) {
		VkImageSubresourceRange telemetry_clear_range;
		VkClearColorValue telemetry_clear_value;

		record_image_layout_transition( vk.cmd->command_buffer, vk.volumetric_telemetry_image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT );

		Com_Memset( &telemetry_clear_range, 0, sizeof( telemetry_clear_range ) );
		telemetry_clear_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		telemetry_clear_range.levelCount = 1;
		telemetry_clear_range.layerCount = 1;
		Com_Memset( &telemetry_clear_value, 0, sizeof( telemetry_clear_value ) );
		qvkCmdClearColorImage( vk.cmd->command_buffer, vk.volumetric_telemetry_image, VK_IMAGE_LAYOUT_GENERAL, &telemetry_clear_value, 1, &telemetry_clear_range );

		record_image_layout_transition( vk.cmd->command_buffer, vk.volumetric_telemetry_image, VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	}

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
	record_image_layout_transition( vk.cmd->command_buffer, vk.fog_scene_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT );
	{
		VkImageCopy copy_region;
		Com_Memset( &copy_region, 0, sizeof( copy_region ) );
		copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy_region.srcSubresource.layerCount = 1;
		copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy_region.dstSubresource.layerCount = 1;
		copy_region.extent.width = glConfig.vidWidth;
		copy_region.extent.height = glConfig.vidHeight;
		copy_region.extent.depth = 1;
		qvkCmdCopyImage( vk.cmd->command_buffer,
			vk.color_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			vk.fog_scene_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &copy_region );
	}
	record_image_layout_transition( vk.cmd->command_buffer, vk.fog_scene_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT );

	if ( !vk_volumetric_screen_integration_active() ) {
	record_image_layout_transition( vk.cmd->command_buffer, vk.froxel_volume_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	record_image_layout_transition( vk.cmd->command_buffer, vk.froxel_extinction_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT );
	if ( r_fogDebug && r_fogDebug->integer >= 1 && ( vk.volumetric_frame % 120u ) == 0u ) {
		ri.Printf( PRINT_ALL, "[VK][fog] transition froxelVolume/extinction SHADER_READ_ONLY_OPTIMAL->GENERAL (fragment->compute)\n" );
	}

	vk_volumetric_compute_pass();

	if ( r_volumetricFogTemporalWeight->value > 0.0f ) {
		vk_copy_froxel_history();
	} else {
		vk.has_prev_volumetric = qfalse;
	}
	} else {
		if ( r_fogDebug && r_fogDebug->integer >= 1 && ( vk.volumetric_frame % 120u ) == 0u ) {
			ri.Printf( PRINT_DEVELOPER, "[VK][fog] screen integration mode %d: skipping froxel compute\n",
				r_volumetricFogIntegration ? r_volumetricFogIntegration->integer : 0 );
		}
	}

	vk_volumetric_restore_pass_params_for_composite();
	vk_volumetric_composite_pass();
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_AFTER_COMPOSITE, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT );

	/* Render pass finalLayout=SHADER_READ_ONLY transitions color_image automatically on end. */

	vk_post_scene_aa_apply();

	// Restore depth layout for the next frame's main render pass clears/attachments.
	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT );
	vk_write_volumetric_timestamp( VK_VOLUMETRY_QUERY_FOG_END, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT );

	if ( r_volumetricFogValidation && r_volumetricFogValidation->integer > 0 ) {
		int interval = ( r_volumetricFogValidationPrintInterval ) ? r_volumetricFogValidationPrintInterval->integer : 120;
		const volumetric_params_t *params = (const volumetric_params_t *)vk.volumetric_params_ptr;
		if ( interval < 1 ) {
			interval = 1;
		}
		if ( ( vk.volumetric_frame % (uint32_t)interval ) == 0u && params ) {
			ri.Printf( PRINT_ALL,
				"[VK][fog][validate] frame=%u hasHistory=%.0f cameraCut=%.0f forcedCuts=%u totalCuts=%u localShadows spot=%u point=%u lights=%u msaa=%s depthResolve=%s motion=%s\n",
				vk.volumetric_frame,
				params->miscParams[3],
				params->temporalParams[3],
				vk_volumetric_validation_state.forced_camera_cut_events,
				vk_volumetric_validation_state.camera_cut_events,
				vk_volumetric_validation_state.local_shadow_ready_spot,
				vk_volumetric_validation_state.local_shadow_ready_point,
				vk_volumetric_validation_state.local_light_count,
				vk.msaaActive ? "on" : "off",
				( vk.msaaActive && vk.volumetric_depth_image != VK_NULL_HANDLE ) ? "on" : "off",
				( vk.motion_vector_image != VK_NULL_HANDLE ) ? "on" : "off" );
		}
	}

	backEnd.doneFog = qtrue;
}
