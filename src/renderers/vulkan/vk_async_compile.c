/*
=============================================================================
Vulkan Asynchronous Shader Compilation

Stub implementation - framework ready for future development.
=============================================================================
*/

#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "vk.h"

#ifdef USE_VULKAN

// CVars
extern cvar_t *r_vk_asyncShaderCompile;

qboolean vk_async_compile_init(void) {
	if (!r_vk_asyncShaderCompile || !r_vk_asyncShaderCompile->integer) {
		return qtrue;
	}

	// Async compilation not yet implemented - framework ready for future development
	ri.Printf(PRINT_ALL, "Vulkan: Async shader compilation framework ready\n");
	return qtrue;
}

void vk_async_compile_shutdown(void) {
	// Nothing to do yet
}

#endif // USE_VULKAN