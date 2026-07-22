/*
===========================================================================
HDR post-chain stage ownership (exposure → bloom → tonemap → gamma).
Foundation Consolidation.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_hdr_pipeline.h"

static cvar_t *r_hdrStageDebug;
static char s_stageWriters[VK_HDR_STAGE_COUNT][48];
static qboolean s_cmdsRegistered;

static const char *VK_HDR_StageName( vkHdrStage_t stage )
{
	switch ( stage ) {
	case VK_HDR_STAGE_SCENE: return "scene";
	case VK_HDR_STAGE_EXPOSURE: return "exposure";
	case VK_HDR_STAGE_BLOOM: return "bloom";
	case VK_HDR_STAGE_TONEMAP: return "tonemap";
	case VK_HDR_STAGE_GAMMA: return "gamma";
	default: return "unknown";
	}
}

static void VK_HDR_PipelineStatus_f( void )
{
	int i;

	ri.Printf( PRINT_ALL, "======== HDR Pipeline Status ========\n" );
	ri.Printf( PRINT_ALL, "r_hdrStageDebug=%d adaptedExposure=%g postWriter=%s\n",
		r_hdrStageDebug ? r_hdrStageDebug->integer : 0,
		(double)vk.adaptedExposure,
		vk.postChainLastWriter[0] ? vk.postChainLastWriter : "(none)" );
	for ( i = 0; i < (int)VK_HDR_STAGE_COUNT; i++ ) {
		ri.Printf( PRINT_ALL, "  %-10s last=%s\n",
			VK_HDR_StageName( (vkHdrStage_t)i ),
			s_stageWriters[i][0] ? s_stageWriters[i] : "(none)" );
	}
}

void vk_hdr_pipeline_register( void )
{
	r_hdrStageDebug = ri.Cvar_Get( "r_hdrStageDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_hdrStageDebug, "0", "6", CV_INTEGER );
	ri.Cvar_SetDescription( r_hdrStageDebug,
		"HDR stage debug overlay: 1 scene, 2 exposure, 3 bloom, 4 tonemap, 5 gamma, 6 full chain." );
	ri.Cvar_SetGroup( r_hdrStageDebug, CVG_RENDERER );

	if ( !s_cmdsRegistered ) {
		ri.Cmd_AddCommand( "hdr_pipeline_status", VK_HDR_PipelineStatus_f );
		s_cmdsRegistered = qtrue;
		ri.Printf( PRINT_ALL, "[VK][hdr] hdr_pipeline_status ready (r_hdrStageDebug)\n" );
	}
}

void vk_hdr_pipeline_begin_frame( void )
{
	Com_Memset( s_stageWriters, 0, sizeof( s_stageWriters ) );
}

void vk_hdr_pipeline_note_stage( vkHdrStage_t stage, const char *passName )
{
	if ( stage < 0 || stage >= VK_HDR_STAGE_COUNT || !passName || !passName[0] ) {
		return;
	}
	Q_strncpyz( s_stageWriters[stage], passName, sizeof( s_stageWriters[0] ) );
}
