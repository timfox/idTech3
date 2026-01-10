/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/
#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "../renderercommon/tr_frame_graph.h"
#include "vk_utils.h"  // For memory validation functions

/*
=====================
R_ClearPerformanceCounters
=====================
*/
static void R_ClearPerformanceCounters( void ) {
	atomic_store_explicit(&tr.pc.c_sphere_cull_patch_in, 0, memory_order_relaxed);
	atomic_store_explicit(&tr.pc.c_sphere_cull_patch_clip, 0, memory_order_relaxed);
	atomic_store_explicit(&tr.pc.c_sphere_cull_patch_out, 0, memory_order_relaxed);
	atomic_store_explicit(&tr.pc.c_box_cull_patch_in, 0, memory_order_relaxed);
	atomic_store_explicit(&tr.pc.c_box_cull_patch_clip, 0, memory_order_relaxed);
	atomic_store_explicit(&tr.pc.c_box_cull_patch_out, 0, memory_order_relaxed);
	atomic_store_explicit(&tr.pc.c_sphere_cull_md3_in, 0, memory_order_relaxed);
	atomic_store_explicit(&tr.pc.c_sphere_cull_md3_clip, 0, memory_order_relaxed);
	atomic_store_explicit(&tr.pc.c_sphere_cull_md3_out, 0, memory_order_relaxed);
	atomic_store_explicit(&tr.pc.c_box_cull_md3_in, 0, memory_order_relaxed);
	atomic_store_explicit(&tr.pc.c_box_cull_md3_clip, 0, memory_order_relaxed);
	atomic_store_explicit(&tr.pc.c_box_cull_md3_out, 0, memory_order_relaxed);
	atomic_store_explicit(&tr.pc.c_leafs, 0, memory_order_relaxed);
	atomic_store_explicit(&tr.pc.c_dlightSurfaces, 0, memory_order_relaxed);
	atomic_store_explicit(&tr.pc.c_dlightSurfacesCulled, 0, memory_order_relaxed);
#ifdef USE_PMLIGHT
	atomic_store_explicit(&tr.pc.c_light_cull_out, 0, memory_order_relaxed);
	atomic_store_explicit(&tr.pc.c_light_cull_in, 0, memory_order_relaxed);
	atomic_store_explicit(&tr.pc.c_lit_leafs, 0, memory_order_relaxed);
	atomic_store_explicit(&tr.pc.c_lit_surfs, 0, memory_order_relaxed);
	atomic_store_explicit(&tr.pc.c_lit_culls, 0, memory_order_relaxed);
	atomic_store_explicit(&tr.pc.c_lit_masks, 0, memory_order_relaxed);
#endif
}

/*
=====================
RB_ClearPerformanceCounters
=====================
*/
static void RB_ClearPerformanceCounters( void ) {
	atomic_store_explicit(&backEnd.pc.c_surfaces, 0, memory_order_relaxed);
	atomic_store_explicit(&backEnd.pc.c_shaders, 0, memory_order_relaxed);
	atomic_store_explicit(&backEnd.pc.c_vertexes, 0, memory_order_relaxed);
	atomic_store_explicit(&backEnd.pc.c_indexes, 0, memory_order_relaxed);
	atomic_store_explicit(&backEnd.pc.c_totalIndexes, 0, memory_order_relaxed);
	backEnd.pc.c_overDraw = 0;
	atomic_store_explicit(&backEnd.pc.c_dlightVertexes, 0, memory_order_relaxed);
	atomic_store_explicit(&backEnd.pc.c_dlightIndexes, 0, memory_order_relaxed);
	atomic_store_explicit(&backEnd.pc.c_flareAdds, 0, memory_order_relaxed);
	atomic_store_explicit(&backEnd.pc.c_flareTests, 0, memory_order_relaxed);
	atomic_store_explicit(&backEnd.pc.c_flareRenders, 0, memory_order_relaxed);
#ifdef USE_PMLIGHT
	atomic_store_explicit(&backEnd.pc.c_lit_batches, 0, memory_order_relaxed);
	atomic_store_explicit(&backEnd.pc.c_lit_vertices, 0, memory_order_relaxed);
	atomic_store_explicit(&backEnd.pc.c_lit_indices, 0, memory_order_relaxed);
	atomic_store_explicit(&backEnd.pc.c_lit_indices_latecull_in, 0, memory_order_relaxed);
	atomic_store_explicit(&backEnd.pc.c_lit_indices_latecull_out, 0, memory_order_relaxed);
	atomic_store_explicit(&backEnd.pc.c_lit_vertices_lateculltest, 0, memory_order_relaxed);
#endif
}

/*
=====================
R_PerformanceCounters
=====================
*/
static void R_PerformanceCounters( void ) {
	if ( !r_speeds->integer ) {
		// clear the counters even if we aren't printing
		R_ClearPerformanceCounters();
		RB_ClearPerformanceCounters();
		return;
	}

	if (r_speeds->integer == 1) {
		ri.Printf (PRINT_ALL, "%i/%i shaders/surfs %i leafs %i verts %i/%i tris %.2f mtex\n",
			(int)atomic_load_explicit(&backEnd.pc.c_shaders, memory_order_relaxed), 
			(int)atomic_load_explicit(&backEnd.pc.c_surfaces, memory_order_relaxed), 
			(int)atomic_load_explicit(&tr.pc.c_leafs, memory_order_relaxed), 
			(int)atomic_load_explicit(&backEnd.pc.c_vertexes, memory_order_relaxed), 
			(int)atomic_load_explicit(&backEnd.pc.c_indexes, memory_order_relaxed)/3, 
			(int)atomic_load_explicit(&backEnd.pc.c_totalIndexes, memory_order_relaxed)/3, 
			R_SumOfUsedImages()/1000000.0); 
	} else if (r_speeds->integer == 2) {
		ri.Printf (PRINT_ALL, "(patch) %i sin %i sclip  %i sout %i bin %i bclip %i bout\n",
			(int)atomic_load_explicit(&tr.pc.c_sphere_cull_patch_in, memory_order_relaxed), 
			(int)atomic_load_explicit(&tr.pc.c_sphere_cull_patch_clip, memory_order_relaxed), 
			(int)atomic_load_explicit(&tr.pc.c_sphere_cull_patch_out, memory_order_relaxed), 
			(int)atomic_load_explicit(&tr.pc.c_box_cull_patch_in, memory_order_relaxed), 
			(int)atomic_load_explicit(&tr.pc.c_box_cull_patch_clip, memory_order_relaxed), 
			(int)atomic_load_explicit(&tr.pc.c_box_cull_patch_out, memory_order_relaxed) );
		ri.Printf (PRINT_ALL, "(md3) %i sin %i sclip  %i sout %i bin %i bclip %i bout\n",
			(int)atomic_load_explicit(&tr.pc.c_sphere_cull_md3_in, memory_order_relaxed), 
			(int)atomic_load_explicit(&tr.pc.c_sphere_cull_md3_clip, memory_order_relaxed), 
			(int)atomic_load_explicit(&tr.pc.c_sphere_cull_md3_out, memory_order_relaxed), 
			(int)atomic_load_explicit(&tr.pc.c_box_cull_md3_in, memory_order_relaxed), 
			(int)atomic_load_explicit(&tr.pc.c_box_cull_md3_clip, memory_order_relaxed), 
			(int)atomic_load_explicit(&tr.pc.c_box_cull_md3_out, memory_order_relaxed) );
	} else if (r_speeds->integer == 3) {
		ri.Printf (PRINT_ALL, "viewcluster: %i\n", tr.viewCluster );
	} else if (r_speeds->integer == 4) {
		if ( atomic_load_explicit(&backEnd.pc.c_dlightVertexes, memory_order_relaxed) ) {
			ri.Printf (PRINT_ALL, "dlight srf:%i  culled:%i  verts:%i  tris:%i\n", 
				(int)atomic_load_explicit(&tr.pc.c_dlightSurfaces, memory_order_relaxed), 
				(int)atomic_load_explicit(&tr.pc.c_dlightSurfacesCulled, memory_order_relaxed),
				(int)atomic_load_explicit(&backEnd.pc.c_dlightVertexes, memory_order_relaxed), 
				(int)atomic_load_explicit(&backEnd.pc.c_dlightIndexes, memory_order_relaxed) / 3 );
		}
	} 
	else if (r_speeds->integer == 5 )
	{
		ri.Printf( PRINT_ALL, "zFar: %.0f\n", tr.viewParms.zFar );
	}
	else if (r_speeds->integer == 6 )
	{
		ri.Printf( PRINT_ALL, "flare adds:%i tests:%i renders:%i\n", 
			(int)atomic_load_explicit(&backEnd.pc.c_flareAdds, memory_order_relaxed), 
			(int)atomic_load_explicit(&backEnd.pc.c_flareTests, memory_order_relaxed), 
			(int)atomic_load_explicit(&backEnd.pc.c_flareRenders, memory_order_relaxed) );
	}

	R_ClearPerformanceCounters();
	RB_ClearPerformanceCounters();
}


/*
====================
R_IssueRenderCommands
====================
*/
void R_IssueRenderCommands( void ) {
	renderCommandList_t	*cmdList;

	cmdList = &backEndData->commands;

	// add an end-of-list command
	*(int *)(cmdList->cmds + cmdList->used) = RC_END_OF_LIST;

	// clear it out, in case this is a sync and not a buffer flip
	cmdList->used = 0;

	if ( backEnd.screenshotMask == 0 ) {
		if ( ri.CL_IsMinimized() )
			return; // skip backend when minimized
		if ( backEnd.throttle )
			return; // or throttled on demand
	} else {
#ifdef USE_VULKAN
		if ( ri.CL_IsMinimized() && !RE_CanMinimize() ) {
			backEnd.screenshotMask = 0;
			return;
		}
#endif
	}

	// actually start the commands going
	if ( !r_skipBackEnd->integer ) {
		// let it start on the new batch
		RB_ExecuteRenderCommands( cmdList->cmds );
	}

}

// Frame graph execute wrapper
static void RG_ExecuteRenderCommands( void *user ) {
	(void)user;
	R_IssueRenderCommands();
}

// Light clustering pass
static void RG_ExecuteLightClusters( void *user ) {
	(void)user;
	R_BuildLightClusters();
}

// Placeholder post pass (currently no-op). Extend later for post effects.
static void RG_ExecutePostPass( void *user ) {
	(void)user;
}


/*
============
R_GetCommandBufferReserved

make sure there is enough command space
============
*/
static void *R_GetCommandBufferReserved( int bytes, int reservedBytes ) {
	renderCommandList_t	*cmdList;

	cmdList = &backEndData->commands;
	bytes = PAD(bytes, sizeof(void *));

	// always leave room for the end of list command
	if ( cmdList->used + bytes + sizeof( int ) + reservedBytes > MAX_RENDER_COMMANDS ) {
	if ( (size_t)bytes > MAX_RENDER_COMMANDS - sizeof( int ) ) {
			ri.Error( ERR_FATAL, "R_GetCommandBuffer: bad size %i", bytes );
		}
		// if we run out of room, just start dropping commands
		return NULL;
	}

	cmdList->used += bytes;

	return cmdList->cmds + cmdList->used - bytes;
}


/*
=============
R_GetCommandBuffer
returns NULL if there is not enough space for important commands
=============
*/
void *R_GetCommandBuffer( int bytes ) {
#ifdef USE_VULKAN
	tr.lastRenderCommand = RC_END_OF_LIST;
#endif
	return R_GetCommandBufferReserved( bytes, PAD( sizeof( swapBuffersCommand_t ), sizeof(void *) ) );
}


/*
=============
R_AddDrawSurfCmd
=============
*/
void R_AddDrawSurfCmd( drawSurf_t *drawSurfs, int numDrawSurfs ) {
	drawSurfsCommand_t	*cmd;

	cmd = R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_DRAW_SURFS;

	cmd->drawSurfs = drawSurfs;
	cmd->numDrawSurfs = numDrawSurfs;

	cmd->refdef = tr.refdef;
	cmd->viewParms = tr.viewParms;

#ifdef USE_VULKAN
	tr.numDrawSurfCmds++;
	if ( tr.drawSurfCmd == NULL ) {
		tr.drawSurfCmd = cmd;
	}
#endif
}

#ifdef VK_CUBEMAP
/*
=============
R_AddConvolveCubemapsCmd
=============
*/
void R_AddConvolveCubemapCmd( cubemap_t *cubemap , int cubemapId ) {
	convolveCubemapCommand_t	*cmd;
	
	cmd = (convolveCubemapCommand_t *)R_GetCommandBuffer( sizeof( *cmd ));
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_CONVOLVECUBEMAP;
	
	cmd->cubemap = cubemap;
	cmd->cubemapId = cubemapId;
}
#endif

/*
=============
RE_SetColor

Passing NULL will set the color to white
=============
*/
void RE_SetColor( const float *rgba ) {
	setColorCommand_t	*cmd;

	if ( !tr.registered ) {
		return;
	}
	cmd = R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_SET_COLOR;
	if ( !rgba ) {
		rgba = colorWhite;
	}

	cmd->color[0] = rgba[0];
	cmd->color[1] = rgba[1];
	cmd->color[2] = rgba[2];
	cmd->color[3] = rgba[3];
}


/*
=============
RE_StretchPic
=============
*/
void RE_StretchPic( float x, float y, float w, float h,
					float s1, float t1, float s2, float t2, qhandle_t hShader ) {
	stretchPicCommand_t	*cmd;

	if ( !tr.registered ) {
		return;
	}
	cmd = R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_STRETCH_PIC;
	cmd->shader = R_GetShaderByHandle( hShader );
	cmd->x = x;
	cmd->y = y;
	cmd->w = w;
	cmd->h = h;
	cmd->s1 = s1;
	cmd->t1 = t1;
	cmd->s2 = s2;
	cmd->t2 = t2;
}

#define MODE_RED_CYAN	1
#define MODE_RED_BLUE	2
#define MODE_RED_GREEN	3
#define MODE_GREEN_MAGENTA 4
#define MODE_MAX	MODE_GREEN_MAGENTA

#ifndef USE_VULKAN
static void R_SetColorMode(GLboolean *rgba, stereoFrame_t stereoFrame, int colormode)
{
	rgba[0] = rgba[1] = rgba[2] = rgba[3] = GL_TRUE;

	if(colormode > MODE_MAX)
	{
		if(stereoFrame == STEREO_LEFT)
			stereoFrame = STEREO_RIGHT;
		else if(stereoFrame == STEREO_RIGHT)
			stereoFrame = STEREO_LEFT;

		colormode -= MODE_MAX;
	}

	if(colormode == MODE_GREEN_MAGENTA)
	{
		if(stereoFrame == STEREO_LEFT)
			rgba[0] = rgba[2] = GL_FALSE;
		else if(stereoFrame == STEREO_RIGHT)
			rgba[1] = GL_FALSE;
	}
	else
	{
		if(stereoFrame == STEREO_LEFT)
			rgba[1] = rgba[2] = GL_FALSE;
		else if(stereoFrame == STEREO_RIGHT)
		{
			rgba[0] = GL_FALSE;

			if(colormode == MODE_RED_BLUE)
				rgba[1] = GL_FALSE;
			else if(colormode == MODE_RED_GREEN)
				rgba[2] = GL_FALSE;
		}
	}
}
#endif


/*
====================
RE_BeginFrame

If running in stereo, RE_BeginFrame will be called twice
for each RE_EndFrame
====================
*/
void RE_BeginFrame( stereoFrame_t stereoFrame ) {
	drawBufferCommand_t *cmd;

	// Safety check: if Vulkan is not properly initialized, skip rendering to avoid crashes
#ifdef USE_VULKAN
	// Check for device lost FIRST, even if swapchain isn't ready - this allows recovery attempts
	// Attempt immediate recovery instead of disabling rendering completely
	if (vk.device_lost) {
		// Don't attempt recovery during shutdown
		if (!vk.active) {
			ri.Printf(PRINT_DEVELOPER, "Vulkan: Device is lost but renderer is shutting down, skipping recovery\n");
			return;
		}

		// Only log once per second to avoid spam
		static int last_log_time = 0;
		static int last_recovery_attempt = -1; // Initialize to -1 to trigger immediate first attempt
		static int frame_count = 0;
		static int recovery_attempt_count = 0; // Track total recovery attempts
		static const int MAX_RECOVERY_ATTEMPTS = 50; // Limit recovery attempts to prevent infinite loops
		frame_count++;
		int current_time = ri.Milliseconds();
		
		// Log first few frames to confirm RE_BeginFrame is being called
		if (frame_count <= 3) {
			ri.Printf(PRINT_ALL, "Vulkan: RE_BeginFrame called with device_lost=true (frame %d, time=%d)\n", frame_count, current_time);
		}
		
		if (current_time - last_log_time > 1000) {
			ri.Printf(PRINT_WARNING, "Vulkan: Device is lost - attempting recovery. Rendering may be limited.\n");
			last_log_time = current_time;
		}
		
		// Attempt device recovery with delays to allow GPU driver to recover
		// First attempt after 1 second (gives driver time to stabilize), then every 2 seconds
		// Use -1 as sentinel to ensure first attempt happens even if Milliseconds() returns 0
		static int initial_delay_passed = 0;
		int recovery_delay = (initial_delay_passed == 0) ? 1000 : 2000; // 1s first attempt (driver needs time), then 2s
		
		// Check recovery attempt limit
		if (recovery_attempt_count >= MAX_RECOVERY_ATTEMPTS) {
			if (current_time - last_log_time > 5000) { // Log every 5 seconds when max attempts reached
				ri.Printf(PRINT_WARNING, "Vulkan: Maximum recovery attempts (%d) reached. Device recovery may not be possible.\n", 
					MAX_RECOVERY_ATTEMPTS);
				last_log_time = current_time;
			}
			return; // Stop attempting recovery after max attempts
		}

		if (last_recovery_attempt == -1 || current_time - last_recovery_attempt > recovery_delay) {
			if (initial_delay_passed == 0) {
				initial_delay_passed = 1;
			}
			last_recovery_attempt = current_time;
			recovery_attempt_count++;
			ri.Printf(PRINT_ALL, "Vulkan: Attempting device recovery (attempt %d/%d, frame %d, time=%d, delay=%dms)...\n", 
				recovery_attempt_count, MAX_RECOVERY_ATTEMPTS, frame_count, current_time, recovery_delay);
			
			// First, test if device is responsive with a simple operation
			if (vk.device != VK_NULL_HANDLE && vk.physical_device != VK_NULL_HANDLE) {
				// Test device responsiveness using the existing wrapper function
				// This will safely check if device is responsive
				qboolean was_device_lost = vk.device_lost;
				
				// Only test device if it wasn't already lost (to avoid redundant checks)
				if (!was_device_lost) {
					vk_wait_idle();
					// If device was lost during wait, it will be set in vk_wait_idle
					if (vk.device_lost) {
						ri.Printf(PRINT_WARNING, "Vulkan: Device lost during wait test. Will retry in %d seconds.\n", recovery_delay / 1000);
						return; // Skip swapchain operations if device is still lost
					}
				} else {
					// Device was already lost - wait longer before attempting recovery
					// Don't try to recreate swapchain immediately - driver needs time to stabilize
					// The NVIDIA driver crashes if we try to access it too soon after device loss
					ri.Printf(PRINT_WARNING, "Vulkan: Device is lost - waiting for driver to stabilize before recovery attempt.\n");
					ri.Printf(PRINT_WARNING, "Vulkan: Will retry recovery in %d seconds.\n", recovery_delay / 1000);
					return; // Wait longer - don't attempt swapchain recreation yet
				}
				
				// Only proceed with swapchain recreation if device wasn't lost before
				// If device was lost, we've already returned above
				was_device_lost = vk.device_lost;
				
				// If swapchain doesn't exist, try to create it; otherwise recreate it
				VkResult swapchain_result = VK_SUCCESS;
				if (vk.swapchain == VK_NULL_HANDLE) {
					ri.Printf(PRINT_ALL, "Vulkan: Swapchain missing, attempting to create it for recovery...\n");
					// Try to create swapchain - this requires surface to exist
					if (vk.surface != VK_NULL_HANDLE) {
						// Use safe version that returns error code instead of calling ri.Error
						swapchain_result = vk_recreate_swapchain_safe();
					} else {
						ri.Printf(PRINT_WARNING, "Vulkan: Cannot create swapchain - surface not available. Will retry in %d seconds.\n", recovery_delay / 1000);
						vk.device_lost = was_device_lost; // Restore flag
						swapchain_result = VK_ERROR_SURFACE_LOST_KHR; // Mark as failed
					}
				} else {
					// Try recreating existing swapchain - this will fail if device is still lost
					ri.Printf(PRINT_ALL, "Vulkan: Attempting swapchain recreation to test device recovery...\n");
					swapchain_result = vk_recreate_swapchain_safe();
				}
				
				// Handle swapchain recreation errors gracefully
				if (swapchain_result != VK_SUCCESS) {
					if (swapchain_result == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
						ri.Printf(PRINT_WARNING, "Vulkan: Swapchain recreation failed - OUT_OF_DEVICE_MEMORY. GPU driver needs more time to recover.\n");
						ri.Printf(PRINT_WARNING, "Vulkan: This may indicate the driver hasn't fully freed memory yet. Will retry in 30 seconds.\n");
						// Increase retry interval significantly for out-of-memory errors - driver needs more time
						last_recovery_attempt = current_time - (recovery_delay - 30000); // Allow retry in 30 seconds
						vk.device_lost = qtrue; // Mark as lost to prevent further attempts
						return; // Skip further recovery attempts - don't try to acquire image
					} else if (swapchain_result == VK_ERROR_DEVICE_LOST) {
						ri.Printf(PRINT_WARNING, "Vulkan: Device lost during swapchain recreation. Will retry in %d seconds.\n", recovery_delay / 1000);
						vk.device_lost = qtrue; // Ensure flag is set
						// Don't call vk_reset_memory_tracking_on_device_lost() here - it was already called in vk_recreate_swapchain_safe()
						return; // Device still lost, skip further recovery attempts - don't try to acquire image
					} else {
						ri.Printf(PRINT_WARNING, "Vulkan: Swapchain recreation failed: %s. Will retry in %d seconds.\n", 
							vk_result_string(swapchain_result), recovery_delay / 1000);
						// For other errors, restore previous state
						vk.device_lost = was_device_lost;
						return; // Recovery failed, skip further attempts - don't try to acquire image
					}
				}
				
				// Test if swapchain creation/recreation succeeded by trying to acquire an image
				// Only test if swapchain recreation succeeded (swapchain_result == VK_SUCCESS)
				if (swapchain_result == VK_SUCCESS && qvkAcquireNextImageKHR && vk.swapchain != VK_NULL_HANDLE) {
					uint32_t test_index;
					VkResult test_result = qvkAcquireNextImageKHR(vk.device, vk.swapchain, 0, 
						vk.image_available, VK_NULL_HANDLE, &test_index);
					
					if (test_result == VK_SUCCESS || test_result == VK_SUBOPTIMAL_KHR) {
						ri.Printf(PRINT_ALL, "Vulkan: Device recovery successful! Resuming rendering (recovered after %d attempts).\n", 
							recovery_attempt_count);
						vk.device_lost = qfalse; // Device recovered - clear flag to allow rendering
						recovery_attempt_count = 0; // Reset counter on successful recovery
						// Image was acquired, we'll use it in the normal flow
						if (test_result == VK_SUCCESS) {
							vk.current_swapchain_image_index = test_index;
						}
						// Device recovered - fall through to normal rendering path
					} else if (test_result == VK_ERROR_DEVICE_LOST) {
						ri.Printf(PRINT_WARNING, "Vulkan: Device still lost during recovery test. Will retry in %d seconds.\n", recovery_delay / 1000);
						vk.device_lost = qtrue; // Restore flag
						return; // Still lost, skip rendering
					} else {
						ri.Printf(PRINT_WARNING, "Vulkan: Device recovery test failed (result: %d). Will retry in %d seconds.\n", test_result, recovery_delay / 1000);
						vk.device_lost = was_device_lost; // Restore previous state
						return; // Recovery failed, skip rendering
					}
				} else if (vk.swapchain == VK_NULL_HANDLE) {
					ri.Printf(PRINT_WARNING, "Vulkan: Swapchain creation failed during recovery. Will retry in %d seconds.\n", recovery_delay / 1000);
					vk.device_lost = was_device_lost; // Restore flag
					return; // Swapchain not ready, skip rendering
				} else {
					vk.device_lost = was_device_lost; // Restore previous state
					return; // Cannot test recovery, skip rendering
				}
			} else {
				ri.Printf(PRINT_WARNING, "Vulkan: Cannot attempt recovery - device or physical device not available.\n");
				return; // Cannot recover, skip rendering
			}
		} else {
			// Recovery attempt not ready yet - skip rendering for now
			return;
		}
		
		// If we reach here, device was recovered - continue with normal rendering
		// (device_lost flag was cleared above)
	}
	
	// Now check if Vulkan is properly initialized (after handling device lost)
	if (!vk.active || vk.device == VK_NULL_HANDLE || vk.swapchain == VK_NULL_HANDLE || vk.cmd == NULL) {
		ri.Printf(PRINT_DEVELOPER, "Vulkan: Skipping frame - not fully initialized\n");
		return;
	}
#endif

	// Initialize frame ready flag
	vk.cmd->frame_ready = qfalse;

#ifdef USE_VULKAN
	// Memory validation - check for corruption periodically (every 1000 frames to avoid performance impact)
	static int frame_count_for_validation = 0;
	if (++frame_count_for_validation >= 1000) {
		frame_count_for_validation = 0;
		if (vk_memory_tracker.leak_detection_enabled && !vk.device_lost) {
			vk_validate_memory_state();
		}
	}
#endif

	// If we're in headless mode or cinematic state, skip swapchain acquisitions to avoid crashes
#ifdef USE_VULKAN
    // Skip swapchain acquisition in headless mode
    if ( VK_IsHeadless() ) {
        ri.Printf( PRINT_DEVELOPER, "Vulkan: Skipping swapchain acquisition (headless or cinematic)\n" );
        return;
    }
#endif

	if ( !tr.registered ) {
		return;
	}

	glState.finishCalled = qfalse;

#ifdef USE_VULKAN
	backEnd.doneBloom = qfalse;
#endif

	backEnd.color2D.u32 = ~0U;

	// For real Vulkan devices, acquire next swapchain image
	// Skip if device is lost - prevents operations on invalid device
	if (!vk.device_lost && vk.device != (VkDevice)0x20000000 && vk.active && vk.swapchain != VK_NULL_HANDLE && qvkAcquireNextImageKHR) {
		// Acquire next image from swapchain
		VkResult result = qvkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX,
			vk.image_available, VK_NULL_HANDLE, &vk.current_swapchain_image_index);

		if (result == VK_ERROR_DEVICE_LOST) {
			vk.device_lost = qtrue;
			vk_reset_memory_tracking_on_device_lost(); // Reset memory tracking so recovery knows memory is available
			ri.Printf(PRINT_ERROR, "Vulkan: Device lost during image acquisition - GPU driver issue\n");
			return;
		} else if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			// Swapchain is out of date, need to recreate
			ri.Printf(PRINT_WARNING, "Vulkan: Swapchain out of date, skipping frame\n");
			return;
		} else if (result == VK_TIMEOUT) {
			// Timeout acquiring swapchain image (common in headless environments)
			ri.Printf(PRINT_WARNING, "Vulkan: Timeout acquiring swapchain image, skipping frame (headless mode?)\n");
			return;
		} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			// Non-recoverable swapchain acquisition error (after all recovery attempts)
			// This should be rare - most errors are handled above
			ri.Printf(PRINT_ERROR, "Vulkan: Failed to acquire swapchain image: %s (result: %d)\n", 
				vk_result_string(result), result);
			ri.Error(ERR_DROP, "Vulkan: Cannot continue without swapchain image");
		}
		ri.Printf(PRINT_DEVELOPER, "Vulkan: Acquired swapchain image %u\n", vk.current_swapchain_image_index);
	}

	tr.frameCount++;
	tr.frameSceneNum = 0;

	if ( ( cmd = R_GetCommandBuffer( sizeof( *cmd ) ) ) == NULL )
		return;

	cmd->commandId = RC_DRAW_BUFFER;

#ifdef USE_VULKAN
	tr.lastRenderCommand = RC_DRAW_BUFFER;
#endif

	if ( glConfig.stereoEnabled ) {
		if ( stereoFrame == STEREO_LEFT ) {
			cmd->buffer = 0; // Left eye buffer (Vulkan doesn't use GL_BACK_LEFT)
		} else if ( stereoFrame == STEREO_RIGHT ) {
			cmd->buffer = 1; // Right eye buffer (Vulkan doesn't use GL_BACK_RIGHT)
		} else {
			// Programming error: invalid stereo frame when stereo is enabled
			ri.Printf(PRINT_WARNING, "RE_BeginFrame: Stereo is enabled, but stereoFrame was %i (expected STEREO_LEFT or STEREO_RIGHT)\n", stereoFrame);
			// Continue with center frame as fallback
			cmd->buffer = 0; // Default to left/center (Vulkan doesn't use GL_BACK)
		}
	} else {
		if ( stereoFrame != STEREO_CENTER ) {
			// Programming error: stereo frame set when stereo is disabled
			ri.Printf(PRINT_WARNING, "RE_BeginFrame: Stereo is disabled, but stereoFrame was %i (expected STEREO_CENTER)\n", stereoFrame);
			// Continue with center frame
		}

#ifdef USE_VULKAN
		cmd->buffer = 0;
#else
		if ( !Q_stricmp( r_drawBuffer->string, "GL_FRONT" ) )
			cmd->buffer = 0; // Front buffer (Vulkan doesn't use GL_FRONT)
		else
			cmd->buffer = 0; // Back buffer (Vulkan doesn't use GL_BACK)
#endif
	}

#ifndef USE_BUFFER_CLEAR
#ifdef USE_VULKAN
	if ( r_fastsky->integer && vk.clearAttachment ) {
#else
	if ( r_fastsky->integer ) {
#endif
		if ( stereoFrame != STEREO_RIGHT ) {
			clearColorCommand_t *clrcmd; 
			if ( ( clrcmd = R_GetCommandBuffer( sizeof( *clrcmd ) ) ) == NULL )
				return;
			clrcmd->commandId = RC_CLEARCOLOR;
		}
	}
#endif // USE_BUFFER_CLEAR

	tr.refdef.stereoFrame = stereoFrame;
}


/*
=============
RE_EndFrame

Returns the number of msec spent in the back end
=============
*/
void RE_EndFrame( int *frontEndMsec, int *backEndMsec ) {
	swapBuffersCommand_t *cmd;
	rg_frame_graph_t graph;

	if ( !tr.registered ) {
		return;
	}

	// Update async font loading
#ifdef USE_JOBSYSTEM
	extern void RE_UpdateAsyncFonts(void);
	RE_UpdateAsyncFonts();
#endif

	cmd = R_GetCommandBufferReserved( sizeof( *cmd ), 0 );
	if ( !cmd ) {
		return;
	}
	cmd->commandId = RC_SWAP_BUFFERS;

	// Build a minimal frame graph: light clustering -> main -> post.
	RG_Reset( &graph );
	{
		rg_pass_desc_t pass = {
			.name = "light_clusters",
			.execute = RG_ExecuteLightClusters,
			.user = NULL,
			.flags = 0
		};
		RG_AddPass( &graph, &pass );
	}
	{
		rg_pass_desc_t pass = {
			.name = "main",
			.execute = RG_ExecuteRenderCommands,
			.user = NULL,
			.flags = 0
		};
		RG_AddPass( &graph, &pass );
	}
	{
		rg_pass_desc_t pass = {
			.name = "post",
			.execute = RG_ExecutePostPass,
			.user = NULL,
			.flags = 0
		};
		RG_AddPass( &graph, &pass );
	}
	RG_Execute( &graph, NULL );

	R_PerformanceCounters();

	R_InitNextFrame();

	if ( frontEndMsec ) {
		*frontEndMsec = tr.frontEndMsec;
	}
	tr.frontEndMsec = 0;

	if ( backEndMsec ) {
		*backEndMsec = backEnd.pc.msec;
	}
	backEnd.pc.msec = 0;

	backEnd.throttle = qfalse;

	// recompile GPU shaders if needed
	if ( ri.Cvar_CheckGroup( CVG_RENDERER ) ) {

		// texturemode stuff
		if ( r_textureMode->modified ) {
			GL_TextureMode( r_textureMode->string );
		}

		// gamma stuff
		if ( r_gamma->modified ) {
			R_SetColorMappings();
		}

#ifdef USE_VULKAN
		vk_update_post_process_pipelines();
#endif

		// Vulkan: Present the rendered frame
		// Skip if device is lost - prevents operations on invalid device
		if (!vk.device_lost && vk.device != (VkDevice)0x20000000 && vk.active && vk.swapchain != VK_NULL_HANDLE && qvkQueuePresentKHR) {
			// Submit rendering commands and present
			VkPresentInfoKHR present_info = {
				.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
				.pNext = NULL,
				.waitSemaphoreCount = 1,
				.pWaitSemaphores = &vk.rendering_finished,
				.swapchainCount = 1,
				.pSwapchains = &vk.swapchain,
				.pImageIndices = &vk.current_swapchain_image_index,
				.pResults = NULL
			};

			VkResult result = qvkQueuePresentKHR(vk.queue, &present_info);
			if (result == VK_ERROR_DEVICE_LOST) {
				vk.device_lost = qtrue;
				vk_reset_memory_tracking_on_device_lost(); // Reset memory tracking so recovery knows memory is available
				ri.Printf(PRINT_ERROR, "Vulkan: Device lost during present - GPU driver issue\n");
				// Don't reset semaphores if device is lost - they may be invalid
			} else if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
				// Swapchain needs recreation
				ri.Printf(PRINT_WARNING, "Vulkan: Swapchain needs recreation\n");
				// Still reset semaphores - swapchain recreation will handle them
				vk.image_available = VK_NULL_HANDLE;
				vk.rendering_finished = VK_NULL_HANDLE;
			} else if (result != VK_SUCCESS) {
				ri.Printf(PRINT_WARNING, "Vulkan: Failed to present: %d\n", result);
				// Don't reset semaphores on error - they may still be in use
			} else {
				// Success - safe to reset semaphores for next frame
				vk.image_available = VK_NULL_HANDLE;
				vk.rendering_finished = VK_NULL_HANDLE;
			}
		}

		ri.Cvar_ResetGroup( CVG_RENDERER, qtrue /* reset modified flags */ );
	}

#ifdef USE_VULKAN
	// Memory validation on shutdown - detect leaks if enabled
	if (vk_memory_tracker.leak_detection_enabled && !vk.device_lost) {
		vk_detect_memory_leaks();
	}
#endif
}


/*
=============
RE_TakeVideoFrame
=============
*/
void RE_TakeVideoFrame( int width, int height,
		byte *captureBuffer, byte *encodeBuffer, qboolean motionJpeg )
{
	videoFrameCommand_t	*cmd;

	if( !tr.registered ) {
		return;
	}

	backEnd.screenshotMask |= SCREENSHOT_AVI;

	cmd = &backEnd.vcmd;

	//cmd->commandId = RC_VIDEOFRAME;

	cmd->width = width;
	cmd->height = height;
	cmd->captureBuffer = captureBuffer;
	cmd->encodeBuffer = encodeBuffer;
	cmd->motionJpeg = motionJpeg;
}


void RE_ThrottleBackend( void )
{
	backEnd.throttle = qtrue;
}


void RE_FinishBloom( void )
{
	finishBloomCommand_t *cmd;

	if ( !tr.registered ) {
		return;
	}

	cmd = R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd ) {
		return;
	}

	cmd->commandId = RC_FINISHBLOOM;
}


qboolean RE_CanMinimize( void )
{
#ifdef USE_VULKAN
	if ( vk.fboActive || vk.offscreenRender )
		return qtrue;
#endif
	return qfalse;
}


const glconfig_t *RE_GetConfig( void )
{
	return GL_GetConfig();
}


void RE_VertexLighting( qboolean allowed )
{
	tr.vertexLightingAllowed = allowed;
}

#ifdef USE_CIMGUI
qboolean RE_ImGuiBackend_Init( void )
{
	return VK_ImGui_InitBackend();
}

void RE_ImGuiBackend_Shutdown( void )
{
	VK_ImGui_ShutdownBackend();
}

void RE_ImGuiBackend_NewFrame( void )
{
	VK_ImGui_NewFrame();
}

#ifdef USE_CIMGUI
void RE_ImGuiBackend_RenderDrawData( const ImDrawData *drawData )
{
	imguiDrawCommand_t *cmd;

	if ( !drawData )
		return;

	cmd = R_GetCommandBuffer( sizeof( *cmd ) );
	if ( !cmd )
		return;

	cmd->commandId = RC_IMGUI_DRAW;
	cmd->drawData = drawData;
}
#endif
#endif
