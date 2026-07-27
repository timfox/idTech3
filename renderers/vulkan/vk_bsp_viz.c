#include "tr_local.h"
#include "vk_bsp_viz.h"

#ifdef USE_VULKAN

/*
 * BSP visualization consumes the production visibility result (MarkLeaves +
 * RecursiveWorldNode + R_AddWorldSurface). It must not independently draw the
 * entire BSP tree in visible-only modes.
 */

static cvar_t *r_bspViz;
static cvar_t *r_bspVizThroughWalls;
static cvar_t *r_bspVizCullReasons;
static cvar_t *r_bspVisibilityMode;

static bspVisibilityFrame_t s_frame;
static bspVisibilityFrame_t s_prev;
static uint32_t s_mapGeneration;
static uint64_t s_frameCounter;
static uint32_t s_visGeneration;

static void BspViz_Status_f( void )
{
	const bspVisibilityFrame_t *f = &s_frame;
	ri.Printf( PRINT_ALL,
		"=== bsp_viz_status ===\n"
		"  mode=%d throughWalls=%d visibilityMode=%d cullReasons=%d\n"
		"  frame=%llu gen=%u mapGen=%u stale=%d\n"
		"  viewLeaf=%d viewCluster=%d viewArea=%d novis=%d\n"
			"  visibleLeaves=%u acceptedSurfaces=%u submitted=%u sky=%u nonSky=%u\n"
		"  duplicates=%u backface=%u frustumReject=%u\n"
		"  drawListSource=production_R_AddWorldSurfaces\n"
		"  depthPolicy=%s\n"
		"  compose=RB_EndSurface DrawTris overlay (no bloom/TAA/OIT)\n",
		r_bspViz ? r_bspViz->integer : 0,
		r_bspVizThroughWalls ? r_bspVizThroughWalls->integer : 0,
		r_bspVisibilityMode ? r_bspVisibilityMode->integer : 0,
		r_bspVizCullReasons ? r_bspVizCullReasons->integer : 0,
		(unsigned long long)f->frameNumber,
		f->generation,
		f->mapGeneration,
		f->stale ? 1 : 0,
		f->viewLeaf,
		f->viewCluster,
		f->viewArea,
		f->novisActive ? 1 : 0,
			f->visibleLeafCount,
			f->visibleSurfaceCount,
			f->submittedSurfaceCount,
			f->submittedSkySurfaceCount,
			f->submittedNonSkySurfaceCount,
		f->duplicateRejects,
		f->backfaceRejects,
		f->frustumRejects,
		vk_bsp_viz_want_through_walls() ?
			"DEPTH_RANGE_ZERO (through-walls developer)" :
			"DEPTH_RANGE_NORMAL GREATER_OR_EQUAL write=off" );
}

static void BspViz_Validate_f( void )
{
	const bspVisibilityFrame_t *f = &s_frame;
	int issues = 0;

	ri.Printf( PRINT_ALL, "=== bsp_viz_validate ===\n" );
	if ( f->stale ) {
		ri.Printf( PRINT_ALL, "  FAIL: STALE_VIEW_CLUSTER / frame identity\n" );
		issues++;
	}
	if ( f->novisActive && r_bspViz && r_bspViz->integer == 1 ) {
		ri.Printf( PRINT_ALL,
			"  WARN: r_novis active — mode 1 still shows production set which is all leaves\n"
			"        use r_bspViz 4 for explicit draw-all through walls\n" );
	}
	if ( f->visibleLeafCount == 0 && tr.world && !( tr.refdef.rdflags & RDF_NOWORLDMODEL ) ) {
		ri.Printf( PRINT_ALL, "  WARN: zero visible leaves (empty PVS or no world)\n" );
	}
	if ( f->submittedSurfaceCount > 0 &&
		f->visibleSurfaceCount > 0 &&
		f->submittedSurfaceCount > f->visibleSurfaceCount * 8u ) {
		ri.Printf( PRINT_ALL, "  WARN: BSP_VISIBILITY_COLLAPSED_TO_DRAW_ALL heuristic\n" );
		issues++;
	}
	if ( issues == 0 ) {
		ri.Printf( PRINT_ALL, "  OK: visualization bound to current production visibility frame\n" );
	}
}

static void BspViz_View_f( void )
{
	ri.Printf( PRINT_ALL,
		"=== bsp_view_status ===\n"
		"  viewLeaf=%d cluster=%d area=%d\n"
		"  prevLeaf=%d prevCluster=%d\n"
		"  clusterChanged=%d\n"
		"  pvsOrigin=(%.1f %.1f %.1f)\n",
		s_frame.viewLeaf, s_frame.viewCluster, s_frame.viewArea,
		s_prev.viewLeaf, s_prev.viewCluster,
		( s_frame.viewCluster != s_prev.viewCluster ) ? 1 : 0,
		tr.viewParms.pvsOrigin[0], tr.viewParms.pvsOrigin[1], tr.viewParms.pvsOrigin[2] );
}

static void BspViz_CullReport_f( void )
{
	ri.Printf( PRINT_ALL,
		"=== bsp_viz_cull_report ===\n"
		"  leavesAccepted=%u frustumRejected=%u\n"
		"  surfacesAccepted=%u duplicates=%u backfaces=%u\n",
		s_frame.visibleLeafCount, s_frame.frustumRejects,
		s_frame.visibleSurfaceCount, s_frame.duplicateRejects, s_frame.backfaceRejects );
}

static void BspViz_Stats_f( void )
{
	BspViz_Status_f();
	BspViz_CullReport_f();
}

void vk_bsp_viz_register( void )
{
	r_bspViz = ri.Cvar_Get( "r_bspViz", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_bspViz, "0", "4", CV_INTEGER );
	ri.Cvar_SetDescription( r_bspViz,
		"BSP visualization (authoritative visibility):\n"
		" 0 off\n"
		" 1 visible production surfaces only (depth-tested)\n"
		" 2 PVS-visible including backfaces (uses production list + face cull off via r_facePlaneCull)\n"
		" 3 current-cluster surfaces (production list)\n"
		" 4 ALL BSP surfaces through walls — explicit developer mode" );
	r_bspVizThroughWalls = ri.Cvar_Get( "r_bspVizThroughWalls", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_bspVizThroughWalls, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_bspVizThroughWalls,
		"Force through-wall BSP/showtris overlay (DEPTH_RANGE_ZERO). Default 0." );
	r_bspVizCullReasons = ri.Cvar_Get( "r_bspVizCullReasons", "0", CVAR_TEMP );
	r_bspVisibilityMode = ri.Cvar_Get( "r_bspVisibilityMode", "0", CVAR_CHEAT );
	ri.Cvar_CheckRange( r_bspVisibilityMode, "0", "2", CV_INTEGER );
	ri.Cvar_SetDescription( r_bspVisibilityMode,
		"0=production 1=CPU reference (stats only) 2=compare (logs mismatches)." );

	ri.Cmd_AddCommand( "bsp_viz_status", BspViz_Status_f );
	ri.Cmd_AddCommand( "bsp_viz_validate", BspViz_Validate_f );
	ri.Cmd_AddCommand( "bsp_viz_depth_status", BspViz_Status_f );
	ri.Cmd_AddCommand( "bsp_viz_depth_validate", BspViz_Validate_f );
	ri.Cmd_AddCommand( "bsp_view_status", BspViz_View_f );
	ri.Cmd_AddCommand( "bsp_view_validate", BspViz_View_f );
	ri.Cmd_AddCommand( "bsp_viz_cull_report", BspViz_CullReport_f );
	ri.Cmd_AddCommand( "bsp_visibility_stats", BspViz_Stats_f );
	ri.Cmd_AddCommand( "bsp_pvs_status", BspViz_View_f );
}

void vk_bsp_viz_begin_frame( void )
{
	s_prev = s_frame;
	Com_Memset( &s_frame, 0, sizeof( s_frame ) );
	s_frameCounter++;
	s_frame.frameNumber = s_frameCounter;
	s_frame.mapGeneration = s_mapGeneration;
	s_frame.generation = s_visGeneration;
	s_frame.viewLeaf = -1;
	s_frame.viewCluster = -1;
	s_frame.viewArea = -1;
}

void vk_bsp_viz_on_map_change( void )
{
	s_mapGeneration++;
	s_visGeneration++;
	Com_Memset( &s_frame, 0, sizeof( s_frame ) );
	Com_Memset( &s_prev, 0, sizeof( s_prev ) );
	s_frame.mapGeneration = s_mapGeneration;
	s_frame.generation = s_visGeneration;
	s_frame.viewLeaf = -1;
	s_frame.viewCluster = -1;
	s_frame.viewArea = -1;
	s_frame.stale = qtrue;
}

void vk_bsp_viz_note_mark_leaves( int32_t viewLeaf, int32_t viewCluster, int32_t viewArea,
	qboolean novisFallback )
{
	if ( viewCluster != s_prev.viewCluster || novisFallback != s_prev.novisActive ) {
		s_visGeneration++;
	}
	s_frame.viewLeaf = viewLeaf;
	s_frame.viewCluster = viewCluster;
	s_frame.viewArea = viewArea;
	s_frame.novisActive = novisFallback;
	s_frame.pvsGeneration = s_visGeneration;
	s_frame.areaMaskGeneration = tr.refdef.areamaskModified ? ( s_visGeneration + 1u ) : s_visGeneration;
	s_frame.frustumGeneration = s_visGeneration;
	s_frame.generation = s_visGeneration;
	s_frame.stale = qfalse;
}

void vk_bsp_viz_note_leaf_accepted( void )
{
	s_frame.visibleLeafCount++;
}

void vk_bsp_viz_note_surface_accepted( void )
{
	s_frame.visibleSurfaceCount++;
	s_frame.submittedSurfaceCount++;
}

void vk_bsp_viz_note_surface_classified( qboolean isSky )
{
	if ( isSky ) {
		s_frame.submittedSkySurfaceCount++;
	} else {
		s_frame.submittedNonSkySurfaceCount++;
	}
}

void vk_bsp_viz_note_surface_duplicate( void )
{
	s_frame.duplicateRejects++;
}

void vk_bsp_viz_note_surface_backface( void )
{
	s_frame.backfaceRejects++;
}

void vk_bsp_viz_note_leaf_frustum_reject( void )
{
	s_frame.frustumRejects++;
}

void vk_bsp_viz_finalize_world( void )
{
	if ( s_frame.novisActive && r_bspViz && r_bspViz->integer == 1 &&
		s_frame.visibleLeafCount > 1000u ) {
		ri.Printf( PRINT_DEVELOPER,
			"[VK][bsp_viz] WARN: novis + mode1 with %u leaves — not a through-walls draw-all mode\n",
			s_frame.visibleLeafCount );
	}
}

qboolean vk_bsp_viz_want_visible_overlay( void )
{
	if ( !r_bspViz || r_bspViz->integer <= 0 ) {
		return qfalse;
	}
	if ( vk_bsp_viz_want_through_walls() ) {
		return qfalse;
	}
	return qtrue;
}

qboolean vk_bsp_viz_want_through_walls( void )
{
	if ( r_bspVizThroughWalls && r_bspVizThroughWalls->integer ) {
		return qtrue;
	}
	if ( r_bspViz && r_bspViz->integer >= 4 ) {
		return qtrue;
	}
	return qfalse;
}

int vk_bsp_viz_force_showtris_mode( void )
{
	if ( !r_bspViz || r_bspViz->integer <= 0 ) {
		return 0;
	}
	if ( vk_bsp_viz_want_through_walls() ) {
		return 2;
	}
	/* Modes 1–3: depth-tested overlay of the authoritative submitted set. */
	return 1;
}

int vk_bsp_viz_effective_showtris( void )
{
	const int forced = vk_bsp_viz_force_showtris_mode();
	const int base = ( r_showtris ) ? r_showtris->integer : 0;

	/*
	 * r_bspViz owns the overlay when enabled so through-wall policy cannot
	 * silently leak from a stale r_showtris 2 while mode 1 is requested.
	 * When r_bspViz is off, preserve legacy r_showtris behavior.
	 */
	if ( forced > 0 ) {
		return forced;
	}
	return base;
}

const bspVisibilityFrame_t *vk_bsp_viz_current_frame( void )
{
	return &s_frame;
}

#endif /* USE_VULKAN */
