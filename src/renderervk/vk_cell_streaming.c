/*
=============================================================================
Cell Streaming System Implementation
=============================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_cell_streaming.h"

#ifdef USE_VULKAN

// CVars (extern declarations - defined in tr_init.c)
extern cvar_t *r_cellStreaming;
extern cvar_t *r_cellLoadRadius;
extern cvar_t *r_cellUnloadDistance;

/*
=============================================================================
Cell Streaming Initialization
=============================================================================
*/
void vk_cell_streaming_init( void )
{
	if ( vk.cellStreaming.initialized ) {
		return;
	}
	
	ri.Printf( PRINT_ALL, "Initializing cell streaming system...\n" );
	
	vk.cellStreaming.cellCount = 0;
	vk.cellStreaming.activeCellCount = 0;
	vk.cellStreaming.currentCellX = 0;
	vk.cellStreaming.currentCellY = 0;
	vk.cellStreaming.currentCellZ = 0;
	vk.cellStreaming.loadQueueCount = 0;
	vk.cellStreaming.unloadQueueCount = 0;
	vk.cellStreaming.frameCounter = 0;
	vk.cellStreaming.cellsLoadedThisFrame = 0;
	vk.cellStreaming.cellsUnloadedThisFrame = 0;
	
	Com_Memset( vk.cellStreaming.cells, 0, sizeof( vk.cellStreaming.cells ) );
	
	ri.Printf( PRINT_ALL, "Cell streaming: Initialized\n" );
	
	vk.cellStreaming.initialized = qtrue;
}

/*
=============================================================================
Cell Streaming Shutdown
=============================================================================
*/
void vk_cell_streaming_shutdown( void )
{
	if ( !vk.cellStreaming.initialized ) {
		return;
	}
	
	// Unload all cells
	for ( uint32_t i = 0; i < vk.cellStreaming.cellCount; i++ ) {
		stream_cell_t *cell = &vk.cellStreaming.cells[i];
		if ( cell->state == CELL_STATE_LOADED ) {
			// Free cell resources
			if ( cell->models ) {
				ri.Free( cell->models );
				cell->models = NULL;
			}
			if ( cell->textures ) {
				ri.Free( cell->textures );
				cell->textures = NULL;
			}
			cell->state = CELL_STATE_UNLOADED;
		}
	}
	
	vk.cellStreaming.cellCount = 0;
	vk.cellStreaming.activeCellCount = 0;
	
	vk.cellStreaming.initialized = qfalse;
	ri.Printf( PRINT_ALL, "Cell streaming: Shutdown complete\n" );
}

/*
=============================================================================
Calculate Cell Coordinates from World Position
=============================================================================
*/
static void world_to_cell( const vec3_t worldPos, int32_t *cellX, int32_t *cellY, int32_t *cellZ )
{
	*cellX = (int32_t)floorf( worldPos[0] / CELL_SIZE );
	*cellY = (int32_t)floorf( worldPos[1] / CELL_SIZE );
	*cellZ = (int32_t)floorf( worldPos[2] / CELL_SIZE );
}

/*
=============================================================================
Calculate World Bounds from Cell Coordinates
=============================================================================
*/
static void cell_to_world_bounds( int32_t cellX, int32_t cellY, int32_t cellZ, vec3_t min, vec3_t max )
{
	min[0] = cellX * CELL_SIZE;
	min[1] = cellY * CELL_SIZE;
	min[2] = cellZ * CELL_SIZE;
	
	max[0] = min[0] + CELL_SIZE;
	max[1] = min[1] + CELL_SIZE;
	max[2] = min[2] + CELL_SIZE;
}

/*
=============================================================================
Find or Create Cell
=============================================================================
*/
static stream_cell_t *find_or_create_cell( int32_t cellX, int32_t cellY, int32_t cellZ )
{
	// Search for existing cell
	for ( uint32_t i = 0; i < vk.cellStreaming.cellCount; i++ ) {
		stream_cell_t *cell = &vk.cellStreaming.cells[i];
		if ( cell->cellX == cellX && cell->cellY == cellY && cell->cellZ == cellZ ) {
			return cell;
		}
	}
	
	// Create new cell if we have room
	if ( vk.cellStreaming.cellCount >= MAX_STREAM_CELLS ) {
		return NULL;
	}
	
	stream_cell_t *cell = &vk.cellStreaming.cells[vk.cellStreaming.cellCount];
	cell->cellX = cellX;
	cell->cellY = cellY;
	cell->cellZ = cellZ;
	cell_to_world_bounds( cellX, cellY, cellZ, cell->worldMin, cell->worldMax );
	cell->state = CELL_STATE_UNLOADED;
	cell->priority = 0xFFFFFFFF;
	cell->modelCount = 0;
	cell->models = NULL;
	cell->textureCount = 0;
	cell->textures = NULL;
	cell->memoryUsed = 0;
	cell->lastAccessFrame = 0;
	
	vk.cellStreaming.cellCount++;
	
	return cell;
}

/*
=============================================================================
Set Player Position
=============================================================================
*/
void vk_cell_streaming_set_player_position( const vec3_t pos )
{
	world_to_cell( pos, &vk.cellStreaming.currentCellX, &vk.cellStreaming.currentCellY, &vk.cellStreaming.currentCellZ );
}

/*
=============================================================================
Get Cell
=============================================================================
*/
stream_cell_t *vk_cell_streaming_get_cell( int32_t cellX, int32_t cellY, int32_t cellZ )
{
	return find_or_create_cell( cellX, cellY, cellZ );
}

/*
=============================================================================
Check if Cell is Loaded
=============================================================================
*/
qboolean vk_cell_streaming_is_cell_loaded( int32_t cellX, int32_t cellY, int32_t cellZ )
{
	stream_cell_t *cell = find_or_create_cell( cellX, cellY, cellZ );
	if ( !cell ) {
		return qfalse;
	}
	
	return ( cell->state == CELL_STATE_LOADED );
}

/*
=============================================================================
Cell Streaming Update
=============================================================================
*/
void vk_cell_streaming_update( const vec3_t playerPos )
{
	if ( !vk.cellStreaming.enabled || !vk.cellStreaming.initialized ) {
		return;
	}
	
	vk.cellStreaming.frameCounter++;
	vk_cell_streaming_set_player_position( playerPos );
	
	int32_t loadRadius = r_cellLoadRadius ? r_cellLoadRadius->integer : CELL_LOAD_RADIUS;
	int32_t unloadDistance = r_cellUnloadDistance ? r_cellUnloadDistance->integer : CELL_UNLOAD_DISTANCE;
	
	// Build load queue (cells within load radius)
	vk.cellStreaming.loadQueueCount = 0;
	for ( int32_t z = -loadRadius; z <= loadRadius; z++ ) {
		for ( int32_t y = -loadRadius; y <= loadRadius; y++ ) {
			for ( int32_t x = -loadRadius; x <= loadRadius; x++ ) {
				int32_t cellX = vk.cellStreaming.currentCellX + x;
				int32_t cellY = vk.cellStreaming.currentCellY + y;
				int32_t cellZ = vk.cellStreaming.currentCellZ + z;
				
				// Calculate priority (distance from player cell)
				uint32_t distance = abs(x) + abs(y) + abs(z);
				
				stream_cell_t *cell = find_or_create_cell( cellX, cellY, cellZ );
				if ( cell && cell->state == CELL_STATE_UNLOADED ) {
					cell->priority = distance;
					if ( vk.cellStreaming.loadQueueCount < MAX_STREAM_CELLS ) {
						vk.cellStreaming.loadQueue[vk.cellStreaming.loadQueueCount++] = 
							(uint32_t)(cell - vk.cellStreaming.cells);
					}
				}
			}
		}
	}
	
	// Build unload queue (cells beyond unload distance)
	vk.cellStreaming.unloadQueueCount = 0;
	for ( uint32_t i = 0; i < vk.cellStreaming.cellCount; i++ ) {
		stream_cell_t *cell = &vk.cellStreaming.cells[i];
		if ( cell->state == CELL_STATE_LOADED ) {
			int32_t dx = abs( cell->cellX - vk.cellStreaming.currentCellX );
			int32_t dy = abs( cell->cellY - vk.cellStreaming.currentCellY );
			int32_t dz = abs( cell->cellZ - vk.cellStreaming.currentCellZ );
			
			if ( dx > unloadDistance || dy > unloadDistance || dz > unloadDistance ) {
				if ( vk.cellStreaming.unloadQueueCount < MAX_STREAM_CELLS ) {
					vk.cellStreaming.unloadQueue[vk.cellStreaming.unloadQueueCount++] = i;
				}
			} else {
				cell->lastAccessFrame = vk.cellStreaming.frameCounter;
			}
		}
	}
	
	// Process load queue (load one cell per frame to avoid stuttering)
	if ( vk.cellStreaming.loadQueueCount > 0 ) {
		// Sort by priority (simple insertion sort for small arrays)
		for ( uint32_t i = 1; i < vk.cellStreaming.loadQueueCount; i++ ) {
			uint32_t j = i;
			while ( j > 0 && vk.cellStreaming.cells[vk.cellStreaming.loadQueue[j]].priority < 
					vk.cellStreaming.cells[vk.cellStreaming.loadQueue[j-1]].priority ) {
				uint32_t temp = vk.cellStreaming.loadQueue[j];
				vk.cellStreaming.loadQueue[j] = vk.cellStreaming.loadQueue[j-1];
				vk.cellStreaming.loadQueue[j-1] = temp;
				j--;
			}
		}
		
		// Load highest priority cell
		uint32_t cellIndex = vk.cellStreaming.loadQueue[0];
		stream_cell_t *cell = &vk.cellStreaming.cells[cellIndex];
		
		if ( cell->state == CELL_STATE_UNLOADED ) {
			cell->state = CELL_STATE_LOADING;
			
			// Load cell assets (models, textures, etc.)
			// This would involve loading BSP data, models, textures for this cell
			// For now, allocate placeholder arrays
			cell->modelCount = 0;
			cell->models = NULL; // Would be allocated and populated with actual model handles
			cell->textureCount = 0;
			cell->textures = NULL; // Would be allocated and populated with actual texture handles
			cell->memoryUsed = 0;
			
			// In a full implementation, this would:
			// 1. Query BSP for geometry in this cell's bounds
			// 2. Load models that intersect this cell
			// 3. Load textures referenced by surfaces in this cell
			// 4. Track memory usage
			
			cell->state = CELL_STATE_LOADED;
			vk.cellStreaming.activeCellCount++;
			vk.cellStreaming.cellsLoadedThisFrame++;
		}
	}
	
	// Process unload queue
	for ( uint32_t i = 0; i < vk.cellStreaming.unloadQueueCount; i++ ) {
		uint32_t cellIndex = vk.cellStreaming.unloadQueue[i];
		stream_cell_t *cell = &vk.cellStreaming.cells[cellIndex];
		
		if ( cell->state == CELL_STATE_LOADED ) {
			cell->state = CELL_STATE_UNLOADING;
			
			// Unload cell assets
			// In a full implementation, this would:
			// 1. Unregister models from renderer
			// 2. Unregister textures from renderer
			// 3. Free BSP geometry references
			// 4. Release any GPU resources
			
			if ( cell->models ) {
				ri.Free( cell->models );
				cell->models = NULL;
				cell->modelCount = 0;
			}
			if ( cell->textures ) {
				ri.Free( cell->textures );
				cell->textures = NULL;
				cell->textureCount = 0;
			}
			
			cell->memoryUsed = 0;
			cell->state = CELL_STATE_UNLOADED;
			vk.cellStreaming.activeCellCount--;
			vk.cellStreaming.cellsUnloadedThisFrame++;
		}
	}
}

#endif // USE_VULKAN

