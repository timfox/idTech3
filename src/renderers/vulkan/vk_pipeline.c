#include "tr_local.h"
#include "vk_pipeline.h"
#include "vk.h"
#include "vk_commands.h"
#include "vk_memory.h"
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

// Renderer interface
extern refimport_t ri;
extern cvar_t *r_vk_hotReload;

// Vulkan function pointer extern declarations
extern PFN_vkCreateShaderModule qvkCreateShaderModule;
extern PFN_vkDestroyShaderModule qvkDestroyShaderModule;
extern PFN_vkCreateGraphicsPipelines qvkCreateGraphicsPipelines;
extern PFN_vkCreateComputePipelines qvkCreateComputePipelines;
extern PFN_vkDestroyPipeline qvkDestroyPipeline;
extern PFN_vkCreatePipelineCache qvkCreatePipelineCache;
extern PFN_vkDestroyPipelineCache qvkDestroyPipelineCache;
extern PFN_vkGetPipelineCacheData qvkGetPipelineCacheData;
extern PFN_vkMergePipelineCaches qvkMergePipelineCaches;
extern PFN_vkCreatePipelineLayout qvkCreatePipelineLayout;
extern PFN_vkDestroyPipelineLayout qvkDestroyPipelineLayout;
extern PFN_vkCreateDescriptorSetLayout qvkCreateDescriptorSetLayout;
extern PFN_vkDestroyDescriptorSetLayout qvkDestroyDescriptorSetLayout;
extern PFN_vkCmdBindPipeline qvkCmdBindPipeline;
extern PFN_vkCmdPipelineBarrier qvkCmdPipelineBarrier;
extern PFN_vkGetPipelineExecutablePropertiesKHR qvkGetPipelineExecutablePropertiesKHR;

// Utility functions
// Com_Memcpy and Com_Memset are defined in q_shared.h
extern void *Z_Malloc(int size);
extern void Z_Free(void *ptr);

// Object naming function
extern void vk_set_object_name(uint64_t obj, const char *name, VkDebugReportObjectTypeEXT type);

// Advanced features struct
typedef struct {
    qboolean synchronization2;        // VK_KHR_synchronization2
    qboolean dynamicRendering;        // VK_KHR_dynamic_rendering
    qboolean meshShaders;             // VK_EXT_mesh_shader
    qboolean rayTracing;              // VK_KHR_ray_tracing_pipeline
    qboolean dlssSupported;           // NVIDIA DLSS (framework ready)
    qboolean fsrSupported;            // AMD FSR (framework ready)
    qboolean pipelineBinaries;        // VK_KHR_pipeline_executable_properties
} vk_advanced_features_t;
extern vk_advanced_features_t vk_advanced;

// Forward declarations for backend structures
// backEndState_t is declared in tr_local.h
extern trGlobals_t tr;

// Pipeline cache path
static const char *VK_PIPELINE_CACHE_PATH = "release/pipeline_cache_vk.bin";

// Shader file watching for hot reload
__attribute__((unused)) static shader_file_watch_t shader_watched_files[64];
__attribute__((unused)) static uint32_t shader_watched_file_count = 0;

// Pipeline cache operations
void vk_pipeline_cache_load(void **data_out, size_t *size_out) {
    FILE *f;
    long len;

    *data_out = NULL;
    *size_out = 0;

    f = fopen(VK_PIPELINE_CACHE_PATH, "rb");
    if (!f) {
        return;
    }

    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (len > 0) {
        *data_out = Z_Malloc(len);
        if (*data_out) {
            size_t bytes_read = fread(*data_out, 1, len, f);
            if (bytes_read == (size_t)len) {
                *size_out = len;
            } else {
                ri.Printf(PRINT_WARNING, "Vulkan: Failed to read %ld bytes from pipeline cache file (read %zu)\n", (long)len, bytes_read);
                Z_Free(*data_out);
                *data_out = NULL;
                *size_out = 0;
            }
        }
    }

    fclose(f);
}

void vk_pipeline_cache_save(void) {
    FILE *f;
    size_t size;
    void *data = NULL;

    if (!qvkGetPipelineCacheData || !vk.pipelineCache) {
        return;
    }

    if (qvkGetPipelineCacheData(vk.device, vk.pipelineCache, &size, NULL) != VK_SUCCESS) {
        return;
    }

    if (size == 0) {
        return;
    }

    data = Z_Malloc(size);
    if (!data) {
        return;
    }

    if (qvkGetPipelineCacheData(vk.device, vk.pipelineCache, &size, data) != VK_SUCCESS) {
        Z_Free(data);
        return;
    }

    f = fopen(VK_PIPELINE_CACHE_PATH, "wb");
    if (f) {
        fwrite(data, 1, size, f);
        fclose(f);
        ri.Printf(PRINT_DEVELOPER, "Vulkan: Saved pipeline cache (%zu bytes)\n", size);
    }

    Z_Free(data);
}

// Pipeline binary operations (VK_KHR_pipeline_executable_properties)
#define PIPELINE_BINARY_DIR "release/pipeline_binaries/"
#define PIPELINE_BINARY_VERSION 1

__attribute__((unused)) void vk_pipeline_binary_save(VkPipeline pipeline, uint64_t pipeline_hash) {
	if (!vk_advanced.pipelineBinaries || !qvkGetPipelineExecutablePropertiesKHR || !pipeline) {
		return;
	}

	// Get pipeline executable properties
	uint32_t executable_count = 0;
	VkResult res = qvkGetPipelineExecutablePropertiesKHR(vk.device, &(VkPipelineInfoKHR){
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR,
		.pNext = NULL,
		.pipeline = pipeline
	}, &executable_count, NULL);

	if (res != VK_SUCCESS || executable_count == 0) {
		return;
	}

	VkPipelineExecutablePropertiesKHR *executables = (VkPipelineExecutablePropertiesKHR*)ri.Malloc(
		executable_count * sizeof(VkPipelineExecutablePropertiesKHR));
	for (uint32_t i = 0; i < executable_count; i++) {
		executables[i].sType = VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR;
		executables[i].pNext = NULL;
	}

	res = qvkGetPipelineExecutablePropertiesKHR(vk.device, &(VkPipelineInfoKHR){
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR,
		.pNext = NULL,
		.pipeline = pipeline
	}, &executable_count, executables);

	if (res != VK_SUCCESS) {
		ri.Free(executables);
		return;
	}

	// Get device properties for binary validation
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(vk.physical_device, &props);

	// Create binary directory if it doesn't exist
	// Note: This is a simplified implementation - full version would use platform-specific directory creation
	char binary_path[256];
	Com_sprintf(binary_path, sizeof(binary_path), "%s%016llx.bin", PIPELINE_BINARY_DIR, (unsigned long long)pipeline_hash);

	// For now, just log that binary saving is available
	// Full implementation would save the binary data
	ri.Printf(PRINT_DEVELOPER, "Vulkan: Pipeline binary save framework ready (hash: %016llx)\n", (unsigned long long)pipeline_hash);

	ri.Free(executables);
}

__attribute__((unused)) qboolean vk_pipeline_binary_load(uint64_t pipeline_hash, void **binary_data, VkDeviceSize *binary_size) {
	if (!vk_advanced.pipelineBinaries || !binary_data || !binary_size) {
		return qfalse;
	}

	*binary_data = NULL;
	*binary_size = 0;

	char binary_path[256];
	Com_sprintf(binary_path, sizeof(binary_path), "%s%016llx.bin", PIPELINE_BINARY_DIR, (unsigned long long)pipeline_hash);

	FILE *f = fopen(binary_path, "rb");
	if (!f) {
		return qfalse;
	}

	// Read header
	pipeline_binary_header_t header;
	if (fread(&header, sizeof(header), 1, f) != 1) {
		fclose(f);
		return qfalse;
	}

	// Validate version and device
	if (header.version != PIPELINE_BINARY_VERSION) {
		fclose(f);
		return qfalse;
	}

	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(vk.physical_device, &props);
	if (header.device_vendor_id != props.vendorID || header.device_id != props.deviceID) {
		fclose(f);
		return qfalse; // Binary from different device
	}

	// Read binary data
	void *buf = malloc(header.binary_size);
	if (!buf) {
		fclose(f);
		return qfalse;
	}

	if (fread(buf, 1, header.binary_size, f) != header.binary_size) {
		free(buf);
		fclose(f);
		return qfalse;
	}

	fclose(f);
	*binary_data = buf;
	*binary_size = header.binary_size;

	ri.Printf(PRINT_DEVELOPER, "Vulkan: Loaded pipeline binary (hash: %016llx, size: %zu)\n", 
		(unsigned long long)pipeline_hash, (size_t)header.binary_size);

	return qtrue;
}

VkPipeline vk_create_pipeline_from_binary(uint64_t pipeline_hash, __attribute__((unused)) VkPipelineLayout layout,
    const Vk_Pipeline_Def *def, renderPass_t renderPassIndex, uint32_t def_index) {

    void *binary_data = NULL;
    VkDeviceSize binary_size = 0;

    if (!vk_pipeline_binary_load(pipeline_hash, &binary_data, &binary_size)) {
        return VK_NULL_HANDLE;
    }

    // Note: VK_KHR_pipeline_library would be needed for full binary pipeline creation
    // For now, fall back to normal pipeline creation
    Z_Free(binary_data);
    return create_pipeline(def, renderPassIndex, def_index);
}

// Shader hot reload system
__attribute__((unused)) static void vk_hot_reload_init(void) {
    if (!r_vk_hotReload || !r_vk_hotReload->integer) {
        return;
    }

    Com_Memset(&vk.hot_reload, 0, sizeof(vk.hot_reload));
    vk.hot_reload.enabled = qtrue;

    // Note: Platform-specific file watching would be implemented here
    // For Linux: inotify, for macOS: FSEvents, for Windows: ReadDirectoryChangesW
    // This is a framework - full implementation requires platform-specific code

    ri.Printf(PRINT_ALL, "Vulkan: Shader hot reload system initialized (framework ready)\n");
}

__attribute__((unused)) void vk_check_shader_hot_reload(void) {
    if (!vk.hot_reload.enabled || !r_vk_hotReload || !r_vk_hotReload->integer) {
        return;
    }

    // Check shader files for changes using file modification time
    static const char *shader_files[] = {
        "src/renderers/vulkan/shaders/glsl/color.vert",
        "src/renderers/vulkan/shaders/glsl/color.frag",
        "src/renderers/vulkan/shaders/glsl/fog.vert",
        "src/renderers/vulkan/shaders/glsl/fog.frag",
        "src/renderers/vulkan/shaders/glsl/dot.vert",
        "src/renderers/vulkan/shaders/glsl/dot.frag",
        // Add more shader files as needed
    };

    static time_t last_mod_times[sizeof(shader_files)/sizeof(shader_files[0])] = {0};
    qboolean shaders_changed = qfalse;

    for (size_t i = 0; i < sizeof(shader_files)/sizeof(shader_files[0]); i++) {
        struct stat st;
        if (stat(shader_files[i], &st) == 0) {
            if (st.st_mtime > last_mod_times[i]) {
                ri.Printf(PRINT_ALL, "Vulkan: Shader file changed: %s\n", shader_files[i]);
                last_mod_times[i] = st.st_mtime;
                shaders_changed = qtrue;
            }
        }
    }

    if (shaders_changed) {
        ri.Printf(PRINT_ALL, "Vulkan: Shaders changed, marking pipelines dirty\n");
        vk_mark_pipelines_dirty();
    }
}

__attribute__((unused)) qboolean vk_reload_shader(const char *shader_name) {
    if (!vk.hot_reload.enabled) {
        return qfalse;
    }

    // This would:
    // 1. Reload shader file from disk
    // 2. Recompile shader module
    // 3. Mark pipelines using this shader as dirty
    // 4. Recreate pipelines on next use

    ri.Printf(PRINT_ALL, "Vulkan: Shader reload requested for: %s\n", shader_name ? shader_name : "unknown");
    vk.hot_reload.shaders_reloaded++;

    return qtrue;
}

// SHADER_MODULE function moved to vk.c where it's actually used

// Object naming macro
#define SET_OBJECT_NAME(obj,objName,objType) vk_set_object_name( (uint64_t)(obj), (objName), (objType) )

void vk_create_shader_modules(void)
{
	ri.Printf(PRINT_ALL, "DEBUG: vk_create_shader_modules called\n");
	// Always call vk_bind_generated_shaders - it's safe to call even if USE_VK_PBR is not defined
	vk_bind_generated_shaders();
	ri.Printf(PRINT_ALL, "DEBUG: vk_bind_generated_shaders completed\n");

	// specialized depth-fragment shader
	// Note: These shader arrays are generated by compile.sh
	// If compilation fails, these will be undefined and cause linker errors
	// Run: bash src/renderervk/shaders/compile.sh
}

static uint32_t vk_alloc_pipeline(const Vk_Pipeline_Def *def) {
	// Pipeline is allocated as part of the pipelines array
	if (vk.pipelines_count >= 32) {
		ri.Error(ERR_DROP, "alloc_pipeline: MAX_VK_PIPELINES reached");
		return 0;
	} else {
		int j;
		uint32_t index = vk.pipelines_count;
		vk.pipelines[index].def = *def;
		for (j = 0; j < RENDER_PASS_COUNT; j++) {
			vk.pipelines[index].handle[j] = VK_NULL_HANDLE;
		}
		return vk.pipelines_count++;
	}
	return 0; // Should not reach here
}

VkPipeline vk_gen_pipeline(uint32_t index) {
	if (index < vk.pipelines_count) {
		// Access the pipeline structure directly from the array
		const renderPass_t pass = vk.renderPassIndex;
		// Pipeline caching: check if already created for this render pass
		if (pass < 0 || pass >= RENDER_PASS_COUNT) {
			ri.Printf(PRINT_WARNING, "%s(%u): invalid render pass %d, skipping pipeline\n", __func__, index, (int)pass);
			return VK_NULL_HANDLE;
		}
		if (vk.pipelines[index].handle[pass] == VK_NULL_HANDLE) {
			// Create pipeline lazily - Vulkan pipeline cache will handle deduplication
			vk.pipelines[index].handle[pass] = create_pipeline(&vk.pipelines[index].def, pass, index);
			if (vk.pipelines[index].handle[pass] == VK_NULL_HANDLE) {
				return VK_NULL_HANDLE;
			}
		}
		return vk.pipelines[index].handle[pass];
	} else {
		ri.Error(ERR_FATAL, "%s(%i): NULL pipeline", __func__, index);
		return VK_NULL_HANDLE;
	}
}

VkPipeline vk_find_pipeline_ext(int base_pipeline, Vk_Pipeline_Def* def, qboolean create_if_missing) {
	ri.Printf(PRINT_ALL, "DEBUG: vk_find_pipeline_ext shader_type=%d create_if_missing=%d\n", def->shader_type, create_if_missing);
	const Vk_Pipeline_Def *cur_def;
	uint32_t index;

	if (def == NULL) {
		ri.Printf(PRINT_ERROR, "vk_find_pipeline_ext: def is NULL\n");
		return VK_NULL_HANDLE;
	}

	// Validate shader_type
	if (def->shader_type >= TYPE_GENERIC_BEGIN + 100) { // Arbitrary large number
		ri.Printf(PRINT_ERROR, "vk_find_pipeline_ext: invalid shader_type %d\n", def->shader_type);
		return VK_NULL_HANDLE;
	}

	for (index = base_pipeline; index < vk.pipelines_count; index++) {
		cur_def = &vk.pipelines[index].def;
		if (memcmp(cur_def, def, sizeof(*def)) == 0) {
			goto found;
		}
	}

	index = vk_alloc_pipeline(def);
	if (index == 0) {
		ri.Printf(PRINT_ERROR, "vk_find_pipeline_ext: failed to allocate pipeline\n");
		return VK_NULL_HANDLE;
	}

found:

	if (create_if_missing) {
		VkPipeline pipeline = vk_gen_pipeline(index);
		if (pipeline == VK_NULL_HANDLE) {
			ri.Printf(PRINT_ERROR, "vk_find_pipeline_ext: failed to generate pipeline %u\n", index);
			return VK_NULL_HANDLE;
		}
		return pipeline;
	}

	return vk_gen_pipeline(index);
}

void vk_get_pipeline_def(VkPipeline pipeline, Vk_Pipeline_Def *def) {
	// Search for the pipeline in the array to find its definition
	for (uint32_t i = 0; i < vk.pipelines_count; i++) {
		for (int j = 0; j < RENDER_PASS_COUNT; j++) {
			if (vk.pipelines[i].handle[j] == pipeline) {
				Com_Memcpy(def, &vk.pipelines[i].def, sizeof(*def));
				return;
			}
		}
	}
	// Pipeline not found, return empty definition
	Com_Memset(def, 0, sizeof(*def));
}

void vk_bind_pipeline(VkPipeline pipeline) {
	if (pipeline != vk.cmd->last_pipeline) {
		qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
		vk.cmd->last_pipeline = pipeline;
	}

	// Note: Pipeline state tracking would need to be implemented here if needed
	// For now, we'll assume depth attachment state is managed elsewhere
}

// Stub implementations for pipeline functions - will be fully implemented
void get_viewport_rect(VkRect2D *r) {
	if (backEnd.projection2D) {
		r->offset.x = 0;
		r->offset.y = 0;
		r->extent.width = vk.renderWidth;
		r->extent.height = vk.renderHeight;
	} else {
		r->offset.x = backEnd.viewParms.viewportX * vk.renderScaleX;
		r->offset.y = vk.renderHeight - (backEnd.viewParms.viewportY + backEnd.viewParms.viewportHeight) * vk.renderScaleY;
		r->extent.width = (float)backEnd.viewParms.viewportWidth * vk.renderScaleX;
		r->extent.height = (float)backEnd.viewParms.viewportHeight * vk.renderScaleY;
	}
}

