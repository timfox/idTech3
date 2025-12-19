/*
=============================================================================
GPU Scene Graph (stub implementation)
=============================================================================
*/

#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "vk.h"
#include "vk_scene_graph.h"

#ifdef USE_VULKAN

extern cvar_t *r_gpuSceneGraph;
extern cvar_t *r_gpuSceneDebug;

static vk_scene_graph_t sg_state;

void Vk_SceneGraph_Init(void) {
	if (sg_state.enabled) {
		return;
	}
	sg_state.enabled = (r_gpuSceneGraph && r_gpuSceneGraph->integer);
	sg_state.nodeCount = 0;
	if (sg_state.enabled) {
		ri.Printf(PRINT_ALL, "GPU Scene Graph: enabled (stub)\n");
	}
}

void Vk_SceneGraph_Shutdown(void) {
	sg_state.enabled = qfalse;
	sg_state.nodeCount = 0;
}

void Vk_SceneGraph_Frame(void) {
	if (!sg_state.enabled) {
		return;
	}
	if (r_gpuSceneDebug && r_gpuSceneDebug->integer) {
		ri.Printf(PRINT_DEVELOPER, "GPU Scene Graph: nodes=%u\n", sg_state.nodeCount);
	}
}

#endif // USE_VULKAN

