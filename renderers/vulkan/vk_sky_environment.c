/*
 * Independently implemented from public behavioral specifications and
 * published graphics techniques. No proprietary third-party engine code used.
 *
 * Reduced-scale secondary world environment for horizon extension.
 * Camera transform: origin' = envOrigin + (cameraOrigin - envOrigin) / scale
 */
#include "tr_local.h"
#ifdef USE_VULKAN
#include "vk_world_presentation.h"
#include "vk_sky_environment.h"

static skyEnvironment_t s_skyEnv;
static cvar_t *r_skyEnvironment;
static cvar_t *r_skyEnvironmentDebug;
static qboolean s_cmds;

static void SkyEnv_Status_f( void )
{
	ri.Printf( PRINT_ALL, "======== sky_environment_status ========\n" );
	ri.Printf( PRINT_ALL, "enabled=%d scale=%.3g origin=(%.1f %.1f %.1f)\n",
		r_skyEnvironment && r_skyEnvironment->integer,
		s_skyEnv.scale, s_skyEnv.origin[0], s_skyEnv.origin[1], s_skyEnv.origin[2] );
	ri.Printf( PRINT_ALL, "fog dens=%.3g start=%.1f end=%.1f\n",
		s_skyEnv.fogDensity, s_skyEnv.fogStart, s_skyEnv.fogEnd );
	ri.Printf( PRINT_ALL, "order: far HDR sky → 3D sky env → main opaque\n" );
	ri.Printf( PRINT_ALL, "constraints: no main PVS, no weapon depth, no duplicate bloom/AE\n" );
	ri.Printf( PRINT_ALL, "========================================\n" );
}

static void SkyEnv_Validate_f( void )
{
	int fails = 0;
	if ( s_skyEnv.scale <= 0.0f ) {
		ri.Printf( PRINT_WARNING, "FAIL: sky environment scale <= 0\n" );
		fails++;
	} else {
		ri.Printf( PRINT_ALL, "PASS: scale=%.3g\n", s_skyEnv.scale );
	}
	ri.Printf( PRINT_ALL, "sky_environment_validate: %s (%d fail)\n", fails ? "FAIL" : "PASS", fails );
}

void vk_sky_environment_register( void )
{
	r_skyEnvironment = ri.Cvar_Get( "r_skyEnvironment", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_skyEnvironment, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_skyEnvironment, "Render scaled sky-environment world behind main opaque scene." );
	r_skyEnvironmentDebug = ri.Cvar_Get( "r_skyEnvironmentDebug", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_skyEnvironmentDebug, "0", "7", CV_INTEGER );

	Com_Memset( &s_skyEnv, 0, sizeof( s_skyEnv ) );
	s_skyEnv.scale = 16.0f;
	s_skyEnv.fogStart = 0.0f;
	s_skyEnv.fogEnd = 1.0f;

	if ( r_skyEnvironment && r_skyEnvironment->integer ) {
		vk_world_presentation_set_feature( WORLD_FEATURE_SKY_ENVIRONMENT, qtrue );
	}

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "sky_environment_status", SkyEnv_Status_f );
		ri.Cmd_AddCommand( "sky_environment_validate", SkyEnv_Validate_f );
		s_cmds = qtrue;
	}
	(void)r_skyEnvironmentDebug;
}
#endif
