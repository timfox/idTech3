#pragma once
#ifdef USE_VULKAN
void vk_oit_lab_register( void );
void vk_oit_lab_shutdown( void );
/* Schedule snapshot after successful WBOIT resolve (same command buffer). */
void vk_oit_lab_on_oit_resolved( void );
/* After rendering_finished_fence for cmdIndex — finalize snapshot and evaluate. */
void vk_oit_lab_finalize_frame( int cmdIndex );
/* True while cert fixtures should suppress world transparent draws. */
qboolean vk_oit_lab_isolate_world( void );
#endif
