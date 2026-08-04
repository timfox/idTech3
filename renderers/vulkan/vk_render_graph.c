/*
===========================================================================
Vulkan render graph core.
===========================================================================
*/

#include "tr_local.h"
#include "vk_render_graph.h"
#include <stdarg.h>

#define VK_RG_MAX_EDGES_PER_PASS 16
#define VK_RG_MAX_DEPS 256
#define VK_RG_MAX_ACTIVE 16

typedef struct {
	qboolean declared;
	qboolean observed;
	vkSpineResourceEdge reads[VK_RG_MAX_EDGES_PER_PASS];
	int readCount;
	vkSpineResourceEdge writes[VK_RG_MAX_EDGES_PER_PASS];
	int writeCount;
	vkRenderGraphExecuteFn execute;
	void *userData;
} vkRenderGraphNode;

typedef struct {
	vkSpinePassId from;
	vkSpinePassId to;
	vkSpineResourceId resource;
	uint32_t access;
} vkRenderGraphDependency;

typedef struct {
	qboolean initialized;
	qboolean compiled;
	qboolean cycleDetected;
	uint32_t frameIndex;
	uint32_t observedCount;
	uint32_t compiledCount;
	uint32_t dependencyCount;
	uint32_t frameViolationCount;
	uint32_t violationCount;
	vkSpinePassId activeStack[VK_RG_MAX_ACTIVE];
	uint32_t activeCount;
	qboolean imported[VK_SPINE_RES_COUNT];
	vkRenderGraphNode nodes[VK_SPINE_PASS_COUNT];
	vkRenderGraphDependency deps[VK_RG_MAX_DEPS];
	vkSpinePassId order[VK_SPINE_PASS_COUNT];
	char lastError[160];
} vkRenderGraphState;

static vkRenderGraphState s_rg;

static qboolean vk_rg_valid_pass( vkSpinePassId pass )
{
	return ( pass > VK_SPINE_PASS_NONE && pass < VK_SPINE_PASS_COUNT ) ? qtrue : qfalse;
}

static qboolean vk_rg_valid_resource( vkSpineResourceId res )
{
	return ( res > VK_SPINE_RES_NONE && res < VK_SPINE_RES_COUNT ) ? qtrue : qfalse;
}

static qboolean vk_rg_access_writes( uint32_t access )
{
	return ( access & ( VK_SPINE_ACCESS_STORAGE_WRITE | VK_SPINE_ACCESS_COLOR_WRITE |
		VK_SPINE_ACCESS_DEPTH_WRITE | VK_SPINE_ACCESS_TRANSFER_WRITE |
		VK_SPINE_ACCESS_HISTORY_WRITE ) ) != 0u ? qtrue : qfalse;
}

static qboolean vk_rg_access_reads( uint32_t access )
{
	return ( access & ( VK_SPINE_ACCESS_SAMPLED_READ | VK_SPINE_ACCESS_STORAGE_READ |
		VK_SPINE_ACCESS_DEPTH_READ | VK_SPINE_ACCESS_TRANSFER_READ |
		VK_SPINE_ACCESS_INDIRECT_READ | VK_SPINE_ACCESS_AS_READ |
		VK_SPINE_ACCESS_HISTORY_READ ) ) != 0u ? qtrue : qfalse;
}

static const char *vk_rg_access_name( uint32_t access )
{
	if ( access & VK_SPINE_ACCESS_INDIRECT_READ ) {
		return "indirect-read";
	}
	if ( access & VK_SPINE_ACCESS_STORAGE_READ ) {
		return "storage-read";
	}
	if ( access & VK_SPINE_ACCESS_SAMPLED_READ ) {
		return "sampled-read";
	}
	if ( access & VK_SPINE_ACCESS_DEPTH_READ ) {
		return "depth-read";
	}
	if ( access & VK_SPINE_ACCESS_DEPTH_WRITE ) {
		return "depth-write";
	}
	if ( access & VK_SPINE_ACCESS_COLOR_WRITE ) {
		return "color-write";
	}
	if ( access & VK_SPINE_ACCESS_STORAGE_WRITE ) {
		return "storage-write";
	}
	if ( access & VK_SPINE_ACCESS_TRANSFER_READ ) {
		return "transfer-read";
	}
	if ( access & VK_SPINE_ACCESS_TRANSFER_WRITE ) {
		return "transfer-write";
	}
	if ( access & VK_SPINE_ACCESS_AS_READ ) {
		return "as-read";
	}
	if ( access & VK_SPINE_ACCESS_HISTORY_READ ) {
		return "history-read";
	}
	if ( access & VK_SPINE_ACCESS_HISTORY_WRITE ) {
		return "history-write";
	}
	return "access";
}

static void vk_rg_record_error( const char *fmt, ... )
{
	va_list ap;
	char buf[sizeof( s_rg.lastError )];

	va_start( ap, fmt );
	Q_vsnprintf( buf, sizeof( buf ), fmt, ap );
	va_end( ap );

	s_rg.violationCount++;
	s_rg.frameViolationCount++;
	Q_strncpyz( s_rg.lastError, buf, sizeof( s_rg.lastError ) );
	ri.Printf( PRINT_DEVELOPER, S_COLOR_YELLOW "[VK][render_graph] %s\n", buf );
}

static void vk_rg_default_imports( void )
{
	vk_render_graph_import_resource( VK_SPINE_RES_SWAPCHAIN_COLOR );
	vk_render_graph_import_resource( VK_SPINE_RES_DEPTH );
	vk_render_graph_import_resource( VK_SPINE_RES_HDR_COLOR );
	vk_render_graph_import_resource( VK_SPINE_RES_TAA_HISTORY );
	vk_render_graph_import_resource( VK_SPINE_RES_AV_HISTORY );
	vk_render_graph_import_resource( VK_SPINE_RES_FORWARD_PLUS_LIGHTS );
	vk_render_graph_import_resource( VK_SPINE_RES_SHADOW_SUN );
	vk_render_graph_import_resource( VK_SPINE_RES_PROBE_GRID );
	vk_render_graph_import_resource( VK_SPINE_RES_VISIBILITY_CLASS );
	vk_render_graph_import_resource( VK_SPINE_RES_VIRTUAL_GEOMETRY_MESHLETS );
	vk_render_graph_import_resource( VK_SPINE_RES_VIRTUAL_GEOMETRY_INDIRECT );
	vk_render_graph_import_resource( VK_SPINE_RES_AV_FILTERED );
	vk_render_graph_import_resource( VK_SPINE_RES_SURFEL_HASH );
	vk_render_graph_import_resource( VK_SPINE_RES_SURFEL_IRRADIANCE );
}

static void vk_rg_add_dependency( vkSpinePassId from, vkSpinePassId to,
	vkSpineResourceId resource, uint32_t access )
{
	uint32_t i;

	if ( !vk_rg_valid_pass( from ) || !vk_rg_valid_pass( to ) || from == to ) {
		return;
	}
	for ( i = 0u; i < s_rg.dependencyCount; i++ ) {
		if ( s_rg.deps[i].from == from && s_rg.deps[i].to == to &&
			s_rg.deps[i].resource == resource ) {
			s_rg.deps[i].access |= access;
			return;
		}
	}
	if ( s_rg.dependencyCount >= VK_RG_MAX_DEPS ) {
		vk_rg_record_error( "dependency overflow at %s -> %s",
			vk_spine_pass_name( from ), vk_spine_pass_name( to ) );
		return;
	}
	s_rg.deps[s_rg.dependencyCount].from = from;
	s_rg.deps[s_rg.dependencyCount].to = to;
	s_rg.deps[s_rg.dependencyCount].resource = resource;
	s_rg.deps[s_rg.dependencyCount].access = access;
	s_rg.dependencyCount++;
}

void vk_render_graph_init( void )
{
	Com_Memset( &s_rg, 0, sizeof( s_rg ) );
	s_rg.initialized = qtrue;
	if ( ri.Cmd_AddCommand ) {
		ri.Cmd_AddCommand( "render_graph_status", vk_render_graph_status_f );
		ri.Cmd_AddCommand( "render_graph_dot", vk_render_graph_dot_f );
	}
}

void vk_render_graph_shutdown( void )
{
	if ( ri.Cmd_RemoveCommand ) {
		ri.Cmd_RemoveCommand( "render_graph_dot" );
		ri.Cmd_RemoveCommand( "render_graph_status" );
	}
	Com_Memset( &s_rg, 0, sizeof( s_rg ) );
}

void vk_render_graph_declare_pass( vkSpinePassId pass,
	const vkSpineResourceEdge *reads, int readCount,
	const vkSpineResourceEdge *writes, int writeCount )
{
	vkRenderGraphNode *node;

	if ( !vk_rg_valid_pass( pass ) ) {
		return;
	}
	node = &s_rg.nodes[pass];
	node->declared = qtrue;
	node->readCount = 0;
	node->writeCount = 0;
	if ( reads && readCount > 0 ) {
		if ( readCount > VK_RG_MAX_EDGES_PER_PASS ) {
			vk_rg_record_error( "pass %s has too many read edges (%d)",
				vk_spine_pass_name( pass ), readCount );
			readCount = VK_RG_MAX_EDGES_PER_PASS;
		}
		Com_Memcpy( node->reads, reads, sizeof( node->reads[0] ) * readCount );
		node->readCount = readCount;
	}
	if ( writes && writeCount > 0 ) {
		if ( writeCount > VK_RG_MAX_EDGES_PER_PASS ) {
			vk_rg_record_error( "pass %s has too many write edges (%d)",
				vk_spine_pass_name( pass ), writeCount );
			writeCount = VK_RG_MAX_EDGES_PER_PASS;
		}
		Com_Memcpy( node->writes, writes, sizeof( node->writes[0] ) * writeCount );
		node->writeCount = writeCount;
	}
}

void vk_render_graph_set_pass_executor( vkSpinePassId pass,
	vkRenderGraphExecuteFn execute, void *userData )
{
	if ( !vk_rg_valid_pass( pass ) ) {
		return;
	}
	s_rg.nodes[pass].execute = execute;
	s_rg.nodes[pass].userData = userData;
}

void vk_render_graph_begin_frame( void )
{
	int i;

	if ( !s_rg.initialized ) {
		return;
	}
	s_rg.frameIndex++;
	s_rg.compiled = qfalse;
	s_rg.cycleDetected = qfalse;
	s_rg.observedCount = 0u;
	s_rg.compiledCount = 0u;
	s_rg.dependencyCount = 0u;
	s_rg.frameViolationCount = 0u;
	s_rg.lastError[0] = '\0';
	s_rg.activeCount = 0u;
	Com_Memset( s_rg.activeStack, 0, sizeof( s_rg.activeStack ) );
	Com_Memset( s_rg.imported, 0, sizeof( s_rg.imported ) );
	for ( i = 0; i < VK_SPINE_PASS_COUNT; i++ ) {
		s_rg.nodes[i].observed = qfalse;
		s_rg.order[i] = VK_SPINE_PASS_NONE;
	}
	vk_rg_default_imports();
}

void vk_render_graph_import_resource( vkSpineResourceId res )
{
	if ( vk_rg_valid_resource( res ) ) {
		s_rg.imported[res] = qtrue;
	}
}

void vk_render_graph_observe_pass( vkSpinePassId pass )
{
	if ( !s_rg.initialized || !vk_rg_valid_pass( pass ) ) {
		return;
	}
	if ( !s_rg.nodes[pass].declared ) {
		vk_rg_record_error( "observed undeclared pass %s", vk_spine_pass_name( pass ) );
		return;
	}
	if ( !s_rg.nodes[pass].observed ) {
		s_rg.nodes[pass].observed = qtrue;
		s_rg.observedCount++;
	}
	s_rg.compiled = qfalse;
}

qboolean vk_render_graph_enter_pass( vkSpinePassId pass )
{
	if ( !s_rg.initialized || !vk_rg_valid_pass( pass ) ) {
		return qfalse;
	}
	if ( !s_rg.nodes[pass].declared ) {
		vk_rg_record_error( "entered undeclared pass %s", vk_spine_pass_name( pass ) );
		return qfalse;
	}
	/* Entering a pass is the graph ownership boundary.  The legacy recorder
	 * remains free to issue commands, but it can only do so inside a declared
	 * graph pass. */
	vk_render_graph_observe_pass( pass );
	if ( s_rg.activeCount >= VK_RG_MAX_ACTIVE ) {
		vk_rg_record_error( "pass stack overflow entering %s", vk_spine_pass_name( pass ) );
		return qfalse;
	}
	s_rg.activeStack[s_rg.activeCount++] = pass;
	return qtrue;
}

void vk_render_graph_leave_pass( vkSpinePassId pass )
{
	if ( !s_rg.initialized || !vk_rg_valid_pass( pass ) ) {
		return;
	}
	if ( s_rg.activeCount == 0u || s_rg.activeStack[s_rg.activeCount - 1u] != pass ) {
		vk_rg_record_error( "pass %s left while %s is active",
			vk_spine_pass_name( pass ), s_rg.activeCount ?
			vk_spine_pass_name( s_rg.activeStack[s_rg.activeCount - 1u] ) : "none" );
		return;
	}
	s_rg.activeCount--;
}

qboolean vk_render_graph_compile( void )
{
	vkSpinePassId lastWriter[VK_SPINE_RES_COUNT];
	vkSpinePassId lastReader[VK_SPINE_RES_COUNT];
	uint8_t indegree[VK_SPINE_PASS_COUNT];
	qboolean emitted[VK_SPINE_PASS_COUNT];
	uint32_t emittedCount = 0u;
	uint32_t i;

	if ( !s_rg.initialized ) {
		return qfalse;
	}
	if ( s_rg.compiled ) {
		return s_rg.cycleDetected ? qfalse : qtrue;
	}
	s_rg.dependencyCount = 0u;
	s_rg.compiledCount = 0u;
	s_rg.cycleDetected = qfalse;
	Com_Memset( lastWriter, 0, sizeof( lastWriter ) );
	Com_Memset( lastReader, 0, sizeof( lastReader ) );
	Com_Memset( indegree, 0, sizeof( indegree ) );
	Com_Memset( emitted, 0, sizeof( emitted ) );

	for ( i = 1u; i < (uint32_t)VK_SPINE_PASS_COUNT; i++ ) {
		vkRenderGraphNode *node = &s_rg.nodes[i];
		int e;

		if ( !node->observed ) {
			continue;
		}
		for ( e = 0; e < node->readCount; e++ ) {
			vkSpineResourceId res = node->reads[e].resource;
			if ( !vk_rg_valid_resource( res ) || !vk_rg_access_reads( node->reads[e].access ) ) {
				continue;
			}
			if ( lastWriter[res] != VK_SPINE_PASS_NONE ) {
				vk_rg_add_dependency( lastWriter[res], (vkSpinePassId)i, res, node->reads[e].access );
			} else if ( !s_rg.imported[res] ) {
				vk_rg_record_error( "unresolved read: %s reads %s with no writer/import",
					vk_spine_pass_name( (vkSpinePassId)i ), vk_spine_resource_name( res ) );
			}
			lastReader[res] = (vkSpinePassId)i;
		}
		for ( e = 0; e < node->writeCount; e++ ) {
			vkSpineResourceId res = node->writes[e].resource;
			if ( !vk_rg_valid_resource( res ) || !vk_rg_access_writes( node->writes[e].access ) ) {
				continue;
			}
			if ( lastWriter[res] != VK_SPINE_PASS_NONE ) {
				vk_rg_add_dependency( lastWriter[res], (vkSpinePassId)i, res, node->writes[e].access );
			}
			if ( lastReader[res] != VK_SPINE_PASS_NONE ) {
				vk_rg_add_dependency( lastReader[res], (vkSpinePassId)i, res, node->writes[e].access );
				lastReader[res] = VK_SPINE_PASS_NONE;
			}
			lastWriter[res] = (vkSpinePassId)i;
		}
	}

	for ( i = 0u; i < s_rg.dependencyCount; i++ ) {
		if ( vk_rg_valid_pass( s_rg.deps[i].to ) && indegree[s_rg.deps[i].to] < 255u ) {
			indegree[s_rg.deps[i].to]++;
		}
	}

	while ( emittedCount < s_rg.observedCount ) {
		vkSpinePassId p;
		qboolean found = qfalse;

		for ( p = (vkSpinePassId)1; p < VK_SPINE_PASS_COUNT; p++ ) {
			if ( s_rg.nodes[p].observed && !emitted[p] && indegree[p] == 0u ) {
				uint32_t d;
				s_rg.order[emittedCount++] = p;
				emitted[p] = qtrue;
				found = qtrue;
				for ( d = 0u; d < s_rg.dependencyCount; d++ ) {
					if ( s_rg.deps[d].from == p && indegree[s_rg.deps[d].to] > 0u ) {
						indegree[s_rg.deps[d].to]--;
					}
				}
				break;
			}
		}
		if ( !found ) {
			s_rg.cycleDetected = qtrue;
			vk_rg_record_error( "cycle detected while compiling observed render graph" );
			break;
		}
	}

	s_rg.compiled = qtrue;
	s_rg.compiledCount = emittedCount;
	return s_rg.cycleDetected ? qfalse : qtrue;
}

qboolean vk_render_graph_execute( void )
{
	uint32_t i;

	if ( !vk_render_graph_compile() ) {
		return qfalse;
	}
	for ( i = 0u; i < s_rg.compiledCount; i++ ) {
		vkRenderGraphNode *node = &s_rg.nodes[s_rg.order[i]];
		if ( node->execute ) {
			node->execute( node->userData );
		}
	}
	return qtrue;
}

void vk_render_graph_end_frame( void )
{
	if ( s_rg.initialized ) {
		vk_render_graph_compile();
	}
}

uint32_t vk_render_graph_observed_count( void )
{
	return s_rg.observedCount;
}

uint32_t vk_render_graph_compiled_count( void )
{
	if ( s_rg.initialized && !s_rg.compiled ) {
		vk_render_graph_compile();
	}
	return s_rg.compiledCount;
}

uint32_t vk_render_graph_dependency_count( void )
{
	if ( s_rg.initialized && !s_rg.compiled ) {
		vk_render_graph_compile();
	}
	return s_rg.dependencyCount;
}

uint32_t vk_render_graph_violation_count( void )
{
	return s_rg.violationCount;
}

uint32_t vk_render_graph_frame_violation_count( void )
{
	return s_rg.frameViolationCount;
}

const char *vk_render_graph_last_error( void )
{
	return s_rg.lastError;
}

void vk_render_graph_status_f( void )
{
	uint32_t i;

	vk_render_graph_compile();
	ri.Printf( PRINT_ALL, "Vulkan render graph: frame=%u observed=%u compiled=%u deps=%u frameViolations=%u totalViolations=%u%s\n",
		s_rg.frameIndex, s_rg.observedCount, s_rg.compiledCount, s_rg.dependencyCount,
		s_rg.frameViolationCount, s_rg.violationCount, s_rg.cycleDetected ? " cycle=yes" : "" );
	if ( s_rg.lastError[0] ) {
		ri.Printf( PRINT_ALL, "  last: %s\n", s_rg.lastError );
	}
	ri.Printf( PRINT_ALL, "  order:" );
	for ( i = 0u; i < s_rg.compiledCount; i++ ) {
		ri.Printf( PRINT_ALL, " %s", vk_spine_pass_name( s_rg.order[i] ) );
	}
	ri.Printf( PRINT_ALL, "\n" );
	for ( i = 0u; i < s_rg.dependencyCount; i++ ) {
		ri.Printf( PRINT_ALL, "  dep: %s -> %s via %s (%s)\n",
			vk_spine_pass_name( s_rg.deps[i].from ),
			vk_spine_pass_name( s_rg.deps[i].to ),
			vk_spine_resource_name( s_rg.deps[i].resource ),
			vk_rg_access_name( s_rg.deps[i].access ) );
	}
}

void vk_render_graph_dot_f( void )
{
	uint32_t i;

	vk_render_graph_compile();
	ri.Printf( PRINT_ALL, "digraph vulkan_render_graph {\n" );
	ri.Printf( PRINT_ALL, "  rankdir=LR;\n" );
	ri.Printf( PRINT_ALL, "  node [shape=box,fontname=\"monospace\"];\n" );
	for ( i = 0u; i < s_rg.compiledCount; i++ ) {
		ri.Printf( PRINT_ALL, "  \"%s\";\n", vk_spine_pass_name( s_rg.order[i] ) );
	}
	for ( i = 0u; i < s_rg.dependencyCount; i++ ) {
		ri.Printf( PRINT_ALL, "  \"%s\" -> \"%s\" [label=\"%s %s\"];\n",
			vk_spine_pass_name( s_rg.deps[i].from ),
			vk_spine_pass_name( s_rg.deps[i].to ),
			vk_spine_resource_name( s_rg.deps[i].resource ),
			vk_rg_access_name( s_rg.deps[i].access ) );
	}
	ri.Printf( PRINT_ALL, "}\n" );
}
