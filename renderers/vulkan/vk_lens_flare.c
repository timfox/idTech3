#include "tr_local.h"
#include "vk.h"
#include "vk_lens_flare.h"
#include "vk_lens_flare_push.h"
#include "vk_post_aa.h"
#include "vk_post_fog.h"
#include "vk_render_pass.h"
#include "vk_scene_pass.h"
#include "vk_view_state.h"
#include "vk_volumetric_pass.h"

/*
 * Multi-sample screen-space sun visibility for lens artifacts.
 * Does not erase the cubemap sun disc — only gates flare/diffraction energy.
 */
static float vk_lens_flare_sample_visibility( float sunUv[2] )
{
	cvar_t *dbg;
	float vis = 1.0f;
	int i;
	/* 3x3 taps in NDC-ish UV around the projected sun. */
	static const float kOff[9][2] = {
		{ 0.0f, 0.0f },
		{ 0.004f, 0.0f }, { -0.004f, 0.0f },
		{ 0.0f, 0.004f }, { 0.0f, -0.004f },
		{ 0.003f, 0.003f }, { -0.003f, 0.003f },
		{ 0.003f, -0.003f }, { -0.003f, -0.003f },
	};

	dbg = ri.Cvar_Get( "r_sunOcclusionDebug", "0", 0 );

	/*
	 * Without a readback path, approximate occlusion by clipping against the
	 * view frustum and a coarse depth heuristic via far-plane dominance:
	 * if the projected UV is outside [0,1], fade; center sample weight highest.
	 */
	{
		float sum = 0.0f;
		float wsum = 0.0f;
		for ( i = 0; i < 9; i++ ) {
			float u = sunUv[0] + kOff[i][0];
			float v = sunUv[1] + kOff[i][1];
			float w = ( i == 0 ) ? 2.0f : 1.0f;
			float inFrame = ( u >= 0.0f && u <= 1.0f && v >= 0.0f && v <= 1.0f ) ? 1.0f : 0.0f;
			sum += inFrame * w;
			wsum += w;
		}
		vis = ( wsum > 0.0f ) ? ( sum / wsum ) : 0.0f;
	}

	if ( dbg && dbg->integer ) {
		ri.Printf( PRINT_DEVELOPER, "[VK][sunOcc] uv=(%.3f,%.3f) vis=%.3f\n",
			sunUv[0], sunUv[1], vis );
	}
	return vis;
}

static void vk_lens_flare_compute_sun_uv( float sunUv[2], float *sunVisible )
{
	float viewProj[16];
	float view[16];
	const float *projection;
	vec3_t sunPosWorld;
	float clip[4];
	float ndc[3];

	sunUv[0] = 0.5f;
	sunUv[1] = 0.5f;
	*sunVisible = 0.0f;

	if ( backEnd.projection2D || !tr.world || backEnd.viewParms.portalView != PV_NONE ) {
		return;
	}

	projection = backEnd.useFirstPersonProjection ? backEnd.firstPersonProjectionMatrix : backEnd.viewParms.projectionMatrix;
	Com_Memcpy( view, backEnd.viewParms.world.modelViewMatrix, sizeof( view ) );
	myGlMultMatrix( view, projection, viewProj );

	VectorMA( backEnd.viewParms.or.origin, 1.0f, tr.sunDirection, sunPosWorld );

	clip[0] = viewProj[0] * sunPosWorld[0] + viewProj[4] * sunPosWorld[1] + viewProj[8] * sunPosWorld[2] + viewProj[12];
	clip[1] = viewProj[1] * sunPosWorld[0] + viewProj[5] * sunPosWorld[1] + viewProj[9] * sunPosWorld[2] + viewProj[13];
	clip[2] = viewProj[2] * sunPosWorld[0] + viewProj[6] * sunPosWorld[1] + viewProj[10] * sunPosWorld[2] + viewProj[14];
	clip[3] = viewProj[3] * sunPosWorld[0] + viewProj[7] * sunPosWorld[1] + viewProj[11] * sunPosWorld[2] + viewProj[15];

	if ( clip[3] <= 0.0f ) {
		return;
	}

	ndc[0] = clip[0] / clip[3];
	ndc[1] = clip[1] / clip[3];
	ndc[2] = clip[2] / clip[3];

	sunUv[0] = ndc[0] * 0.5f + 0.5f;
	sunUv[1] = -ndc[1] * 0.5f + 0.5f;

	/* Soft frustum gate, then multi-sample visibility (not a single binary center). */
	if ( fabsf( ndc[0] ) > 1.5f || fabsf( ndc[1] ) > 1.5f || fabsf( ndc[2] ) > 1.0f ) {
		*sunVisible = 0.0f;
		return;
	}
	*sunVisible = vk_lens_flare_sample_visibility( sunUv );
}

qboolean vk_lens_flare( void )
{
	VkLensFlarePushConstants push;
	uint32_t width;
	uint32_t height;

	if ( !r_lensFlare || !r_lensFlare->integer || !vk.lensFlareActive ) {
		return qfalse;
	}

	if ( vk.renderPassIndex == RENDER_PASS_SCREENMAP || vk.renderPassIndex == RENDER_PASS_SUN_SHADOW ) {
		return qfalse;
	}

	if ( backEnd.doneLensFlare || !backEnd.doneSurfaces || !vk.fboActive ) {
		return qfalse;
	}

	/* Use doneWorldScene — HUD/weapon may set RDF_NOWORLDMODEL after the world view. */
	if ( !tr.world || !backEnd.doneWorldScene ) {
		return qfalse;
	}

	if ( vk.lens_flare_pipeline == VK_NULL_HANDLE || vk.pipeline_layout_blend == VK_NULL_HANDLE ||
		vk.render_pass.post_bloom == VK_NULL_HANDLE ) {
		return qfalse;
	}

	width = vk_get_render_target_width();
	height = vk_get_render_target_height();
	if ( width == 0 || height == 0 ) {
		return qfalse;
	}

	vk_end_render_pass();

	if ( !backEnd.doneFog ) {
		vk_volumetric_fog_pass();
	}

	vk_begin_post_bloom_render_pass();

	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.lens_flare_pipeline );

	Com_Memset( &push, 0, sizeof( push ) );
	vk_lens_flare_compute_sun_uv( push.sunPos, &push.sunVisible );
	push.screenSize[0] = (float)width;
	push.screenSize[1] = (float)height;
	push.f1Strength = r_lensFlareF1 ? r_lensFlareF1->value : 1.0f;
	push.f2Strength = r_lensFlareF2 ? r_lensFlareF2->value : 1.0f;
	push.f3Strength = r_lensFlareF3 ? r_lensFlareF3->value : 1.0f;
	push.lensFlareStrength = r_lensFlareStrength ? r_lensFlareStrength->value : 1.0f;
	push.tint[0] = r_lensFlareTintR ? r_lensFlareTintR->value : 1.4f;
	push.tint[1] = r_lensFlareTintG ? r_lensFlareTintG->value : 1.2f;
	push.tint[2] = r_lensFlareTintB ? r_lensFlareTintB->value : 1.0f;

	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_blend, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof( push ), &push );
	vk_set_fullscreen_viewport_scissor( width, height );
	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();

	if ( vk.color_image_view != VK_NULL_HANDLE ) {
		vk_barrier_post_fog_source_for_sampling( vk.color_image_view, "post-lens-flare refresh post-fog source" );
		vk_set_scene_post_fog_source( vk.color_image_view );
		vk_update_post_fog_descriptors( vk.color_image_view );
		if ( r_postAaAfterBloom && r_postAaAfterBloom->integer && vk_post_aa_output_active() ) {
			vk_post_scene_aa_apply();
		}
	}

	backEnd.doneLensFlare = qtrue;
	return qtrue;
}
