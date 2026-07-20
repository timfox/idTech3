/*
===========================================================================
Raster Ultra 1.10 — cinematic / physical camera.
===========================================================================
*/

#include "tr_local.h"
#include "vk_cinematic_camera.h"
#include "vk_raster_ultra.h"

static cvar_t *r_cinematicCamera;
static cvar_t *r_cineFocalLength;
static cvar_t *r_cineSensorWidth;
static cvar_t *r_cineAperture;
static cvar_t *r_cineFocusDistance;
static cvar_t *r_cineShutterAngle;
static cvar_t *r_cineISO;
static cvar_t *r_cineAnamorphic;
static cvar_t *r_cineGameplayMode;
static cvar_t *r_cineDof;
static cvar_t *r_cineMotionBlur;
static cvar_t *r_cineExcludeWeapon;
static cvar_t *r_cineLowLatency;
static qboolean s_cmds;
static vkCinematicCamera_t s_cam;

void vk_cinematic_camera_register_cvars( void )
{
	if ( r_cinematicCamera ) {
		return;
	}
	r_cinematicCamera = ri.Cvar_Get( "r_cinematicCamera", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_cinematicCamera, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_cinematicCamera,
		"Raster Ultra 1.10 cinematic camera (latched).\n"
		"Physical focal/aperture/focus; optional DOF + motion blur. UI never blurred." );
	ri.Cvar_SetGroup( r_cinematicCamera, CVG_RENDERER );

	r_cineFocalLength = ri.Cvar_Get( "r_cineFocalLength", "35", CVAR_ARCHIVE_ND );
	r_cineSensorWidth = ri.Cvar_Get( "r_cineSensorWidth", "36", CVAR_ARCHIVE_ND );
	r_cineAperture = ri.Cvar_Get( "r_cineAperture", "2.8", CVAR_ARCHIVE_ND );
	r_cineFocusDistance = ri.Cvar_Get( "r_cineFocusDistance", "250", CVAR_ARCHIVE_ND );
	r_cineShutterAngle = ri.Cvar_Get( "r_cineShutterAngle", "180", CVAR_ARCHIVE_ND );
	r_cineISO = ri.Cvar_Get( "r_cineISO", "800", CVAR_ARCHIVE_ND );
	r_cineAnamorphic = ri.Cvar_Get( "r_cineAnamorphic", "1.0", CVAR_ARCHIVE_ND );
	r_cineGameplayMode = ri.Cvar_Get( "r_cineGameplayMode", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_cineGameplayMode, "0", "1", CV_INTEGER );
	r_cineDof = ri.Cvar_Get( "r_cineDof", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_cineDof, "0", "1", CV_INTEGER );
	r_cineMotionBlur = ri.Cvar_Get( "r_cineMotionBlur", "0", CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_cineMotionBlur, "0", "1", CV_INTEGER );
	r_cineExcludeWeapon = ri.Cvar_Get( "r_cineExcludeWeapon", "1", CVAR_ARCHIVE_ND );
	r_cineLowLatency = ri.Cvar_Get( "r_cineLowLatency", "1", CVAR_ARCHIVE_ND );
	ri.Cvar_SetDescription( r_cineLowLatency,
		"When 1, disable motion blur for low-latency gameplay (no extra frame queue)." );
}

void vk_cinematic_camera_init( void )
{
	vk_cinematic_camera_register_cvars();
	Com_Memset( &s_cam, 0, sizeof( s_cam ) );
	s_cam.excludeUI = qtrue;
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "cinematic_camera_status", vk_cinematic_camera_status_f );
		s_cmds = qtrue;
	}
	ri.Printf( PRINT_ALL, "[VK][CinematicCamera] %s (UI excluded; MB no frame-gen)\n",
		( r_cinematicCamera && r_cinematicCamera->integer ) ? "enabled" : "off" );
}

void vk_cinematic_camera_shutdown( void )
{
}

qboolean vk_cinematic_camera_active( void )
{
	return ( r_cinematicCamera && r_cinematicCamera->integer ) ? qtrue : qfalse;
}

const vkCinematicCamera_t *vk_cinematic_camera_state( void )
{
	return &s_cam;
}

void vk_cinematic_camera_begin_frame( void )
{
	cvar_t *dof;
	cvar_t *mb;

	if ( !vk_cinematic_camera_active() ) {
		return;
	}
	s_cam.focalLengthMm = r_cineFocalLength ? r_cineFocalLength->value : 35.0f;
	s_cam.sensorWidthMm = r_cineSensorWidth ? r_cineSensorWidth->value : 36.0f;
	s_cam.apertureF = r_cineAperture ? r_cineAperture->value : 2.8f;
	s_cam.focusDistance = r_cineFocusDistance ? r_cineFocusDistance->value : 250.0f;
	s_cam.shutterAngleDeg = r_cineShutterAngle ? r_cineShutterAngle->value : 180.0f;
	s_cam.iso = r_cineISO ? r_cineISO->value : 800.0f;
	s_cam.anamorphicRatio = r_cineAnamorphic ? r_cineAnamorphic->value : 1.0f;
	s_cam.cropFactor = 36.0f / ( s_cam.sensorWidthMm > 1.0f ? s_cam.sensorWidthMm : 36.0f );
	s_cam.gameplayMode = ( !r_cineGameplayMode || r_cineGameplayMode->integer ) ? qtrue : qfalse;
	s_cam.excludeWeapon = ( !r_cineExcludeWeapon || r_cineExcludeWeapon->integer ) ? qtrue : qfalse;
	s_cam.excludeUI = qtrue;
	s_cam.lowLatencyDisableMB = ( !r_cineLowLatency || r_cineLowLatency->integer ) ? qtrue : qfalse;
	s_cam.dofEnabled = ( r_cineDof && r_cineDof->integer ) ? qtrue : qfalse;
	s_cam.motionBlurEnabled = ( r_cineMotionBlur && r_cineMotionBlur->integer &&
		!s_cam.lowLatencyDisableMB ) ? qtrue : qfalse;

	/* Drive existing postfx toggles only while cinematic camera owns them. */
	dof = ri.Cvar_Get( "r_depthOfField", "0", 0 );
	mb = ri.Cvar_Get( "r_motionBlur", "0", 0 );
	if ( s_cam.dofEnabled && dof ) {
		ri.Cvar_Set( "r_depthOfField", "1" );
		ri.Cvar_Set( "r_dofFocusDistance", va( "%.1f", s_cam.focusDistance ) );
	}
	if ( s_cam.motionBlurEnabled && mb ) {
		ri.Cvar_Set( "r_motionBlur", "1" );
		/* Shutter angle → strength: 180° ≈ 0.5 */
		ri.Cvar_Set( "r_motionBlurStrength",
			va( "%.3f", Com_Clamp( 0.05f, 1.0f, s_cam.shutterAngleDeg / 360.0f ) ) );
	} else if ( s_cam.lowLatencyDisableMB && mb && r_cineMotionBlur && r_cineMotionBlur->integer ) {
		ri.Cvar_Set( "r_motionBlur", "0" );
	}
}

void vk_cinematic_camera_status_f( void )
{
	ri.Printf( PRINT_ALL, "=== Cinematic Camera (Raster Ultra 1.10) ===\n" );
	ri.Printf( PRINT_ALL, "active         : %s gameplay=%s\n",
		vk_cinematic_camera_active() ? "yes" : "no",
		s_cam.gameplayMode ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "optics         : %.0fmm f/%.1f sensor=%.0fmm crop=%.2f anam=%.2f\n",
		s_cam.focalLengthMm, s_cam.apertureF, s_cam.sensorWidthMm,
		s_cam.cropFactor, s_cam.anamorphicRatio );
	ri.Printf( PRINT_ALL, "focus/shutter  : dist=%.0f shutter=%.0f ISO=%.0f\n",
		s_cam.focusDistance, s_cam.shutterAngleDeg, s_cam.iso );
	ri.Printf( PRINT_ALL, "DOF/MB         : dof=%s mb=%s lowLatency=%s\n",
		s_cam.dofEnabled ? "on" : "off",
		s_cam.motionBlurEnabled ? "on" : "off",
		s_cam.lowLatencyDisableMB ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "exclusions     : UI=yes weapon=%s\n",
		s_cam.excludeWeapon ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "order          : world recon → weapon → bloom → (DOF/MB) → tonemap → grade → UI\n" );
	ri.Printf( PRINT_ALL, "policy         : no frame-gen; current-frame MVs; HUD never blurred\n" );
}
