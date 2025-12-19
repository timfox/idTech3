/*
=============================================================================
Cell Streaming System Implementation
=============================================================================
*/

#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
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
			// Query BSP for surfaces within cell bounds and load referenced assets
			cell->modelCount = 0;
			cell->models = NULL;
			cell->textureCount = 0;
			cell->textures = NULL;
			cell->memoryUsed = 0;
			
			// Allocate arrays for tracking assets
			#define MAX_CELL_MODELS 256
			#define MAX_CELL_TEXTURES 512
			qhandle_t *models = (qhandle_t *)ri.Malloc( MAX_CELL_MODELS * sizeof( qhandle_t ) );
			image_t **textures = (image_t **)ri.Malloc( MAX_CELL_TEXTURES * sizeof( image_t * ) );
			uint32_t modelCount = 0;
			uint32_t textureCount = 0;
			
			// Query BSP surfaces within cell bounds
			if ( tr.world && tr.world->surfaces ) {
				// Iterate through world surfaces and check if they intersect cell bounds
				for ( int i = 0; i < tr.world->numsurfaces; i++ ) {
					msurface_t *surf = &tr.world->surfaces[i];
					if ( !surf || !surf->shader ) continue;
					
					// Calculate surface bounds from surface data
					// For now, we'll use a simple approach: check if surface is in cell by
					// examining the surface type and getting bounds from it
					vec3_t surfMins, surfMaxs;
					qboolean hasBounds = qfalse;
					
					// Try to get bounds from surface data based on type
					if ( surf->data ) {
						surfaceType_t *surfaceType = surf->data;
						switch ( *surfaceType ) {
							case SF_GRID:
								{
									srfGridMesh_t *grid = (srfGridMesh_t *)surf->data;
									VectorCopy( grid->meshBounds[0], surfMins );
									VectorCopy( grid->meshBounds[1], surfMaxs );
									hasBounds = qtrue;
								}
								break;
							case SF_TRIANGLES:
								{
									srfTriangles_t *tris = (srfTriangles_t *)surf->data;
									if ( tris->numVerts > 0 && tris->bounds[0][0] <= tris->bounds[1][0] ) {
										// Use precomputed bounds if available
										VectorCopy( tris->bounds[0], surfMins );
										VectorCopy( tris->bounds[1], surfMaxs );
										hasBounds = qtrue;
									}
								}
								break;
							case SF_FACE:
								{
									// Face surfaces don't have easy bounds, skip for now
									hasBounds = qfalse;
								}
								break;
							default:
								// For other surface types, skip bounds check
								// and include them anyway (they're likely small)
								hasBounds = qfalse;
								break;
						}
					}
					
					// Check if surface bounds intersect cell bounds
					qboolean intersects = qtrue;
					if ( hasBounds ) {
						if ( surfMins[0] > cell->worldMax[0] ||
							 surfMaxs[0] < cell->worldMin[0] ||
							 surfMins[1] > cell->worldMax[1] ||
							 surfMaxs[1] < cell->worldMin[1] ||
							 surfMins[2] > cell->worldMax[2] ||
							 surfMaxs[2] < cell->worldMin[2] ) {
							intersects = qfalse;
						}
					}
					// If we don't have bounds, include the surface anyway
					
					if ( intersects ) {
						// Load textures referenced by this surface's shader
						for ( int stage = 0; stage < MAX_SHADER_STAGES; stage++ ) {
							shaderStage_t *pStage = surf->shader->stages[stage];
							if ( !pStage ) continue;
							
							// Check each texture bundle
							for ( int bundle = 0; bundle < NUM_TEXTURE_BUNDLES; bundle++ ) {
								if ( pStage->bundle[bundle].image[0] ) {
									image_t *img = pStage->bundle[bundle].image[0];
									
									// Check if texture already tracked
									qboolean found = qfalse;
									for ( uint32_t j = 0; j < textureCount; j++ ) {
										if ( textures[j] == img ) {
											found = qtrue;
											break;
										}
									}
									
									if ( !found && textureCount < MAX_CELL_TEXTURES ) {
										textures[textureCount++] = img;
										// Estimate texture memory (rough approximation)
										cell->memoryUsed += img->uploadWidth * img->uploadHeight * 4; // RGBA8
									}
								}
							}
						}
					}
				}
			}
			
			// Query brush models within cell bounds
			// Brush models are already loaded as part of the BSP, but we track them for reference
			if ( tr.world && tr.world->bmodels ) {
				// Iterate through all loaded models to find brush models
				for ( int i = 1; i < tr.numModels; i++ ) {
					model_t *mod = tr.models[i];
					if ( !mod || mod->type != MOD_BRUSH || !mod->bmodel ) {
						continue;
					}
					
					bmodel_t *bmodel = mod->bmodel;
					
					// Check if brush model bounds intersect cell bounds
					// Simple AABB intersection test
					qboolean intersects = qtrue;
					if ( bmodel->bounds[0][0] > cell->worldMax[0] ||
						 bmodel->bounds[1][0] < cell->worldMin[0] ||
						 bmodel->bounds[0][1] > cell->worldMax[1] ||
						 bmodel->bounds[1][1] < cell->worldMin[1] ||
						 bmodel->bounds[0][2] > cell->worldMax[2] ||
						 bmodel->bounds[1][2] < cell->worldMin[2] ) {
						intersects = qfalse;
					}
					
					if ( intersects ) {
						// Check if model already tracked
						qboolean found = qfalse;
						for ( uint32_t j = 0; j < modelCount; j++ ) {
							if ( models[j] == mod->index ) {
								found = qtrue;
								break;
							}
						}
						
						if ( !found && modelCount < MAX_CELL_MODELS ) {
							models[modelCount++] = mod->index;
							// Estimate model memory (rough approximation based on surface count)
							cell->memoryUsed += bmodel->numSurfaces * 1024; // ~1KB per surface estimate
						}
					}
				}
			}
			
			// Store loaded assets
			if ( modelCount > 0 ) {
				cell->models = (qhandle_t *)ri.Malloc( modelCount * sizeof( qhandle_t ) );
				Com_Memcpy( cell->models, models, modelCount * sizeof( qhandle_t ) );
				cell->modelCount = modelCount;
			}
			ri.Free( models );
			
			if ( textureCount > 0 ) {
				cell->textures = (image_t **)ri.Malloc( textureCount * sizeof( image_t * ) );
				Com_Memcpy( cell->textures, textures, textureCount * sizeof( image_t * ) );
				cell->textureCount = textureCount;
			}
			ri.Free( textures );
			
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
			// Note: Models and textures are reference-counted by the renderer,
			// so we don't actually unload them here - we just stop tracking them.
			// The renderer will unload them when their reference count reaches zero.
			
			if ( cell->models ) {
				// Models are managed by the renderer's model cache
				// We just stop tracking them - they'll be unloaded automatically
				// when no longer referenced
				ri.Free( cell->models );
				cell->models = NULL;
				cell->modelCount = 0;
			}
			
			if ( cell->textures ) {
				// Textures are managed by the renderer's image cache
				// We just stop tracking them - they'll be unloaded automatically
				// when no longer referenced by any surfaces
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

