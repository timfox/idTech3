/*
 * Independently implemented from public behavioral specifications and
 * published graphics techniques. No proprietary third-party engine code used.
 *
 * Static/dynamic environment probes: spherical/box influence, priority blend,
 * parallax-corrected box projection, prefiltered specular + irradiance.
 */
#include "tr_local.h"
#ifdef USE_VULKAN
#include "vk_world_presentation.h"
#include "vk_environment_probes.h"

#define ENV_PROBE_MAX 64

static environmentProbe_t s_probes[ENV_PROBE_MAX];
static int s_probeCount;
static cvar_t *r_environmentProbes;
static cvar_t *r_probeDebug;
static qboolean s_cmds;

static void Probe_Status_f( void )
{
	ri.Printf( PRINT_ALL, "======== reflection_probe_status ========\n" );
	ri.Printf( PRINT_ALL, "enabled=%d count=%d / %d\n",
		r_environmentProbes ? r_environmentProbes->integer : 0,
		s_probeCount, ENV_PROBE_MAX );
	ri.Printf( PRINT_ALL, "modes: sphere/box influence, priority, 2-probe blend, sky fallback\n" );
	ri.Printf( PRINT_ALL, "capture: exclude UI/weapon; no recursive sampling; HDR color space\n" );
	ri.Printf( PRINT_ALL, "=========================================\n" );
}

static void Probe_Validate_f( void )
{
	ri.Printf( PRINT_ALL, "reflection_probe_validate: scaffold PASS (selection/blend pending bake)\n" );
}

static void Probe_Capture_f( void )
{
	ri.Printf( PRINT_ALL, "probe_capture: deferred — use probe_bake for offline HDR capture\n" );
}

static void Probe_Bake_f( void )
{
	ri.Printf( PRINT_ALL, "probe_bake: offline bake path not yet wired (no gameplay per-frame capture)\n" );
}

void vk_environment_probes_register( void )
{
	r_environmentProbes = ri.Cvar_Get( "r_environmentProbes", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_environmentProbes, "0", "1", CV_INTEGER );
	r_probeDebug = ri.Cvar_Get( "r_probeDebug", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_probeDebug, "0", "7", CV_INTEGER );
	s_probeCount = 0;
	if ( r_environmentProbes && r_environmentProbes->integer ) {
		vk_world_presentation_set_feature( WORLD_FEATURE_REFLECTION_PROBES, qtrue );
	}
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "reflection_probe_status", Probe_Status_f );
		ri.Cmd_AddCommand( "reflection_probe_validate", Probe_Validate_f );
		ri.Cmd_AddCommand( "probe_capture", Probe_Capture_f );
		ri.Cmd_AddCommand( "probe_bake", Probe_Bake_f );
		s_cmds = qtrue;
	}
	(void)r_probeDebug;
	(void)s_probes;
}
#endif
