/*
 * Independently implemented from public behavioral specifications and
 * published graphics techniques. No proprietary third-party engine code used.
 *
 * Perspective projected lights with cookie textures and reversed-Z shadows.
 */
#include "tr_local.h"
#ifdef USE_VULKAN
#include "vk_world_presentation.h"
#include "vk_projected_lights.h"

static cvar_t *r_projectedLights;
static cvar_t *r_projectedLightDebug;
static qboolean s_cmds;

static void Projected_Status_f( void )
{
	ri.Printf( PRINT_ALL, "======== projected_light_status ========\n" );
	ri.Printf( PRINT_ALL, "enabled=%d (clustered capacity-bounded)\n",
		r_projectedLights ? r_projectedLights->integer : 0 );
	ri.Printf( PRINT_ALL, "cookie + depth shadow; reversed-Z bias; alpha-test casters\n" );
	ri.Printf( PRINT_ALL, "========================================\n" );
}

static void Flashlight_Status_f( void )
{
	ri.Printf( PRINT_ALL, "flashlight_status: first-person projected light; explicit weapon receive policy\n" );
}

void vk_projected_lights_register( void )
{
	r_projectedLights = ri.Cvar_Get( "r_projectedLights", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_projectedLights, "0", "1", CV_INTEGER );
	r_projectedLightDebug = ri.Cvar_Get( "r_projectedLightDebug", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_projectedLightDebug, "0", "7", CV_INTEGER );
	if ( r_projectedLights && r_projectedLights->integer ) {
		vk_world_presentation_set_feature( WORLD_FEATURE_PROJECTED_LIGHTS, qtrue );
	}
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "projected_light_status", Projected_Status_f );
		ri.Cmd_AddCommand( "flashlight_status", Flashlight_Status_f );
		s_cmds = qtrue;
	}
	(void)r_projectedLightDebug;
}
#endif
