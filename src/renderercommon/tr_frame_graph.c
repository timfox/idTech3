/*
===========================================================================
Minimal Frame Graph

Executes a linear list of passes through the active backend interface. This
keeps pass orchestration separate from the backend details and prepares for
future resource tracking and scheduling.
===========================================================================
*/

#include "tr_frame_graph.h"

#include <string.h>

void RG_Reset( rg_frame_graph_t *graph ) {
	if ( !graph ) {
		return;
	}
	memset( graph, 0, sizeof( *graph ) );
}

qboolean RG_AddPass( rg_frame_graph_t *graph, const rg_pass_desc_t *desc ) {
	if ( !graph || !desc ) {
		return qfalse;
	}
	if ( graph->pass_count >= RG_MAX_PASSES ) {
		return qfalse;
	}
	graph->passes[ graph->pass_count ] = *desc;
	++graph->pass_count;
	return qtrue;
}

void RG_Execute( const rg_frame_graph_t *graph, const rb_backend_iface_t *backend ) {
	if ( !graph || graph->pass_count <= 0 ) {
		return;
	}

	const rb_backend_iface_t *iface = backend ? backend : RB_GetBackendInterface();
	if ( !iface ) {
		return;
	}

	if ( iface->begin_frame ) {
		iface->begin_frame();
	}

	for ( int i = 0; i < graph->pass_count; ++i ) {
		const rg_pass_desc_t *pass = &graph->passes[ i ];

		if ( iface->begin_pass ) {
			const char *name = pass->name ? pass->name : "unnamed_pass";
			iface->begin_pass( name );
		}

		if ( pass->execute ) {
			pass->execute( pass->user );
		}

		if ( iface->end_pass ) {
			iface->end_pass();
		}
	}

	if ( iface->end_frame ) {
		iface->end_frame();
	}
}


