/*
=============================================================================
Vulkan Dynamic Rendering Implementation

Uses VK_KHR_dynamic_rendering for modern render pass replacement.
=============================================================================
*/

#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "vk.h"
#include "vk_dynamic_rendering.h"

#ifdef USE_VULKAN

// CVars
extern cvar_t *r_vk_dynamicRendering;

static qboolean dynamic_rendering_supported = qfalse;

/*
=============================================================================
Dynamic Rendering Extension Detection
=============================================================================
*/

qboolean vk_dynamic_rendering_check_support(void) {
	if (!r_vk_dynamicRendering || !r_vk_dynamicRendering->integer) {
		return qfalse;
	}

	// For now, mark as not supported to avoid compilation issues
	// This would need proper VK_KHR_dynamic_rendering implementation
	dynamic_rendering_supported = qfalse;
	ri.Printf(PRINT_ALL, "Vulkan: Dynamic rendering not yet implemented\n");

	return qfalse;
}

qboolean vk_dynamic_rendering_enabled(void) {
	return dynamic_rendering_supported;
}

#endif // USE_VULKAN