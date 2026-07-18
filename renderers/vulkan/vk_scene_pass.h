#pragma once

#include "../common/tr_types.h"
#include "../common/vulkan/vulkan.h"

void vk_reset_scene_src_rect_tracking( void );
void vk_get_active_render_extent( uint32_t *width, uint32_t *height );
void vk_resume_current_render_pass( void );
void vk_resume_main_render_pass( void );

/* Pass ownership tracing (Phase 1 renderer modernization). */
void vk_pass_diag_reset( void );
void vk_pass_diag_begin( const char *passName, uint32_t width, uint32_t height );
void vk_pass_diag_end( const char *passName );
void vk_pass_diag_stage( const char *stageName );
void vk_pass_diag_resume( const char *targetName, qboolean selfHeal );
void vk_assert_ui_pass_consistency( const char *where );
void vk_report_device_lost_context( const char *where );
void vk_fatal_device_lost( const char *where, VkResult res );
