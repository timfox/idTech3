#pragma once

/* Vulkan sync primitives: semaphores and fences for swapchain and upload queue.
 * Create/destroy called from vk_initialize and vk_restart_swapchain.
 */

void vk_create_sync_primitives( void );
void vk_destroy_sync_primitives( void );
