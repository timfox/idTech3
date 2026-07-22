#include "tr_local.h"
#include "vk.h"
#include "vk_black_frame.h"
#include "vk_gpu_scene.h"
#include "vk_renderer_perf.h"
#include "vk_forward_plus.h"

#ifdef USE_VULKAN

static qboolean s_cmdsRegistered;

static void VK_RendererPerf_f( void )
{
	uint32_t candidates = 0, frustumRej = 0, hizRej = 0, draws = 0;

	ri.Printf( PRINT_ALL, "======== Renderer Perf ========\n" );
	ri.Printf( PRINT_ALL, "gpuScene active=%s driven=%s generation=%u\n",
		vk_gpu_scene_active() ? "yes" : "no",
		vk_gpu_scene_driven_active() ? "yes" : "no",
		vk_gpu_scene_generation() );
	ri.Printf( PRINT_ALL, "draws opaque=%u forwardOpaque=%u gbuffer=%u oit=%u\n",
		vk_black_frame_draw_count( VK_BF_DRAW_OPAQUE ),
		vk_black_frame_draw_count( VK_BF_DRAW_FORWARD_OPAQUE ),
		vk_black_frame_draw_count( VK_BF_DRAW_DEFERRED_GBUFFER ),
		vk_black_frame_draw_count( VK_BF_DRAW_OIT ) );
	vk_gpu_scene_telemetry( &candidates, &frustumRej, &hizRej, &draws );
	ri.Printf( PRINT_ALL, "cull: candidates=%u frustumRej=%u hizRej=%u draws=%u\n",
		candidates, frustumRej, hizRej, draws );
	ri.Printf( PRINT_ALL, "backend: surfaces=%i indexes=%i clusterGen=%u gbufferGen=%u\n",
		backEnd.pc.c_surfaces, backEnd.pc.c_indexes,
		vk.forward_plus.cluster_list_generation, vk.deferredGbufferGeneration );
	ri.Printf( PRINT_ALL, "extent: render=%ux%u mainColor=%ux%u\n",
		vk.renderWidth, vk.renderHeight, vk.mainColorWidth, vk.mainColorHeight );
}

static void VK_RendererMemory_f( void )
{
	uint32_t w = vk.mainColorWidth ? vk.mainColorWidth : vk.renderWidth;
	uint32_t h = vk.mainColorHeight ? vk.mainColorHeight : vk.renderHeight;
	uint32_t colorBytes = w * h * 8u;
	uint32_t depthBytes = w * h * 4u;
	uint32_t gbufferBytes = vk.deferredGbufferAllocated ? ( w * h * 16u ) : 0u;
	uint32_t sceneObjBytes = (uint32_t)sizeof( GpuSceneObject ) * 4096u;

	ri.Printf( PRINT_ALL, "======== Renderer Memory (estimate) ========\n" );
	ri.Printf( PRINT_ALL, "sceneHDR ~%.2f MiB depth ~%.2f MiB gbuffer ~%.2f MiB\n",
		(double)colorBytes / ( 1024.0 * 1024.0 ),
		(double)depthBytes / ( 1024.0 * 1024.0 ),
		(double)gbufferBytes / ( 1024.0 * 1024.0 ) );
	ri.Printf( PRINT_ALL, "GpuSceneObject[%u] sizeof=%zu budget ~%.2f MiB\n",
		4096u, sizeof( GpuSceneObject ),
		(double)sceneObjBytes / ( 1024.0 * 1024.0 ) );
	ri.Printf( PRINT_ALL, "staging=%zu geometry=%zu\n",
		(size_t)vk.staging_buffer.size, (size_t)vk.defaults.geometry_size );
	ri.Printf( PRINT_ALL,
		"budgets (targets): upload/cull/depth/gbuffer/deferred/fp/oit/weapon/bloom/tonemap — measure before optimizing\n" );
}

void vk_renderer_perf_register( void )
{
	if ( !s_cmdsRegistered ) {
		ri.Cmd_AddCommand( "renderer_perf", VK_RendererPerf_f );
		ri.Cmd_AddCommand( "renderer_memory", VK_RendererMemory_f );
		s_cmdsRegistered = qtrue;
		ri.Printf( PRINT_ALL, "[VK][perf] ready: renderer_perf, renderer_memory\n" );
	}
}

void vk_renderer_perf_begin_frame( void )
{
}

#endif
