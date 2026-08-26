#include "tr_local.h"


#include "vk_ghost_lighting.h"

/*
 * Split diagnostics for negative silhouette ghosts vs fullbright lighting escape.
 * Complements mesh_halo / temporal ghost docs; does not change WBOIT.
 */

static cvar_t *r_negativeGhostDebug;
static cvar_t *r_fullbrightDebug;
static cvar_t *r_historyQuarantine;
static cvar_t *r_negativeGhostSequenceDebug;
static cvar_t *r_negativeGhostVelocityDebug;
static cvar_t *r_fullbrightExposureDebug;

static void GhostLighting_ApplyQuarantine( int mode )
{
	/* mode mirrors the plan: disable one temporal owner at a time */
	switch ( mode ) {
	case 1: /* TAA */
		ri.Cvar_Set( "r_taa", "0" );
		ri.Cvar_Set( "r_tsr", "0" );
		break;
	case 2: /* AO */
		ri.Cvar_Set( "r_ssao", "0" );
		ri.Cvar_Set( "r_ambientVisibilityMode", "0" );
		break;
	case 3: /* GI */
		ri.Cvar_Set( "r_rcgi", "0" );
		ri.Cvar_Set( "r_rasterGi", "0" );
		break;
	case 4: /* SSR */
		ri.Cvar_Set( "r_ssr", "0" );
		ri.Cvar_Set( "r_temporalSSR", "0" );
		break;
	case 5: /* volumetric */
		ri.Cvar_Set( "r_volumetricFog", "0" );
		ri.Cvar_Set( "r_temporalFog", "0" );
		break;
	case 6: /* shadow history */
		ri.Cvar_Set( "r_pbrSunShadowTemporal", "0" );
		break;
	case 7: /* weapon history */
		ri.Cvar_Set( "r_weaponTemporalMode", "0" );
		break;
	case 8: /* all image histories */
		ri.Cvar_Set( "r_taa", "0" );
		ri.Cvar_Set( "r_tsr", "0" );
		ri.Cvar_Set( "r_ssao", "0" );
		ri.Cvar_Set( "r_ambientVisibilityMode", "0" );
		ri.Cvar_Set( "r_ssr", "0" );
		ri.Cvar_Set( "r_temporalSSR", "0" );
		ri.Cvar_Set( "r_volumetricFog", "0" );
		ri.Cvar_Set( "r_temporalFog", "0" );
		ri.Cvar_Set( "r_bloom", "0" );
		ri.Cvar_Set( "r_motionBlur", "0" );
		ri.Cvar_Set( "r_weaponTemporalMode", "0" );
		ri.Cvar_Set( "r_exposure_auto", "0" );
		break;
	default:
		break;
	}
}

static void GhostLighting_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"=== ghost_lighting_status ===\n"
		"  classifications: NEGATIVE_TEMPORAL_GHOST STALE_OCCLUSION_HISTORY\n"
		"                   STALE_GI_HISTORY STALE_VOLUMETRIC_HISTORY STALE_SHADOW_MASK\n"
		"                   DISOCCLUSION_REPROJECTION_LEAK LOW_RES_LIGHTING_UPSAMPLE_HALO\n"
		"  FULLBRIGHT: FULLBRIGHT_LIGHTING_ESCAPE UNLIT_ALBEDO_COMPOSITE\n"
		"              MISSING_LIGHTING_OWNER EXPOSURE_CLIPPING\n"
		"  FIRST_STAGE_LOSING_LIGHTING (BSP30): white vertex colors before lighting-lump sample\n"
		"  FIRST_PASS_INTRODUCING_NEGATIVE_GHOST (suspects): r_temporalSSR / AV upsample / bloom\n"
		"  r_negativeGhostDebug=%d r_fullbrightDebug=%d r_historyQuarantine=%d\n"
		"  live: r_taa=%d r_ssr=%d r_temporalSSR=%d r_ssao=%d r_ambientVisibilityMode=%d\n"
		"        r_bloom=%d r_exposure_auto=%d r_exposureSkyWeight=%s r_oit=%d\n",
		r_negativeGhostDebug ? r_negativeGhostDebug->integer : 0,
		r_fullbrightDebug ? r_fullbrightDebug->integer : 0,
		r_historyQuarantine ? r_historyQuarantine->integer : 0,
		ri.Cvar_VariableIntegerValue( "r_taa" ),
		ri.Cvar_VariableIntegerValue( "r_ssr" ),
		ri.Cvar_VariableIntegerValue( "r_temporalSSR" ),
		ri.Cvar_VariableIntegerValue( "r_ssao" ),
		ri.Cvar_VariableIntegerValue( "r_ambientVisibilityMode" ),
		ri.Cvar_VariableIntegerValue( "r_bloom" ),
		ri.Cvar_VariableIntegerValue( "r_exposure_auto" ),
		ri.Cvar_VariableString( "r_exposureSkyWeight" ),
		ri.Cvar_VariableIntegerValue( "r_oit" ) );
}

static void GhostLighting_Capture_f( void )
{
	ri.Printf( PRINT_ALL,
		"=== ghost_lighting_capture ===\n"
		"  1) exec config/ghost_fullbright_ref.cfg   (fixed EV, histories off, lighting on)\n"
		"  2) screenshot + note silhouette / contrast\n"
		"  3) re-enable one consumer: r_historyQuarantine 1..8 or exec bisect steps\n"
		"  4) record FIRST_PASS_INTRODUCING_NEGATIVE_GHOST / FIRST_STAGE_LOSING_LIGHTING\n" );
}

static void GhostLighting_Bisect_f( void )
{
	ri.Printf( PRINT_ALL,
		"=== ghost_lighting_bisect ===\n"
		"  Opaque lit reference → +shadows → +AO → +GI → +SSR → +fog → +volumetrics\n"
		"  → +WBOIT → +world temporal → +weapon → +bloom → +exposure → +tonemap → +SMAA\n"
		"  Use r_historyQuarantine 8 first; then enable one history at a time.\n" );
}

static void Fullbright_Status_f( void )
{
	ri.Printf( PRINT_ALL,
		"=== fullbright_status ===\n"
		"  BSP30 root cause addressed: lighting lump → vertex colors (style 0 luxels)\n"
		"  Owners: MATERIAL_LIGHTING_OWNER_LIGHTMAP (vertex-sampled) |\n"
		"           MATERIAL_LIGHTING_OWNER_FULLBRIGHT_EXPLICIT (special/unlit faces)\n"
		"  Check load log: '...BSP30 lighting: N faces sampled...'\n"
		"  If SceneHDR still flat: lock r_exposure_auto 0; compare before/after exposure\n"
		"  r_fullbrightDebug=%d r_fullbrightExposureDebug=%d r_lightmap=%d r_fullbright=%d\n",
		r_fullbrightDebug ? r_fullbrightDebug->integer : 0,
		r_fullbrightExposureDebug ? r_fullbrightExposureDebug->integer : 0,
		ri.Cvar_VariableIntegerValue( "r_lightmap" ),
		ri.Cvar_VariableIntegerValue( "r_fullbright" ) );
}

static void Fullbright_Capture_f( void )
{
	ri.Printf( PRINT_ALL,
		"=== fullbright_capture ===\n"
		"  Fixed exposure: r_exposure_auto 0; r_exposure 1\n"
		"  Compare albedo vs lit: r_lightmap 1 (light only) vs 0\n"
		"  If lit ≈ base color on ordinary faces → WORLD_FULLBRIGHT_ESCAPE\n" );
}

static void Fullbright_Bisect_f( void )
{
	ri.Printf( PRINT_ALL,
		"=== fullbright_bisect ===\n"
		"  1 lock manual exposure\n"
		"  2 disable histories (r_historyQuarantine 8)\n"
		"  3 inspect opaque SceneHDR contrast\n"
		"  4 if flat before post → lighting ownership / lightmap sample\n"
		"  5 if contrast OK before exposure → exposure / tonemap\n" );
}

static void NegativeGhost_Sequence_f( void )
{
	ri.Printf( PRINT_ALL,
		"=== negative_ghost_sequence ===\n"
		"  FRAME 0-30 static → 31-60 pan → 61 stop → 62-120 static → 121 cut\n"
		"  Offset silhouette after stop ⇒ STALE_*_HISTORY or upsample halo\n"
		"  r_negativeGhostSequenceDebug=%d\n",
		r_negativeGhostSequenceDebug ? r_negativeGhostSequenceDebug->integer : 0 );
}

static void NegativeGhost_Status_f( void )
{
	GhostLighting_Status_f();
}

static void HistoryQuarantine_f( void )
{
	int mode = r_historyQuarantine ? r_historyQuarantine->integer : 0;
	GhostLighting_ApplyQuarantine( mode );
	ri.Printf( PRINT_ALL, "r_historyQuarantine %d applied (see ghost_lighting_status)\n", mode );
}

void vk_ghost_lighting_register( void )
{
	static qboolean commandsRegistered = qfalse;

	r_negativeGhostDebug = ri.Cvar_Get( "r_negativeGhostDebug", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_negativeGhostDebug, "0", "8", CV_INTEGER );
	ri.Cvar_SetDescription( r_negativeGhostDebug,
		"Negative ghost debug: 0=off 1=diff 2=AO 3=GI 4=vol 5=shadow 6=velocity 7=disocclusion 8=owner" );

	r_fullbrightDebug = ri.Cvar_Get( "r_fullbrightDebug", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_fullbrightDebug, "0", "10", CV_INTEGER );
	ri.Cvar_SetDescription( r_fullbrightDebug,
		"Fullbright debug: 1=base 2=owner 3=direct 4=indirect 5=lightmap 6=shadow 7=emissive 8=HDR 9=HDR-base 10=exposure" );

	r_historyQuarantine = ri.Cvar_Get( "r_historyQuarantine", "0", CVAR_TEMP );
	ri.Cvar_CheckRange( r_historyQuarantine, "0", "8", CV_INTEGER );
	ri.Cvar_SetDescription( r_historyQuarantine,
		"0=configured 1=noTAA 2=noAO 3=noGI 4=noSSR 5=noVol 6=noShadowHist 7=noWeaponHist 8=all histories off" );

	r_negativeGhostSequenceDebug = ri.Cvar_Get( "r_negativeGhostSequenceDebug", "0", CVAR_TEMP );
	r_negativeGhostVelocityDebug = ri.Cvar_Get( "r_negativeGhostVelocityDebug", "0", CVAR_TEMP );
	r_fullbrightExposureDebug = ri.Cvar_Get( "r_fullbrightExposureDebug", "0", CVAR_TEMP );

	if ( commandsRegistered ) {
		return;
	}
	commandsRegistered = qtrue;
	ri.Cmd_AddCommand( "ghost_lighting_status", GhostLighting_Status_f );
	ri.Cmd_AddCommand( "ghost_lighting_capture", GhostLighting_Capture_f );
	ri.Cmd_AddCommand( "ghost_lighting_bisect", GhostLighting_Bisect_f );
	ri.Cmd_AddCommand( "fullbright_status", Fullbright_Status_f );
	ri.Cmd_AddCommand( "fullbright_capture", Fullbright_Capture_f );
	ri.Cmd_AddCommand( "fullbright_bisect", Fullbright_Bisect_f );
	ri.Cmd_AddCommand( "negative_ghost_sequence", NegativeGhost_Sequence_f );
	ri.Cmd_AddCommand( "negative_ghost_status", NegativeGhost_Status_f );
	ri.Cmd_AddCommand( "history_quarantine_apply", HistoryQuarantine_f );
}

