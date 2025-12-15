/*
=============================================================================
Cell Streaming System
Implements region-based world streaming for larger, contiguous levels
=============================================================================
*/

#pragma once

#include "tr_local.h"
#include "vk.h"

#ifdef USE_VULKAN

// Forward declaration (full definition in vk.h)
// stream_cell_t is already typedef'd in vk.h

// Cell streaming configuration
#ifndef MAX_STREAM_CELLS
#define MAX_STREAM_CELLS 256
#endif
#define CELL_SIZE 1024.0f  // World units per cell
#define CELL_LOAD_RADIUS 2  // Load cells within N cells of player
#define CELL_UNLOAD_DISTANCE 4  // Unload cells beyond N cells

// Cell state flags
#define CELL_STATE_UNLOADED    0x00
#define CELL_STATE_LOADING     0x01
#define CELL_STATE_LOADED      0x02
#define CELL_STATE_UNLOADING   0x03
#define CELL_STATE_VISIBLE     0x04

// Full definition moved to vk.h to avoid incomplete type errors
// This file now uses the definition from vk.h

// Streaming system state
typedef struct {
	qboolean enabled;
	qboolean initialized;
	
	stream_cell_t cells[MAX_STREAM_CELLS];
	uint32_t cellCount;
	uint32_t activeCellCount;
	
	// Current player cell
	int32_t currentCellX, currentCellY, currentCellZ;
	
	// Loading queue
	uint32_t loadQueue[MAX_STREAM_CELLS];
	uint32_t loadQueueCount;
	
	// Unloading queue
	uint32_t unloadQueue[MAX_STREAM_CELLS];
	uint32_t unloadQueueCount;
	
	// Frame tracking
	uint32_t frameCounter;
	uint32_t cellsLoadedThisFrame;
	uint32_t cellsUnloadedThisFrame;
} cell_streaming_system_t;

// External API
void vk_cell_streaming_init( void );
void vk_cell_streaming_shutdown( void );
void vk_cell_streaming_update( const vec3_t playerPos );
void vk_cell_streaming_set_player_position( const vec3_t pos );
stream_cell_t *vk_cell_streaming_get_cell( int32_t cellX, int32_t cellY, int32_t cellZ );
qboolean vk_cell_streaming_is_cell_loaded( int32_t cellX, int32_t cellY, int32_t cellZ );

// CVars
extern cvar_t *r_cellStreaming;
extern cvar_t *r_cellLoadRadius;
extern cvar_t *r_cellUnloadDistance;

#endif // USE_VULKAN

