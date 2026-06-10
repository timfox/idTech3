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

#ifndef USE_EXPERIMENTAL_RENDERERS

static void VK_ExpStubLogOnce( const char *tag )
{
	static qboolean s_logged[12];
	static const char *const s_tags[] = {
		"NIV", "NIST", "NVC", "VFGI", "NDGI", "NSLM",
		"RenderFormer", "VkSplat", "WPT", "MGS", "WSP", NULL
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

#endif /* !USE_EXPERIMENTAL_RENDERERS */
