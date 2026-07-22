/*
===========================================================================
Black-frame / SceneHDR composition diagnostics (cheat / developer).

Tracks the SceneHDR writer chain and draw ownership so regressions like
post-OIT G-buffer capture (all-black 3D, UI alive) are identifiable without
guessing lighting math.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	VK_BF_DRAW_OPAQUE = 0,
	VK_BF_DRAW_FORWARD_OPAQUE,
	VK_BF_DRAW_DEFERRED_GBUFFER,
	VK_BF_DRAW_TRANSPARENT,
	VK_BF_DRAW_OIT,
	VK_BF_DRAW_DEPTH_ONLY,
	VK_BF_DRAW_COUNT
} vkBlackFrameDrawKind_t;

void vk_black_frame_register( void );
void vk_black_frame_begin_frame( void );
void vk_black_frame_note_writer( const char *passName );
void vk_black_frame_note_draw( vkBlackFrameDrawKind_t kind, uint32_t count );
uint32_t vk_black_frame_draw_count( vkBlackFrameDrawKind_t kind );
void vk_black_frame_note_indices( uint32_t indices );
void vk_black_frame_check_before_ui( void );
qboolean vk_black_frame_force_minimal_scene( void );
int vk_black_frame_output_debug( void );
int vk_black_frame_force_pass_color( void ); /* 0=off, else pack: passId in low 8 */
uint32_t vk_black_frame_last_validate_fails( void );

#ifdef __cplusplus
}
#endif
