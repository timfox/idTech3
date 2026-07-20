#include "tr_local.h"
#include "vk_atmosphere.h"
#include "vk.h"
#include "vk_image_layout.h"
#include "vk_postfx.h"
#include "vk_render_pass.h"
#include "vk_scene_pass.h"
#include "vk_sky_owner.h"
#include "vk_weather.h"
#include <math.h>

void vk_atmosphere_build_push_constants( vkAtmospherePushConstants_t *pc )
{
	float tanHalfX;
	float tanHalfY;
	float aerosol;
	float len;

	if ( !pc ) {
		return;
	}

	Com_Memset( pc, 0, sizeof( *pc ) );

	/* Sole sun direction owner: tr.sunDirection (map/q3map_sun). Cvar fallback only if unset. */
	if ( tr.sunDirection[0] != 0.0f || tr.sunDirection[1] != 0.0f || tr.sunDirection[2] != 0.0f ) {
		pc->sunDir[0] = tr.sunDirection[0];
		pc->sunDir[1] = tr.sunDirection[1];
		pc->sunDir[2] = tr.sunDirection[2];
	} else {
		PostFX_Atmosphere_GetSunDirection( &pc->sunDir[0], &pc->sunDir[1], &pc->sunDir[2] );
	}
	len = sqrtf( pc->sunDir[0] * pc->sunDir[0] + pc->sunDir[1] * pc->sunDir[1] +
		pc->sunDir[2] * pc->sunDir[2] );
	if ( len > 1e-5f ) {
		pc->sunDir[0] /= len;
		pc->sunDir[1] /= len;
		pc->sunDir[2] /= len;
	}
	pc->sunDir[3] = 0.0f;
	pc->sunColor[0] = 1.0f;
	pc->sunColor[1] = 0.98f;
	pc->sunColor[2] = 0.92f;
	pc->sunColor[3] = PostFX_Atmosphere_GetSunIntensity() * vk_weather_sun_visibility();

	aerosol = vk_weather_active() ? vk_weather_state()->aerosol : 1.0f;
	pc->rayleigh[0] = 5.5e-6f;
	pc->rayleigh[1] = 13.0e-6f;
	pc->rayleigh[2] = 22.4e-6f;
	pc->rayleigh[3] = 1.0f;
	pc->mie[0] = 21e-6f * aerosol;
	pc->mie[1] = PostFX_Atmosphere_GetMieG();
	pc->mie[2] = 0.0f;
	pc->mie[3] = 0.0f;
	pc->atmParams[0] = PostFX_Atmosphere_GetRayleighHeight();
	pc->atmParams[1] = PostFX_Atmosphere_GetMieHeight();
	pc->atmParams[2] = aerosol; /* turbidity / aerosol density */
	pc->atmParams[3] = PostFX_Atmosphere_GetScale();
	pc->viewOrigin[0] = backEnd.viewParms.or.origin[0];
	pc->viewOrigin[1] = backEnd.viewParms.or.origin[1];
	pc->viewOrigin[2] = backEnd.viewParms.or.origin[2];
	pc->viewOrigin[3] = 1.0f;
	pc->viewForward[0] = backEnd.viewParms.or.axis[0][0];
	pc->viewForward[1] = backEnd.viewParms.or.axis[0][1];
	pc->viewForward[2] = backEnd.viewParms.or.axis[0][2];
	pc->viewForward[3] = 0.0f;
	pc->viewRight[0] = backEnd.viewParms.or.axis[1][0];
	pc->viewRight[1] = backEnd.viewParms.or.axis[1][1];
	pc->viewRight[2] = backEnd.viewParms.or.axis[1][2];
	pc->viewRight[3] = 0.0f;
	pc->viewUp[0] = backEnd.viewParms.or.axis[2][0];
	pc->viewUp[1] = backEnd.viewParms.or.axis[2][1];
	pc->viewUp[2] = backEnd.viewParms.or.axis[2][2];
	pc->viewUp[3] = 0.0f;

	tanHalfX = tanf( DEG2RAD( backEnd.viewParms.fovX * 0.5f ) );
	tanHalfY = tanf( DEG2RAD( backEnd.viewParms.fovY * 0.5f ) );
	pc->viewParams[0] = tanHalfX > 0.0f ? tanHalfX : 1.0f;
	pc->viewParams[1] = tanHalfY > 0.0f ? tanHalfY : 1.0f;
	pc->viewParams[2] = 0.0f;
	pc->viewParams[3] = 0.0f;
}

void vk_atmosphere_pass( void )
{
	VkImageAspectFlags depth_aspect;
	vkAtmospherePushConstants_t pc;
	uint32_t passWidth;
	uint32_t passHeight;

	/*
	 * Raster Ultra 1.7 sky ownership: physical atmosphere only paints when it owns the sky.
	 * Classic maps (r_skyOwner 0) never get dual sky even if r_atmosphere leftover is 1.
	 */
	if ( !vk_sky_owner_wants_physical_sky() ) {
		return;
	}
	if ( !PostFX_Atmosphere_IsEnabled() || vk.atmosphere_pipeline == VK_NULL_HANDLE ||
		vk.render_pass.atmosphere == VK_NULL_HANDLE || vk.framebuffers.atmosphere[0] == VK_NULL_HANDLE ) {
		return;
	}
	if ( !tr.world || !backEnd.doneWorldScene ) {
		return;
	}

	vk_get_active_render_extent( &passWidth, &passHeight );

	depth_aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
	if ( glConfig.stencilBits > 0 ) {
		depth_aspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}

	record_depth_image_layout_transition( vk.cmd->command_buffer, depth_aspect,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 0, 0 );

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT );

	vk_begin_render_pass_tracked( vk.render_pass.atmosphere,
		vk.framebuffers.atmosphere[ vk.cmd->swapchain_image_index ],
		qtrue, passWidth, passHeight );

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.atmosphere_pipeline );
	vk_atmosphere_build_push_constants( &pc );
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_atmosphere,
		VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( pc ), &pc );

	vk_set_fullscreen_viewport_scissor( passWidth, passHeight );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );

	vk_end_render_pass();
	vk.depth_image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	record_image_layout_transition( vk.cmd->command_buffer, vk.color_image, VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT );
}
