/*
===========================================================================
Vulkan render graph core.

This is the frame-graph layer above the Spine registry. Spine pass scopes
observe the current renderer path; the graph compiles those observed passes
into resource-derived dependencies and a deterministic topological order.
===========================================================================
*/
#ifndef VK_RENDER_GRAPH_H
#define VK_RENDER_GRAPH_H

#include "vk_pass_registry.h"

typedef void ( *vkRenderGraphExecuteFn )( void *userData );

void vk_render_graph_init( void );
void vk_render_graph_shutdown( void );

void vk_render_graph_declare_pass( vkSpinePassId pass,
	const vkSpineResourceEdge *reads, int readCount,
	const vkSpineResourceEdge *writes, int writeCount );
void vk_render_graph_set_pass_executor( vkSpinePassId pass,
	vkRenderGraphExecuteFn execute, void *userData );

void vk_render_graph_begin_frame( void );
void vk_render_graph_import_resource( vkSpineResourceId res );
void vk_render_graph_observe_pass( vkSpinePassId pass );
qboolean vk_render_graph_compile( void );
qboolean vk_render_graph_execute( void );
void vk_render_graph_end_frame( void );

uint32_t vk_render_graph_observed_count( void );
uint32_t vk_render_graph_compiled_count( void );
uint32_t vk_render_graph_dependency_count( void );
uint32_t vk_render_graph_violation_count( void );
const char *vk_render_graph_last_error( void );
void vk_render_graph_status_f( void );

#endif /* VK_RENDER_GRAPH_H */
