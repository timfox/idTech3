#pragma once

#include "tr_local.h"

uint32_t vk_rtx_world_count_primitives( const world_t *w, uint32_t maxPrimitives );
uint32_t vk_rtx_world_pack( const world_t *w, uint32_t maxPrimitives,
	float *positions, uint32_t *indices );
