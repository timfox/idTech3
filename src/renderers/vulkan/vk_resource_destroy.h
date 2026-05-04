#ifndef VK_RESOURCE_DESTROY_H
#define VK_RESOURCE_DESTROY_H

void vk_destroy_render_passes( void );
void vk_destroy_pipelines( qboolean resetCounter );
/* Destroy VkPipeline handles for shader-table rows at/after vk.pipelines_world_base only.
 * Preserves slot indices and Vk_Pipeline_Def entries so cached shader_t/vk_pipeline IDs stay valid;
 * vk_gen_pipeline() lazily recreates handles. Do not shrink vk.pipelines_count (would desync indices). */
void vk_destroy_world_graphics_pipelines( void );

#endif
