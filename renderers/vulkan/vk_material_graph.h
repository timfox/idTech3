#pragma once


#include "vk_material_ir.h"

/*
 * Raster Ultra 1.8 — controlled material graph (bounded node set).
 * Compiles into IR feature flags / constants. No loops or unrestricted branching.
 */

typedef enum {
	VK_MAT_NODE_TEXTURE = 0,
	VK_MAT_NODE_CONSTANT,
	VK_MAT_NODE_PARAM,
	VK_MAT_NODE_ADD,
	VK_MAT_NODE_MUL,
	VK_MAT_NODE_LERP,
	VK_MAT_NODE_CLAMP,
	VK_MAT_NODE_REMAP,
	VK_MAT_NODE_POWER,
	VK_MAT_NODE_NORMAL_BLEND,
	VK_MAT_NODE_HEIGHT_BLEND,
	VK_MAT_NODE_UV_TRANSFORM,
	VK_MAT_NODE_TRIPLANAR,
	VK_MAT_NODE_WORLD_MASK,
	VK_MAT_NODE_NORMAL_MASK,
	VK_MAT_NODE_VERTEX_COLOR,
	VK_MAT_NODE_LAYER_BLEND,
	VK_MAT_NODE_COUNT
} vkMaterialNodeClass_t;

#define VK_MAT_GRAPH_MAX_NODES 32
#define VK_MAT_GRAPH_VERSION   1

typedef struct vkMaterialGraphNode_s {
	vkMaterialNodeClass_t klass;
	int inputA;
	int inputB;
	int inputC;
	float constant[4];
	char paramName[32];
} vkMaterialGraphNode_t;

typedef struct vkMaterialGraph_s {
	int nodeCount;
	vkMaterialGraphNode_t nodes[VK_MAT_GRAPH_MAX_NODES];
	int outputNode;
	uint32_t version;
	qboolean valid;
} vkMaterialGraph_t;

void vk_material_graph_register_cvars( void );
void vk_material_graph_init( void );
void vk_material_graph_shutdown( void );

void vk_material_graph_reset( vkMaterialGraph_t *g );

/* Compile graph into IR static features + constants. Rejects cycles / excess nodes. */
qboolean vk_material_graph_compile( const vkMaterialGraph_t *g, vkMaterialIR_t *ir );

/* Seed a height-blend + wetness graph for validation assets. */
void vk_material_graph_seed_height_wet( vkMaterialGraph_t *g );

void vk_material_graph_status_f( void );

