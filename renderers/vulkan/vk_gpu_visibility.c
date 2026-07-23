/*
===========================================================================
GPU-Driven Visibility Milestone 1 — candidate stages + occlusion policy.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_gpu_visibility.h"
#include "vk_gpu_frustum_math.h"
#include "vk_gpu_scene.h"
#include "vk_hiz.h"

#define VK_GPU_VIS_MAX_CANDIDATES 8192

static cvar_t *r_visibilityDebug;
static cvar_t *r_gpuOcclusion;
static cvar_t *r_gpuOcclusionConservative;
static cvar_t *r_gpuOcclusionGraceFrames;
static cvar_t *r_gpuOcclusionBias;
static cvar_t *r_gpuOcclusionMaxMip;
static cvar_t *r_gpuOcclusionDebug;

static uint32_t s_candidates[VK_GPU_VIS_MAX_CANDIDATES];
static uint32_t s_candidateCount;
static uint32_t s_rejectByStage[VISIBILITY_STAGE_COUNT];
static uint32_t s_frameCandidates;
static uint32_t s_frameAccepted;
static qboolean s_cmds;
static qboolean s_logged;

void vk_gpu_visibility_register_cvars( void )
{
	if ( r_visibilityDebug ) {
		return;
	}

	r_visibilityDebug = ri.Cvar_Get( "r_visibilityDebug", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_visibilityDebug, "0", "6", CV_INTEGER );
	ri.Cvar_SetDescription( r_visibilityDebug,
		"GPU visibility debug:\n"
		" 1 PVS candidates  2 frustum survivors  3 Hi-Z survivors\n"
		" 4 final draws  5 rejection reason  6 object bounds" );
	ri.Cvar_SetGroup( r_visibilityDebug, CVG_RENDERER );

	r_gpuOcclusion = ri.Cvar_Get( "r_gpuOcclusion", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_gpuOcclusion, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_gpuOcclusion,
		"When r_gpuSceneCull 1: Hi-Z occlusion after frustum (0 disables occlusion only)." );
	ri.Cvar_SetGroup( r_gpuOcclusion, CVG_RENDERER );

	r_gpuOcclusionConservative = ri.Cvar_Get( "r_gpuOcclusionConservative", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_gpuOcclusionConservative, "0", "1", CV_INTEGER );

	r_gpuOcclusionGraceFrames = ri.Cvar_Get( "r_gpuOcclusionGraceFrames", "2", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_gpuOcclusionGraceFrames, "0", "16", CV_INTEGER );

	r_gpuOcclusionBias = ri.Cvar_Get( "r_gpuOcclusionBias", "0.0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_gpuOcclusionBias, "-0.1", "0.1", CV_FLOAT );

	r_gpuOcclusionMaxMip = ri.Cvar_Get( "r_gpuOcclusionMaxMip", "8", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_gpuOcclusionMaxMip, "0", "11", CV_INTEGER );

	r_gpuOcclusionDebug = ri.Cvar_Get( "r_gpuOcclusionDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_gpuOcclusionDebug, "0", "7", CV_INTEGER );
	ri.Cvar_SetDescription( r_gpuOcclusionDebug,
		"1 rect 2 mip 3 objDepth 4 occluder 5 result 6 uncertainty 7 grace" );
}

void vk_gpu_visibility_init( void )
{
	vk_gpu_visibility_register_cvars();
	s_candidateCount = 0;
	Com_Memset( s_rejectByStage, 0, sizeof( s_rejectByStage ) );
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "gpu_visibility_status", vk_gpu_visibility_status_f );
		ri.Cmd_AddCommand( "gpu_visibility_perf", vk_gpu_visibility_perf_f );
		ri.Cmd_AddCommand( "gpu_visibility_freeze", vk_gpu_visibility_status_f );
		s_cmds = qtrue;
	}
	if ( !s_logged ) {
		ri.Printf( PRINT_ALL,
			"[VK][GpuVis] Milestone 1: PVS→frustum→Hi-Z stages (BSP/PVS Stage 0 preserved)\n" );
		s_logged = qtrue;
	}
}

void vk_gpu_visibility_shutdown( void )
{
	s_candidateCount = 0;
}

void vk_gpu_visibility_begin_frame( void )
{
	s_candidateCount = 0;
	Com_Memset( s_rejectByStage, 0, sizeof( s_rejectByStage ) );
	s_frameCandidates = 0;
	s_frameAccepted = 0;
}

void vk_gpu_visibility_clear_candidates( void )
{
	s_candidateCount = 0;
}

qboolean vk_gpu_visibility_add_candidate( uint32_t objectHandle, vkVisibilityStage_t passedStage )
{
	(void)passedStage;
	if ( objectHandle == 0u || s_candidateCount >= VK_GPU_VIS_MAX_CANDIDATES ) {
		return qfalse;
	}
	s_candidates[s_candidateCount++] = objectHandle;
	s_frameCandidates++;
	return qtrue;
}

uint32_t vk_gpu_visibility_candidate_count( void )
{
	return s_candidateCount;
}

const uint32_t *vk_gpu_visibility_candidates( void )
{
	return s_candidates;
}

void vk_gpu_visibility_note_reject( vkVisibilityStage_t stage )
{
	if ( (unsigned)stage < VISIBILITY_STAGE_COUNT ) {
		s_rejectByStage[stage]++;
	}
}

void vk_gpu_visibility_telemetry( uint32_t outRejected[VISIBILITY_STAGE_COUNT] )
{
	if ( outRejected ) {
		Com_Memcpy( outRejected, s_rejectByStage, sizeof( s_rejectByStage ) );
	}
}

qboolean vk_gpu_occlusion_enabled( void )
{
	return ( !r_gpuOcclusion || r_gpuOcclusion->integer ) ? qtrue : qfalse;
}

uint32_t vk_gpu_occlusion_grace_frames( void )
{
	return r_gpuOcclusionGraceFrames ? (uint32_t)r_gpuOcclusionGraceFrames->integer : 2u;
}

float vk_gpu_occlusion_bias( void )
{
	return r_gpuOcclusionBias ? r_gpuOcclusionBias->value : 0.0f;
}

qboolean vk_gpu_frustum_sphere_visible( const float sphere[4],
	const float planeNormals[4][3], const float planeDists[4] )
{
	return GpuFrustum_SphereVisible( sphere, planeNormals, planeDists ) ? qtrue : qfalse;
}

void vk_gpu_visibility_status_f( void )
{
	static const char *stageNames[VISIBILITY_STAGE_COUNT] = {
		"PVS", "FRUSTUM", "HIZ", "LOD", "RENDER_PATH", "FINAL"
	};
	int i;

	ri.Printf( PRINT_ALL, "======== GPU Visibility (M1) ========\n" );
	ri.Printf( PRINT_ALL, "candidates  : %u / %u (frame=%u)\n",
		s_candidateCount, VK_GPU_VIS_MAX_CANDIDATES, s_frameCandidates );
	ri.Printf( PRINT_ALL, "occlusion   : %s grace=%u bias=%.4f maxMip=%d debug=%d\n",
		vk_gpu_occlusion_enabled() ? "on" : "off",
		vk_gpu_occlusion_grace_frames(),
		vk_gpu_occlusion_bias(),
		r_gpuOcclusionMaxMip ? r_gpuOcclusionMaxMip->integer : 8,
		r_gpuOcclusionDebug ? r_gpuOcclusionDebug->integer : 0 );
	ri.Printf( PRINT_ALL, "visDebug    : %d\n",
		r_visibilityDebug ? r_visibilityDebug->integer : 0 );
	for ( i = 0; i < VISIBILITY_STAGE_COUNT; i++ ) {
		ri.Printf( PRINT_ALL, "  reject[%s]=%u\n", stageNames[i], s_rejectByStage[i] );
	}
	ri.Printf( PRINT_ALL, "note        : PVS false positives OK; false negatives forbidden\n" );
	ri.Printf( PRINT_ALL, "=====================================\n" );
}

void vk_gpu_visibility_perf_f( void )
{
	uint32_t cand = 0, fr = 0, hz = 0, draws = 0;
	vk_gpu_scene_telemetry( &cand, &fr, &hz, &draws );
	ri.Printf( PRINT_ALL, "======== GPU Visibility Perf ========\n" );
	ri.Printf( PRINT_ALL, "instances=%u frustumRej=%u hizRej=%u draws=%u candidates=%u\n",
		cand, fr, hz, draws, s_candidateCount );
	ri.Printf( PRINT_ALL, "Hi-Z companion: " );
	vk_hiz_status_f();
}
