/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

No-op stubs for experimental neural / scaffold renderer paths when built
with USE_EXPERIMENTAL_RENDERERS=OFF. See docs/NEURAL_RENDERER_PHASES.md.
===========================================================================
*/

#include "tr_local.h"
#include "vk_niv.h"
#include "vk_nist.h"
#include "vk_nvc.h"
#include "vk_vfgi.h"
#include "vk_ndgi.h"
#include "vk_nslm.h"
#include "vk_renderformer.h"
#include "vk_vksplat.h"
#include "vk_wpt.h"
#include "vk_mgs.h"
#include "vk_wsp.h"
#include "extensions/scaffold/vk_arc_blanc.h"
#include "extensions/scaffold/vk_curast.h"
#include "extensions/scaffold/vk_dressi.h"
#include "extensions/scaffold/vk_iris.h"
#include "extensions/scaffold/vk_mimir.h"
#include "extensions/scaffold/vk_vuda.h"
#include "extensions/rtx/vk_fsa.h"
#include "extensions/rtx/vk_grtx.h"
#include "extensions/rtx/vk_hybrid1.h"
#include "extensions/rtx/vk_pathtrace.h"
#include "extensions/rtx/vk_raygun.h"
#include "extensions/splats/vk_squeezeme.h"

#ifndef USE_EXPERIMENTAL_RENDERERS

static void VK_ExpStubLogOnce( const char *tag )
{
	static qboolean s_logged[13];
	static const char *const s_tags[] = {
		"NIV", "NIST", "NVC", "VFGI", "NDGI", "NSLM",
		"RenderFormer", "VkSplat", "WPT", "MGS", "WSP", "ArcBlanc", NULL
	};
	int i;

	for ( i = 0; s_tags[i]; i++ ) {
		if ( !Q_stricmp( tag, s_tags[i] ) ) {
			if ( !s_logged[i] ) {
				ri.Printf( PRINT_ALL,
					"[VK][%s] stub (build with -DUSE_EXPERIMENTAL_RENDERERS=ON)\n", tag );
				s_logged[i] = qtrue;
			}
			return;
		}
	}
}

static cvar_t *VK_ExpStubCvar( const char *name, const char *value, int flags )
{
	cvar_t *cv;

	cv = ri.Cvar_Get( name, value, flags );
	ri.Cvar_SetDescription( cv,
		"Experimental renderer scaffold; requires USE_EXPERIMENTAL_RENDERERS=ON at build time." );
	return cv;
}

/* --- NIV --- */
void R_NIV_Init( void ) { VK_ExpStubLogOnce( "NIV" ); (void)VK_ExpStubCvar( "r_niv", "0", CVAR_ARCHIVE_ND | CVAR_LATCH ); }
void R_NIV_Shutdown( void ) {}
void R_NIV_OnMapLoad( const char *mapBaseName ) { (void)mapBaseName; }
void vk_niv_apply_after_geometry( void ) {}
qboolean R_NIV_Active( void ) { return qfalse; }

/* --- NIST --- */
void R_NIST_Init( void ) { VK_ExpStubLogOnce( "NIST" ); (void)VK_ExpStubCvar( "r_nist", "0", CVAR_ARCHIVE_ND | CVAR_LATCH ); }
void R_NIST_Shutdown( void ) {}
void R_NIST_OnMapLoad( const char *mapBaseName ) { (void)mapBaseName; }
void vk_nist_apply_after_geometry( void ) {}
qboolean R_NIST_Active( void ) { return qfalse; }

/* --- NVC --- */
void R_NVC_Init( void ) { VK_ExpStubLogOnce( "NVC" ); (void)VK_ExpStubCvar( "r_nvc", "0", CVAR_ARCHIVE_ND | CVAR_LATCH ); }
void R_NVC_Shutdown( void ) {}
void R_NVC_OnMapLoad( const char *mapBaseName ) { (void)mapBaseName; }
void vk_nvc_apply_after_geometry( void ) {}
qboolean R_NVC_Active( void ) { return qfalse; }

/* --- VFGI --- */
void R_VFGI_Init( void ) { VK_ExpStubLogOnce( "VFGI" ); (void)VK_ExpStubCvar( "r_vfgi", "0", CVAR_ARCHIVE_ND | CVAR_LATCH ); }
void R_VFGI_Shutdown( void ) {}
void R_VFGI_OnMapLoad( const char *mapBaseName ) { (void)mapBaseName; }
void vk_vfgi_apply_after_geometry( void ) {}
qboolean R_VFGI_Active( void ) { return qfalse; }

/* --- NDGI --- */
void R_NDGI_Init( void ) { VK_ExpStubLogOnce( "NDGI" ); (void)VK_ExpStubCvar( "r_ndgi", "0", CVAR_ARCHIVE_ND | CVAR_LATCH ); }
void R_NDGI_Shutdown( void ) {}
void R_NDGI_OnMapLoad( const char *mapBaseName ) { (void)mapBaseName; }
void R_NDGI_FrameUpdate( void ) {}
qboolean R_NDGI_Active( void ) { return qfalse; }

/* --- NSLM --- */
void R_NSLM_Init( void ) { VK_ExpStubLogOnce( "NSLM" ); (void)VK_ExpStubCvar( "r_nslm", "0", CVAR_ARCHIVE_ND | CVAR_LATCH ); }
void R_NSLM_Shutdown( void ) {}
void R_NSLM_OnMapLoad( const char *mapBaseName ) { (void)mapBaseName; }
void vk_nslm_apply_to_froxels( uint32_t groups_x, uint32_t groups_y, uint32_t groups_z )
{
	(void)groups_x;
	(void)groups_y;
	(void)groups_z;
}
qboolean R_NSLM_Active( void ) { return qfalse; }

/* --- RenderFormer --- */
void R_RenderFormer_Init( void )
{
	VK_ExpStubLogOnce( "RenderFormer" );
	(void)VK_ExpStubCvar( "r_renderformer", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
}
void R_RenderFormer_Shutdown( void ) {}
void R_RenderFormer_OnMapLoad( const char *mapBaseName ) { (void)mapBaseName; }
void vk_renderformer_apply_after_geometry( void ) {}
qboolean R_RenderFormer_Active( void ) { return qfalse; }

/* --- VkSplat --- */
void R_VKSplat_Init( void )
{
	VK_ExpStubLogOnce( "VkSplat" );
	(void)VK_ExpStubCvar( "r_vksplat", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
}
void R_VKSplat_Shutdown( void ) {}
qboolean R_VKSplat_Active( void ) { return qfalse; }
qboolean R_VKSplat_RunTrainSteps( int steps ) { (void)steps; return qfalse; }

/* --- WPT --- */
void R_WPT_Init( void ) { VK_ExpStubLogOnce( "WPT" ); (void)VK_ExpStubCvar( "r_wpt", "0", CVAR_ARCHIVE_ND | CVAR_LATCH ); }
void R_WPT_Shutdown( void ) {}
void vk_wpt_apply_after_geometry( void ) {}
qboolean R_WPT_Active( void ) { return qfalse; }

/* --- MGS --- */
void R_MGS_Init( void ) { VK_ExpStubLogOnce( "MGS" ); (void)VK_ExpStubCvar( "r_mgs", "0", CVAR_ARCHIVE_ND | CVAR_LATCH ); }
void R_MGS_Shutdown( void ) {}
void R_MGS_OnMapLoad( const char *mapBaseName ) { (void)mapBaseName; }
void vk_mgs_apply_after_geometry( void ) {}
qboolean R_MGS_Active( void ) { return qfalse; }
int R_MGS_EffectiveTier( void ) { return 0; }
qboolean R_MGS_UploadGaussians( uint32_t count, const void *src, size_t srcStride )
{
	(void)count;
	(void)src;
	(void)srcStride;
	return qfalse;
}
void R_MGS_EnsurePipelines( void ) {}
void R_MGS_MarkLoaded( const char *mapBaseName, uint32_t gaussianCount )
{
	(void)mapBaseName;
	(void)gaussianCount;
}

/* --- WSP --- */
void R_WSP_Init( void ) { VK_ExpStubLogOnce( "WSP" ); (void)VK_ExpStubCvar( "r_wsp", "0", CVAR_ARCHIVE_ND | CVAR_LATCH ); }
void R_WSP_Shutdown( void ) {}
void R_WSP_OnMapLoad( const char *mapBaseName ) { (void)mapBaseName; }
void vk_wsp_apply_after_geometry( void ) {}
qboolean R_WSP_Active( void ) { return qfalse; }

/* --- SqueezeMe (splats; full path when USE_EXPERIMENTAL_RENDERERS=ON) --- */
void R_SQZ_Init( void ) { (void)VK_ExpStubCvar( "r_squeezeme", "0", CVAR_ARCHIVE_ND | CVAR_LATCH ); }
void R_SQZ_Shutdown( void ) {}
void R_SQZ_OnMapLoad( const char *mapBaseName ) { (void)mapBaseName; }
void R_SQZ_FrameUpdate( void ) {}
void vk_sqz_apply_after_geometry( void ) {}
qboolean R_SQZ_Active( void ) { return qfalse; }
qboolean R_SQZ_Enabled( void ) { return qfalse; }
int R_SQZ_EffectiveMgsTier( void ) { return 0; }

/* --- Arc Blanc (scaffold; full path when USE_EXPERIMENTAL_RENDERERS=ON) --- */
void R_ArcBlanc_Init( void )
{
	VK_ExpStubLogOnce( "ArcBlanc" );
	(void)VK_ExpStubCvar( "r_arcBlanc", "0", CVAR_ARCHIVE );
	(void)VK_ExpStubCvar( "r_arcBlancDraw", "1", CVAR_ARCHIVE );
}
void R_ArcBlanc_AddSurfaces( void ) {}
void VK_ArcBlanc_Shutdown( void ) {}
void RE_ArcBlancUploadHeightMap( const byte *rgba, int width, int height )
{
	(void)rgba;
	(void)width;
	(void)height;
}
qboolean RE_ArcBlancGpuOceanStep( const arcBlancGpuParams_t *params )
{
	(void)params;
	return qfalse;
}

/* --- FSA (Forget Superresolution; full path when USE_EXPERIMENTAL_RENDERERS=ON) --- */
void R_FSA_Init( void ) { (void)VK_ExpStubCvar( "r_fsa", "0", CVAR_ARCHIVE_ND | CVAR_LATCH ); }
void R_FSA_Shutdown( void ) {}
void R_FSA_OnMapLoad( const char *mapBaseName ) { (void)mapBaseName; }
qboolean R_FSA_Active( void ) { return qfalse; }
qboolean vk_fsa_rtx_adaptive_wanted( void ) { return qfalse; }
void vk_fsa_build_importance_after_geometry( void ) {}
void vk_fsa_denoise_after_rtx( VkCommandBuffer cmd ) { (void)cmd; }
VkImageView vk_fsa_get_importance_view( void ) { return VK_NULL_HANDLE; }
void vk_fsa_patch_rtx_trace_params( float traceParams[4], uint32_t frameSeed )
{
	(void)frameSeed;
	if ( traceParams ) {
		traceParams[0] = traceParams[1] = traceParams[2] = traceParams[3] = 0.0f;
	}
}
void vk_fsa_write_rtx_importance_descriptor( VkDescriptorSet rtxSet ) { (void)rtxSet; }

/* --- RTX profile extensions (raygun / hybrid1 / pathtrace / grtx) --- */
void R_Raygun_Init( void ) { (void)VK_ExpStubCvar( "r_raygun", "0", CVAR_ARCHIVE_ND | CVAR_LATCH ); }
void R_Raygun_Shutdown( void ) {}
void vk_raygun_init( void ) {}
void vk_raygun_shutdown( void ) {}
void vk_raygun_frame_begin( void ) {}
qboolean vk_raygun_active( void ) { return qfalse; }
void vk_raygun_record_pass( VkCommandBuffer cmd ) { (void)cmd; }

void R_GRTX_Init( void ) { (void)VK_ExpStubCvar( "r_grtx", "0", CVAR_ARCHIVE_ND | CVAR_LATCH ); }
void R_GRTX_Shutdown( void ) {}
void vk_grtx_init( void ) {}
void vk_grtx_shutdown( void ) {}
void vk_grtx_frame_begin( void ) {}
void vk_grtx_on_map_load( const char *mapBaseName ) { (void)mapBaseName; }
void vk_grtx_record_pass( VkCommandBuffer cmd ) { (void)cmd; }
qboolean vk_grtx_active( void ) { return qfalse; }

void vk_pathtrace_init( void ) {}
void vk_pathtrace_shutdown( void ) {}
void vk_pathtrace_frame_begin( void ) {}
qboolean vk_pathtrace_active( void ) { return qfalse; }
void vk_pathtrace_record_pass( VkCommandBuffer cmd ) { (void)cmd; }

void vk_hybrid1_init( void ) {}
void vk_hybrid1_shutdown( void ) {}
void vk_hybrid1_frame_begin( void ) {}
qboolean vk_hybrid1_active( void ) { return qfalse; }
void vk_hybrid1_record_pass( VkCommandBuffer cmd ) { (void)cmd; }

/* --- Scaffold profile extensions --- */
void R_CuRast_Init( void ) { (void)VK_ExpStubCvar( "r_curast", "0", CVAR_ARCHIVE_ND | CVAR_LATCH ); }
void R_CuRast_Shutdown( void ) {}
qboolean R_CuRast_Active( void ) { return qfalse; }
qboolean R_CuRast_RenderFrame( void ) { return qfalse; }

void R_Dressi_Init( void ) { (void)VK_ExpStubCvar( "r_dressi", "0", CVAR_ARCHIVE_ND | CVAR_LATCH ); }
void R_Dressi_Shutdown( void ) {}
qboolean vk_dressi_active( void ) { return qfalse; }
void vk_dressi_record_pass( VkCommandBuffer cmd ) { (void)cmd; }

void R_Iris_Init( void ) { (void)VK_ExpStubCvar( "r_iris", "0", CVAR_ARCHIVE_ND | CVAR_LATCH ); }
void R_Iris_Shutdown( void ) {}
qboolean R_Iris_Active( void ) { return qfalse; }
qboolean R_Iris_PanNewFOV( void ) { return qfalse; }
qboolean vk_iris_overlay_active( void ) { return qfalse; }
void vk_iris_record_overlay( VkCommandBuffer cmd ) { (void)cmd; }

void R_Mimir_Init( void ) { (void)VK_ExpStubCvar( "r_mimir", "0", CVAR_ARCHIVE_ND | CVAR_LATCH ); }
void R_Mimir_Shutdown( void ) {}
qboolean R_Mimir_Active( void ) { return qfalse; }
qboolean R_Mimir_RunStep( void ) { return qfalse; }

void R_VUDA_Init( void ) { (void)VK_ExpStubCvar( "r_vuda", "0", CVAR_ARCHIVE_ND | CVAR_LATCH ); }
void R_VUDA_Shutdown( void ) {}
qboolean R_VUDA_Active( void ) { return qfalse; }
qboolean R_VUDA_InteropReady( void ) { return qfalse; }
qboolean R_VUDA_GetExportBundle( vudaExportBundle_t *out ) { (void)out; return qfalse; }
qboolean R_VUDA_GetSlotExport( int slot, vudaSlotExport_t *out ) { (void)slot; (void)out; return qfalse; }
void R_VUDA_TryBuildInterop( void ) {}
void vk_vuda_frame_begin( void ) {}
void vk_vuda_after_queue_submit( void ) {}
qboolean vk_vuda_consume_compute_window( void ) { return qfalse; }
void vk_vuda_notify_cuda_complete( uint64_t value ) { (void)value; }

#endif /* !USE_EXPERIMENTAL_RENDERERS */
