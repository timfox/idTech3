#ifndef VK_RESOURCE_DESTROY_H
#define VK_RESOURCE_DESTROY_H

void vk_destroy_render_passes( void );
void vk_destroy_pipelines( qboolean resetCounter );
/* Destroy only shader-table pipelines at/after vk.pipelines_world_base (PBR world draw),
 * not post/gamma/smaa bloom etc. Use when a cvar only invalidates world graphics (e.g. r_forwardPlusShade). */
void vk_destroy_world_graphics_pipelines( void );

#endif
