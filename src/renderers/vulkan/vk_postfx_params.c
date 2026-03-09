#include "tr_local.h"
#include "vk.h"
#include "vk_postfx.h"
#include "vk_postfx_params.h"
#include "vk_temporal.h"
#include <math.h>

static qboolean vk_postfx_mat4_inverse( const float *m, float *out )
{
	float tmp[16];
	float det;
	int i;

	tmp[0] = m[5]  * m[10] * m[15] -
	         m[5]  * m[11] * m[14] -
	         m[9]  * m[6]  * m[15] +
	         m[9]  * m[7]  * m[14] +
	         m[13] * m[6]  * m[11] -
	         m[13] * m[7]  * m[10];

	tmp[4] = -m[4]  * m[10] * m[15] +
	          m[4]  * m[11] * m[14] +
	          m[8]  * m[6]  * m[15] -
	          m[8]  * m[7]  * m[14] -
	          m[12] * m[6]  * m[11] +
	          m[12] * m[7]  * m[10];

	tmp[8] = m[4]  * m[9] * m[15] -
	         m[4]  * m[11] * m[13] -
	         m[8]  * m[5] * m[15] +
	         m[8]  * m[7] * m[13] +
	         m[12] * m[5] * m[11] -
	         m[12] * m[7] * m[9];

	tmp[12] = -m[4]  * m[9] * m[14] +
	           m[4]  * m[10] * m[13] +
	           m[8]  * m[5] * m[14] -
	           m[8]  * m[6] * m[13] -
	           m[12] * m[5] * m[10] +
	           m[12] * m[6] * m[9];

	tmp[1] = -m[1]  * m[10] * m[15] +
	          m[1]  * m[11] * m[14] +
	          m[9]  * m[2] * m[15] -
	          m[9]  * m[3] * m[14] -
	          m[13] * m[2] * m[11] +
	          m[13] * m[3] * m[10];

	tmp[5] = m[0]  * m[10] * m[15] -
	         m[0]  * m[11] * m[14] -
	         m[8]  * m[2] * m[15] +
	         m[8]  * m[3] * m[14] +
	         m[12] * m[2] * m[11] -
	         m[12] * m[3] * m[10];

	tmp[9] = -m[0]  * m[9] * m[15] +
	          m[0]  * m[11] * m[13] +
	          m[8]  * m[1] * m[15] -
	          m[8]  * m[3] * m[13] -
	          m[12] * m[1] * m[11] +
	          m[12] * m[3] * m[9];

	tmp[13] = m[0]  * m[9] * m[14] -
	          m[0]  * m[10] * m[13] -
	          m[8]  * m[1] * m[14] +
	          m[8]  * m[2] * m[13] +
	          m[12] * m[1] * m[10] -
	          m[12] * m[2] * m[9];

	tmp[2] = m[1]  * m[6] * m[15] -
	         m[1]  * m[7] * m[14] -
	         m[5]  * m[2] * m[15] +
	         m[5]  * m[3] * m[14] +
	         m[13] * m[2] * m[7] -
	         m[13] * m[3] * m[6];

	tmp[6] = -m[0]  * m[6] * m[15] +
	          m[0]  * m[7] * m[14] +
	          m[4]  * m[2] * m[15] -
	          m[4]  * m[3] * m[14] -
	          m[12] * m[2] * m[7] +
	          m[12] * m[3] * m[6];

	tmp[10] = m[0]  * m[5] * m[15] -
	          m[0]  * m[7] * m[13] -
	          m[4]  * m[1] * m[15] +
	          m[4]  * m[3] * m[13] +
	          m[12] * m[1] * m[7] -
	          m[12] * m[3] * m[5];

	tmp[14] = -m[0]  * m[5] * m[14] +
	           m[0]  * m[6] * m[13] +
	           m[4]  * m[1] * m[14] -
	           m[4]  * m[2] * m[13] -
	           m[12] * m[1] * m[6] +
	           m[12] * m[2] * m[5];

	tmp[3] = -m[1] * m[6] * m[11] +
	          m[1] * m[7] * m[10] +
	          m[5] * m[2] * m[11] -
	          m[5] * m[3] * m[10] -
	          m[9] * m[2] * m[7] +
	          m[9] * m[3] * m[6];

	tmp[7] = m[0] * m[6] * m[11] -
	         m[0] * m[7] * m[10] -
	         m[4] * m[2] * m[11] +
	         m[4] * m[3] * m[10] +
	         m[8] * m[2] * m[7] -
	         m[8] * m[3] * m[6];

	tmp[11] = -m[0] * m[5] * m[11] +
	           m[0] * m[7] * m[9] +
	           m[4] * m[1] * m[11] -
	           m[4] * m[3] * m[9] -
	           m[8] * m[1] * m[7] +
	           m[8] * m[3] * m[5];

	tmp[15] = m[0] * m[5] * m[10] -
	          m[0] * m[6] * m[9] -
	          m[4] * m[1] * m[10] +
	          m[4] * m[2] * m[9] +
	          m[8] * m[1] * m[6] -
	          m[8] * m[2] * m[5];

	det = m[0] * tmp[0] + m[1] * tmp[4] + m[2] * tmp[8] + m[3] * tmp[12];
	if ( fabsf( det ) < 1e-9f ) {
		return qfalse;
	}

	det = 1.0f / det;
	for ( i = 0; i < 16; i++ ) {
		out[i] = tmp[i] * det;
	}
	return qtrue;
}

void vk_update_postfx_params( uint32_t cmd_index )
{
	VkPostFXParams params;
	const float *projection;
	const float *view;
	float viewProj[16];
	qboolean motion_valid = qfalse;

	if ( cmd_index >= NUM_COMMAND_BUFFERS || !vk.postfx_params_ptr[cmd_index] ) {
		return;
	}

	Com_Memset( &params, 0, sizeof( params ) );

	params.invViewProj[0] = 1.0f;
	params.invViewProj[5] = 1.0f;
	params.invViewProj[10] = 1.0f;
	params.invViewProj[15] = 1.0f;
	params.prevViewProj[0] = 1.0f;
	params.prevViewProj[5] = 1.0f;
	params.prevViewProj[10] = 1.0f;
	params.prevViewProj[15] = 1.0f;
	params.viewMatrix[0] = 1.0f;
	params.viewMatrix[5] = 1.0f;
	params.viewMatrix[10] = 1.0f;
	params.viewMatrix[15] = 1.0f;

	params.motionBlur[0] = PostFX_MotionBlur_IsEnabled() ? 1.0f : 0.0f;
	params.motionBlur[1] = PostFX_MotionBlur_GetStrength();
	params.motionBlur[2] = (float)Com_Clamp( 4, 32, PostFX_MotionBlur_GetSamples() );
	params.motionBlur[3] = Com_Clamp( 0.0f, 64.0f, PostFX_MotionBlur_GetMaxRadius() );
	params.depthOfField[0] = PostFX_DepthOfField_IsEnabled() ? 1.0f : 0.0f;
	params.depthOfField[1] = Com_Clamp( 0.0f, 8.0f, PostFX_DepthOfField_GetAperture() );
	params.depthOfField[2] = fmaxf( PostFX_DepthOfField_GetFocusDistance(), 0.0f );
	params.depthOfField[3] = fmaxf( PostFX_DepthOfField_GetFocusRange(), 1.0f );
	params.frameInfo[0] = Com_Clamp( 0.0f, 64.0f, PostFX_DepthOfField_GetMaxBlur() );
	params.frameInfo[1] = ( vk.renderWidth > 0 ) ? ( 1.0f / (float)vk.renderWidth ) : 0.0f;
	params.frameInfo[2] = ( vk.renderHeight > 0 ) ? ( 1.0f / (float)vk.renderHeight ) : 0.0f;

	if ( backEnd.projection2D || !tr.world || backEnd.viewParms.portalView != PV_NONE ) {
		Com_Memcpy( vk.postfx_params_ptr[cmd_index], &params, sizeof( params ) );
		return;
	}

	projection = backEnd.useFirstPersonProjection ? backEnd.firstPersonProjectionMatrix : backEnd.viewParms.projectionMatrix;
	view = backEnd.viewParms.world.modelViewMatrix;

	myGlMultMatrix( view, projection, viewProj );
	Com_Memcpy( params.viewMatrix, view, sizeof( params.viewMatrix ) );

	if ( !vk_postfx_mat4_inverse( viewProj, params.invViewProj ) ) {
		Com_Memcpy( params.invViewProj, viewProj, sizeof( params.invViewProj ) );
	}

	if ( vk_prev_matrices_valid && !vk_temporal_has_reason( VK_TEMPORAL_RESET_CAMERA_CUT | VK_TEMPORAL_RESET_MISSING_PREV_DATA |
		VK_TEMPORAL_RESET_RENDERER_INIT | VK_TEMPORAL_RESET_SWAPCHAIN_CHANGE | VK_TEMPORAL_RESET_RENDER_SIZE_CHANGE |
		VK_TEMPORAL_RESET_WORLD_CHANGE | VK_TEMPORAL_RESET_CLIENT_STATE_CHANGE | VK_TEMPORAL_RESET_EXPLICIT_DEBUG ) ) {
		Com_Memcpy( params.prevViewProj, vk_prev_viewproj_matrix, sizeof( params.prevViewProj ) );
		motion_valid = qtrue;
	} else {
		Com_Memcpy( params.prevViewProj, viewProj, sizeof( params.prevViewProj ) );
	}

	params.frameInfo[3] = motion_valid ? 1.0f : 0.0f;

	{
		float zNear = ( r_znear && r_znear->value > 0.0f ) ? r_znear->value : 8.0f;
		float zFar = backEnd.viewParms.zFar;
		if ( zFar <= zNear ) {
			zFar = zNear + 100.0f;
		}
		params.depthParams[0] = zNear;
		params.depthParams[1] = zFar;
	}

	Com_Memcpy( vk.postfx_params_ptr[cmd_index], &params, sizeof( params ) );
}
