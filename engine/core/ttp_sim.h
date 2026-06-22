#pragma once

#include "ttp_model.h"

typedef struct {
	const char *label;
	int tree_depth;
	int arity;
} ttp_scene_preset_t;

void TTP_SimulateTraversal( ttp_traversal_mode_t mode, int tree_depth, int arity, int ray_seed,
	ttp_pop_histogram_t *hist );

const ttp_scene_preset_t *TTP_LumibenchPresets( int *count );
