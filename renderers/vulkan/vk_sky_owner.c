/*
===========================================================================
Raster Ultra 1.7 — exclusive sky ownership.
Prevents classic skybox + physical atmosphere from both contributing full sky.
===========================================================================
*/

#include "tr_local.h"
#include "vk_sky_owner.h"
#include "vk_raster_ultra.h"

static cvar_t *r_skyOwner;
static qboolean s_cmds;

void vk_sky_owner_register_cvars( void )
{
	if ( r_skyOwner ) {
		return;
	}
	r_skyOwner = ri.Cvar_Get( "r_skyOwner", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_skyOwner, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_skyOwner,
		"Exclusive sky radiance owner (Raster Ultra 1.7):\n"
		" 0 classic skybox / skyParms (default — classic maps)\n"
		" 1 physical atmosphere (Rayleigh/Mie; no RT)\n"
		" 2 authored HDR cubemap / panorama\n"
		" 3 solid / no sky\n"
		"Only one owner contributes visible sky. Do not force physical onto maps that omit it." );
	ri.Cvar_SetGroup( r_skyOwner, CVG_RENDERER );
}

void vk_sky_owner_init( void )
{
	vk_sky_owner_register_cvars();
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "sky_owner_status", vk_sky_owner_status_f );
		s_cmds = qtrue;
	}
	ri.Printf( PRINT_ALL, "[VK][SkyOwner] %s (r_skyOwner %d)\n",
		vk_sky_owner_name( vk_sky_owner() ), r_skyOwner ? r_skyOwner->integer : 0 );
}

vkSkyOwner_t vk_sky_owner( void )
{
	int v;

	if ( !r_skyOwner ) {
		return VK_SKY_OWNER_CLASSIC;
	}
	v = r_skyOwner->integer;
	if ( v < 0 || v >= VK_SKY_OWNER_COUNT ) {
		return VK_SKY_OWNER_CLASSIC;
	}
	return (vkSkyOwner_t)v;
}

const char *vk_sky_owner_name( vkSkyOwner_t owner )
{
	switch ( owner ) {
	case VK_SKY_OWNER_PHYSICAL: return "physical_atmosphere";
	case VK_SKY_OWNER_HDR: return "hdr_cubemap";
	case VK_SKY_OWNER_SOLID: return "solid";
	case VK_SKY_OWNER_CLASSIC:
	default: return "classic_skybox";
	}
}

qboolean vk_sky_owner_wants_classic_skybox( void )
{
	return ( vk_sky_owner() == VK_SKY_OWNER_CLASSIC ) ? qtrue : qfalse;
}

qboolean vk_sky_owner_wants_physical_sky( void )
{
	return ( vk_sky_owner() == VK_SKY_OWNER_PHYSICAL ) ? qtrue : qfalse;
}

qboolean vk_sky_owner_wants_hdr_sky( void )
{
	return ( vk_sky_owner() == VK_SKY_OWNER_HDR ) ? qtrue : qfalse;
}

void vk_sky_owner_status_f( void )
{
	ri.Printf( PRINT_ALL, "======== Sky Ownership (Raster Ultra 1.7) ========\n" );
	ri.Printf( PRINT_ALL, "owner           : %s (%d)\n",
		vk_sky_owner_name( vk_sky_owner() ), (int)vk_sky_owner() );
	ri.Printf( PRINT_ALL, "classic_skybox  : %s\n",
		vk_sky_owner_wants_classic_skybox() ? "draw" : "suppressed" );
	ri.Printf( PRINT_ALL, "physical_sky    : %s\n",
		vk_sky_owner_wants_physical_sky() ? "draw" : "suppressed" );
	ri.Printf( PRINT_ALL, "hdr_sky         : %s\n",
		vk_sky_owner_wants_hdr_sky() ? "draw" : "suppressed" );
	ri.Printf( PRINT_ALL, "dual_sky        : forbidden (exclusive owner)\n" );
	ri.Printf( PRINT_ALL, "RT / ray queries: off\n" );
	ri.Printf( PRINT_ALL, "==================================================\n" );
}
