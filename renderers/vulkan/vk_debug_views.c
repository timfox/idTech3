/*
===========================================================================
Debug View System for SceneHDR/Depth/Composite Investigation
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_debug_views.h"

// Debug view cvars
cvar_t *r_debugDepth;
cvar_t *r_debugSkyMask;
cvar_t *r_debugSceneHDR;
cvar_t *r_debugSceneHDR_Sky;
cvar_t *r_debugSceneHDR_Particles;
cvar_t *r_debugSceneHDR_OIT;
cvar_t *r_debugFogRadiance;
cvar_t *r_debugFogTransmittance;
cvar_t *r_debugTemporalInput;
cvar_t *r_debugHistoryColor;
cvar_t *r_debugPreviousDepth;
cvar_t *r_debugReactiveMask;
cvar_t *r_debugTemporalResolved;
cvar_t *r_debugPreTonemapHDR;
cvar_t *r_debugFinalLDR;

// Debug view state
static vkDebugViewState_t s_debugViewState;

// Console command to show current debug view state
static void VK_DebugViews_Status_f( void )
{
    const char *modeNames[] = {
        "NONE",
        "DEPTH",
        "SKY_MASK",
        "SCENEHDR_RAW",
        "SCENEHDR_SKY",
        "SCENEHDR_PARTICLES",
        "SCENEHDR_OIT",
        "FOG_RADIANCE",
        "FOG_TRANSMITTANCE",
        "TEMPORAL_INPUT",
        "HISTORY_COLOR",
        "PREVIOUS_DEPTH",
        "REACTIVE_MASK",
        "TEMPORAL_RESOLVED",
        "PRE_TONEMAP_HDR",
        "FINAL_LDR"
    };

    ri.Printf( PRINT_ALL, "======== Debug View Status ========\n" );
    ri.Printf( PRINT_ALL, "Current mode: %s\n", 
               s_debugViewState.currentMode < DEBUG_VIEW_COUNT ? 
               modeNames[s_debugViewState.currentMode] : "UNKNOWN" );
    ri.Printf( PRINT_ALL, "Enabled: %s\n", s_debugViewState.enabled ? "yes" : "no" );
    
    ri.Printf( PRINT_ALL, "\nDebug View Cvars:\n" );
    ri.Printf( PRINT_ALL, "  r_debugDepth: %d\n", r_debugDepth ? r_debugDepth->integer : 0 );
    ri.Printf( PRINT_ALL, "  r_debugSkyMask: %d\n", r_debugSkyMask ? r_debugSkyMask->integer : 0 );
    ri.Printf( PRINT_ALL, "  r_debugSceneHDR: %d\n", r_debugSceneHDR ? r_debugSceneHDR->integer : 0 );
    ri.Printf( PRINT_ALL, "  r_debugSceneHDR_Sky: %d\n", r_debugSceneHDR_Sky ? r_debugSceneHDR_Sky->integer : 0 );
    ri.Printf( PRINT_ALL, "  r_debugSceneHDR_Particles: %d\n", r_debugSceneHDR_Particles ? r_debugSceneHDR_Particles->integer : 0 );
    ri.Printf( PRINT_ALL, "  r_debugSceneHDR_OIT: %d\n", r_debugSceneHDR_OIT ? r_debugSceneHDR_OIT->integer : 0 );
    ri.Printf( PRINT_ALL, "  r_debugFogRadiance: %d\n", r_debugFogRadiance ? r_debugFogRadiance->integer : 0 );
    ri.Printf( PRINT_ALL, "  r_debugFogTransmittance: %d\n", r_debugFogTransmittance ? r_debugFogTransmittance->integer : 0 );
    ri.Printf( PRINT_ALL, "  r_debugTemporalInput: %d\n", r_debugTemporalInput ? r_debugTemporalInput->integer : 0 );
    ri.Printf( PRINT_ALL, "  r_debugHistoryColor: %d\n", r_debugHistoryColor ? r_debugHistoryColor->integer : 0 );
    ri.Printf( PRINT_ALL, "  r_debugPreviousDepth: %d\n", r_debugPreviousDepth ? r_debugPreviousDepth->integer : 0 );
    ri.Printf( PRINT_ALL, "  r_debugReactiveMask: %d\n", r_debugReactiveMask ? r_debugReactiveMask->integer : 0 );
    ri.Printf( PRINT_ALL, "  r_debugTemporalResolved: %d\n", r_debugTemporalResolved ? r_debugTemporalResolved->integer : 0 );
    ri.Printf( PRINT_ALL, "  r_debugPreTonemapHDR: %d\n", r_debugPreTonemapHDR ? r_debugPreTonemapHDR->integer : 0 );
    ri.Printf( PRINT_ALL, "  r_debugFinalLDR: %d\n", r_debugFinalLDR ? r_debugFinalLDR->integer : 0 );
}

// Initialize debug view system
void vk_debug_views_init( void )
{
    // Initialize debug view cvars
    r_debugDepth = ri.Cvar_Get( "r_debugDepth", "0", CVAR_CHEAT );
    ri.Cvar_CheckRange( r_debugDepth, "0", "1", CV_INTEGER );
    ri.Cvar_SetDescription( r_debugDepth, "Debug view: depth buffer" );
    ri.Cvar_SetGroup( r_debugDepth, CVG_RENDERER );

    r_debugSkyMask = ri.Cvar_Get( "r_debugSkyMask", "0", CVAR_CHEAT );
    ri.Cvar_CheckRange( r_debugSkyMask, "0", "1", CV_INTEGER );
    ri.Cvar_SetDescription( r_debugSkyMask, "Debug view: sky mask" );
    ri.Cvar_SetGroup( r_debugSkyMask, CVG_RENDERER );

    r_debugSceneHDR = ri.Cvar_Get( "r_debugSceneHDR", "0", CVAR_CHEAT );
    ri.Cvar_CheckRange( r_debugSceneHDR, "0", "1", CV_INTEGER );
    ri.Cvar_SetDescription( r_debugSceneHDR, "Debug view: raw opaque SceneHDR" );
    ri.Cvar_SetGroup( r_debugSceneHDR, CVG_RENDERER );

    r_debugSceneHDR_Sky = ri.Cvar_Get( "r_debugSceneHDR_Sky", "0", CVAR_CHEAT );
    ri.Cvar_CheckRange( r_debugSceneHDR_Sky, "0", "1", CV_INTEGER );
    ri.Cvar_SetDescription( r_debugSceneHDR_Sky, "Debug view: SceneHDR after sky" );
    ri.Cvar_SetGroup( r_debugSceneHDR_Sky, CVG_RENDERER );

    r_debugSceneHDR_Particles = ri.Cvar_Get( "r_debugSceneHDR_Particles", "0", CVAR_CHEAT );
    ri.Cvar_CheckRange( r_debugSceneHDR_Particles, "0", "1", CV_INTEGER );
    ri.Cvar_SetDescription( r_debugSceneHDR_Particles, "Debug view: SceneHDR after particles" );
    ri.Cvar_SetGroup( r_debugSceneHDR_Particles, CVG_RENDERER );

    r_debugSceneHDR_OIT = ri.Cvar_Get( "r_debugSceneHDR_OIT", "0", CVAR_CHEAT );
    ri.Cvar_CheckRange( r_debugSceneHDR_OIT, "0", "1", CV_INTEGER );
    ri.Cvar_SetDescription( r_debugSceneHDR_OIT, "Debug view: SceneHDR after OIT" );
    ri.Cvar_SetGroup( r_debugSceneHDR_OIT, CVG_RENDERER );

    r_debugFogRadiance = ri.Cvar_Get( "r_debugFogRadiance", "0", CVAR_CHEAT );
    ri.Cvar_CheckRange( r_debugFogRadiance, "0", "1", CV_INTEGER );
    ri.Cvar_SetDescription( r_debugFogRadiance, "Debug view: fog radiance" );
    ri.Cvar_SetGroup( r_debugFogRadiance, CVG_RENDERER );

    r_debugFogTransmittance = ri.Cvar_Get( "r_debugFogTransmittance", "0", CVAR_CHEAT );
    ri.Cvar_CheckRange( r_debugFogTransmittance, "0", "1", CV_INTEGER );
    ri.Cvar_SetDescription( r_debugFogTransmittance, "Debug view: fog transmittance" );
    ri.Cvar_SetGroup( r_debugFogTransmittance, CVG_RENDERER );

    r_debugTemporalInput = ri.Cvar_Get( "r_debugTemporalInput", "0", CVAR_CHEAT );
    ri.Cvar_CheckRange( r_debugTemporalInput, "0", "1", CV_INTEGER );
    ri.Cvar_SetDescription( r_debugTemporalInput, "Debug view: current temporal input" );
    ri.Cvar_SetGroup( r_debugTemporalInput, CVG_RENDERER );

    r_debugHistoryColor = ri.Cvar_Get( "r_debugHistoryColor", "0", CVAR_CHEAT );
    ri.Cvar_CheckRange( r_debugHistoryColor, "0", "1", CV_INTEGER );
    ri.Cvar_SetDescription( r_debugHistoryColor, "Debug view: history color" );
    ri.Cvar_SetGroup( r_debugHistoryColor, CVG_RENDERER );

    r_debugPreviousDepth = ri.Cvar_Get( "r_debugPreviousDepth", "0", CVAR_CHEAT );
    ri.Cvar_CheckRange( r_debugPreviousDepth, "0", "1", CV_INTEGER );
    ri.Cvar_SetDescription( r_debugPreviousDepth, "Debug view: previous depth" );
    ri.Cvar_SetGroup( r_debugPreviousDepth, CVG_RENDERER );

    r_debugReactiveMask = ri.Cvar_Get( "r_debugReactiveMask", "0", CVAR_CHEAT );
    ri.Cvar_CheckRange( r_debugReactiveMask, "0", "1", CV_INTEGER );
    ri.Cvar_SetDescription( r_debugReactiveMask, "Debug view: reactive mask" );
    ri.Cvar_SetGroup( r_debugReactiveMask, CVG_RENDERER );

    r_debugTemporalResolved = ri.Cvar_Get( "r_debugTemporalResolved", "0", CVAR_CHEAT );
    ri.Cvar_CheckRange( r_debugTemporalResolved, "0", "1", CV_INTEGER );
    ri.Cvar_SetDescription( r_debugTemporalResolved, "Debug view: temporal resolved SceneHDR" );
    ri.Cvar_SetGroup( r_debugTemporalResolved, CVG_RENDERER );

    r_debugPreTonemapHDR = ri.Cvar_Get( "r_debugPreTonemapHDR", "0", CVAR_CHEAT );
    ri.Cvar_CheckRange( r_debugPreTonemapHDR, "0", "1", CV_INTEGER );
    ri.Cvar_SetDescription( r_debugPreTonemapHDR, "Debug view: pre-tonemap HDR" );
    ri.Cvar_SetGroup( r_debugPreTonemapHDR, CVG_RENDERER );

    r_debugFinalLDR = ri.Cvar_Get( "r_debugFinalLDR", "0", CVAR_CHEAT );
    ri.Cvar_CheckRange( r_debugFinalLDR, "0", "1", CV_INTEGER );
    ri.Cvar_SetDescription( r_debugFinalLDR, "Debug view: final LDR" );
    ri.Cvar_SetGroup( r_debugFinalLDR, CVG_RENDERER );

    // Register console command
    ri.Cmd_AddCommand( "debug_views_status", VK_DebugViews_Status_f );

    // Initialize debug view state
    s_debugViewState.currentMode = DEBUG_VIEW_NONE;
    s_debugViewState.enabled = qfalse;
    s_debugViewState.debugView = VK_NULL_HANDLE;
    s_debugViewState.debugRenderPass = VK_NULL_HANDLE;
    s_debugViewState.debugFramebuffer = VK_NULL_HANDLE;

    ri.Printf( PRINT_ALL, "[VK][debug] Debug view system initialized\n" );
}

// Shutdown debug view system
void vk_debug_views_shutdown( void )
{
    // Clear debug view state
    s_debugViewState.currentMode = DEBUG_VIEW_NONE;
    s_debugViewState.enabled = qfalse;
    s_debugViewState.debugView = VK_NULL_HANDLE;
    s_debugViewState.debugRenderPass = VK_NULL_HANDLE;
    s_debugViewState.debugFramebuffer = VK_NULL_HANDLE;

    ri.Printf( PRINT_ALL, "[VK][debug] Debug view system shutdown\n" );
}

// Begin frame for debug views
void vk_debug_views_begin_frame( void )
{
    // Check which debug view is enabled and set the mode
    if ( r_debugDepth && r_debugDepth->integer ) {
        s_debugViewState.currentMode = DEBUG_VIEW_DEPTH;
        s_debugViewState.enabled = qtrue;
    } else if ( r_debugSkyMask && r_debugSkyMask->integer ) {
        s_debugViewState.currentMode = DEBUG_VIEW_SKY_MASK;
        s_debugViewState.enabled = qtrue;
    } else if ( r_debugSceneHDR && r_debugSceneHDR->integer ) {
        s_debugViewState.currentMode = DEBUG_VIEW_SCENEHDR_RAW;
        s_debugViewState.enabled = qtrue;
    } else if ( r_debugSceneHDR_Sky && r_debugSceneHDR_Sky->integer ) {
        s_debugViewState.currentMode = DEBUG_VIEW_SCENEHDR_SKY;
        s_debugViewState.enabled = qtrue;
    } else if ( r_debugSceneHDR_Particles && r_debugSceneHDR_Particles->integer ) {
        s_debugViewState.currentMode = DEBUG_VIEW_SCENEHDR_PARTICLES;
        s_debugViewState.enabled = qtrue;
    } else if ( r_debugSceneHDR_OIT && r_debugSceneHDR_OIT->integer ) {
        s_debugViewState.currentMode = DEBUG_VIEW_SCENEHDR_OIT;
        s_debugViewState.enabled = qtrue;
    } else if ( r_debugFogRadiance && r_debugFogRadiance->integer ) {
        s_debugViewState.currentMode = DEBUG_VIEW_FOG_RADIANCE;
        s_debugViewState.enabled = qtrue;
    } else if ( r_debugFogTransmittance && r_debugFogTransmittance->integer ) {
        s_debugViewState.currentMode = DEBUG_VIEW_FOG_TRANSMITTANCE;
        s_debugViewState.enabled = qtrue;
    } else if ( r_debugTemporalInput && r_debugTemporalInput->integer ) {
        s_debugViewState.currentMode = DEBUG_VIEW_TEMPORAL_INPUT;
        s_debugViewState.enabled = qtrue;
    } else if ( r_debugHistoryColor && r_debugHistoryColor->integer ) {
        s_debugViewState.currentMode = DEBUG_VIEW_HISTORY_COLOR;
        s_debugViewState.enabled = qtrue;
    } else if ( r_debugPreviousDepth && r_debugPreviousDepth->integer ) {
        s_debugViewState.currentMode = DEBUG_VIEW_PREVIOUS_DEPTH;
        s_debugViewState.enabled = qtrue;
    } else if ( r_debugReactiveMask && r_debugReactiveMask->integer ) {
        s_debugViewState.currentMode = DEBUG_VIEW_REACTIVE_MASK;
        s_debugViewState.enabled = qtrue;
    } else if ( r_debugTemporalResolved && r_debugTemporalResolved->integer ) {
        s_debugViewState.currentMode = DEBUG_VIEW_TEMPORAL_RESOLVED;
        s_debugViewState.enabled = qtrue;
    } else if ( r_debugPreTonemapHDR && r_debugPreTonemapHDR->integer ) {
        s_debugViewState.currentMode = DEBUG_VIEW_PRE_TONEMAP_HDR;
        s_debugViewState.enabled = qtrue;
    } else if ( r_debugFinalLDR && r_debugFinalLDR->integer ) {
        s_debugViewState.currentMode = DEBUG_VIEW_FINAL_LDR;
        s_debugViewState.enabled = qtrue;
    } else {
        s_debugViewState.currentMode = DEBUG_VIEW_NONE;
        s_debugViewState.enabled = qfalse;
    }
}

// Record debug view pass
void vk_debug_views_record_pass( void )
{
    if ( !s_debugViewState.enabled || s_debugViewState.currentMode == DEBUG_VIEW_NONE ) {
        return;
    }

    // TODO: Implement debug view pass recording
    // This will depend on the specific debug view mode
    // For now, we'll just log the current mode
    ri.Printf( PRINT_DEVELOPER, "[VK][debug] Recording debug view pass: %d\n", s_debugViewState.currentMode );
}

// Get current debug view mode
vkDebugViewMode_t vk_debug_views_get_mode( void )
{
    return s_debugViewState.currentMode;
}

// Set debug view mode
void vk_debug_views_set_mode( vkDebugViewMode_t mode )
{
    if ( mode < DEBUG_VIEW_COUNT ) {
        s_debugViewState.currentMode = mode;
        s_debugViewState.enabled = ( mode != DEBUG_VIEW_NONE );
    }
}

// Check if debug view is enabled
qboolean vk_debug_views_is_enabled( void )
{
    return s_debugViewState.enabled;
}

// Get debug view image view
VkImageView vk_debug_views_get_image_view( void )
{
    // TODO: Return the appropriate image view based on current debug mode
    // This will be implemented when we have the actual debug view implementations
    return VK_NULL_HANDLE;
}
"