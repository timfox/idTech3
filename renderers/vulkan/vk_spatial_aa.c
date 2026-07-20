/*
===========================================================================
Raster Ultra 2.1 — Unified Spatial Antialiasing Controller (Slice A).
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_spatial_aa.h"
#include "vk_frequency_aware.h"
#include "vk_post_fog.h"
#include "vk_pass_registry.h"
#include "vk_render_pass.h"
#include "vk_view_state.h"
#include "vk_raster_ultra.h"

typedef struct {
	float invResolutionX;
	float invResolutionY;
	float riskThreshold;
	float sampleBudget;
	float sharpen;
	float debugMode;
} SpatialAaPushConstants_t;

static cvar_t *r_spatialAa;
static cvar_t *r_spatialAaTier;
static cvar_t *r_spatialAaClassify;
static cvar_t *r_spatialAaAdaptiveSS;
static cvar_t *r_spatialAaSelectiveMsaa;
static cvar_t *r_spatialAaRisk;
static cvar_t *r_spatialAaBudget;
static cvar_t *r_spatialAaSharpen;
static cvar_t *r_spatialAaDebug;
static qboolean s_cmds;
static vkSpatialAaState_t s_saa;

static void vk_saa_force_zero( const char *name )
{
	cvar_t *cv = ri.Cvar_Get( name, "0", 0 );
	if ( cv && cv->integer != 0 ) {
		ri.Printf( PRINT_ALL, S_COLOR_YELLOW
			"[VK][SpatialAA] forcing %s 0 (was %d)\n" S_COLOR_WHITE,
			name, cv->integer );
		ri.Cvar_Set( name, "0" );
	}
}

static void vk_saa_set_int_if( const char *name, int value )
{
	cvar_t *cv = ri.Cvar_Get( name, "0", 0 );
	char buf[16];
	if ( !cv || cv->integer == value ) {
		return;
	}
	Com_sprintf( buf, sizeof( buf ), "%d", value );
	ri.Cvar_Set( name, buf );
}

void vk_spatial_aa_register_cvars( void )
{
	if ( r_spatialAa ) {
		return;
	}

	r_spatialAa = ri.Cvar_Get( "r_spatialAa", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_spatialAa, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_spatialAa,
		"Raster Ultra 2.1 Spatial AA controller (latched).\n"
		"Unifies frequency-aware filters, current-frame adaptive SS, and SMAA.\n"
		"Forces TAA and RT off. Does not change boot default." );
	ri.Cvar_SetGroup( r_spatialAa, CVG_RENDERER );

	r_spatialAaTier = ri.Cvar_Get( "r_spatialAaTier", "3", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_spatialAaTier, "0", "5", CV_INTEGER );
	ri.Cvar_SetDescription( r_spatialAaTier,
		"0 off, 1 low, 2 medium, 3 high, 4 ultra, 5 spatial reference lab." );

	r_spatialAaClassify = ri.Cvar_Get( "r_spatialAaClassify", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_spatialAaClassify, "0", "1", CV_INTEGER );

	r_spatialAaAdaptiveSS = ri.Cvar_Get( "r_spatialAaAdaptiveSS", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_spatialAaAdaptiveSS, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_spatialAaAdaptiveSS,
		"Current-frame adaptive supersample before SMAA (history-free)." );

	r_spatialAaSelectiveMsaa = ri.Cvar_Get( "r_spatialAaSelectiveMsaa", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_spatialAaSelectiveMsaa, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_spatialAaSelectiveMsaa,
		"Policy flag for selective MSAA (Slice A scaffold; deferred G-buffer stays 1x by default)." );

	r_spatialAaRisk = ri.Cvar_Get( "r_spatialAaRisk", "0.18", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_spatialAaRisk, "0.02", "0.95", CV_FLOAT );

	r_spatialAaBudget = ri.Cvar_Get( "r_spatialAaBudget", "0.85", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_spatialAaBudget, "0", "1", CV_FLOAT );

	r_spatialAaSharpen = ri.Cvar_Get( "r_spatialAaSharpen", "0.35", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_spatialAaSharpen, "0", "1", CV_FLOAT );

	r_spatialAaDebug = ri.Cvar_Get( "r_spatialAaDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_spatialAaDebug, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_spatialAaDebug,
		"0 off, 1 risk heat, 2 force all pixels SS, 3 force passthrough." );
}

void vk_spatial_aa_init( void )
{
	vk_spatial_aa_register_cvars();
	Com_Memset( &s_saa, 0, sizeof( s_saa ) );
	s_saa.forceTaaOff = qtrue;
	s_saa.forceRtOff = qtrue;
	s_saa.riskThreshold = 0.18f;
	s_saa.sampleBudget = 0.85f;

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "spatial_aa_status", vk_spatial_aa_status_f );
		s_cmds = qtrue;
	}

	if ( r_spatialAa && r_spatialAa->integer ) {
		ri.Printf( PRINT_ALL,
			"[VK] Spatial AA 2.1: active (tier=%s, adaptiveSS policy on, TAA/RT locked off)\n",
			vk_spatial_aa_tier_name( (vkSpatialAaTier_t)( r_spatialAaTier ? r_spatialAaTier->integer : 3 ) ) );
	}
}

void vk_spatial_aa_shutdown( void )
{
	if ( s_cmds ) {
		ri.Cmd_RemoveCommand( "spatial_aa_status" );
		s_cmds = qfalse;
	}
	Com_Memset( &s_saa, 0, sizeof( s_saa ) );
}

qboolean vk_spatial_aa_active( void )
{
	return ( r_spatialAa && r_spatialAa->integer ) ? qtrue : qfalse;
}

const vkSpatialAaState_t *vk_spatial_aa_state( void )
{
	return &s_saa;
}

qboolean vk_spatial_aa_wants_adaptive_ss( void )
{
	if ( !vk_spatial_aa_active() ) {
		return qfalse;
	}
	if ( !s_saa.adaptiveSS ) {
		return qfalse;
	}
	if ( !tr.world || !backEnd.doneWorldScene ) {
		return qfalse;
	}
	return qtrue;
}

qboolean vk_spatial_aa_adaptive_pipeline_ready( void )
{
	return ( vk.spatial_adaptive_pipeline != VK_NULL_HANDLE &&
		vk.render_pass.taa != VK_NULL_HANDLE &&
		vk.framebuffers.taa[0] != VK_NULL_HANDLE &&
		vk.modules.spatial_adaptive_fs != VK_NULL_HANDLE &&
		vk.taa_history_image_view[0] != VK_NULL_HANDLE ) ? qtrue : qfalse;
}

void vk_spatial_aa_enforce_contract( void )
{
	if ( !vk_spatial_aa_active() ) {
		return;
	}

	/* Non-negotiable: no TAA / RT dependence for cinematic spatial path. */
	vk_saa_force_zero( "r_taa" );
	vk_saa_force_zero( "r_hybrid1" );
	vk_saa_force_zero( "r_hybrid1_taa" );
	vk_saa_force_zero( "r_rtx" );
	vk_saa_force_zero( "r_rtxDemo" );
	vk_saa_force_zero( "r_pathtrace" );
	vk_saa_force_zero( "r_surfelGi" );
	vk_saa_force_zero( "r_rcgi" );
	vk_saa_force_zero( "r_grtx" );
	vk_saa_force_zero( "r_wpt" );
	vk_saa_force_zero( "r_fsa" );

	/* SMAA remains the residual edge owner; never promote aaMode 3 (temporal). */
	if ( r_aaMode && r_aaMode->integer >= 3 && r_aaMode->integer <= 5 ) {
		ri.Printf( PRINT_ALL, S_COLOR_YELLOW
			"[VK][SpatialAA] demoting r_aaMode %d → 2 (SMAA; temporal forbidden)\n" S_COLOR_WHITE,
			r_aaMode->integer );
		ri.Cvar_Set( "r_aaMode", "2" );
	}

	/* Moiré foundation: frequency-aware + selective SS responses. */
	vk_saa_set_int_if( "r_frequencyAware", 1 );
	if ( s_saa.tier >= VK_SPATIAL_AA_TIER_HIGH ) {
		vk_saa_set_int_if( "r_frequencyTier", 3 );
		vk_saa_set_int_if( "r_frequencySelectiveSS", 1 );
	} else if ( s_saa.tier >= VK_SPATIAL_AA_TIER_MEDIUM ) {
		vk_saa_set_int_if( "r_frequencyTier", 2 );
	}
	vk_saa_set_int_if( "r_frequencySpecularAA", 1 );
	vk_saa_set_int_if( "r_frequencyAlphaCoverage", 1 );
	vk_saa_set_int_if( "r_ext_texture_filter_anisotropic", 1 );
}

void vk_spatial_aa_begin_frame( void )
{
	int tier;

	if ( !vk_spatial_aa_active() ) {
		s_saa.tier = VK_SPATIAL_AA_TIER_OFF;
		return;
	}

	tier = r_spatialAaTier ? r_spatialAaTier->integer : 3;
	if ( tier < 0 ) {
		tier = 0;
	}
	if ( tier > 5 ) {
		tier = 5;
	}
	s_saa.tier = (vkSpatialAaTier_t)tier;
	s_saa.frameCount++;
	s_saa.classify = ( r_spatialAaClassify && r_spatialAaClassify->integer ) ? qtrue : qfalse;
	s_saa.adaptiveSS = ( r_spatialAaAdaptiveSS && r_spatialAaAdaptiveSS->integer &&
		tier >= VK_SPATIAL_AA_TIER_MEDIUM ) ? qtrue : qfalse;
	s_saa.selectiveMsaa = ( r_spatialAaSelectiveMsaa && r_spatialAaSelectiveMsaa->integer ) ? qtrue : qfalse;
	s_saa.smaaCleanup = qtrue;
	s_saa.frequencyAware = qtrue;
	s_saa.riskThreshold = r_spatialAaRisk ? r_spatialAaRisk->value : 0.18f;
	s_saa.sampleBudget = r_spatialAaBudget ? r_spatialAaBudget->value : 0.85f;
	/* Rough coverage estimate for status (not a GPU counter yet). */
	s_saa.lastEstimatedCoverage = Com_Clamp( 0.05f, 0.55f,
		( 1.0f - s_saa.riskThreshold ) * 0.45f * s_saa.sampleBudget );

	vk_spatial_aa_enforce_contract();
}

VkImageView vk_spatial_aa_prepare_input( VkImageView color_source )
{
	SpatialAaPushConstants_t pc;
	uint32_t w, h;
	VkDescriptorSet sets[2];
	VkViewport viewport;
	VkRect2D scissor;

	if ( color_source == VK_NULL_HANDLE ) {
		return color_source;
	}
	if ( !vk_spatial_aa_wants_adaptive_ss() || !vk_spatial_aa_adaptive_pipeline_ready() ) {
		return color_source;
	}
	if ( vk.pipeline_layout_smaa == VK_NULL_HANDLE ||
		vk.color_descriptor[vk.cmd_index] == VK_NULL_HANDLE ||
		vk.depth_descriptor[vk.cmd_index] == VK_NULL_HANDLE ) {
		return color_source;
	}

	w = vk_get_render_target_width();
	h = vk_get_render_target_height();
	if ( w < 1u ) {
		w = 1u;
	}
	if ( h < 1u ) {
		h = 1u;
	}

	vk_barrier_post_fog_source_for_sampling( color_source, "pre-spatial-adaptive-ss" );
	vk_update_color_descriptor_image( color_source );

	vk_spine_pass_begin( VK_SPINE_PASS_SPATIAL_AA );
	vk_begin_render_pass_tracked( vk.render_pass.taa, vk.framebuffers.taa[0], qfalse, w, h );
	qvkCmdBindPipeline( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk.spatial_adaptive_pipeline );

	pc.invResolutionX = 1.0f / (float)w;
	pc.invResolutionY = 1.0f / (float)h;
	pc.riskThreshold = s_saa.riskThreshold;
	pc.sampleBudget = s_saa.sampleBudget;
	pc.sharpen = r_spatialAaSharpen ? r_spatialAaSharpen->value : 0.35f;
	pc.debugMode = r_spatialAaDebug ? (float)r_spatialAaDebug->integer : 0.0f;
	qvkCmdPushConstants( vk.cmd->command_buffer, vk.pipeline_layout_smaa, VK_SHADER_STAGE_FRAGMENT_BIT,
		0, sizeof( pc ), &pc );

	sets[0] = vk.color_descriptor[vk.cmd_index];
	sets[1] = vk.depth_descriptor[vk.cmd_index];
	qvkCmdBindDescriptorSets( vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
		vk.pipeline_layout_smaa, 0, 2, sets, 0, NULL );

	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)w;
	viewport.height = (float)h;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = w;
	scissor.extent.height = h;
	qvkCmdSetViewport( vk.cmd->command_buffer, 0, 1, &viewport );
	qvkCmdSetScissor( vk.cmd->command_buffer, 0, 1, &scissor );

	qvkCmdDraw( vk.cmd->command_buffer, 4, 1, 0, 0 );
	vk_end_render_pass();
	vk_spine_pass_end( VK_SPINE_PASS_SPATIAL_AA );

	s_saa.adaptivePasses++;
	vk_barrier_post_fog_source_for_sampling( vk.taa_history_image_view[0], "post-spatial-adaptive-ss" );
	return vk.taa_history_image_view[0];
}

const char *vk_spatial_aa_tier_name( vkSpatialAaTier_t t )
{
	static const char *names[] = {
		"off", "low", "medium", "high", "ultra", "reference"
	};
	if ( t < 0 || t > VK_SPATIAL_AA_TIER_REFERENCE ) {
		return "invalid";
	}
	return names[t];
}

void vk_spatial_aa_status_f( void )
{
	ri.Printf( PRINT_ALL, "=== Spatial AA (Raster Ultra 2.1 Slice A) ===\n" );
	ri.Printf( PRINT_ALL, "active         : %s tier=%s frames=%u adaptivePasses=%u\n",
		vk_spatial_aa_active() ? "yes" : "no",
		vk_spatial_aa_tier_name( s_saa.tier ),
		s_saa.frameCount, s_saa.adaptivePasses );
	ri.Printf( PRINT_ALL, "pipeline       : classify=%s adaptiveSS=%s selectiveMsaa=%s smaa=%s freqAware=%s\n",
		s_saa.classify ? "yes" : "no",
		s_saa.adaptiveSS ? "yes" : "no",
		s_saa.selectiveMsaa ? "policy" : "off",
		s_saa.smaaCleanup ? "yes" : "no",
		s_saa.frequencyAware ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "adaptive ready : %s\n",
		vk_spatial_aa_adaptive_pipeline_ready() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "risk/budget    : thr=%.3f budget=%.3f estCoverage~%.0f%%\n",
		s_saa.riskThreshold, s_saa.sampleBudget, s_saa.lastEstimatedCoverage * 100.0f );
	ri.Printf( PRINT_ALL, "contract       : TAA=off RT=off (enforced when active)\n" );
	ri.Printf( PRINT_ALL, "surface policy : opaque→freq+SMAA+adaptive; transparent→A2C/WBOIT; weapon→forward; UI→no world AA\n" );
	ri.Printf( PRINT_ALL, "debug          : r_spatialAaDebug=%d (1=risk heat)\n",
		r_spatialAaDebug ? r_spatialAaDebug->integer : 0 );
	if ( vk_frequency_aware_active() ) {
		const vkFreqState_t *fs = vk_frequency_aware_state();
		ri.Printf( PRINT_ALL, "frequency      : selectiveSS=%s alphaCov=%s specAA=%.2f\n",
			fs->selectiveSS ? "yes" : "no",
			fs->alphaCoverage ? "yes" : "no",
			fs->specularAaStrength );
	}
}
