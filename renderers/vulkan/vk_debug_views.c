/*
===========================================================================
Debug View System for SceneHDR/Depth/Composite Investigation
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_debug_views.h"

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

static vkDebugViewState_t s_debugViewState;

static const char *VK_DebugViews_ModeName( vkDebugViewMode_t mode )
{
    static const char *modeNames[] = {
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

    return ( mode >= DEBUG_VIEW_NONE && mode < DEBUG_VIEW_COUNT ) ? modeNames[mode] : "UNKNOWN";
}

static cvar_t *VK_DebugViews_CvarForMode( vkDebugViewMode_t mode )
{
    switch ( mode ) {
    case DEBUG_VIEW_DEPTH: return r_debugDepth;
    case DEBUG_VIEW_SKY_MASK: return r_debugSkyMask;
    case DEBUG_VIEW_SCENEHDR_RAW: return r_debugSceneHDR;
    case DEBUG_VIEW_SCENEHDR_SKY: return r_debugSceneHDR_Sky;
    case DEBUG_VIEW_SCENEHDR_PARTICLES: return r_debugSceneHDR_Particles;
    case DEBUG_VIEW_SCENEHDR_OIT: return r_debugSceneHDR_OIT;
    case DEBUG_VIEW_FOG_RADIANCE: return r_debugFogRadiance;
    case DEBUG_VIEW_FOG_TRANSMITTANCE: return r_debugFogTransmittance;
    case DEBUG_VIEW_TEMPORAL_INPUT: return r_debugTemporalInput;
    case DEBUG_VIEW_HISTORY_COLOR: return r_debugHistoryColor;
    case DEBUG_VIEW_PREVIOUS_DEPTH: return r_debugPreviousDepth;
    case DEBUG_VIEW_REACTIVE_MASK: return r_debugReactiveMask;
    case DEBUG_VIEW_TEMPORAL_RESOLVED: return r_debugTemporalResolved;
    case DEBUG_VIEW_PRE_TONEMAP_HDR: return r_debugPreTonemapHDR;
    case DEBUG_VIEW_FINAL_LDR: return r_debugFinalLDR;
    default: return NULL;
    }
}

static void VK_DebugViews_ClearCvars( void )
{
    vkDebugViewMode_t mode;

    for ( mode = DEBUG_VIEW_DEPTH; mode < DEBUG_VIEW_COUNT; mode++ ) {
        cvar_t *cv = VK_DebugViews_CvarForMode( mode );
        if ( cv ) {
            ri.Cvar_Set( cv->name, "0" );
        }
    }
}

static void VK_DebugViews_Status_f( void )
{
    ri.Printf( PRINT_ALL, "======== Debug View Status ========\n" );
    ri.Printf( PRINT_ALL, "Current mode: %s\n", VK_DebugViews_ModeName( s_debugViewState.currentMode ) );
    ri.Printf( PRINT_ALL, "Enabled: %s\n", s_debugViewState.enabled ? "yes" : "no" );
    ri.Printf( PRINT_ALL, "Image view: %s\n", vk_debug_views_get_image_view() != VK_NULL_HANDLE ? "ready" : "unavailable" );
    
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

static void VK_DebugViews_Set_f( void )
{
    int mode;
    cvar_t *cv;

    if ( ri.Cmd_Argc() < 2 ) {
        ri.Printf( PRINT_ALL, "usage: debug_view <0-%d|off>\n", DEBUG_VIEW_COUNT - 1 );
        VK_DebugViews_Status_f();
        return;
    }

    if ( !Q_stricmp( ri.Cmd_Argv( 1 ), "off" ) ) {
        mode = DEBUG_VIEW_NONE;
    } else {
        mode = atoi( ri.Cmd_Argv( 1 ) );
    }

    if ( mode < DEBUG_VIEW_NONE || mode >= DEBUG_VIEW_COUNT ) {
        ri.Printf( PRINT_WARNING, "debug_view: invalid mode %d\n", mode );
        return;
    }

    VK_DebugViews_ClearCvars();
    cv = VK_DebugViews_CvarForMode( (vkDebugViewMode_t)mode );
    if ( cv ) {
        ri.Cvar_Set( cv->name, "1" );
    }
    vk_debug_views_set_mode( (vkDebugViewMode_t)mode );
    ri.Printf( PRINT_ALL, "debug_view: %s\n", VK_DebugViews_ModeName( s_debugViewState.currentMode ) );
}

void vk_debug_views_init( void )
{
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

    ri.Cmd_AddCommand( "debug_views_status", VK_DebugViews_Status_f );
    ri.Cmd_AddCommand( "debug_view", VK_DebugViews_Set_f );

    s_debugViewState.currentMode = DEBUG_VIEW_NONE;
    s_debugViewState.enabled = qfalse;
    s_debugViewState.debugView = VK_NULL_HANDLE;
    s_debugViewState.debugRenderPass = VK_NULL_HANDLE;
    s_debugViewState.debugFramebuffer = VK_NULL_HANDLE;

    ri.Printf( PRINT_ALL, "[VK][debug] Debug view system initialized\n" );
}

void vk_debug_views_shutdown( void )
{
    ri.Cmd_RemoveCommand( "debug_view" );
    ri.Cmd_RemoveCommand( "debug_views_status" );

    s_debugViewState.currentMode = DEBUG_VIEW_NONE;
    s_debugViewState.enabled = qfalse;
    s_debugViewState.debugView = VK_NULL_HANDLE;
    s_debugViewState.debugRenderPass = VK_NULL_HANDLE;
    s_debugViewState.debugFramebuffer = VK_NULL_HANDLE;

    ri.Printf( PRINT_ALL, "[VK][debug] Debug view system shutdown\n" );
}

void vk_debug_views_begin_frame( void )
{
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

    s_debugViewState.debugView = vk_debug_views_get_image_view();
}

void vk_debug_views_record_pass( void )
{
    if ( !s_debugViewState.enabled || s_debugViewState.currentMode == DEBUG_VIEW_NONE ) {
        return;
    }

    ri.Printf( PRINT_DEVELOPER, "[VK][debug] active debug view: %s (%s)\n",
        VK_DebugViews_ModeName( s_debugViewState.currentMode ),
        s_debugViewState.debugView != VK_NULL_HANDLE ? "image-ready" : "image-unavailable" );
}

vkDebugViewMode_t vk_debug_views_get_mode( void )
{
    return s_debugViewState.currentMode;
}

void vk_debug_views_set_mode( vkDebugViewMode_t mode )
{
    if ( mode < DEBUG_VIEW_COUNT ) {
        s_debugViewState.currentMode = mode;
        s_debugViewState.enabled = ( mode != DEBUG_VIEW_NONE );
    }
}

qboolean vk_debug_views_is_enabled( void )
{
    return s_debugViewState.enabled;
}

VkImageView vk_debug_views_get_image_view( void )
{
    const uint32_t historyIndex = vk.temporal.taaHistoryIndex & 1u;
    const uint32_t prevHistoryIndex = ( historyIndex ^ 1u ) & 1u;
    const uint32_t prevDepthIndex = vk.temporal.prevDepthIndex & 1u;

    switch ( s_debugViewState.currentMode ) {
    case DEBUG_VIEW_DEPTH:
    case DEBUG_VIEW_SKY_MASK:
        return vk.depth_image_view_sample ? vk.depth_image_view_sample : vk.depth_image_view;
    case DEBUG_VIEW_SCENEHDR_RAW:
    case DEBUG_VIEW_SCENEHDR_SKY:
    case DEBUG_VIEW_SCENEHDR_PARTICLES:
    case DEBUG_VIEW_SCENEHDR_OIT:
    case DEBUG_VIEW_TEMPORAL_INPUT:
    case DEBUG_VIEW_PRE_TONEMAP_HDR:
        return vk.scene_post_fog_color_source ? vk.scene_post_fog_color_source : vk.color_image_view;
    case DEBUG_VIEW_FOG_RADIANCE:
    case DEBUG_VIEW_FOG_TRANSMITTANCE:
        return vk.fog_scene_image_view ? vk.fog_scene_image_view : vk.color_image_view;
    case DEBUG_VIEW_HISTORY_COLOR:
        return vk.taa_history_image_view[prevHistoryIndex] ? vk.taa_history_image_view[prevHistoryIndex] :
            vk.taa_history_image_view[historyIndex];
    case DEBUG_VIEW_PREVIOUS_DEPTH:
        return vk.temporal_prev_depth_view[prevDepthIndex];
    case DEBUG_VIEW_REACTIVE_MASK:
        return vk.reactive_mask_view ? vk.reactive_mask_view : vk.reactive_mask_stub_view;
    case DEBUG_VIEW_TEMPORAL_RESOLVED:
        return vk.taa_history_image_view[historyIndex] ? vk.taa_history_image_view[historyIndex] : vk.color_image_view;
    case DEBUG_VIEW_FINAL_LDR:
        return vk.post_fog_color_source ? vk.post_fog_color_source : vk.color_image_view;
    default:
        return VK_NULL_HANDLE;
    }
}
