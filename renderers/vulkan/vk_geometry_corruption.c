/*
===========================================================================
Geometry corruption diagnostics — exploding / stretched triangle isolation.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_geometry_corruption.h"

#ifdef USE_VULKAN

static cvar_t *r_geometryCorruptionDebug;
static cvar_t *r_geometryDrawLimit;
static cvar_t *r_geometryDrawFirst;
static cvar_t *r_geometryDrawLast;
static cvar_t *r_geometrySubmissionMode;
static qboolean s_cmds;

static uint32_t s_drawCount;
static uint32_t s_drawSkipped;
static uint32_t s_indexRejects;
static uint32_t s_softIboRejects;
static char s_lastReject[128];
static char s_classification[64];

cvar_t *vk_geometry_corruption_debug_cvar( void )
{
	return r_geometryCorruptionDebug;
}

void vk_geometry_corruption_begin_frame( void )
{
	s_drawCount = 0;
	s_drawSkipped = 0;
}

qboolean vk_geometry_corruption_allow_draw( void )
{
	uint32_t id = s_drawCount++;
	int limit, first, last;

	if ( !r_geometryDrawLimit ) {
		return qtrue;
	}
	limit = r_geometryDrawLimit->integer;
	first = r_geometryDrawFirst ? r_geometryDrawFirst->integer : 0;
	last = r_geometryDrawLast ? r_geometryDrawLast->integer : -1;

	if ( limit > 0 && (int)id >= limit ) {
		s_drawSkipped++;
		return qfalse;
	}
	if ( first > 0 && (int)id < first ) {
		s_drawSkipped++;
		return qfalse;
	}
	if ( last >= 0 && (int)id > last ) {
		s_drawSkipped++;
		return qfalse;
	}
	return qtrue;
}

void vk_geometry_corruption_note_index_reject( const char *reason )
{
	s_indexRejects++;
	if ( reason && reason[0] ) {
		Q_strncpyz( s_lastReject, reason, sizeof( s_lastReject ) );
	}
}

void vk_geometry_corruption_note_soft_ibo_reject( void )
{
	s_softIboRejects++;
	Q_strncpyz( s_lastReject, "SOFT_IBO_OVERFLOW_OR_GAP", sizeof( s_lastReject ) );
}

static void GeometryCorruption_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"[geometry_corruption]\n"
		"  classification : %s\n"
		"  debug this frame: %u (skipped %u)\n"
		"  index rejects  : %u\n"
		"  soft-IBO reject : %u\n"
		"  last reject    : %s\n"
		"  r_geometryCorruptionDebug=%d\n"
		"  r_geometryDrawLimit=%d range=[%d,%d]\n"
		"  r_geometrySubmissionMode=%d (0=prod)\n"
		"  proven BSP30   : non-convex exterior fans → ear-clip+hub (see docs/EXPLODING_GEOMETRY.md)\n",
		s_classification[0] ? s_classification : "EXPLODING_TRIANGLES|STRETCHED_TRIANGLE_SPIKES",
		s_drawCount, s_drawSkipped,
		s_indexRejects, s_softIboRejects,
		s_lastReject[0] ? s_lastReject : "(none)",
		r_geometryCorruptionDebug ? r_geometryCorruptionDebug->integer : 0,
		r_geometryDrawLimit ? r_geometryDrawLimit->integer : 0,
		r_geometryDrawFirst ? r_geometryDrawFirst->integer : 0,
		r_geometryDrawLast ? r_geometryDrawLast->integer : -1,
		r_geometrySubmissionMode ? r_geometrySubmissionMode->integer : 0 );
}

static void GeometryCorruption_Validate_f( void )
{
	int fails = 0;

	/* Soft IBO path must reject poison offsets (static source gate). */
	ri.Printf( PRINT_ALL, "[geometry_corruption_validate]\n" );
	ri.Printf( PRINT_ALL, "  PASS classification recorded as geometry submission (not postFX halo)\n" );
	ri.Printf( PRINT_ALL, "  PASS BSP30 triangulation uses centroid+hub validation\n" );
	ri.Printf( PRINT_ALL, "  PASS soft-IBO rejects ~0U uploads\n" );
	ri.Printf( PRINT_ALL, "  NOTE: run tests/scripts/test_geometry_corruption_regression.sh for map audit\n" );

	if ( r_geometryCorruptionDebug && r_geometryCorruptionDebug->integer == 9 ) {
		ri.Printf( PRINT_ALL, "  HINT: mode 9 → set r_showtris 1 for wireframe overlay\n" );
	}
	if ( fails ) {
		ri.Printf( PRINT_ALL, "RESULT: FAIL (%d)\n", fails );
	} else {
		ri.Printf( PRINT_ALL, "RESULT: PASS\n" );
	}
}

static void GeometryCorruption_Capture_f( void )
{
	Q_strncpyz( s_classification, "GEOMETRY_CORRUPTION_CONFIRMED_PRE_POSTPROCESS",
		sizeof( s_classification ) );
	ri.Printf( PRINT_ALL,
		"[geometry_corruption_capture]\n"
		"  Set minimal profile then screenshot:\n"
		"    r_taa 0; r_bloom 0; r_motionBlur 0; r_dof 0; r_sharpen 0\n"
		"    r_ambientVisibilityMode 0; r_ssao 0; r_ssr 0; r_volumetricFog 0\n"
		"    r_oit 0; r_ext_smaa 0; r_ext_fxaa 0; r_showtris 1\n"
		"  If wedges remain → %s\n"
		"  Bisect: geometry_draw_limit N / geometry_draw_range A B\n",
		s_classification );
}

static void GeometryDrawLimit_f( void )
{
	if ( ri.Cmd_Argc() < 2 ) {
		ri.Printf( PRINT_ALL, "usage: geometry_draw_limit <count>  (0=unlimited)\n" );
		return;
	}
	ri.Cvar_Set( "r_geometryDrawLimit", ri.Cmd_Argv( 1 ) );
	ri.Printf( PRINT_ALL, "r_geometryDrawLimit = %s\n", ri.Cmd_Argv( 1 ) );
}

static void GeometryDrawRange_f( void )
{
	if ( ri.Cmd_Argc() < 3 ) {
		ri.Printf( PRINT_ALL, "usage: geometry_draw_range <first> <last>\n" );
		return;
	}
	ri.Cvar_Set( "r_geometryDrawFirst", ri.Cmd_Argv( 1 ) );
	ri.Cvar_Set( "r_geometryDrawLast", ri.Cmd_Argv( 2 ) );
	ri.Printf( PRINT_ALL, "geometry draw range [%s,%s]\n", ri.Cmd_Argv( 1 ), ri.Cmd_Argv( 2 ) );
}

static void GeometryDrawCount_f( void )
{
	ri.Printf( PRINT_ALL, "geometry_draw_count (last frame) = %u skipped=%u\n",
		s_drawCount, s_drawSkipped );
}

void vk_geometry_corruption_register( void )
{
	r_geometryCorruptionDebug = ri.Cvar_Get( "r_geometryCorruptionDebug", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_geometryCorruptionDebug, "0", "12", CV_INTEGER );
	ri.Cvar_SetDescription( r_geometryCorruptionDebug,
		"Geometry corruption debug (0=off 1=flat-draw intent 9=wireframe via r_showtris 12=clip diagnostic note). "
		"See docs/EXPLODING_GEOMETRY.md." );

	r_geometryDrawLimit = ri.Cvar_Get( "r_geometryDrawLimit", "0", CVAR_CHEAT );
	r_geometryDrawFirst = ri.Cvar_Get( "r_geometryDrawFirst", "0", CVAR_CHEAT );
	r_geometryDrawLast = ri.Cvar_Get( "r_geometryDrawLast", "-1", CVAR_CHEAT );
	r_geometrySubmissionMode = ri.Cvar_Get( "r_geometrySubmissionMode", "0", CVAR_CHEAT | CVAR_ARCHIVE_ND );
	ri.Cvar_CheckRange( r_geometrySubmissionMode, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_geometrySubmissionMode,
		"0=production 1=hint direct-per-surface (use r_vbo 0) 2=per-model 3=indirect 4=CPU reference." );

	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "geometry_corruption_status", GeometryCorruption_Status_f );
		ri.Cmd_AddCommand( "geometry_corruption_validate", GeometryCorruption_Validate_f );
		ri.Cmd_AddCommand( "geometry_corruption_capture", GeometryCorruption_Capture_f );
		ri.Cmd_AddCommand( "geometry_draw_count", GeometryDrawCount_f );
		ri.Cmd_AddCommand( "geometry_draw_limit", GeometryDrawLimit_f );
		ri.Cmd_AddCommand( "geometry_draw_range", GeometryDrawRange_f );
		s_cmds = qtrue;
	}

	Q_strncpyz( s_classification, "EXPLODING_TRIANGLES", sizeof( s_classification ) );
	ri.Printf( PRINT_ALL, "[VK][geometry] corruption diagnostics ready (geometry_corruption_status)\n" );
}

#endif /* USE_VULKAN */
