/*
===========================================================================
Raster Ultra 1.10 — presentation color contract + HDR display probe.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_present_color.h"
#include "vk_raster_ultra.h"

#ifndef VK_COLOR_SPACE_HDR10_ST2084_EXT
#define VK_COLOR_SPACE_HDR10_ST2084_EXT ((VkColorSpaceKHR)1000104004)
#endif
#ifndef VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT
#define VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT ((VkColorSpaceKHR)1000104002)
#endif

static cvar_t *r_presentColor;
static cvar_t *r_presentPaperWhite;
static cvar_t *r_presentPeakNits;
static cvar_t *r_presentUiWhite;
static cvar_t *r_presentTonemapPreference;
static qboolean s_cmds;
static vkPresentColorContract_t s_contract;
static qboolean s_hdr10Seen;
static qboolean s_scrgbSeen;
static VkSurfaceFormatKHR s_hdr10Fmt;
static VkSurfaceFormatKHR s_scrgbFmt;

void vk_present_color_register_cvars( void )
{
	if ( r_presentColor ) {
		return;
	}
	r_presentColor = ri.Cvar_Get( "r_presentColor", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_presentColor, "0", "3", CV_INTEGER );
	ri.Cvar_SetDescription( r_presentColor,
		"Raster Ultra 1.10 display color mode (latched):\n"
		" 0 SDR sRGB (certified default)\n"
		" 1 SDR wide-gamut (policy; falls back to sRGB if unavailable)\n"
		" 2 HDR10 PQ (when WSI reports ST2084)\n"
		" 3 scRGB extended linear (when WSI reports EXTENDED_SRGB_LINEAR)\n"
		"Do not assume swapchain is sRGB." );
	ri.Cvar_SetGroup( r_presentColor, CVG_RENDERER );

	r_presentPaperWhite = ri.Cvar_Get( "r_presentPaperWhite", "203", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_presentPaperWhite, "80", "400", CV_FLOAT );
	r_presentPeakNits = ri.Cvar_Get( "r_presentPeakNits", "1000", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_presentPeakNits, "200", "10000", CV_FLOAT );
	r_presentUiWhite = ri.Cvar_Get( "r_presentUiWhite", "203", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_presentUiWhite, "80", "400", CV_FLOAT );

	r_presentTonemapPreference = ri.Cvar_Get( "r_presentTonemapPreference", "-1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_presentTonemapPreference, "-1", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_presentTonemapPreference,
		"Preferred tonemap when Ultra 1.10 active: -1 keep r_tonemap, 0..4 override (4=AgX)." );
}

void vk_present_color_init( void )
{
	cvar_t *pre;

	vk_present_color_register_cvars();
	Com_Memset( &s_contract, 0, sizeof( s_contract ) );
	s_contract.workingSpace = "scene-linear (Rec.709 primaries, relative radiance)";
	s_contract.sceneUnits = "scene-referred linear; pre-exposure applied once before tonemap";
	s_contract.doubleGammaForbidden = qtrue;
	s_contract.paperWhiteNits = r_presentPaperWhite ? r_presentPaperWhite->value : 203.0f;
	s_contract.peakLuminanceNits = r_presentPeakNits ? r_presentPeakNits->value : 1000.0f;
	s_contract.uiReferenceWhiteNits = r_presentUiWhite ? r_presentUiWhite->value : 203.0f;
	s_contract.displayMode = (vkPresentColorMode_t)( r_presentColor ? r_presentColor->integer : 0 );
	pre = ri.Cvar_Get( "r_pre_exposure_scale", "1", 0 );
	s_contract.preExposure = pre ? pre->value : 1.0f;
	s_contract.prevPreExposure = s_contract.preExposure;
	s_hdr10Seen = qfalse;
	s_scrgbSeen = qfalse;

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "present_color_status", vk_present_color_status_f );
		s_cmds = qtrue;
	}
	ri.Printf( PRINT_ALL,
		"[VK][PresentColor] mode=%d paperWhite=%.0f peak=%.0f uiWhite=%.0f (scene-linear; no double gamma)\n",
		(int)s_contract.displayMode, s_contract.paperWhiteNits,
		s_contract.peakLuminanceNits, s_contract.uiReferenceWhiteNits );
}

void vk_present_color_shutdown( void )
{
}

qboolean vk_present_color_active( void )
{
	return ( r_presentColor && r_presentColor->integer > 0 ) ? qtrue : qfalse;
}

const vkPresentColorContract_t *vk_present_color_contract( void )
{
	cvar_t *pre = ri.Cvar_Get( "r_pre_exposure_scale", "1", 0 );
	s_contract.prevPreExposure = s_contract.preExposure;
	s_contract.preExposure = pre ? pre->value : 1.0f;
	s_contract.swapchainIsSrgb = ( r_vk_swapchain_srgb && r_vk_swapchain_srgb->integer ) ? qtrue : qfalse;
	s_contract.displayMode = (vkPresentColorMode_t)( r_presentColor ? r_presentColor->integer : 0 );
	s_contract.hdrDisplayRequested = ( s_contract.displayMode == VK_PRESENT_COLOR_HDR10_PQ ||
		s_contract.displayMode == VK_PRESENT_COLOR_SCRGB ) ? qtrue : qfalse;
	s_contract.hdrDisplayAvailable = ( s_hdr10Seen || s_scrgbSeen ) ? qtrue : qfalse;
	if ( r_tonemap ) {
		s_contract.tonemapMode = (vkTonemapMode_t)Com_Clamp( 0, 5, r_tonemap->integer );
	}
	return &s_contract;
}

int vk_present_color_preferred_tonemap( void )
{
	if ( r_presentTonemapPreference && r_presentTonemapPreference->integer >= 0 ) {
		return Com_Clamp( 0, 4, r_presentTonemapPreference->integer );
	}
	return r_tonemap ? Com_Clamp( 0, 5, r_tonemap->integer ) : 3;
}

void vk_present_color_on_surface_formats( const VkSurfaceFormatKHR *candidates, uint32_t count )
{
	uint32_t i;

	s_hdr10Seen = qfalse;
	s_scrgbSeen = qfalse;
	if ( !candidates || count == 0 ) {
		return;
	}
	for ( i = 0; i < count; i++ ) {
		if ( candidates[i].colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT ) {
			s_hdr10Seen = qtrue;
			s_hdr10Fmt = candidates[i];
		}
		if ( candidates[i].colorSpace == VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT ) {
			s_scrgbSeen = qtrue;
			s_scrgbFmt = candidates[i];
		}
	}
}

void vk_present_color_apply_selection( VkSurfaceFormatKHR *inoutPresent )
{
	int mode;

	if ( !inoutPresent ) {
		return;
	}
	mode = r_presentColor ? r_presentColor->integer : 0;
	if ( mode == VK_PRESENT_COLOR_HDR10_PQ && s_hdr10Seen ) {
		*inoutPresent = s_hdr10Fmt;
		ri.Printf( PRINT_ALL, "[VK][PresentColor] selected HDR10 PQ format %s\n",
			vk_format_string( inoutPresent->format ) );
		return;
	}
	if ( mode == VK_PRESENT_COLOR_SCRGB && s_scrgbSeen ) {
		*inoutPresent = s_scrgbFmt;
		ri.Printf( PRINT_ALL, "[VK][PresentColor] selected scRGB format %s\n",
			vk_format_string( inoutPresent->format ) );
		return;
	}
	if ( mode == VK_PRESENT_COLOR_HDR10_PQ || mode == VK_PRESENT_COLOR_SCRGB ) {
		ri.Printf( PRINT_ALL, S_COLOR_YELLOW
			"[VK][PresentColor] HDR display requested but unavailable — keeping SDR sRGB\n" S_COLOR_WHITE );
	}
}

void vk_present_color_status_f( void )
{
	const vkPresentColorContract_t *c = vk_present_color_contract();
	static const char *modeNames[] = { "sdr_srgb", "sdr_wide", "hdr10_pq", "scrgb" };

	ri.Printf( PRINT_ALL, "=== Present Color (Raster Ultra 1.10) ===\n" );
	ri.Printf( PRINT_ALL, "workingSpace     : %s\n", c->workingSpace );
	ri.Printf( PRINT_ALL, "sceneUnits       : %s\n", c->sceneUnits );
	ri.Printf( PRINT_ALL, "preExposure      : %.3f (prev %.3f)\n", c->preExposure, c->prevPreExposure );
	ri.Printf( PRINT_ALL, "displayMode      : %s (%d)\n",
		modeNames[(int)Com_Clamp( 0, 3, (int)c->displayMode )], (int)c->displayMode );
	ri.Printf( PRINT_ALL, "HDR requested    : %s available=%s (hdr10=%s scrgb=%s)\n",
		c->hdrDisplayRequested ? "yes" : "no",
		c->hdrDisplayAvailable ? "yes" : "no",
		s_hdr10Seen ? "yes" : "no", s_scrgbSeen ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "paper/peak/ui    : %.0f / %.0f / %.0f nits\n",
		c->paperWhiteNits, c->peakLuminanceNits, c->uiReferenceWhiteNits );
	ri.Printf( PRINT_ALL, "swapchain sRGB   : %s tonemapPref=%d (AgX=4)\n",
		c->swapchainIsSrgb ? "yes" : "no", vk_present_color_preferred_tonemap() );
	ri.Printf( PRINT_ALL, "policy           : no double gamma; UI after tonemap; no post to hide lighting errors\n" );
}
