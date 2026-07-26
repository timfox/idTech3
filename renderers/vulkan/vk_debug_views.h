/*
===========================================================================
Debug View System for SceneHDR/Depth/Composite Investigation
===========================================================================
*/

#ifndef VK_DEBUG_VIEWS_H
#define VK_DEBUG_VIEWS_H

#include "vk.h"

// Debug view cvars
extern cvar_t *r_debugDepth;
extern cvar_t *r_debugSkyMask;
extern cvar_t *r_debugSceneHDR;
extern cvar_t *r_debugSceneHDR_Sky;
extern cvar_t *r_debugSceneHDR_Particles;
extern cvar_t *r_debugSceneHDR_OIT;
extern cvar_t *r_debugFogRadiance;
extern cvar_t *r_debugFogTransmittance;
extern cvar_t *r_debugTemporalInput;
extern cvar_t *r_debugHistoryColor;
extern cvar_t *r_debugPreviousDepth;
extern cvar_t *r_debugReactiveMask;
extern cvar_t *r_debugTemporalResolved;
extern cvar_t *r_debugPreTonemapHDR;
extern cvar_t *r_debugFinalLDR;

// Debug view modes
typedef enum {
    DEBUG_VIEW_NONE = 0,
    DEBUG_VIEW_DEPTH,
    DEBUG_VIEW_SKY_MASK,
    DEBUG_VIEW_SCENEHDR_RAW,
    DEBUG_VIEW_SCENEHDR_SKY,
    DEBUG_VIEW_SCENEHDR_PARTICLES,
    DEBUG_VIEW_SCENEHDR_OIT,
    DEBUG_VIEW_FOG_RADIANCE,
    DEBUG_VIEW_FOG_TRANSMITTANCE,
    DEBUG_VIEW_TEMPORAL_INPUT,
    DEBUG_VIEW_HISTORY_COLOR,
    DEBUG_VIEW_PREVIOUS_DEPTH,
    DEBUG_VIEW_REACTIVE_MASK,
    DEBUG_VIEW_TEMPORAL_RESOLVED,
    DEBUG_VIEW_PRE_TONEMAP_HDR,
    DEBUG_VIEW_FINAL_LDR,
    DEBUG_VIEW_COUNT
} vkDebugViewMode_t;

// Debug view state
typedef struct {
    vkDebugViewMode_t currentMode;
    qboolean enabled;
    VkImageView debugView;
    VkRenderPass debugRenderPass;
    VkFramebuffer debugFramebuffer;
} vkDebugViewState_t;

// Initialize debug view system
void vk_debug_views_init( void );

// Shutdown debug view system
void vk_debug_views_shutdown( void );

// Begin frame for debug views
void vk_debug_views_begin_frame( void );

// Record debug view pass
void vk_debug_views_record_pass( void );

// Get current debug view mode
vkDebugViewMode_t vk_debug_views_get_mode( void );

// Set debug view mode
void vk_debug_views_set_mode( vkDebugViewMode_t mode );

// Check if debug view is enabled
qboolean vk_debug_views_is_enabled( void );

// Get debug view image view
VkImageView vk_debug_views_get_image_view( void );

#endif // VK_DEBUG_VIEWS_H