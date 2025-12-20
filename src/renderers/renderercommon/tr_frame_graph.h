/*
===========================================================================
Minimal Frame Graph

Lightweight pass scheduler that executes a linear list of passes through the
backend interface. Intended as a stepping stone toward a full render graph
with resource aliasing and barriers.
===========================================================================
*/

#pragma once

#include "../../common/q_shared.h"
#include "tr_backend_iface.h"

#ifdef __cplusplus
extern "C" {
#endif

#define RG_MAX_PASSES 32

typedef void (*rg_execute_fn)( void *user );

typedef struct {
	const char *name;      // Debug name (optional)
	rg_execute_fn execute; // Callback executed between begin_pass/end_pass
	void *user;            // Opaque pointer forwarded to execute()
	uint32_t flags;        // Reserved for future use (async, compute, etc.)
} rg_pass_desc_t;

typedef struct {
	rg_pass_desc_t passes[RG_MAX_PASSES];
	int pass_count;
} rg_frame_graph_t;

// Reset to an empty graph.
void RG_Reset( rg_frame_graph_t *graph );

// Append a pass; returns qtrue on success, qfalse if full or invalid input.
qboolean RG_AddPass( rg_frame_graph_t *graph, const rg_pass_desc_t *desc );

// Execute the graph using the provided backend (or the currently set backend if NULL).
void RG_Execute( const rg_frame_graph_t *graph, const rb_backend_iface_t *backend );

#ifdef __cplusplus
} // extern "C"
#endif


