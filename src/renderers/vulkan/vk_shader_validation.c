/*
=============================================================================
Vulkan Shader Validation System
Centralized detection and handling of problematic shaders
=============================================================================
*/

#include "tr_local.h"
#include "vk.h"

#ifdef USE_VULKAN

// Renderer import interface
extern refimport_t ri;

// List of problematic shader names that cause device loss or crashes
static const char *problematic_shader_names[] = {
	"models/mapobjects/banner/q3banner02",
	"models/mapobjects/banner/q3banner04",
	// Add more problematic shaders here as they are discovered
	NULL  // Sentinel
};

// List of problematic shader types that cause SIGFPE or crashes
static const int problematic_shader_types[] = {
	TYPE_SINGLE_TEXTURE,                    // Causes SIGFPE
	TYPE_SINGLE_TEXTURE_FIXED_COLOR,        // Causes crashes
	TYPE_MULTI_TEXTURE_MUL2_IDENTITY,      // Causes crashes
	-1  // Sentinel
};

// Statistics
static struct {
	int total_shaders_checked;
	int problematic_shaders_skipped;
	int device_lost_events;
	int pipeline_creation_failures;
} validation_stats = {0};

/*
==================
vk_is_problematic_shader_name
==================
Returns qtrue if the shader name is known to cause problems
*/
qboolean vk_is_problematic_shader_name(const char *shader_name) {
	if (!shader_name || shader_name[0] == '\0') {
		return qfalse;
	}

	validation_stats.total_shaders_checked++;

	for (int i = 0; problematic_shader_names[i] != NULL; i++) {
		if (!Q_stricmp(shader_name, problematic_shader_names[i])) {
			validation_stats.problematic_shaders_skipped++;
			ri.Printf(PRINT_WARNING, "Vulkan: Detected problematic shader: %s\n", shader_name);
			return qtrue;
		}
	}

	// Check for partial matches (e.g., shader paths containing problematic names)
	for (int i = 0; problematic_shader_names[i] != NULL; i++) {
		if (strstr(shader_name, problematic_shader_names[i]) != NULL) {
			validation_stats.problematic_shaders_skipped++;
			ri.Printf(PRINT_WARNING, "Vulkan: Detected problematic shader pattern: %s (matches %s)\n", 
				shader_name, problematic_shader_names[i]);
			return qtrue;
		}
	}

	return qfalse;
}

/*
==================
vk_is_problematic_shader_type
==================
Returns qtrue if the shader type is known to cause problems
*/
qboolean vk_is_problematic_shader_type(int shader_type) {
	for (int i = 0; problematic_shader_types[i] != -1; i++) {
		if (shader_type == problematic_shader_types[i]) {
			ri.Printf(PRINT_WARNING, "Vulkan: Detected problematic shader type: %d\n", shader_type);
			return qtrue;
		}
	}
	return qfalse;
}

/*
==================
vk_validate_shader_before_pipeline
==================
Comprehensive validation before pipeline creation
Returns qtrue if shader is safe to use, qfalse if it should be skipped
*/
qboolean vk_validate_shader_before_pipeline(const char *shader_name, int shader_type, 
											const Vk_Pipeline_Def *def) {
	// Check device state
	if (vk.device_lost) {
		ri.Printf(PRINT_WARNING, "Vulkan: Skipping shader validation - device is lost\n");
		validation_stats.device_lost_events++;
		return qfalse;
	}

	// Check device handle
	if (vk.device == VK_NULL_HANDLE) {
		ri.Printf(PRINT_WARNING, "Vulkan: Skipping shader validation - device not initialized\n");
		return qfalse;
	}

	// Check shader name
	if (shader_name && vk_is_problematic_shader_name(shader_name)) {
		return qfalse;
	}

	// Check shader type
	if (vk_is_problematic_shader_type(shader_type)) {
		return qfalse;
	}

	// Validate pipeline definition
	if (def) {
		// Check for invalid shader modules
		if (def->shaders.vs_module == VK_NULL_HANDLE || 
			def->shaders.fs_module == VK_NULL_HANDLE) {
			ri.Printf(PRINT_WARNING, "Vulkan: Shader has NULL shader modules (name=%s, type=%d)\n",
				shader_name ? shader_name : "unknown", shader_type);
			return qfalse;
		}

		// Check for invalid pipeline layout
		if (def->pipeline_layout == VK_NULL_HANDLE) {
			ri.Printf(PRINT_WARNING, "Vulkan: Shader has NULL pipeline layout (name=%s, type=%d)\n",
				shader_name ? shader_name : "unknown", shader_type);
			return qfalse;
		}
	}

	return qtrue;
}

/*
==================
vk_handle_pipeline_creation_error
==================
Handles errors during pipeline creation and updates statistics
*/
void vk_handle_pipeline_creation_error(VkResult result, const char *shader_name, int shader_type) {
	validation_stats.pipeline_creation_failures++;

	if (result == VK_ERROR_DEVICE_LOST) {
		vk.device_lost = qtrue;
		validation_stats.device_lost_events++;
		ri.Printf(PRINT_ERROR, "Vulkan: Device lost during pipeline creation for shader %s (type=%d)\n",
			shader_name ? shader_name : "unknown", shader_type);
		ri.Printf(PRINT_WARNING, "Vulkan: This shader will be marked as problematic\n");
	} else if (result == VK_ERROR_OUT_OF_DEVICE_MEMORY) {
		ri.Printf(PRINT_WARNING, "Vulkan: Out of device memory during pipeline creation for shader %s (type=%d)\n",
			shader_name ? shader_name : "unknown", shader_type);
	} else if (result == VK_ERROR_OUT_OF_HOST_MEMORY) {
		ri.Printf(PRINT_WARNING, "Vulkan: Out of host memory during pipeline creation for shader %s (type=%d)\n",
			shader_name ? shader_name : "unknown", shader_type);
	} else {
		ri.Printf(PRINT_WARNING, "Vulkan: Pipeline creation failed for shader %s (type=%d): %s\n",
			shader_name ? shader_name : "unknown", shader_type, vk_result_string(result));
	}
}

/*
==================
vk_get_shader_validation_stats
==================
Returns validation statistics for debugging
*/
void vk_get_shader_validation_stats(int *total_checked, int *skipped, 
									int *device_lost, int *pipeline_failures) {
	if (total_checked) *total_checked = validation_stats.total_shaders_checked;
	if (skipped) *skipped = validation_stats.problematic_shaders_skipped;
	if (device_lost) *device_lost = validation_stats.device_lost_events;
	if (pipeline_failures) *pipeline_failures = validation_stats.pipeline_creation_failures;
}

/*
==================
vk_reset_shader_validation_stats
==================
Resets validation statistics
*/
void vk_reset_shader_validation_stats(void) {
	Com_Memset(&validation_stats, 0, sizeof(validation_stats));
}

/*
==================
vk_print_shader_validation_report
==================
Prints a report of shader validation statistics
*/
void vk_print_shader_validation_report(void) {
	ri.Printf(PRINT_ALL, "=== Vulkan Shader Validation Report ===\n");
	ri.Printf(PRINT_ALL, "Total shaders checked: %d\n", validation_stats.total_shaders_checked);
	ri.Printf(PRINT_ALL, "Problematic shaders skipped: %d\n", validation_stats.problematic_shaders_skipped);
	ri.Printf(PRINT_ALL, "Device lost events: %d\n", validation_stats.device_lost_events);
	ri.Printf(PRINT_ALL, "Pipeline creation failures: %d\n", validation_stats.pipeline_creation_failures);
	ri.Printf(PRINT_ALL, "\nKnown problematic shaders:\n");
	for (int i = 0; problematic_shader_names[i] != NULL; i++) {
		ri.Printf(PRINT_ALL, "  - %s\n", problematic_shader_names[i]);
	}
	ri.Printf(PRINT_ALL, "\nKnown problematic shader types:\n");
	for (int i = 0; problematic_shader_types[i] != -1; i++) {
		ri.Printf(PRINT_ALL, "  - Type %d\n", problematic_shader_types[i]);
	}
	ri.Printf(PRINT_ALL, "========================================\n");
}

#endif // USE_VULKAN
