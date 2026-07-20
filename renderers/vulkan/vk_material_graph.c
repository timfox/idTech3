/*
===========================================================================
Raster Ultra 1.8 — bounded material graph compiler.
===========================================================================
*/

#include "tr_local.h"
#include "vk_material_ir.h"
#include "vk_material_graph.h"

static cvar_t *r_materialGraph;
static qboolean s_cmds;
static uint32_t s_compileOk;
static uint32_t s_compileReject;

void vk_material_graph_register_cvars( void )
{
	if ( r_materialGraph ) {
		return;
	}
	r_materialGraph = ri.Cvar_Get( "r_materialGraph", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
	ri.Cvar_CheckRange( r_materialGraph, "0", "1", CV_INTEGER );
	ri.Cvar_SetDescription( r_materialGraph,
		"Raster Ultra 1.8 controlled material graph (latched).\n"
		"Bounded node set only — no loops / unrestricted dynamic branching." );
	ri.Cvar_SetGroup( r_materialGraph, CVG_RENDERER );
}

void vk_material_graph_init( void )
{
	vk_material_graph_register_cvars();
	s_compileOk = 0;
	s_compileReject = 0;
	if ( !s_cmds ) {
		ri.Cmd_AddCommand( "material_graph_status", vk_material_graph_status_f );
		s_cmds = qtrue;
	}
	ri.Printf( PRINT_ALL, "[VK][MaterialGraph] %s version=%d maxNodes=%d\n",
		( r_materialGraph && r_materialGraph->integer ) ? "enabled" : "off",
		VK_MAT_GRAPH_VERSION, VK_MAT_GRAPH_MAX_NODES );
}

void vk_material_graph_shutdown( void )
{
}

void vk_material_graph_reset( vkMaterialGraph_t *g )
{
	if ( !g ) {
		return;
	}
	Com_Memset( g, 0, sizeof( *g ) );
	g->outputNode = -1;
	g->version = VK_MAT_GRAPH_VERSION;
}

void vk_material_graph_seed_height_wet( vkMaterialGraph_t *g )
{
	vk_material_graph_reset( g );
	if ( !g ) {
		return;
	}
	/* texture0 -> height_blend with vertex color -> wetness param lerp */
	g->nodes[0].klass = VK_MAT_NODE_TEXTURE;
	g->nodes[1].klass = VK_MAT_NODE_VERTEX_COLOR;
	g->nodes[2].klass = VK_MAT_NODE_HEIGHT_BLEND;
	g->nodes[2].inputA = 0;
	g->nodes[2].inputB = 1;
	g->nodes[3].klass = VK_MAT_NODE_PARAM;
	Q_strncpyz( g->nodes[3].paramName, "wetness", sizeof( g->nodes[3].paramName ) );
	g->nodes[4].klass = VK_MAT_NODE_LERP;
	g->nodes[4].inputA = 2;
	g->nodes[4].inputB = 3;
	g->nodeCount = 5;
	g->outputNode = 4;
	g->valid = qtrue;
}

qboolean vk_material_graph_compile( const vkMaterialGraph_t *g, vkMaterialIR_t *ir )
{
	int i;
	uint8_t seen[VK_MAT_GRAPH_MAX_NODES];

	if ( !g || !ir || !g->valid ) {
		s_compileReject++;
		return qfalse;
	}
	if ( g->nodeCount <= 0 || g->nodeCount > VK_MAT_GRAPH_MAX_NODES ) {
		s_compileReject++;
		return qfalse;
	}
	if ( g->outputNode < 0 || g->outputNode >= g->nodeCount ) {
		s_compileReject++;
		return qfalse;
	}

	/* Acyclic: inputs must reference lower indices only (topo-ordered authoring). */
	Com_Memset( seen, 0, sizeof( seen ) );
	for ( i = 0; i < g->nodeCount; i++ ) {
		const vkMaterialGraphNode_t *n = &g->nodes[i];
		if ( n->klass < 0 || n->klass >= VK_MAT_NODE_COUNT ) {
			s_compileReject++;
			return qfalse;
		}
		if ( n->inputA >= i || n->inputB >= i || n->inputC >= i ) {
			s_compileReject++;
			return qfalse;
		}
		switch ( n->klass ) {
		case VK_MAT_NODE_HEIGHT_BLEND:
		case VK_MAT_NODE_LAYER_BLEND:
			ir->staticFeatures |= VK_MAT_FEAT_HEIGHT_BLEND;
			if ( ir->layerCount < 2 ) {
				ir->layerCount = 2;
			}
			break;
		case VK_MAT_NODE_TRIPLANAR:
			ir->staticFeatures |= VK_MAT_FEAT_TRIPLANAR;
			break;
		case VK_MAT_NODE_PARAM:
			if ( !Q_stricmp( n->paramName, "wetness" ) ) {
				ir->dynamicFeatures |= VK_MAT_DYN_WETNESS;
				ir->staticFeatures |= VK_MAT_FEAT_EVOLUTION;
			} else if ( !Q_stricmp( n->paramName, "snow" ) ) {
				ir->dynamicFeatures |= VK_MAT_DYN_SNOW;
				ir->staticFeatures |= VK_MAT_FEAT_EVOLUTION;
			} else if ( !Q_stricmp( n->paramName, "dust" ) ) {
				ir->dynamicFeatures |= VK_MAT_DYN_DUST;
				ir->staticFeatures |= VK_MAT_FEAT_EVOLUTION;
			} else if ( !Q_stricmp( n->paramName, "rust" ) ) {
				ir->dynamicFeatures |= VK_MAT_DYN_RUST;
				ir->staticFeatures |= VK_MAT_FEAT_EVOLUTION;
			}
			break;
		default:
			break;
		}
		seen[i] = 1;
	}

	ir->permutationKey = vk_material_ir_permutation_key( ir );
	ir->valid = qtrue;
	s_compileOk++;
	return qtrue;
}

void vk_material_graph_status_f( void )
{
	ri.Printf( PRINT_ALL, "=== Material Graph (Raster Ultra 1.8) ===\n" );
	ri.Printf( PRINT_ALL, "r_materialGraph  : %d\n", r_materialGraph ? r_materialGraph->integer : 0 );
	ri.Printf( PRINT_ALL, "version          : %d\n", VK_MAT_GRAPH_VERSION );
	ri.Printf( PRINT_ALL, "maxNodes         : %d classes=%d\n",
		VK_MAT_GRAPH_MAX_NODES, VK_MAT_NODE_COUNT );
	ri.Printf( PRINT_ALL, "compileOk        : %u reject=%u\n", s_compileOk, s_compileReject );
	ri.Printf( PRINT_ALL, "policy           : topo-ordered inputs only; no loops\n" );
}
