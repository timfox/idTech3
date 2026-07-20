/*
===========================================================================
Raster Ultra 1.0 — lock RT off; report raster completeness.
===========================================================================
*/

#include "tr_local.h"
#include "vk_raster_ultra.h"
#include "vk_selective_sun_shadow.h"
#include "vk_selective_reflection.h"

static cvar_t *r_rasterUltra;
static qboolean s_inited;
static qboolean s_enforcedOnce;

static int vk_ru_named_int( const char *name )
{
	cvar_t *cv = ri.Cvar_Get( name, "0", 0 );
	return cv ? cv->integer : 0;
}

static void vk_ru_force_zero( const char *name )
{
	cvar_t *cv = ri.Cvar_Get( name, "0", 0 );
	if ( cv && cv->integer != 0 ) {
		ri.Printf( PRINT_ALL, S_COLOR_YELLOW
			"[VK][RasterUltra] forcing %s 0 (was %d)\n" S_COLOR_WHITE,
			name, cv->integer );
		ri.Cvar_Set( name, "0" );
	}
}

/*
===============
VK_RasterUltra_Init
===============
*/
void VK_RasterUltra_Init( void )
{
	if ( s_inited ) {
		return;
	}

	r_rasterUltra = ri.Cvar_Get( "r_rasterUltra", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_rasterUltra, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_rasterUltra,
		"Raster Ultra 1.0 contract (latched):\n"
		" 0 - inactive (default; certified modern_vulkan unchanged)\n"
		" 1 - high-end raster profile active; forces RT masters off\n"
		"     (r_hybrid1, r_rtx, r_pathtrace, selective hybrid, RTAO)\n"
		"Enable via: exec modern_raster_ultra.cfg; vid_restart" );
	ri.Cvar_SetGroup( r_rasterUltra, CVG_RENDERER );

	s_inited = qtrue;
	s_enforcedOnce = qfalse;

	if ( r_rasterUltra->integer ) {
		VK_RasterUltra_Enforce();
	}

	ri.Printf( PRINT_ALL, "[VK][RasterUltra] active=%d (0=off; experimental high-end raster, RT locked)\n",
		r_rasterUltra->integer );
}

void VK_RasterUltra_Shutdown( void )
{
	s_inited = qfalse;
	s_enforcedOnce = qfalse;
}

qboolean VK_RasterUltra_Active( void )
{
	return ( r_rasterUltra && r_rasterUltra->integer ) ? qtrue : qfalse;
}

void VK_RasterUltra_Enforce( void )
{
	if ( !VK_RasterUltra_Active() ) {
		return;
	}

	vk_ru_force_zero( "r_hybrid1" );
	vk_ru_force_zero( "r_rtx" );
	vk_ru_force_zero( "r_rtxDemo" );
	vk_ru_force_zero( "r_pathtrace" );
	vk_ru_force_zero( "r_selectiveHybridSunShadow" );
	vk_ru_force_zero( "r_selectiveHybridReflection" );
	vk_ru_force_zero( "r_surfelGi" );
	vk_ru_force_zero( "r_rcgi" );
	vk_ru_force_zero( "r_grtx" );
	vk_ru_force_zero( "r_wpt" );
	vk_ru_force_zero( "r_fsa" );

	/* Ambient visibility stays raster GTAO (mode 2); demote RT-backed AV modes. */
	{
		cvar_t *av = ri.Cvar_Get( "r_ambientVisibilityMode", "2", 0 );
		if ( av && av->integer > 2 ) {
			ri.Printf( PRINT_ALL, S_COLOR_YELLOW
				"[VK][RasterUltra] r_ambientVisibilityMode %d → 2 (GTAO)\n" S_COLOR_WHITE,
				av->integer );
			ri.Cvar_Set( "r_ambientVisibilityMode", "2" );
		}
	}

	if ( !s_enforcedOnce ) {
		ri.Printf( PRINT_ALL,
			"[VK][RasterUltra] RT locked off (hybrid1/rtx/pathtrace/selective/RTAO/surfel/rcgi)\n" );
		s_enforcedOnce = qtrue;
	}
}

int VK_RasterUltra_RTRequestedCount( void )
{
	int n = 0;
	if ( vk_ru_named_int( "r_hybrid1" ) ) n++;
	if ( vk_ru_named_int( "r_rtx" ) ) n++;
	if ( vk_ru_named_int( "r_rtxDemo" ) ) n++;
	if ( vk_ru_named_int( "r_pathtrace" ) ) n++;
	if ( vk_ru_named_int( "r_selectiveHybridSunShadow" ) ) n++;
	if ( vk_ru_named_int( "r_selectiveHybridReflection" ) ) n++;
	if ( vk_ru_named_int( "r_surfelGi" ) ) n++;
	if ( vk_ru_named_int( "r_rcgi" ) ) n++;
	if ( vk_ru_named_int( "r_grtx" ) ) n++;
	return n;
}

int VK_RasterUltra_RTEffectiveCount( void )
{
	int n = 0;
	if ( vk_shs_rt_owns_sun() ) {
		n++;
	}
	if ( vk_shr_rt_owns() ) {
		n++;
	}
	/* Latched RT masters still count as effective intent when non-zero. */
	if ( vk_ru_named_int( "r_hybrid1" ) > 0 ) {
		n++;
	}
	if ( vk_ru_named_int( "r_rtx" ) > 0 ) {
		n++;
	}
	if ( vk_ru_named_int( "r_pathtrace" ) > 0 ) {
		n++;
	}
	return n;
}

const char *VK_RasterUltra_Completeness( void )
{
	int req;
	int eff;
	int mode;
	int zSlices;
	int avMode;

	if ( !VK_RasterUltra_Active() ) {
		return "inactive";
	}

	req = VK_RasterUltra_RTRequestedCount();
	eff = VK_RasterUltra_RTEffectiveCount();
	if ( req > 0 || eff > 0 ) {
		return "rt_leak";
	}

	mode = ( r_renderMode ) ? r_renderMode->integer : 0;
	zSlices = ( r_forwardPlusZSlices ) ? r_forwardPlusZSlices->integer : 1;
	avMode = ri.Cvar_VariableIntegerValue( "r_ambientVisibilityMode" );
	if ( mode != 3 || zSlices <= 1 ) {
		return "partial";
	}
	if ( !r_deferredLighting || !r_deferredLighting->integer ) {
		return "partial";
	}
	if ( avMode != 2 ) {
		return "partial";
	}
	return "complete";
}

void VK_RasterUltra_PrintStatus( void )
{
	ri.Printf( PRINT_ALL,
		"rasterUltra: active=%d completeness=%s rtReq=%d rtEff=%d "
		"(mode=%d zSlices=%d gtao=%d specularAA=%d sunShadow=%d)\n",
		VK_RasterUltra_Active() ? 1 : 0,
		VK_RasterUltra_Completeness(),
		VK_RasterUltra_RTRequestedCount(),
		VK_RasterUltra_RTEffectiveCount(),
		r_renderMode ? r_renderMode->integer : -1,
		r_forwardPlusZSlices ? r_forwardPlusZSlices->integer : -1,
		ri.Cvar_VariableIntegerValue( "r_ambientVisibilityMode" ),
		r_pbr_specularAA ? r_pbr_specularAA->integer : -1,
		r_pbrSunShadow ? r_pbrSunShadow->integer : -1 );
}
