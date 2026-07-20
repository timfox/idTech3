/*
===========================================================================
Raster Ultra 1.10 — color grade ownership wrapper.
===========================================================================
*/

#include "tr_local.h"
#include "vk_color_grade.h"
#include "vk_raster_ultra.h"

static cvar_t *r_colorGrade;
static qboolean s_cmds;
static vkColorGradeState_t s_grade;

void vk_color_grade_register_cvars( void )
{
	if ( r_colorGrade ) {
		return;
	}
	r_colorGrade = ri.Cvar_Get( "r_colorGrade", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_colorGrade, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_colorGrade,
		"Raster Ultra 1.10 professional color grade ownership (latched).\n"
		"Uses existing r_grade_* / LUT / CDL-style controls in scene-linear→display space.\n"
		"Does not conceal incorrect lighting energy." );
	ri.Cvar_SetGroup( r_colorGrade, CVG_RENDERER );
}

void vk_color_grade_init( void )
{
	cvar_t *lut;

	vk_color_grade_register_cvars();
	Com_Memset( &s_grade, 0, sizeof( s_grade ) );
	s_grade.slope[0] = s_grade.slope[1] = s_grade.slope[2] = 1.0f;
	s_grade.power[0] = s_grade.power[1] = s_grade.power[2] = 1.0f;
	s_grade.saturation = 1.0f;
	s_grade.contrast = 1.0f;
	lut = ri.Cvar_Get( "r_grade_lut", "", 0 );
	s_grade.lutActive = ( lut && lut->string[0] ) ? qtrue : qfalse;

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "color_grade_status", vk_color_grade_status_f );
		s_cmds = qtrue;
	}
	ri.Printf( PRINT_ALL, "[VK][ColorGrade] %s (ASC-CDL/LUT via r_grade_*; working=scene-linear→display)\n",
		( r_colorGrade && r_colorGrade->integer ) ? "enabled" : "off" );
}

void vk_color_grade_shutdown( void )
{
}

qboolean vk_color_grade_active( void )
{
	return ( r_colorGrade && r_colorGrade->integer ) ? qtrue : qfalse;
}

const vkColorGradeState_t *vk_color_grade_state( void )
{
	cvar_t *sat = ri.Cvar_Get( "r_grade_saturation", "1", 0 );
	cvar_t *con = ri.Cvar_Get( "r_grade_contrast", "1", 0 );
	cvar_t *temp = ri.Cvar_Get( "r_grade_temperature", "0", 0 );
	cvar_t *tint = ri.Cvar_Get( "r_grade_tint", "0", 0 );
	cvar_t *lut = ri.Cvar_Get( "r_grade_lut", "", 0 );

	s_grade.saturation = sat ? sat->value : 1.0f;
	s_grade.contrast = con ? con->value : 1.0f;
	s_grade.temperature = temp ? temp->value : 0.0f;
	s_grade.tint = tint ? tint->value : 0.0f;
	s_grade.lutActive = ( lut && lut->string[0] ) ? qtrue : qfalse;
	return &s_grade;
}

void vk_color_grade_status_f( void )
{
	const vkColorGradeState_t *g = vk_color_grade_state();

	ri.Printf( PRINT_ALL, "=== Color Grade (Raster Ultra 1.10) ===\n" );
	ri.Printf( PRINT_ALL, "active       : %s\n", vk_color_grade_active() ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "sat/contrast : %.2f / %.2f\n", g->saturation, g->contrast );
	ri.Printf( PRINT_ALL, "temp/tint    : %.2f / %.2f LUT=%s\n",
		g->temperature, g->tint, g->lutActive ? "yes" : "no" );
	ri.Printf( PRINT_ALL, "space        : grade after tonemap in display-referred path; "
		"scene-linear contract owned by present_color\n" );
	ri.Printf( PRINT_ALL, "policy       : gamut-safe; not a lighting substitute\n" );
}
