/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Synthetic BVH traversal simulator for TTP pop-streak characterization.
===========================================================================
*/

#include "ttp_sim.h"

#include <stdlib.h>

#define TTP_MAX_STACK 4096
#define TTP_MAX_QUEUE 4096

typedef struct {
	int node_id;
	int depth;
} ttp_node_ref_t;

static int ttp_child_count( int arity, int depth, int max_depth, int node_id, int ray_seed )
{
	int i;
	int hits;

	(void)node_id;
	if ( depth >= max_depth ) {
		return 0;
	}

	hits = 0;
	for ( i = 0; i < arity; i++ ) {
		/* Deterministic pseudo-intersection: mix depth, child index, and ray seed. */
		unsigned h = (unsigned)( depth * 7919 + i * 104729 + ray_seed * 1543 + 17 );
		if ( ( h % 100u ) < 55u ) {
			hits++;
		}
	}
	if ( hits == 0 ) {
		hits = 1;
	}
	return hits;
}

static void ttp_histogram_reset( ttp_pop_histogram_t *hist )
{
	int i;

	if ( !hist ) {
		return;
	}
	hist->total_pops = 0u;
	for ( i = 0; i < 4; i++ ) {
		hist->streak_pops[i] = 0u;
	}
}

static void ttp_histogram_record_pop( ttp_pop_histogram_t *hist, int streak_after_pop )
{
	int idx;

	if ( !hist ) {
		return;
	}
	hist->total_pops++;
	idx = streak_after_pop - 1;
	if ( idx < 0 ) {
		idx = 0;
	}
	if ( idx > 3 ) {
		idx = 3;
	}
	hist->streak_pops[idx]++;
}

void TTP_SimulateTraversal( ttp_traversal_mode_t mode, int tree_depth, int arity, int ray_seed,
	ttp_pop_histogram_t *hist )
{
	ttp_node_ref_t stack[TTP_MAX_STACK];
	ttp_node_ref_t queue[TTP_MAX_QUEUE];
	int stack_top;
	int queue_head;
	int queue_tail;
	int pop_streak;
	int next_id;

	if ( tree_depth < 1 ) {
		tree_depth = 1;
	}
	if ( arity < 2 ) {
		arity = 2;
	}
	if ( ray_seed == 0 ) {
		ray_seed = 1;
	}

	ttp_histogram_reset( hist );
	if ( !hist ) {
		return;
	}

	next_id = 1;

	if ( mode == TTP_TRAVERSAL_DFS ) {
		stack_top = 0;
		stack[stack_top].node_id = 0;
		stack[stack_top].depth = 0;
		pop_streak = 0;

		while ( stack_top >= 0 ) {
			ttp_node_ref_t cur;
			int child_hits;
			int i;

			cur = stack[stack_top];
			stack_top--;
			pop_streak++;
			ttp_histogram_record_pop( hist, pop_streak );

			child_hits = ttp_child_count( arity, cur.depth, tree_depth, cur.node_id, ray_seed );
			if ( child_hits <= 0 || cur.depth + 1 >= tree_depth ) {
				continue;
			}

			pop_streak = 0;
			for ( i = child_hits - 1; i >= 0; i-- ) {
				if ( stack_top + 1 >= TTP_MAX_STACK ) {
					break;
				}
				stack_top++;
				stack[stack_top].node_id = next_id++;
				stack[stack_top].depth = cur.depth + 1;
			}
		}
	} else {
		queue_head = 0;
		queue_tail = 0;
		queue[queue_tail].node_id = 0;
		queue[queue_tail].depth = 0;
		queue_tail = ( queue_tail + 1 ) % TTP_MAX_QUEUE;
		pop_streak = 0;

		while ( queue_head != queue_tail ) {
			ttp_node_ref_t cur;
			int child_hits;
			int i;

			cur = queue[queue_head];
			queue_head = ( queue_head + 1 ) % TTP_MAX_QUEUE;
			pop_streak++;
			ttp_histogram_record_pop( hist, pop_streak );

			child_hits = ttp_child_count( arity, cur.depth, tree_depth, cur.node_id, ray_seed );
			if ( child_hits <= 0 || cur.depth + 1 >= tree_depth ) {
				continue;
			}

			pop_streak = 0;
			for ( i = 0; i < child_hits; i++ ) {
				int next;

				next = ( queue_tail + 1 ) % TTP_MAX_QUEUE;
				if ( next == queue_head ) {
					break;
				}
				queue[queue_tail].node_id = next_id++;
				queue[queue_tail].depth = cur.depth + 1;
				queue_tail = next;
			}
		}
	}
}

static const ttp_scene_preset_t lumibench_presets[] = {
	{ "wknd", 7, 6 },
	{ "ship", 12, 6 },
	{ "bunny", 11, 6 },
	{ "spnza", 16, 6 },
	{ "chsnt", 12, 6 },
	{ "bath", 16, 6 },
	{ "ref", 13, 6 },
	{ "crnvl", 16, 6 },
	{ "fox", 15, 6 },
	{ "party", 14, 6 },
	{ "sprng", 14, 6 },
	{ "lands", 12, 6 },
	{ "frst", 14, 6 },
	{ "park", 14, 6 },
	{ "car", 16, 6 },
	{ "robot", 18, 6 }
};

const ttp_scene_preset_t *TTP_LumibenchPresets( int *count )
{
	if ( count ) {
		*count = (int)( sizeof( lumibench_presets ) / sizeof( lumibench_presets[0] ) );
	}
	return lumibench_presets;
}
