/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

macOS Vulkan support via MoltenVK.
Loads libMoltenVK.dylib or libvulkan.dylib (Vulkan SDK) at runtime
and creates a Metal surface for Vulkan rendering.

Requires MoltenVK 1.2+ or the LunarG Vulkan SDK for macOS.
Install: brew install molten-vk  (or download from
https://github.com/KhronosGroup/MoltenVK/releases)
===========================================================================
*/

#if defined(__APPLE__)

#include "../../qcommon/q_shared.h"
#include "../../qcommon/qcommon.h"
#include "linux_local.h"
#include <dlfcn.h>

#define VK_USE_PLATFORM_METAL_EXT
#include "../../renderers/common/vulkan/vulkan.h"

static PFN_vkGetInstanceProcAddr qvkGetInstanceProcAddr;
static PFN_vkCreateMetalSurfaceEXT qvkCreateMetalSurfaceEXT;

void QVK_Shutdown( void )
{
	if ( glw_state.VulkanLib )
	{
		Com_Printf( "...unloading Vulkan DLL\n" );
		dlclose( glw_state.VulkanLib );
		glw_state.VulkanLib = NULL;
		qvkGetInstanceProcAddr = NULL;
	}
	qvkCreateMetalSurfaceEXT = NULL;
}

void *VK_GetInstanceProcAddr( VkInstance instance, const char *name )
{
	return qvkGetInstanceProcAddr( instance, name );
}

qboolean VK_CreateSurface( VkInstance instance, VkSurfaceKHR *surface )
{
	/* Surface creation on macOS requires a CAMetalLayer from the window.
	   SDL2 handles this via SDL_Vulkan_CreateSurface(), which is called
	   by the SDL glimp layer. This function is a fallback for non-SDL paths. */

	qvkCreateMetalSurfaceEXT = (PFN_vkCreateMetalSurfaceEXT)
		VK_GetInstanceProcAddr( instance, "vkCreateMetalSurfaceEXT" );

	if ( !qvkCreateMetalSurfaceEXT ) {
		Com_Printf( S_COLOR_YELLOW "VK_CreateSurface: vkCreateMetalSurfaceEXT not available\n" );
		Com_Printf( "  Falling back to SDL_Vulkan_CreateSurface\n" );
		return qfalse;
	}

	/* The actual Metal layer is obtained from the SDL window at a higher level.
	   This path exists for documentation; SDL handles surface creation. */
	Com_Printf( S_COLOR_YELLOW "VK_CreateSurface: use SDL_Vulkan_CreateSurface on macOS\n" );
	return qfalse;
}

static void *load_vulkan_library( const char *dllname )
{
	void *lib;

	lib = dlopen( dllname, RTLD_NOW | RTLD_LOCAL );
	if ( lib )
	{
		qvkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr) dlsym( lib, "vkGetInstanceProcAddr" );
		if ( qvkGetInstanceProcAddr )
		{
			return lib;
		}
		dlclose( lib );
	}

	return NULL;
}

qboolean QVK_Init( void )
{
	Com_Printf( "...initializing QVK (macOS / MoltenVK)\n" );

	if ( glw_state.VulkanLib == NULL )
	{
		/* Try MoltenVK first, then Vulkan SDK loader, then Homebrew paths */
		const char *dllnames[] = {
			"libMoltenVK.dylib",
			"libvulkan.1.dylib",
			"libvulkan.dylib",
			"/usr/local/lib/libMoltenVK.dylib",
			"/opt/homebrew/lib/libMoltenVK.dylib",
			"/usr/local/lib/libvulkan.dylib",
			"/opt/homebrew/lib/libvulkan.dylib"
		};
		int i;

		for ( i = 0; i < (int)( sizeof(dllnames) / sizeof(dllnames[0]) ); i++ )
		{
			glw_state.VulkanLib = load_vulkan_library( dllnames[i] );
			Com_Printf( "...loading '%s' : %s\n", dllnames[i],
				glw_state.VulkanLib ? "success" : "failed" );
			if ( glw_state.VulkanLib )
			{
				break;
			}
		}

		if ( !glw_state.VulkanLib )
		{
			Com_Printf( S_COLOR_RED "Failed to load Vulkan library\n" );
			Com_Printf( "Install MoltenVK: brew install molten-vk\n" );
			Com_Printf( "Or download from: https://github.com/KhronosGroup/MoltenVK/releases\n" );
			return qfalse;
		}
	}

	return qtrue;
}

#endif /* __APPLE__ */
