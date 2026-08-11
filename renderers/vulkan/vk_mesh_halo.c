#include "tr_local.h"


#include "vk_mesh_halo.h"

/*
 * Mesh silhouette halo diagnostics — screen-space edge contamination.
 * Complements geometry_corruption (submission) with post-process isolation.
 */

static cvar_t *r_meshHaloDebug;
static cvar_t *r_meshHaloPassDebug;
static int s_meshHaloFrame;

static void MeshHalo_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"=== mesh_halo_status ===\n"
		"  classifications: SILHOUETTE_CORONA DEPTH_EDGE_HALO FOREGROUND_DILATION\n"
		"                   BACKGROUND_BLEED SCREEN_SPACE_EDGE_CONTAMINATION\n"
		"                   TEMPORAL_DISOCCLUSION_LEAK LOW_RES_RECONSTRUCTION_HALO\n"
		"                   BLOOM_EDGE_EXPANSION AO_GI_EDGE_BLEED WEAPON_WORLD_COMPOSITE_HALO\n"
		"  audited defect: AV half-res filter addressed full-res depth/normal with trace texel coordinates\n"
		"  correction: exact AV texelFetch + normalized trace texel-center UV for full-res guidance\n"
		"  r_meshHaloDebug=%d  r_meshHaloPassDebug=%d  frame=%d\n"
		"  Surf defaults of interest: r_bloom r_ssr r_ambientVisibilityMode r_ext_smaa r_taa\n",
		r_meshHaloDebug ? r_meshHaloDebug->integer : 0,
		r_meshHaloPassDebug ? r_meshHaloPassDebug->integer : 0,
		s_meshHaloFrame );
}

static void MeshHalo_Capture_f( void )
{
	ri.Printf( PRINT_ALL,
		"=== mesh_halo_capture ===\n"
		"  Apply isolation profile then screenshot:\n"
		"    r_bloom 0; r_ssr 0; r_ambientVisibilityMode 0; r_ssao 0\n"
		"    r_ext_smaa 0; r_sharpen 0; r_taa 0; r_volumetricFog 0\n"
		"    r_weaponTemporalMode 0\n"
		"  Re-enable one feature at a time to find FIRST_PASS_INTRODUCING_HALO.\n"
		"  Classification hints:\n"
		"    STATIC_HALO — present on first frame without motion\n"
		"    MOTION_ONLY_HALO / TEMPORAL_TRAIL — appears after camera/weapon move\n"
		"    BLOOM_ONLY_CORONA — vanishes with r_bloom 0\n"
		"    LOW_RES_UPSAMPLE_BLEED — vanishes with full-res AV / r_gtaoHalfRes 0\n"
		"    COMPOSITE_ORDER_ERROR — weapon/world ownership mismatch\n" );
}

static void MeshHalo_Validate_f( void )
{
	ri.Printf( PRINT_ALL,
		"=== mesh_halo_validate ===\n"
		"  Edge reconstruction contract:\n"
		"    * bilateral / temporal depth uses positive reversed-Z view depth\n"
		"    * reduced-res filters address full-res guidance with normalized texel-center UVs\n"
		"    * bloom extract gates far-side silhouette neighbors\n"
		"    * SMAA compose rejects blends across relative depth discontinuities\n"
		"    * AV spatial filter uses texelFetch (no LINEAR pre-mix)\n"
		"    * weapon TAA depth confidence is relative view-depth\n"
		"  Commands: mesh_halo_status | mesh_halo_capture | mesh_halo_pass_bisect\n" );
}

static void MeshHalo_PassBisect_f( void )
{
	ri.Printf( PRINT_ALL,
		"=== mesh_halo_pass_bisect ===\n"
		"  Suggested binary isolation order (Surf):\n"
		"    1 opaque only (all screen-space off)\n"
		"    2 + lighting / GI\n"
		"    3 + AV/GTAO (r_ambientVisibilityMode)\n"
		"    4 + SSR\n"
		"    5 + SMAA\n"
		"    6 + bloom\n"
		"    7 + sharpen\n"
		"    8 + weapon temporal (r_weaponTemporalMode 2)\n"
		"  Record FIRST_PASS_INTRODUCING_HALO and SECONDARY_PASS_AMPLIFYING_HALO.\n" );
}

void vk_mesh_halo_begin_frame( void )
{
	s_meshHaloFrame++;
}

void vk_mesh_halo_register( void )
{
	static qboolean commandsRegistered = qfalse;

	r_meshHaloDebug = ri.Cvar_Get( "r_meshHaloDebug", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_meshHaloDebug, "0", "8", CV_INTEGER );
	ri.Cvar_SetDescription( r_meshHaloDebug,
		"Mesh silhouette halo debug overlay mode (0=off, 1..8 reserved views)." );
	r_meshHaloPassDebug = ri.Cvar_Get( "r_meshHaloPassDebug", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_meshHaloPassDebug, "0", "32", CV_INTEGER );

	if ( commandsRegistered ) {
		return;
	}
	commandsRegistered = qtrue;
	ri.Cmd_AddCommand( "mesh_halo_status", MeshHalo_Status_f );
	ri.Cmd_AddCommand( "mesh_halo_capture", MeshHalo_Capture_f );
	ri.Cmd_AddCommand( "mesh_halo_validate", MeshHalo_Validate_f );
	ri.Cmd_AddCommand( "mesh_halo_pass_bisect", MeshHalo_PassBisect_f );
	ri.Cmd_AddCommand( "mesh_halo_pass_status", MeshHalo_PassBisect_f );
}

