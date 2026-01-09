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
	if (!vk.active || vk.device == VK_NULL_HANDLE || vk.swapchain == VK_NULL_HANDLE || vk.cmd == NULL) {
		ri.Printf(PRINT_DEVELOPER, "Vulkan: Skipping frame - not fully initialized\n");
		return;
	}
	// Skip rendering if device is lost (prevents video playback and all rendering)
	if (vk.device_lost) {
		// Only log once per second to avoid spam
		static int last_log_time = 0;
		static int last_recovery_attempt = 0;
		int current_time = ri.Milliseconds();
		if (current_time - last_log_time > 1000) {
			ri.Printf(PRINT_WARNING, "Vulkan: Device is lost - rendering disabled. Video playback will not work.\n");
			ri.Printf(PRINT_WARNING, "Vulkan: Try restarting the application or updating GPU drivers.\n");
			last_log_time = current_time;
		}
		
		// Attempt device recovery every 5 seconds
		if (current_time - last_recovery_attempt > 5000) {
			last_recovery_attempt = current_time;
			ri.Printf(PRINT_ALL, "Vulkan: Attempting device recovery...\n");
			
			// Test if device is responsive by checking device properties
			if (vk.device != VK_NULL_HANDLE && vk.physical_device != VK_NULL_HANDLE && qvkGetPhysicalDeviceProperties) {
				VkPhysicalDeviceProperties props;
				qvkGetPhysicalDeviceProperties(vk.physical_device, &props);
				
				// Try to recreate swapchain as a test
				if (vk.swapchain != VK_NULL_HANDLE && vk_surface != VK_NULL_HANDLE) {
					ri.Printf(PRINT_ALL, "Vulkan: Testing device recovery by checking device state...\n");
					
					// Test device responsiveness with a simple operation
					if (qvkDeviceWaitIdle) {
						VkResult wait_result = qvkDeviceWaitIdle(vk.device);
						if (wait_result == VK_ERROR_DEVICE_LOST) {
							ri.Printf(PRINT_WARNING, "Vulkan: Device still lost. Will retry in 5 seconds.\n");
							return;
						}
						// If we get here, device might be recovered
					}
					
					// Try recreating swapchain
					ri.Printf(PRINT_ALL, "Vulkan: Attempting swapchain recreation...\n");
					vk_recreate_swapchain();
					
					// Test if swapchain recreation succeeded by trying to acquire an image
					if (qvkAcquireNextImageKHR && vk.swapchain != VK_NULL_HANDLE) {
						uint32_t test_index;
						VkResult test_result = qvkAcquireNextImageKHR(vk.device, vk.swapchain, 0, 
							vk.image_available, VK_NULL_HANDLE, &test_index);
						
						if (test_result == VK_SUCCESS || test_result == VK_SUBOPTIMAL_KHR) {
							ri.Printf(PRINT_ALL, "Vulkan: Device recovery successful! Resuming rendering.\n");
							vk.device_lost = qfalse; // Reset device lost flag
							// Release the test image - we'll acquire it properly in the normal flow
							if (test_result == VK_SUCCESS) {
								// Image was acquired, we'll use it in the normal flow
								vk.current_swapchain_image_index = test_index;
							}
						} else if (test_result == VK_ERROR_DEVICE_LOST) {
							ri.Printf(PRINT_WARNING, "Vulkan: Device still lost during recovery test. Will retry in 5 seconds.\n");
						} else {
							ri.Printf(PRINT_WARNING, "Vulkan: Device recovery test failed (result: %d). Will retry in 5 seconds.\n", test_result);
						}
					}
				}
			}
		}
		
		return;
	}
#endif

	// Initialize frame ready flag
	vk.cmd->frame_ready = qfalse;

#ifdef USE_VULKAN
	// Memory validation disabled to avoid compilation issues
	// TODO: Re-enable when function linking is resolved
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
	if (vk.device != (VkDevice)0x20000000 && vk.active && vk.swapchain != VK_NULL_HANDLE && qvkAcquireNextImageKHR) {
		// Acquire next image from swapchain
		VkResult result = qvkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX,
			vk.image_available, VK_NULL_HANDLE, &vk.current_swapchain_image_index);

		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			// Swapchain is out of date, need to recreate
			ri.Printf(PRINT_WARNING, "Vulkan: Swapchain out of date, skipping frame\n");
			return;
		} else if (result == VK_TIMEOUT) {
			// Timeout acquiring swapchain image (common in headless environments)
			ri.Printf(PRINT_WARNING, "Vulkan: Timeout acquiring swapchain image, skipping frame (headless mode?)\n");
			return;
		} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			ri.Error(ERR_FATAL, "Vulkan: Failed to acquire swapchain image: %d\n", result);
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
			cmd->buffer = (int)GL_BACK_LEFT;
		} else if ( stereoFrame == STEREO_RIGHT ) {
			cmd->buffer = (int)GL_BACK_RIGHT;
		} else {
			ri.Error( ERR_FATAL, "RE_BeginFrame: Stereo is enabled, but stereoFrame was %i", stereoFrame );
		}
	} else {
		if ( stereoFrame != STEREO_CENTER ) {
			ri.Error( ERR_FATAL, "RE_BeginFrame: Stereo is disabled, but stereoFrame was %i", stereoFrame );
		}

#ifdef USE_VULKAN
		cmd->buffer = 0;
#else
		if ( !Q_stricmp( r_drawBuffer->string, "GL_FRONT" ) )
			cmd->buffer = (int)GL_FRONT;
		else
			cmd->buffer = (int)GL_BACK;
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
		if (vk.device != (VkDevice)0x20000000 && vk.active && vk.swapchain != VK_NULL_HANDLE && qvkQueuePresentKHR) {
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
			if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
				// Swapchain needs recreation
				ri.Printf(PRINT_WARNING, "Vulkan: Swapchain needs recreation\n");
			} else if (result != VK_SUCCESS) {
				ri.Printf(PRINT_WARNING, "Vulkan: Failed to present: %d\n", result);
			}

			// Reset semaphores for next frame
			vk.image_available = VK_NULL_HANDLE;
			vk.rendering_finished = VK_NULL_HANDLE;
		}

		ri.Cvar_ResetGroup( CVG_RENDERER, qtrue /* reset modified flags */ );
	}

#ifdef USE_VULKAN
	// Memory validation disabled to avoid compilation issues
	// TODO: Re-enable when function linking is resolved
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
