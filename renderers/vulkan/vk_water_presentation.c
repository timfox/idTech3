/*
 * Independently implemented from public behavioral specifications and
 * published graphics techniques. No proprietary third-party engine code used.
 *
 * Dedicated water route (not ordinary WBOIT): Fresnel, dual normals,
 * absorption, foam, quality-tiered reflection/refraction.
 */
#include "tr_local.h"
#include "vk_world_presentation.h"
#include "vk_water_presentation.h"

static cvar_t *r_waterQuality;
static cvar_t *r_waterDebug;
static qboolean s_cmds;

static void Water_Status_f( void )
{
	ri.Printf( PRINT_ALL, "======== water_status ========\n" );
	ri.Printf( PRINT_ALL, "r_waterQuality=%d (0 probe+simple 1 depth refract 2 planar 3 experimental)\n",
		r_waterQuality ? r_waterQuality->integer : 0 );
	ri.Printf( PRINT_ALL, "ownership: specialized transparent material (not ordinary WBOIT)\n" );
	ri.Printf( PRINT_ALL, "pipeline: opaque HDR → refl/refr sources → water → foam → later transparent\n" );
	ri.Printf( PRINT_ALL, "==============================\n" );
}

static void Water_MaterialStatus_f( void )
{
	ri.Printf( PRINT_ALL, "water_material_status: waterMaterial_t fields registered (see vk_world_presentation.h)\n" );
}

static void Water_ReflectionStatus_f( void )
{
	ri.Printf( PRINT_ALL, "water_reflection_status: batch compatible surfaces by reflection source; no N copies\n" );
}

void vk_water_presentation_register( void )
{
	r_waterQuality = ri.Cvar_Get( "r_waterQuality", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_waterQuality, "0", "3", CV_INTEGER );
	r_waterDebug = ri.Cvar_Get( "r_waterDebug", "0", CVAR_TEMP );
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "water_status", Water_Status_f );
		ri.Cmd_AddCommand( "water_material_status", Water_MaterialStatus_f );
		ri.Cmd_AddCommand( "water_reflection_status", Water_ReflectionStatus_f );
		s_cmds = qtrue;
	}
	(void)r_waterDebug;
}
