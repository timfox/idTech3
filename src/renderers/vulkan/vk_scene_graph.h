/*
=============================================================================
GPU Scene Graph (stub)

Lightweight scaffolding for GPU-resident scene graph buffers. Currently a
placeholder; enable/disable via r_gpuSceneGraph.
=============================================================================
*/
#pragma once

#include "tr_local.h"

typedef struct vk_scene_node_s {
	vec3_t worldMin;
	vec3_t worldMax;
	uint32_t instanceId;
	uint32_t materialId;
} vk_scene_node_t;

typedef struct vk_scene_graph_s {
	qboolean enabled;
	uint32_t nodeCount;
	// Future: VkBuffer/alloc handles; currently CPU-only stub.
} vk_scene_graph_t;

void Vk_SceneGraph_Init(void);
void Vk_SceneGraph_Shutdown(void);
void Vk_SceneGraph_Frame(void);

