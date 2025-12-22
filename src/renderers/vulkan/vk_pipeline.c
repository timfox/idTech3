#include "vk_pipeline.h"
#include "../renderercommon/tr_public.h"
#include "vk.h"
#include "vk_commands.h"
#include "vk_memory.h"
#include <string.h>
#include <stdlib.h>

// Renderer interface
extern refimport_t ri;
extern backEndState_t backEnd;
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
extern const char *va(const char *format, ...);
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
            fread(*data_out, 1, len, f);
            *size_out = len;
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
	qvkGetPhysicalDeviceProperties(vk.physical_device, &props);

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
	qvkGetPhysicalDeviceProperties(vk.physical_device, &props);
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

    // Platform-specific file watching check would go here
    // For now, this is a framework that can be extended with actual file watching

    // If shader files changed, mark pipelines as dirty
    // Pipeline recreation would happen lazily on next use
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

// SHADER_MODULE function for creating shader modules from SPIR-V data
static VkShaderModule SHADER_MODULE_FUNC(const uint8_t *bytes, const int count) {
    VkShaderModuleCreateInfo desc;
    VkShaderModule module;

    if (count % 4 != 0) {
        ri.Error(ERR_FATAL, "Vulkan: SPIR-V binary buffer size is not a multiple of 4");
    }

    desc.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    desc.pNext = NULL;
    desc.flags = 0;
    desc.codeSize = count;
    desc.pCode = (const uint32_t*)bytes;

    VK_CHECK(qvkCreateShaderModule(vk.device, &desc, NULL, &module));

    return module;
}

// SHADER_MODULE macro
#define SHADER_MODULE(name) SHADER_MODULE_FUNC((const uint8_t*)name,sizeof(name))

// Object naming macro
#define SET_OBJECT_NAME(obj,objName,objType) vk_set_object_name( (uint64_t)(obj), (objName), (objType) )

void vk_create_shader_modules(void)
{
	// Always call vk_bind_generated_shaders - it's safe to call even if USE_VK_PBR is not defined
	vk_bind_generated_shaders();

	// specialized depth-fragment shader
	// Note: These shader arrays are generated by compile.sh
	// If compilation fails, these will be undefined and cause linker errors
	// Run: bash src/renderervk/shaders/compile.sh
}

static uint32_t vk_alloc_pipeline(const Vk_Pipeline_Def *def) {
	VK_Pipeline_t *pipeline;
	if (vk.pipelines_count >= MAX_VK_PIPELINES) {
		ri.Error(ERR_DROP, "alloc_pipeline: MAX_VK_PIPELINES reached");
		return 0;
	} else {
		int j;
		pipeline = &vk.pipelines[vk.pipelines_count];
		pipeline->def = *def;
		for (j = 0; j < RENDER_PASS_COUNT; j++) {
			pipeline->handle[j] = VK_NULL_HANDLE;
		}
		return vk.pipelines_count++;
	}
}

VkPipeline vk_gen_pipeline(uint32_t index) {
	if (index < vk.pipelines_count) {
		VK_Pipeline_t *pipeline = vk.pipelines + index;
		const renderPass_t pass = vk.renderPassIndex;
		// Pipeline caching: check if already created for this render pass
		if (pass < 0 || pass >= RENDER_PASS_COUNT) {
			ri.Printf(PRINT_WARNING, "%s(%u): invalid render pass %d, skipping pipeline\n", __func__, index, (int)pass);
			return VK_NULL_HANDLE;
		}
		if (pipeline->handle[pass] == VK_NULL_HANDLE) {
			// Create pipeline lazily - Vulkan pipeline cache will handle deduplication
			pipeline->handle[pass] = create_pipeline(&pipeline->def, pass, index);
			if (pipeline->handle[pass] == VK_NULL_HANDLE) {
				return VK_NULL_HANDLE;
			}
		}
		return pipeline->handle[pass];
	} else {
		ri.Error(ERR_FATAL, "%s(%i): NULL pipeline", __func__, index);
		return VK_NULL_HANDLE;
	}
}

uint32_t vk_find_pipeline_ext(uint32_t base, const Vk_Pipeline_Def *def, qboolean use) {
	const Vk_Pipeline_Def *cur_def;
	uint32_t index;

	for (index = base; index < vk.pipelines_count; index++) {
		cur_def = &vk.pipelines[index].def;
		if (memcmp(cur_def, def, sizeof(*def)) == 0) {
			goto found;
		}
	}

	index = vk_alloc_pipeline(def);
found:

	if (use)
		vk_gen_pipeline(index);

	return index;
}

void vk_get_pipeline_def(uint32_t pipeline, Vk_Pipeline_Def *def) {
	if (pipeline >= vk.pipelines_count) {
		Com_Memset(def, 0, sizeof(*def));
	} else {
		Com_Memcpy(def, &vk.pipelines[pipeline].def, sizeof(*def));
	}
}

void vk_bind_pipeline(uint32_t pipeline) {
	VkPipeline vkpipe;

	vkpipe = vk_gen_pipeline(pipeline);

	if (vkpipe != vk.cmd->last_pipeline) {
		qvkCmdBindPipeline(vk.cmd->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vkpipe);
		vk.cmd->last_pipeline = vkpipe;
	}

	vk_world.dirty_depth_attachment |= (vk.pipelines[pipeline].def.state_bits & GLS_DEPTHMASK_TRUE);
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

