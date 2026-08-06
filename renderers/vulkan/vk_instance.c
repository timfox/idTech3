/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan instance and device creation.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_instance.h"
#include "vk_device.h"
#include "vk_util.h"
#include "vk_validation.h"
#ifdef USE_VUDA
#include "vk_vuda.h"
#endif

/* VK_EXT_extended_dynamic_state3: for vkCmdSetColorWriteMaskEXT (RB_ColorMask) */
#ifndef VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT
#define VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT 1000484000
#endif
#ifndef VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT
#define VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT 1000484004
#endif
typedef struct VkPhysicalDeviceExtendedDynamicState3FeaturesEXT {
	VkStructureType sType;
	void *pNext;
	VkBool32 extendedDynamicState3ColorWriteMask;
} VkPhysicalDeviceExtendedDynamicState3FeaturesEXT;

/* Instance and surface (defined here, declared in vk_instance.h) */
VkInstance vk_instance = VK_NULL_HANDLE;
VkSurfaceKHR vk_surface = VK_NULL_HANDLE;

#ifdef USE_VK_VALIDATION
VkDebugReportCallbackEXT vk_debug_callback = VK_NULL_HANDLE;
#endif

/* qvk* definitions: vk_procs.c */

#define INIT_INSTANCE_FUNCTION(func) \
	do { \
		void *_sym = ri.VK_GetInstanceProcAddr( vk_instance, #func ); \
		Com_Memcpy( &q##func, &_sym, sizeof( q##func ) ); \
		if ( q##func == NULL ) { \
			ri.Error( ERR_FATAL, "Failed to find entrypoint %s", #func ); \
		} \
	} while ( 0 );

#define INIT_INSTANCE_FUNCTION_EXT(func) \
	do { \
		void *_sym = ri.VK_GetInstanceProcAddr( vk_instance, #func ); \
		Com_Memcpy( &q##func, &_sym, sizeof( q##func ) ); \
	} while ( 0 );

#define INIT_DEVICE_FUNCTION(func) \
	do { \
		PFN_vkVoidFunction _sym = qvkGetDeviceProcAddr( vk.device, #func ); \
		Com_Memcpy( &q##func, &_sym, sizeof( q##func ) ); \
		if ( q##func == NULL ) { \
			ri.Error( ERR_FATAL, "Failed to find entrypoint %s", #func ); \
		} \
	} while ( 0 );

#define INIT_DEVICE_FUNCTION_EXT(func) \
	do { \
		PFN_vkVoidFunction _sym = qvkGetDeviceProcAddr( vk.device, #func ); \
		Com_Memcpy( &q##func, &_sym, sizeof( q##func ) ); \
	} while ( 0 );

static void create_instance( void )
{
#ifdef USE_VK_VALIDATION
	const char* validation_layer_name = "VK_LAYER_KHRONOS_validation";
	const char* validation_layer_name2 = "VK_LAYER_LUNARG_standard_validation";
#endif
	VkInstanceCreateInfo desc;
	VkInstanceCreateFlags flags;
	VkExtensionProperties *extension_properties;
	VkResult res;
	const char **extension_names;
	uint32_t i, n, count, extension_count;
	VkApplicationInfo appInfo;

	flags = 0;
	count = 0;
	extension_count = 0;
	VK_CHECK(qvkEnumerateInstanceExtensionProperties(NULL, &count, NULL));

	extension_properties = (VkExtensionProperties *)ri.Malloc(sizeof(VkExtensionProperties) * count);
	extension_names = (const char**)ri.Malloc(sizeof(char *) * count);

	VK_CHECK( qvkEnumerateInstanceExtensionProperties( NULL, &count, extension_properties ) );
	for ( i = 0; i < count; i++ ) {
		const char *ext = extension_properties[i].extensionName;

		if ( !vk_used_instance_extension( ext ) ) {
			continue;
		}

		for ( n = 0; n < extension_count; n++ ) {
			if ( Q_stricmp( ext, extension_names[ n ] ) == 0 ) {
				break;
			}
		}
		if ( n != extension_count ) {
			continue;
		}

		extension_names[ extension_count++ ] = ext;

		if ( Q_stricmp( ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME ) == 0 ) {
			flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
		}

		ri.Printf(PRINT_DEVELOPER, "instance extension: %s\n", ext);
	}

	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pNext = NULL;
	appInfo.pApplicationName = NULL;
	appInfo.applicationVersion = 0x0;
	appInfo.pEngineName = NULL;
	appInfo.engineVersion = 0x0;
#ifdef VK_API_VERSION_1_4
	appInfo.apiVersion = VK_API_VERSION_1_4;
#elif defined(VK_API_VERSION_1_3)
	appInfo.apiVersion = VK_API_VERSION_1_3;
#elif defined(VK_API_VERSION_1_2)
	appInfo.apiVersion = VK_API_VERSION_1_2;
#elif defined(VK_API_VERSION_1_1)
	appInfo.apiVersion = VK_API_VERSION_1_1;
#else
	appInfo.apiVersion = VK_API_VERSION_1_0;
#endif

	desc.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = flags;
	desc.pApplicationInfo = &appInfo;
	desc.enabledExtensionCount = extension_count;
	desc.ppEnabledExtensionNames = extension_names;

#ifdef USE_VK_VALIDATION
	desc.enabledLayerCount = 1;
	desc.ppEnabledLayerNames = &validation_layer_name;

	res = qvkCreateInstance( &desc, NULL, &vk_instance );

	if ( res == VK_ERROR_LAYER_NOT_PRESENT ) {

		desc.enabledLayerCount = 1;
		desc.ppEnabledLayerNames = &validation_layer_name2;

		res = qvkCreateInstance( &desc, NULL, &vk_instance );

		if ( res == VK_ERROR_LAYER_NOT_PRESENT ) {

			ri.Printf( PRINT_WARNING, "...validation layer is not available\n" );

			desc.enabledLayerCount = 0;
			desc.ppEnabledLayerNames = NULL;

			res = qvkCreateInstance( &desc, NULL, &vk_instance );
		}
	}
#else
	desc.enabledLayerCount = 0;
	desc.ppEnabledLayerNames = NULL;

	res = qvkCreateInstance( &desc, NULL, &vk_instance );
#endif

	ri.Free( (void*)extension_names );
	ri.Free( extension_properties );

	if ( res != VK_SUCCESS ) {
		ri.Error( ERR_FATAL, "Vulkan: instance creation failed with %s", vk_result_string( res ) );
	}
}


static qboolean vk_create_device( VkPhysicalDevice physical_device, int device_index )
{
#ifdef _DEBUG
	VkPhysicalDeviceTimelineSemaphoreFeatures timeline_semaphore;
	VkPhysicalDeviceVulkanMemoryModelFeatures memory_model;
	VkPhysicalDeviceBufferDeviceAddressFeatures devaddr_features;
	VkPhysicalDevice8BitStorageFeatures storage_8bit_features;
#endif
	VkPhysicalDeviceMeshShaderFeaturesNV mesh_shader_features_nv;
	/* Must survive until qvkCreateDevice (do not declare inside a narrow if-block). */
	VkPhysicalDeviceHostQueryResetFeatures host_query;

	ri.Printf( PRINT_ALL, "...selected physical device: %i\n", device_index );

	if ( !vk_select_surface_format( physical_device, vk_surface ) ) {
		return qfalse;
	}

	vk_setup_surface_formats( physical_device );

	{
		VkQueueFamilyProperties *queue_families;
		uint32_t queue_family_count;
		uint32_t i;

		qvkGetPhysicalDeviceQueueFamilyProperties( physical_device, &queue_family_count, NULL );
		if ( queue_family_count == 0 ) {
			ri.Printf( PRINT_ERROR, "...no queue families reported\n" );
			return qfalse;
		}
		queue_families = (VkQueueFamilyProperties*)ri.Malloc( queue_family_count * sizeof( VkQueueFamilyProperties ) );
		qvkGetPhysicalDeviceQueueFamilyProperties( physical_device, &queue_family_count, queue_families );

		vk.queue_family_index = ~0U;
		vk.sparseBinding = qfalse;
		vk.sparseResidencyImage2D = qfalse;
		vk.sparseResidencyNonResidentStrict = qfalse;
		{
			uint32_t best = ~0U;
			uint32_t bestSparse = ~0U;

			for ( i = 0; i < queue_family_count; i++ ) {
				VkBool32 presentation_supported;
				VK_CHECK( qvkGetPhysicalDeviceSurfaceSupportKHR( physical_device, i, vk_surface, &presentation_supported ) );

				if ( !presentation_supported || ( queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT ) == 0 ) {
					continue;
				}
				if ( best == ~0U ) {
					best = i;
				}
				if ( ( queue_families[i].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT ) != 0 && bestSparse == ~0U ) {
					bestSparse = i;
				}
			}
			vk.queue_family_index = ( bestSparse != ~0U ) ? bestSparse : best;
			if ( vk.queue_family_index != ~0U &&
				( queue_families[vk.queue_family_index].queueFlags & VK_QUEUE_SPARSE_BINDING_BIT ) != 0 ) {
				vk.sparseBinding = qtrue; /* provisional: queue supports sparse submits */
			}
		}

		ri.Free( queue_families );

		if ( vk.queue_family_index == ~0U ) {
			ri.Printf( PRINT_ERROR, "...failed to find graphics queue family\n" );
			return qfalse;
		}
	}

	{
		const char *device_extension_list[40];
		uint32_t device_extension_count;
		const char *ext, *end;
		char *str;
		const float priority = 1.0;
		VkExtensionProperties *extension_properties;
		VkDeviceQueueCreateInfo queue_desc;
		VkPhysicalDeviceFeatures device_features;
		VkPhysicalDeviceFeatures features;
		VkDeviceCreateInfo device_desc;
#ifdef USE_VULKAN_RTX
		VkPhysicalDeviceFeatures2 rtx_features2;
		VkPhysicalDeviceVulkan12Features rtx_vulkan12_features;
		VkPhysicalDeviceAccelerationStructureFeaturesKHR rtx_accel_features;
		VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtx_pipeline_features;
		VkPhysicalDeviceRayQueryFeaturesKHR rtx_ray_query_features;
#endif
		VkResult res;
		qboolean swapchainSupported = qfalse;
		qboolean dedicatedAllocation = qfalse;
		qboolean memoryRequirements2 = qfalse;
		qboolean debugMarker = qfalse;
		qboolean swapchainMaintenance1 = qfalse;
		qboolean surfaceMaintenance1 = qfalse;
		qboolean provokingVertex = qfalse;
		qboolean nonSeamlessCubeMap = qfalse;
		qboolean primTopoListRestart = qfalse;
		qboolean legacyDithering = qfalse;
		qboolean maintenance9 = qfalse;
		qboolean shaderFma = qfalse;
		qboolean portabilitySubset = qfalse;
		qboolean dynamicRenderingLocalRead = qfalse;
		qboolean globalPriority = qfalse;
		qboolean lineRasterization = qfalse;
		qboolean lineRasterizationExt = qfalse;
		qboolean maintenance5 = qfalse;
		qboolean maintenance8 = qfalse;
		qboolean presentId = qfalse;
		qboolean presentWait = qfalse;
		qboolean hostQueryReset = qfalse;
		qboolean shaderFloatControls2 = qfalse;
		qboolean shaderMaximalReconvergence = qfalse;
		qboolean shaderQuadControl = qfalse;
		qboolean shaderRelaxedExtInstr = qfalse;
		qboolean shaderSubgroupUniformCF = qfalse;
		qboolean extendedDynamicState3 = qfalse;
		qboolean nvMeshShader = qfalse;
#if defined( USE_VUDA ) || defined( USE_MIMIR_CUDA )
		qboolean vudaExtMemory = qfalse;
		qboolean vudaExtMemoryFd = qfalse;
#endif
#ifdef USE_VUDA
		qboolean vudaExtSem = qfalse;
		qboolean vudaExtSemFd = qfalse;
		qboolean vudaTimelineSem = qfalse;
		VkPhysicalDeviceTimelineSemaphoreFeatures vuda_timeline_features;
#endif
#ifdef USE_VULKAN_RTX
		qboolean rtxAccelStruct = qfalse;
		qboolean rtxPipeline = qfalse;
		qboolean rtxDeferredHostOps = qfalse;
		qboolean rtxBufferDeviceAddress = qfalse;
		qboolean rtxRayQuery = qfalse;
#endif
#ifdef _DEBUG
		qboolean timelineSemaphore = qfalse;
		qboolean memoryModel = qfalse;
		qboolean devAddrFeat = qfalse;
		qboolean storage8bit = qfalse;
		const void** pNextPtr;
#endif
		uint32_t i, len, count = 0;

		VK_CHECK( qvkEnumerateDeviceExtensionProperties( physical_device, NULL, &count, NULL ) );
		extension_properties = (VkExtensionProperties*)ri.Malloc( count * sizeof( VkExtensionProperties ) );
		VK_CHECK( qvkEnumerateDeviceExtensionProperties( physical_device, NULL, &count, extension_properties ) );

		str = glConfig.extensions_string; *str = '\0';
		end = &glConfig.extensions_string[ sizeof( glConfig.extensions_string ) - 1];

		for ( i = 0; i < count; i++ ) {
			ext = extension_properties[i].extensionName;
			if ( strcmp( ext, VK_KHR_SWAPCHAIN_EXTENSION_NAME ) == 0 ) {
				swapchainSupported = qtrue;
			} else if ( strcmp( ext, VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME ) == 0 ) {
				dedicatedAllocation = qtrue;
			} else if ( strcmp( ext, VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME ) == 0 ) {
				memoryRequirements2 = qtrue;
			} else if ( strcmp( ext, VK_EXT_DEBUG_MARKER_EXTENSION_NAME ) == 0 ) {
				debugMarker = qtrue;
#ifdef _DEBUG
			} else if ( strcmp( ext, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME ) == 0 ) {
				timelineSemaphore = qtrue;
			} else if ( strcmp( ext, VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME ) == 0 ) {
				memoryModel = qtrue;
			} else if ( strcmp( ext, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME ) == 0 ) {
				devAddrFeat = qtrue;
			} else if ( strcmp( ext, VK_KHR_8BIT_STORAGE_EXTENSION_NAME ) == 0 ) {
				storage8bit = qtrue;
#endif
			}
			if ( strcmp( ext, "VK_KHR_swapchain_maintenance1" ) == 0 ) {
				swapchainMaintenance1 = qtrue;
			} else if ( strcmp( ext, "VK_KHR_surface_maintenance1" ) == 0 ) {
				surfaceMaintenance1 = qtrue;
			} else if ( strcmp( ext, "VK_EXT_provoking_vertex" ) == 0 ) {
				provokingVertex = qtrue;
			} else if ( strcmp( ext, "VK_EXT_non_seamless_cube_map" ) == 0 ) {
				nonSeamlessCubeMap = qtrue;
			} else if ( strcmp( ext, "VK_EXT_primitive_topology_list_restart" ) == 0 ) {
				primTopoListRestart = qtrue;
			} else if ( strcmp( ext, "VK_EXT_legacy_dithering" ) == 0 ) {
				legacyDithering = qtrue;
			} else if ( strcmp( ext, "VK_KHR_maintenance9" ) == 0 ) {
				maintenance9 = qtrue;
			} else if ( strcmp( ext, "VK_KHR_shader_fma" ) == 0 ) {
				shaderFma = qtrue;
			} else if ( strcmp( ext, "VK_KHR_portability_subset" ) == 0 ) {
				portabilitySubset = qtrue;
			} else if ( strcmp( ext, "VK_KHR_dynamic_rendering_local_read" ) == 0 ) {
				dynamicRenderingLocalRead = qtrue;
			} else if ( strcmp( ext, "VK_KHR_global_priority" ) == 0 ) {
				globalPriority = qtrue;
			} else if ( strcmp( ext, "VK_KHR_line_rasterization" ) == 0 ) {
				lineRasterization = qtrue;
			} else if ( strcmp( ext, "VK_EXT_line_rasterization" ) == 0 ) {
				lineRasterizationExt = qtrue;
			} else if ( strcmp( ext, "VK_KHR_maintenance5" ) == 0 ) {
				maintenance5 = qtrue;
			} else if ( strcmp( ext, "VK_KHR_maintenance8" ) == 0 ) {
				maintenance8 = qtrue;
			} else if ( strcmp( ext, "VK_KHR_present_id" ) == 0 || strcmp( ext, "VK_KHR_present_id2" ) == 0 ) {
				presentId = qtrue;
			} else if ( strcmp( ext, "VK_KHR_present_wait" ) == 0 || strcmp( ext, "VK_KHR_present_wait2" ) == 0 ) {
				presentWait = qtrue;
			} else if ( strcmp( ext, VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME ) == 0 ) {
				hostQueryReset = qtrue;
			} else if ( strcmp( ext, "VK_KHR_shader_float_controls2" ) == 0 ) {
				shaderFloatControls2 = qtrue;
			} else if ( strcmp( ext, "VK_KHR_shader_maximal_reconvergence" ) == 0 ) {
				shaderMaximalReconvergence = qtrue;
			} else if ( strcmp( ext, "VK_KHR_shader_quad_control" ) == 0 ) {
				shaderQuadControl = qtrue;
			} else if ( strcmp( ext, "VK_KHR_shader_relaxed_extended_instruction" ) == 0 ) {
				shaderRelaxedExtInstr = qtrue;
			} else if ( strcmp( ext, "VK_KHR_shader_subgroup_uniform_control_flow" ) == 0 ) {
				shaderSubgroupUniformCF = qtrue;
			} else if ( strcmp( ext, "VK_EXT_extended_dynamic_state3" ) == 0 ) {
				extendedDynamicState3 = qtrue;
			} else if ( strcmp( ext, VK_NV_MESH_SHADER_EXTENSION_NAME ) == 0 ) {
				nvMeshShader = qtrue;
			}
#if defined( USE_VUDA ) || defined( USE_MIMIR_CUDA )
			else if ( strcmp( ext, VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME ) == 0 ) {
				vudaExtMemory = qtrue;
			} else if ( strcmp( ext, VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME ) == 0 ) {
				vudaExtMemoryFd = qtrue;
			}
#endif
#ifdef USE_VUDA
			else if ( strcmp( ext, VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME ) == 0 ) {
				vudaExtSem = qtrue;
			} else if ( strcmp( ext, VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME ) == 0 ) {
				vudaExtSemFd = qtrue;
			} else if ( strcmp( ext, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME ) == 0 ) {
				vudaTimelineSem = qtrue;
			}
#endif
#ifdef USE_VULKAN_RTX
			else if ( strcmp( ext, VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME ) == 0 ) {
				rtxAccelStruct = qtrue;
			} else if ( strcmp( ext, VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME ) == 0 ) {
				rtxPipeline = qtrue;
			} else if ( strcmp( ext, VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME ) == 0 ) {
				rtxDeferredHostOps = qtrue;
			} else if ( strcmp( ext, VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME ) == 0 ) {
				rtxBufferDeviceAddress = qtrue;
			} else if ( strcmp( ext, VK_KHR_RAY_QUERY_EXTENSION_NAME ) == 0 ) {
				rtxRayQuery = qtrue;
			}
#endif
			if ( i != 0 ) {
				if ( str + 1 >= end )
					continue;
				str = Q_stradd( str, " " );
			}
			len = (uint32_t)strlen( ext );
			if ( str + len >= end )
				continue;
			str = Q_stradd( str, ext );
		}

		ri.Free( extension_properties );

		device_extension_count = 0;
		vk.meshShaderNV = qfalse;

		if ( !swapchainSupported ) {
			ri.Printf( PRINT_ERROR, "...required device extension is not available: %s\n", VK_KHR_SWAPCHAIN_EXTENSION_NAME );
			return qfalse;
		}

		if ( !memoryRequirements2 )
			dedicatedAllocation = qfalse;
		else
			vk.dedicatedAllocation = dedicatedAllocation;

		device_extension_list[ device_extension_count++ ] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

		if ( vk.dedicatedAllocation ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME;
			device_extension_list[ device_extension_count++ ] = VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME;
		}

		if ( debugMarker ) {
			device_extension_list[ device_extension_count++ ] = VK_EXT_DEBUG_MARKER_EXTENSION_NAME;
			vk.debugMarkers = qtrue;
		}
#ifdef _DEBUG
		if ( timelineSemaphore ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME;
		}

		if ( memoryModel ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME;
		}

		if ( devAddrFeat ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME;
		}

		if ( storage8bit ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_8BIT_STORAGE_EXTENSION_NAME;
		}
#endif

		if ( portabilitySubset ) {
			device_extension_list[ device_extension_count++ ] = "VK_KHR_portability_subset";
		}
		if ( swapchainMaintenance1 && surfaceMaintenance1 ) {
			device_extension_list[ device_extension_count++ ] = "VK_KHR_surface_maintenance1";
			device_extension_list[ device_extension_count++ ] = "VK_KHR_swapchain_maintenance1";
			ri.Printf( PRINT_DEVELOPER, "  VK_KHR_surface_maintenance1 + VK_KHR_swapchain_maintenance1: enabled\n" );
		}
		if ( provokingVertex ) {
			device_extension_list[ device_extension_count++ ] = "VK_EXT_provoking_vertex";
			ri.Printf( PRINT_DEVELOPER, "  VK_EXT_provoking_vertex: enabled\n" );
		}
		if ( nonSeamlessCubeMap ) {
			device_extension_list[ device_extension_count++ ] = "VK_EXT_non_seamless_cube_map";
			ri.Printf( PRINT_DEVELOPER, "  VK_EXT_non_seamless_cube_map: enabled\n" );
		}
		if ( primTopoListRestart ) {
			device_extension_list[ device_extension_count++ ] = "VK_EXT_primitive_topology_list_restart";
			ri.Printf( PRINT_DEVELOPER, "  VK_EXT_primitive_topology_list_restart: enabled\n" );
		}
		if ( maintenance9 ) {
			device_extension_list[ device_extension_count++ ] = "VK_KHR_maintenance9";
			ri.Printf( PRINT_DEVELOPER, "  VK_KHR_maintenance9: enabled\n" );
		}
		if ( shaderFma ) {
			device_extension_list[ device_extension_count++ ] = "VK_KHR_shader_fma";
			ri.Printf( PRINT_DEVELOPER, "  VK_KHR_shader_fma: enabled\n" );
		}
		if ( lineRasterization ) {
			device_extension_list[ device_extension_count++ ] = "VK_KHR_line_rasterization";
			ri.Printf( PRINT_DEVELOPER, "  VK_KHR_line_rasterization: enabled\n" );
		} else if ( lineRasterizationExt ) {
			device_extension_list[ device_extension_count++ ] = "VK_EXT_line_rasterization";
			ri.Printf( PRINT_DEVELOPER, "  VK_EXT_line_rasterization: enabled\n" );
		}
		if ( maintenance5 ) {
			device_extension_list[ device_extension_count++ ] = "VK_KHR_maintenance5";
			ri.Printf( PRINT_DEVELOPER, "  VK_KHR_maintenance5: enabled\n" );
		}
		if ( maintenance8 ) {
			device_extension_list[ device_extension_count++ ] = "VK_KHR_maintenance8";
			ri.Printf( PRINT_DEVELOPER, "  VK_KHR_maintenance8: enabled\n" );
		}
		if ( dynamicRenderingLocalRead ) {
			device_extension_list[ device_extension_count++ ] = "VK_KHR_dynamic_rendering_local_read";
		}
		if ( presentId ) {
			device_extension_list[ device_extension_count++ ] = "VK_KHR_present_id";
		}
		if ( presentWait ) {
			device_extension_list[ device_extension_count++ ] = "VK_KHR_present_wait";
		}
		if ( hostQueryReset ) {
			device_extension_list[ device_extension_count++ ] = VK_EXT_HOST_QUERY_RESET_EXTENSION_NAME;
		}
		if ( shaderQuadControl ) {
			if ( shaderMaximalReconvergence ) {
				device_extension_list[ device_extension_count++ ] = "VK_KHR_shader_maximal_reconvergence";
			}
			device_extension_list[ device_extension_count++ ] = "VK_KHR_shader_quad_control";
		}
		if ( extendedDynamicState3 && r_vk_colorWriteMaskDynamic && r_vk_colorWriteMaskDynamic->integer ) {
			device_extension_list[ device_extension_count++ ] = "VK_EXT_extended_dynamic_state3";
			vk.colorWriteMaskDynamic = qtrue;
			ri.Printf( PRINT_DEVELOPER, "  VK_EXT_extended_dynamic_state3: enabled (RB_ColorMask)\n" );
		} else {
			vk.colorWriteMaskDynamic = qfalse;
		}
		if ( nvMeshShader && r_vk_meshShaderNV && r_vk_meshShaderNV->integer &&
			device_extension_count < ARRAY_LEN( device_extension_list ) ) {
			device_extension_list[ device_extension_count++ ] = VK_NV_MESH_SHADER_EXTENSION_NAME;
			vk.meshShaderNV = qtrue;
			ri.Printf( PRINT_ALL, "[VK] VK_NV_mesh_shader enabled for virtual geometry experiments; meshlet MDI fallback remains available\n" );
		}
#ifdef USE_VUDA
		vk.vudaInteropCapable = qfalse;
		if ( vudaExtMemory && vudaExtMemoryFd && vudaExtSem && vudaExtSemFd && vudaTimelineSem &&
			device_extension_count + 5 <= ARRAY_LEN( device_extension_list ) ) {
			device_extension_list[ device_extension_count++ ] = VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME;
			device_extension_list[ device_extension_count++ ] = VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME;
			device_extension_list[ device_extension_count++ ] = VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME;
			device_extension_list[ device_extension_count++ ] = VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME;
			device_extension_list[ device_extension_count++ ] = VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME;
			vk.vudaInteropCapable = qtrue;
			ri.Printf( PRINT_ALL, "[VUDA] KHR external memory/semaphore fd + timeline: enabled\n" );
		}
#endif
#ifdef USE_MIMIR_CUDA
		vk.mimirInteropCapable = qfalse;
		if ( vudaExtMemory && vudaExtMemoryFd ) {
			qboolean mimir_need_ext = qtrue;
#ifdef USE_VUDA
			if ( vk.vudaInteropCapable ) {
				mimir_need_ext = qfalse;
			}
#endif
			if ( mimir_need_ext && device_extension_count + 2 <= ARRAY_LEN( device_extension_list ) ) {
				device_extension_list[ device_extension_count++ ] = VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME;
				device_extension_list[ device_extension_count++ ] = VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME;
			}
			vk.mimirInteropCapable = qtrue;
			ri.Printf( PRINT_ALL, "[Mímir] KHR external memory fd: enabled (CUDA/Vulkan interop)\n" );
		}
#endif
		(void)globalPriority; (void)shaderFloatControls2; (void)shaderMaximalReconvergence;
		(void)shaderRelaxedExtInstr; (void)shaderSubgroupUniformCF; (void)legacyDithering;
		(void)surfaceMaintenance1;
#ifdef USE_VULKAN_RTX
		vk.rtxAvailable = qfalse;
		vk.rayQueryAvailable = qfalse;
		if ( rtxAccelStruct && rtxPipeline && rtxDeferredHostOps && rtxBufferDeviceAddress && memoryRequirements2 ) {
			cvar_t *r_rtx_cvar = ri.Cvar_Get( "r_rtx", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
			cvar_t *r_hybrid1_cvar = ri.Cvar_Get( "r_hybrid1", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
			cvar_t *r_raygun_cvar = ri.Cvar_Get( "r_raygun", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
			cvar_t *r_surfelGi_cvar = ri.Cvar_Get( "r_surfelGi", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
			cvar_t *r_rcgi_cvar = ri.Cvar_Get( "r_rcgi", "0", CVAR_ARCHIVE_ND | CVAR_LATCH );
			if ( ( r_rtx_cvar && r_rtx_cvar->integer > 0 ) || ( r_hybrid1_cvar && r_hybrid1_cvar->integer > 0 )
				|| ( r_raygun_cvar && r_raygun_cvar->integer > 0 )
				|| ( r_surfelGi_cvar && r_surfelGi_cvar->integer > 0 )
				|| ( r_rcgi_cvar && r_rcgi_cvar->integer > 0 ) ) {
				if ( qvkGetPhysicalDeviceFeatures2 == NULL ) {
					ri.Printf( PRINT_WARNING, "[VK] Ray tracing: vkGetPhysicalDeviceFeatures2 unavailable; cannot verify RT features\n" );
				} else if ( device_extension_count + 5 > ARRAY_LEN( device_extension_list ) ) {
					ri.Printf( PRINT_WARNING, "[VK] Ray tracing: device extension list full; skipping RT\n" );
				} else {
					Com_Memset( &rtx_features2, 0, sizeof( rtx_features2 ) );
					Com_Memset( &rtx_vulkan12_features, 0, sizeof( rtx_vulkan12_features ) );
					Com_Memset( &rtx_accel_features, 0, sizeof( rtx_accel_features ) );
					Com_Memset( &rtx_pipeline_features, 0, sizeof( rtx_pipeline_features ) );
					Com_Memset( &rtx_ray_query_features, 0, sizeof( rtx_ray_query_features ) );
					rtx_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
					rtx_features2.pNext = &rtx_vulkan12_features;
					rtx_vulkan12_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
					rtx_vulkan12_features.pNext = &rtx_accel_features;
					rtx_accel_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
					rtx_accel_features.pNext = &rtx_pipeline_features;
					rtx_pipeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
					rtx_pipeline_features.pNext = &rtx_ray_query_features;
					rtx_ray_query_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
					rtx_ray_query_features.pNext = NULL;
					qvkGetPhysicalDeviceFeatures2( physical_device, &rtx_features2 );
					if ( rtx_vulkan12_features.bufferDeviceAddress && rtx_accel_features.accelerationStructure
						&& rtx_pipeline_features.rayTracingPipeline ) {
						device_extension_list[ device_extension_count++ ] = VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME;
						device_extension_list[ device_extension_count++ ] = VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME;
						device_extension_list[ device_extension_count++ ] = VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME;
						device_extension_list[ device_extension_count++ ] = VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME;
						vk.rtxAvailable = qtrue;
						rtx_vulkan12_features.bufferDeviceAddress = VK_TRUE;
						rtx_vulkan12_features.bufferDeviceAddressCaptureReplay = VK_FALSE;
						rtx_vulkan12_features.bufferDeviceAddressMultiDevice = VK_FALSE;
						rtx_accel_features.accelerationStructure = VK_TRUE;
						rtx_accel_features.accelerationStructureCaptureReplay = VK_FALSE;
						rtx_accel_features.accelerationStructureIndirectBuild = VK_FALSE;
						rtx_accel_features.accelerationStructureHostCommands = VK_FALSE;
						rtx_accel_features.descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE;
						rtx_pipeline_features.rayTracingPipeline = VK_TRUE;
						rtx_pipeline_features.rayTracingPipelineShaderGroupHandleCaptureReplay = VK_FALSE;
						rtx_pipeline_features.rayTracingPipelineShaderGroupHandleCaptureReplayMixed = VK_FALSE;
						rtx_pipeline_features.rayTracingPipelineTraceRaysIndirect = VK_FALSE;
						rtx_pipeline_features.rayTraversalPrimitiveCulling = VK_FALSE;
						if ( rtxRayQuery && rtx_ray_query_features.rayQuery ) {
							device_extension_list[ device_extension_count++ ] = VK_KHR_RAY_QUERY_EXTENSION_NAME;
							rtx_ray_query_features.rayQuery = VK_TRUE;
							vk.rayQueryAvailable = qtrue;
							ri.Printf( PRINT_ALL, "[VK] Ray tracing: KHR AS + RT pipeline + ray query + BDA enabled (rtx_status; Hybrid1 / Raygun / SurfelGI)\n" );
						} else {
							rtx_pipeline_features.pNext = NULL;
							ri.Printf( PRINT_ALL, "[VK] Ray tracing: KHR AS + RT pipeline + BDA enabled (no ray query; Surfel GI unavailable)\n" );
						}
					} else {
						ri.Printf( PRINT_WARNING, "[VK] Ray tracing: extensions present but required features unsupported on this device; skipping\n" );
					}
				}
			} else {
				ri.Printf( PRINT_ALL, "[VK][RTX] chocolate path ready (GPU has KHR RT; set r_rtx / r_hybrid1 / r_raygun / r_surfelGi / r_rcgi 1 + vid_restart)\n" );
			}
		}
#endif
		qvkGetPhysicalDeviceFeatures( physical_device, &device_features );

		if ( device_features.fillModeNonSolid == VK_FALSE ) {
			ri.Printf( PRINT_ERROR, "...fillModeNonSolid feature is not supported\n" );
			return qfalse;
		}

		queue_desc.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_desc.pNext = NULL;
		queue_desc.flags = 0;
		queue_desc.queueFamilyIndex = vk.queue_family_index;
		queue_desc.queueCount = 1;
		queue_desc.pQueuePriorities = &priority;

		Com_Memset( &features, 0, sizeof( features ) );
		features.fillModeNonSolid = VK_TRUE;

#ifdef _DEBUG
		if ( device_features.shaderInt64 ) {
			features.shaderInt64 = VK_TRUE;
		}
#endif
		vk.shaderFloat64 = device_features.shaderFloat64 ? qtrue : qfalse;
		if ( device_features.shaderFloat64 ) {
			if ( ( r_hdr && r_hdr->integer == 3 ) || ( r_fp64Points && r_fp64Points->integer ) ) {
				features.shaderFloat64 = VK_TRUE;
				ri.Printf( PRINT_ALL, "[VK][fp64] shaderFloat64 enabled (r_hdr 3 and/or r_fp64Points 1)\n" );
			}
		}
		if ( device_features.wideLines ) {
			features.wideLines = VK_TRUE;
			vk.wideLines = qtrue;
		}

		if ( device_features.fragmentStoresAndAtomics && device_features.vertexPipelineStoresAndAtomics ) {
			features.vertexPipelineStoresAndAtomics = VK_TRUE;
			features.fragmentStoresAndAtomics = VK_TRUE;
			vk.fragmentStores = qtrue;
		}
#ifdef USE_VK_PBR
		if ( device_features.geometryShader )
			features.geometryShader = VK_TRUE;
#endif
		if ( r_ext_texture_filter_anisotropic->integer && device_features.samplerAnisotropy ) {
			features.samplerAnisotropy = VK_TRUE;
			vk.samplerAnisotropy = qtrue;
		}
		if ( device_features.independentBlend ) {
			features.independentBlend = VK_TRUE;
		}

		/* Sparse residency (VT / MegaTexture-class). Requires features + queue SPARSE_BINDING. */
		{
			qboolean queueSparse = vk.sparseBinding;
			VkPhysicalDeviceProperties props;

			vk.sparseBinding = qfalse;
			vk.sparseResidencyImage2D = qfalse;
			vk.sparseResidencyNonResidentStrict = qfalse;
			if ( queueSparse && device_features.sparseBinding && device_features.sparseResidencyImage2D ) {
				features.sparseBinding = VK_TRUE;
				features.sparseResidencyImage2D = VK_TRUE;
				vk.sparseBinding = qtrue;
				vk.sparseResidencyImage2D = qtrue;
				qvkGetPhysicalDeviceProperties( physical_device, &props );
				if ( props.sparseProperties.residencyNonResidentStrict ) {
					vk.sparseResidencyNonResidentStrict = qtrue;
				}
				ri.Printf( PRINT_ALL, "[VK] sparseBinding + sparseResidencyImage2D enabled%s\n",
					vk.sparseResidencyNonResidentStrict ? " (nonResidentStrict)" : "" );
			} else {
				ri.Printf( PRINT_ALL, "[VK] sparse residency unavailable (features=%d/%d queueSparse=%d)\n",
					(int)device_features.sparseBinding, (int)device_features.sparseResidencyImage2D, (int)queueSparse );
			}
		}

		device_desc.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_desc.pNext = NULL;
		device_desc.flags = 0;
		device_desc.queueCreateInfoCount = 1;
		device_desc.pQueueCreateInfos = &queue_desc;
		device_desc.enabledLayerCount = 0;
		device_desc.ppEnabledLayerNames = NULL;
		device_desc.enabledExtensionCount = device_extension_count;
		device_desc.ppEnabledExtensionNames = device_extension_list;
		device_desc.pEnabledFeatures = &features;

		VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extDynState3 = {0};
		if ( extendedDynamicState3 && r_vk_colorWriteMaskDynamic && r_vk_colorWriteMaskDynamic->integer ) {
			extDynState3.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT;
			extDynState3.pNext = NULL;
			extDynState3.extendedDynamicState3ColorWriteMask = VK_TRUE;
			device_desc.pNext = &extDynState3;
		}

#ifdef _DEBUG
		pNextPtr = (const void **)( ( extendedDynamicState3 && vk.colorWriteMaskDynamic ) ? &extDynState3.pNext : &device_desc.pNext );

		if ( timelineSemaphore ) {
			*pNextPtr = &timeline_semaphore;
			timeline_semaphore.pNext = NULL;
			timeline_semaphore.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
			timeline_semaphore.timelineSemaphore = VK_TRUE;
			pNextPtr = (const void **)&timeline_semaphore.pNext;
		}

		if ( memoryModel ) {
			*pNextPtr = &memory_model;
			memory_model.pNext = NULL;
			memory_model.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES;
			memory_model.vulkanMemoryModel = VK_TRUE;
			memory_model.vulkanMemoryModelAvailabilityVisibilityChains = VK_FALSE;
			memory_model.vulkanMemoryModelDeviceScope = VK_TRUE;
			pNextPtr = (const void **)&memory_model.pNext;
		}

		if ( devAddrFeat
#ifdef USE_VULKAN_RTX
			&& !vk.rtxAvailable
#endif
			) {
			*pNextPtr = &devaddr_features;
			devaddr_features.pNext = NULL;
			devaddr_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
			devaddr_features.bufferDeviceAddress = VK_TRUE;
			devaddr_features.bufferDeviceAddressCaptureReplay = VK_FALSE;
			devaddr_features.bufferDeviceAddressMultiDevice = VK_FALSE;
			pNextPtr = (const void **)&devaddr_features.pNext;
		}

		if ( storage8bit ) {
			*pNextPtr = &storage_8bit_features;
			storage_8bit_features.pNext = NULL;
			storage_8bit_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES;
			storage_8bit_features.storageBuffer8BitAccess = VK_TRUE;
			storage_8bit_features.storagePushConstant8 = VK_FALSE;
			storage_8bit_features.uniformAndStorageBuffer8BitAccess = VK_TRUE;
			pNextPtr = (const void **)&storage_8bit_features.pNext;
		}
#endif
		if ( hostQueryReset ) {
			const void *prev_next = device_desc.pNext;
			Com_Memset( &host_query, 0, sizeof( host_query ) );
			host_query.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_HOST_QUERY_RESET_FEATURES;
			host_query.hostQueryReset = VK_TRUE;
			host_query.pNext = prev_next ? (void *)(uintptr_t)prev_next : NULL;
			device_desc.pNext = &host_query;
		}

#ifdef USE_VUDA
		if ( vk.vudaInteropCapable ) {
			const void *prev_next = device_desc.pNext;
			Com_Memset( &vuda_timeline_features, 0, sizeof( vuda_timeline_features ) );
			vuda_timeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
			vuda_timeline_features.timelineSemaphore = VK_TRUE;
			vuda_timeline_features.pNext = prev_next ? (void *)(uintptr_t)prev_next : NULL;
			device_desc.pNext = &vuda_timeline_features;
		}
#endif

		/* Chain last so _DEBUG / host_query pNext lists stay valid. */
		if ( vk.meshShaderNV ) {
			Com_Memset( &mesh_shader_features_nv, 0, sizeof( mesh_shader_features_nv ) );
			mesh_shader_features_nv.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV;
			mesh_shader_features_nv.pNext = (void *)(uintptr_t)device_desc.pNext;
			mesh_shader_features_nv.taskShader = VK_FALSE;
			mesh_shader_features_nv.meshShader = VK_TRUE;
			device_desc.pNext = &mesh_shader_features_nv;
		}

#ifdef USE_VULKAN_RTX
		if ( vk.rtxAvailable ) {
			Com_Memcpy( &rtx_features2.features, &features, sizeof( features ) );
			rtx_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			rtx_features2.pNext = &rtx_vulkan12_features;
			rtx_vulkan12_features.pNext = &rtx_accel_features;
			rtx_accel_features.pNext = &rtx_pipeline_features;
			rtx_pipeline_features.pNext = (void *)(uintptr_t)device_desc.pNext;
			device_desc.pNext = &rtx_features2;
			device_desc.pEnabledFeatures = NULL;
		}
#endif

		res = qvkCreateDevice( physical_device, &device_desc, NULL, &vk.device );
		if ( res < 0 ) {
			ri.Printf( PRINT_ERROR, "vkCreateDevice returned %s\n", vk_result_string( res ) );
			return qfalse;
		}

#ifdef USE_VULKAN_RTX
		if ( vk.rtxAvailable ) {
			INIT_DEVICE_FUNCTION_EXT( vkGetBufferDeviceAddress );
			INIT_DEVICE_FUNCTION_EXT( vkCreateAccelerationStructureKHR );
			INIT_DEVICE_FUNCTION_EXT( vkDestroyAccelerationStructureKHR );
			INIT_DEVICE_FUNCTION_EXT( vkGetAccelerationStructureBuildSizesKHR );
			INIT_DEVICE_FUNCTION_EXT( vkGetAccelerationStructureDeviceAddressKHR );
			INIT_DEVICE_FUNCTION_EXT( vkCmdBuildAccelerationStructuresKHR );
			INIT_DEVICE_FUNCTION_EXT( vkCreateRayTracingPipelinesKHR );
			INIT_DEVICE_FUNCTION_EXT( vkGetRayTracingShaderGroupHandlesKHR );
			INIT_DEVICE_FUNCTION_EXT( vkCmdTraceRaysKHR );
			if ( !qvkGetBufferDeviceAddress || !qvkCreateAccelerationStructureKHR || !qvkDestroyAccelerationStructureKHR
				|| !qvkGetAccelerationStructureBuildSizesKHR || !qvkGetAccelerationStructureDeviceAddressKHR
				|| !qvkCmdBuildAccelerationStructuresKHR
				|| !qvkCreateRayTracingPipelinesKHR || !qvkGetRayTracingShaderGroupHandlesKHR || !qvkCmdTraceRaysKHR ) {
				ri.Printf( PRINT_WARNING, "[VK] Ray tracing: failed to resolve one or more KHR entry points; abandoning this device\n" );
				qvkDestroyDevice( vk.device, NULL );
				vk.device = VK_NULL_HANDLE;
				vk.rtxAvailable = qfalse;
				vk.meshShaderNV = qfalse;
				return qfalse;
			}
		}
#endif

#ifdef USE_VUDA
		if ( vk.vudaInteropCapable ) {
			INIT_DEVICE_FUNCTION_EXT( vkGetMemoryFdKHR );
			INIT_DEVICE_FUNCTION_EXT( vkGetMemoryFdPropertiesKHR );
			INIT_DEVICE_FUNCTION_EXT( vkGetSemaphoreFdKHR );
			INIT_DEVICE_FUNCTION_EXT( vkWaitSemaphoresKHR );
			INIT_DEVICE_FUNCTION_EXT( vkSignalSemaphoreKHR );
			if ( !qvkGetMemoryFdKHR || !qvkGetSemaphoreFdKHR || !qvkWaitSemaphoresKHR || !qvkSignalSemaphoreKHR ) {
				ri.Printf( PRINT_WARNING, "[VUDA] Failed to resolve interop entry points; disabling\n" );
				vk.vudaInteropCapable = qfalse;
			} else {
				R_VUDA_TryBuildInterop();
			}
		}
#endif
#ifdef USE_MIMIR_CUDA
		if ( vk.mimirInteropCapable && !qvkGetMemoryFdKHR ) {
			INIT_DEVICE_FUNCTION_EXT( vkGetMemoryFdKHR );
			INIT_DEVICE_FUNCTION_EXT( vkGetMemoryFdPropertiesKHR );
			if ( !qvkGetMemoryFdKHR ) {
				ri.Printf( PRINT_WARNING, "[Mímir] Failed to resolve GetMemoryFdKHR; CUDA interop disabled\n" );
				vk.mimirInteropCapable = qfalse;
			}
		}
#endif
	}

	return qtrue;
}


void vk_destroy_instance( void )
{
	if ( vk_surface != VK_NULL_HANDLE ) {
		if ( qvkDestroySurfaceKHR != NULL ) {
			qvkDestroySurfaceKHR( vk_instance, vk_surface, NULL );
		}
		vk_surface = VK_NULL_HANDLE;
	}

#ifdef USE_VK_VALIDATION
	if ( vk_debug_callback ) {
		if ( qvkDestroyDebugReportCallbackEXT != NULL ) {
			qvkDestroyDebugReportCallbackEXT( vk_instance, vk_debug_callback, NULL );
		}
		vk_debug_callback = VK_NULL_HANDLE;
	}
#endif

	if ( vk_instance != VK_NULL_HANDLE ) {
		if ( qvkDestroyInstance ) {
			qvkDestroyInstance( vk_instance, NULL );
		}
		vk_instance = VK_NULL_HANDLE;
	}
}


void vk_init_vulkan_library( void )
{
	VkPhysicalDeviceProperties props;
	VkPhysicalDevice *physical_devices;
	uint32_t device_count;
	int device_index, i;
	VkResult res;

	Com_Memset( &vk, 0, sizeof( vk ) );

	if ( vk_instance == VK_NULL_HANDLE ) {

		vk_destroy_instance();

		INIT_INSTANCE_FUNCTION( vkCreateInstance )
		INIT_INSTANCE_FUNCTION( vkEnumerateInstanceExtensionProperties )

		create_instance();

		INIT_INSTANCE_FUNCTION( vkCreateDevice )
		INIT_INSTANCE_FUNCTION( vkDestroyInstance )
		INIT_INSTANCE_FUNCTION( vkEnumerateDeviceExtensionProperties )
		INIT_INSTANCE_FUNCTION( vkEnumeratePhysicalDevices )
		INIT_INSTANCE_FUNCTION( vkGetDeviceProcAddr )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFeatures )
		INIT_INSTANCE_FUNCTION_EXT( vkGetPhysicalDeviceFeatures2 )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceFormatProperties )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceMemoryProperties )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceProperties )
		INIT_INSTANCE_FUNCTION_EXT( vkGetPhysicalDeviceProperties2 )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceQueueFamilyProperties )
		INIT_INSTANCE_FUNCTION( vkDestroySurfaceKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceCapabilitiesKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceFormatsKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfacePresentModesKHR )
		INIT_INSTANCE_FUNCTION( vkGetPhysicalDeviceSurfaceSupportKHR )

#ifdef USE_VK_VALIDATION
		INIT_INSTANCE_FUNCTION_EXT( vkCreateDebugReportCallbackEXT )
		INIT_INSTANCE_FUNCTION_EXT( vkDestroyDebugReportCallbackEXT )

		if ( qvkCreateDebugReportCallbackEXT && qvkDestroyDebugReportCallbackEXT ) {
			VkDebugReportCallbackCreateInfoEXT desc;
			desc.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
			desc.pNext = NULL;
			desc.flags = VK_DEBUG_REPORT_WARNING_BIT_EXT |
				VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
				VK_DEBUG_REPORT_ERROR_BIT_EXT;
			desc.pfnCallback = &vk_validation_debug_callback;
			desc.pUserData = NULL;

			VK_CHECK( qvkCreateDebugReportCallbackEXT( vk_instance, &desc, NULL, &vk_debug_callback ) );
		}
#endif

		if ( !ri.VK_CreateSurface( vk_instance, &vk_surface ) ) {
			ri.Error( ERR_FATAL, "Error creating Vulkan surface (see SDL_Vulkan_CreateSurface message above)" );
			return;
		}
	}

	res = qvkEnumeratePhysicalDevices( vk_instance, &device_count, NULL );
	if ( device_count == 0 ) {
		ri.Error( ERR_FATAL, "Vulkan: no physical devices found" );
		return;
	}
	else if ( res < 0 ) {
		ri.Error( ERR_FATAL, "vkEnumeratePhysicalDevices returned %s", vk_result_string( res ) );
		return;
	}

	physical_devices = (VkPhysicalDevice*)ri.Malloc( device_count * sizeof( VkPhysicalDevice ) );
	VK_CHECK( qvkEnumeratePhysicalDevices( vk_instance, &device_count, physical_devices ) );

	device_index = r_device->integer;

	ri.Printf( PRINT_ALL, ".......................\nAvailable physical devices:\n" );
	for ( i = 0; (uint32_t) i < device_count; i++ ) {
		qvkGetPhysicalDeviceProperties( physical_devices[ i ], &props );
		ri.Printf( PRINT_ALL, " %i: %s\n", i, vk_device_renderer_name( &props ) );
		if ( device_index == -1 && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ) {
			device_index = i;
		} else if ( device_index == -2 && props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ) {
			device_index = i;
		}
	}
	ri.Printf( PRINT_ALL, ".......................\n" );

	vk.physical_device = VK_NULL_HANDLE;
	for ( i = 0; (uint32_t) i < device_count; i++, device_index++ ) {
		if ( device_index < 0 || (uint32_t)device_index >= device_count ) {
			device_index = 0;
		}
		if ( vk_create_device( physical_devices[ device_index ], device_index ) ) {
			vk.physical_device = physical_devices[ device_index ];
			break;
		}
	}

	ri.Free( physical_devices );

	if ( vk.physical_device == VK_NULL_HANDLE ) {
		ri.Error( ERR_FATAL, "Vulkan: unable to find any suitable physical device" );
		return;
	}

	INIT_DEVICE_FUNCTION(vkAllocateCommandBuffers)
	INIT_DEVICE_FUNCTION(vkAllocateDescriptorSets)
	INIT_DEVICE_FUNCTION(vkAllocateMemory)
	INIT_DEVICE_FUNCTION(vkBeginCommandBuffer)
	INIT_DEVICE_FUNCTION(vkBindBufferMemory)
	INIT_DEVICE_FUNCTION(vkBindImageMemory)
	INIT_DEVICE_FUNCTION(vkCmdBeginRenderPass)
	INIT_DEVICE_FUNCTION(vkCmdBindDescriptorSets)
	INIT_DEVICE_FUNCTION(vkCmdBindIndexBuffer)
	INIT_DEVICE_FUNCTION(vkCmdBindPipeline)
	INIT_DEVICE_FUNCTION(vkCmdBindVertexBuffers)
	INIT_DEVICE_FUNCTION(vkCmdBlitImage)
	INIT_DEVICE_FUNCTION(vkCmdClearAttachments)
	INIT_DEVICE_FUNCTION(vkCmdFillBuffer)
	INIT_DEVICE_FUNCTION(vkCmdCopyBuffer)
	INIT_DEVICE_FUNCTION(vkCmdCopyBufferToImage)
	INIT_DEVICE_FUNCTION(vkCmdCopyImage)
	INIT_DEVICE_FUNCTION(vkCmdCopyImageToBuffer)
	INIT_DEVICE_FUNCTION(vkCmdDraw)
	INIT_DEVICE_FUNCTION(vkCmdDrawIndexed)
	INIT_DEVICE_FUNCTION(vkCmdDrawIndexedIndirect)
	INIT_DEVICE_FUNCTION(vkCmdDispatch)
	INIT_DEVICE_FUNCTION(vkCmdEndRenderPass)
	INIT_DEVICE_FUNCTION(vkCmdNextSubpass)
	INIT_DEVICE_FUNCTION(vkCmdPipelineBarrier)
	INIT_DEVICE_FUNCTION(vkCmdPushConstants)
	INIT_DEVICE_FUNCTION(vkCmdSetDepthBias)
	INIT_DEVICE_FUNCTION(vkCmdSetScissor)
	INIT_DEVICE_FUNCTION(vkCmdSetViewport)
	INIT_DEVICE_FUNCTION(vkCmdWriteTimestamp)
	INIT_DEVICE_FUNCTION(vkCmdBeginQuery)
	INIT_DEVICE_FUNCTION(vkCmdEndQuery)
	INIT_DEVICE_FUNCTION_EXT(vkCmdResetQueryPool)
	INIT_DEVICE_FUNCTION(vkCreateBuffer)
	INIT_DEVICE_FUNCTION(vkCreateCommandPool)
	INIT_DEVICE_FUNCTION(vkCreateDescriptorPool)
	INIT_DEVICE_FUNCTION(vkCreateDescriptorSetLayout)
	INIT_DEVICE_FUNCTION(vkCreateFence)
	INIT_DEVICE_FUNCTION(vkCreateFramebuffer)
	INIT_DEVICE_FUNCTION(vkCreateComputePipelines)
	INIT_DEVICE_FUNCTION(vkCreateGraphicsPipelines)
	INIT_DEVICE_FUNCTION(vkCreateImage)
	INIT_DEVICE_FUNCTION(vkCreateImageView)
	INIT_DEVICE_FUNCTION(vkCreatePipelineCache)
	INIT_DEVICE_FUNCTION(vkCreatePipelineLayout)
	INIT_DEVICE_FUNCTION(vkCreateQueryPool)
	INIT_DEVICE_FUNCTION(vkCreateRenderPass)
	INIT_DEVICE_FUNCTION(vkCreateSampler)
	INIT_DEVICE_FUNCTION(vkCreateSemaphore)
	INIT_DEVICE_FUNCTION(vkCreateShaderModule)
	INIT_DEVICE_FUNCTION(vkDestroyBuffer)
	INIT_DEVICE_FUNCTION(vkDestroyCommandPool)
	INIT_DEVICE_FUNCTION(vkDestroyDescriptorPool)
	INIT_DEVICE_FUNCTION(vkDestroyDescriptorSetLayout)
	INIT_DEVICE_FUNCTION(vkDestroyDevice)
	INIT_DEVICE_FUNCTION(vkDestroyFence)
	INIT_DEVICE_FUNCTION(vkDestroyFramebuffer)
	INIT_DEVICE_FUNCTION(vkDestroyImage)
	INIT_DEVICE_FUNCTION(vkDestroyImageView)
	INIT_DEVICE_FUNCTION(vkDestroyPipeline)
	INIT_DEVICE_FUNCTION(vkDestroyPipelineCache)
	INIT_DEVICE_FUNCTION(vkDestroyPipelineLayout)
	INIT_DEVICE_FUNCTION(vkDestroyQueryPool)
	INIT_DEVICE_FUNCTION(vkDestroyRenderPass)
	INIT_DEVICE_FUNCTION(vkDestroySampler)
	INIT_DEVICE_FUNCTION(vkDestroySemaphore)
	INIT_DEVICE_FUNCTION(vkDestroyShaderModule)
	INIT_DEVICE_FUNCTION(vkDeviceWaitIdle)
	INIT_DEVICE_FUNCTION(vkEndCommandBuffer)
	INIT_DEVICE_FUNCTION(vkFlushMappedMemoryRanges)
	INIT_DEVICE_FUNCTION(vkFreeCommandBuffers)
	INIT_DEVICE_FUNCTION(vkFreeDescriptorSets)
	INIT_DEVICE_FUNCTION(vkFreeMemory)
	INIT_DEVICE_FUNCTION(vkGetBufferMemoryRequirements)
	INIT_DEVICE_FUNCTION(vkGetDeviceQueue)
	INIT_DEVICE_FUNCTION(vkGetImageMemoryRequirements)
	INIT_DEVICE_FUNCTION(vkGetImageSparseMemoryRequirements)
	INIT_DEVICE_FUNCTION(vkGetImageSubresourceLayout)
	INIT_DEVICE_FUNCTION(vkGetPipelineCacheData)
	INIT_DEVICE_FUNCTION(vkInvalidateMappedMemoryRanges)
	INIT_DEVICE_FUNCTION(vkMapMemory)
	INIT_DEVICE_FUNCTION(vkQueueSubmit)
	INIT_DEVICE_FUNCTION(vkQueueBindSparse)
	INIT_DEVICE_FUNCTION(vkQueueWaitIdle)
	INIT_DEVICE_FUNCTION(vkResetCommandBuffer)
	INIT_DEVICE_FUNCTION(vkResetDescriptorPool)
	INIT_DEVICE_FUNCTION(vkResetFences)
	INIT_DEVICE_FUNCTION(vkGetQueryPoolResults)
	INIT_DEVICE_FUNCTION_EXT(vkResetQueryPoolEXT)
	INIT_DEVICE_FUNCTION(vkUnmapMemory)
	INIT_DEVICE_FUNCTION(vkUpdateDescriptorSets)
	INIT_DEVICE_FUNCTION(vkWaitForFences)
	INIT_DEVICE_FUNCTION(vkAcquireNextImageKHR)
	INIT_DEVICE_FUNCTION(vkCreateSwapchainKHR)
	INIT_DEVICE_FUNCTION(vkDestroySwapchainKHR)
	INIT_DEVICE_FUNCTION(vkGetSwapchainImagesKHR)
	INIT_DEVICE_FUNCTION(vkQueuePresentKHR)

	if ( vk.dedicatedAllocation ) {
		INIT_DEVICE_FUNCTION_EXT(vkGetBufferMemoryRequirements2KHR);
		INIT_DEVICE_FUNCTION_EXT(vkGetImageMemoryRequirements2KHR);
		if ( !qvkGetBufferMemoryRequirements2KHR || !qvkGetImageMemoryRequirements2KHR ) {
			vk.dedicatedAllocation = qfalse;
		}
	}

	if ( vk.debugMarkers ) {
		INIT_DEVICE_FUNCTION_EXT(vkDebugMarkerSetObjectNameEXT)
		INIT_DEVICE_FUNCTION_EXT(vkCmdDebugMarkerBeginEXT)
		INIT_DEVICE_FUNCTION_EXT(vkCmdDebugMarkerEndEXT)
		INIT_DEVICE_FUNCTION_EXT(vkCmdDebugMarkerInsertEXT)
	}

	if ( vk.colorWriteMaskDynamic ) {
		INIT_DEVICE_FUNCTION_EXT(vkCmdSetColorWriteMaskEXT)
		if ( !qvkCmdSetColorWriteMaskEXT )
			vk.colorWriteMaskDynamic = qfalse;
	}

	if ( vk.meshShaderNV ) {
		INIT_DEVICE_FUNCTION_EXT( vkCmdDrawMeshTasksNV );
		if ( !qvkCmdDrawMeshTasksNV ) {
			ri.Printf( PRINT_WARNING, "[VK] VK_NV_mesh_shader enabled but vkCmdDrawMeshTasksNV missing; disabling mesh path\n" );
			vk.meshShaderNV = qfalse;
		}
	}

	INIT_DEVICE_FUNCTION_EXT(vkCmdClearColorImage)

#ifdef USE_VUDA
	if ( vk.vudaInteropCapable ) {
		INIT_DEVICE_FUNCTION_EXT( vkGetMemoryFdKHR );
		INIT_DEVICE_FUNCTION_EXT( vkGetMemoryFdPropertiesKHR );
		INIT_DEVICE_FUNCTION_EXT( vkGetSemaphoreFdKHR );
		INIT_DEVICE_FUNCTION_EXT( vkWaitSemaphoresKHR );
		INIT_DEVICE_FUNCTION_EXT( vkSignalSemaphoreKHR );
	}
#endif
#ifdef USE_MIMIR_CUDA
	if ( vk.mimirInteropCapable && !qvkGetMemoryFdKHR ) {
		INIT_DEVICE_FUNCTION_EXT( vkGetMemoryFdKHR );
		INIT_DEVICE_FUNCTION_EXT( vkGetMemoryFdPropertiesKHR );
	}
#endif
}

#undef INIT_INSTANCE_FUNCTION
#undef INIT_INSTANCE_FUNCTION_EXT
#undef INIT_DEVICE_FUNCTION
#undef INIT_DEVICE_FUNCTION_EXT

void vk_deinit_instance_functions( void )
{
	qvkCreateInstance = NULL;
	qvkEnumerateInstanceExtensionProperties = NULL;

	qvkCreateDevice = NULL;
	qvkDestroyInstance = NULL;
	qvkEnumerateDeviceExtensionProperties = NULL;
	qvkEnumeratePhysicalDevices = NULL;
	qvkGetDeviceProcAddr = NULL;
	qvkGetPhysicalDeviceFeatures = NULL;
	qvkGetPhysicalDeviceFeatures2 = NULL;
	qvkGetPhysicalDeviceFormatProperties = NULL;
	qvkGetPhysicalDeviceMemoryProperties = NULL;
	qvkGetPhysicalDeviceProperties = NULL;
	qvkGetPhysicalDeviceProperties2 = NULL;
	qvkGetPhysicalDeviceQueueFamilyProperties = NULL;
	qvkDestroySurfaceKHR = NULL;
	qvkGetPhysicalDeviceSurfaceCapabilitiesKHR = NULL;
	qvkGetPhysicalDeviceSurfaceFormatsKHR = NULL;
	qvkGetPhysicalDeviceSurfacePresentModesKHR = NULL;
	qvkGetPhysicalDeviceSurfaceSupportKHR = NULL;
#ifdef USE_VK_VALIDATION
	qvkCreateDebugReportCallbackEXT = NULL;
	qvkDestroyDebugReportCallbackEXT = NULL;
#endif
}


void vk_deinit_device_functions( void )
{
	qvkAllocateCommandBuffers = NULL;
	qvkAllocateDescriptorSets = NULL;
	qvkAllocateMemory = NULL;
	qvkBeginCommandBuffer = NULL;
	qvkBindBufferMemory = NULL;
	qvkBindImageMemory = NULL;
	qvkCmdBeginRenderPass = NULL;
	qvkCmdBindDescriptorSets = NULL;
	qvkCmdBindIndexBuffer = NULL;
	qvkCmdBindPipeline = NULL;
	qvkCmdBindVertexBuffers = NULL;
	qvkCmdBlitImage = NULL;
	qvkCmdClearAttachments = NULL;
	qvkCmdFillBuffer = NULL;
	qvkCmdCopyBuffer = NULL;
	qvkCmdCopyBufferToImage = NULL;
	qvkCmdCopyImage = NULL;
	qvkCmdCopyImageToBuffer = NULL;
	qvkCmdDraw = NULL;
	qvkCmdDrawIndexed = NULL;
	qvkCmdDrawMeshTasksNV = NULL;
	qvkCmdDispatch = NULL;
	qvkCmdEndRenderPass = NULL;
	qvkCmdNextSubpass = NULL;
	qvkCmdPipelineBarrier = NULL;
	qvkCmdPushConstants = NULL;
	qvkCmdSetDepthBias = NULL;
	qvkCmdSetScissor = NULL;
	qvkCmdSetViewport = NULL;
	qvkCmdSetColorWriteMaskEXT = NULL;
	qvkCmdWriteTimestamp = NULL;
	qvkCmdBeginQuery = NULL;
	qvkCmdEndQuery = NULL;
	qvkCmdResetQueryPool = NULL;
	qvkCreateBuffer = NULL;
	qvkCreateCommandPool = NULL;
	qvkCreateDescriptorPool = NULL;
	qvkCreateDescriptorSetLayout = NULL;
	qvkCreateFence = NULL;
	qvkCreateFramebuffer = NULL;
	qvkCreateComputePipelines = NULL;
	qvkCreateGraphicsPipelines = NULL;
	qvkCreateImage = NULL;
	qvkCreateImageView = NULL;
	qvkCreatePipelineCache = NULL;
	qvkCreatePipelineLayout = NULL;
	qvkCreateQueryPool = NULL;
	qvkCreateRenderPass = NULL;
	qvkCreateSampler = NULL;
	qvkCreateSemaphore = NULL;
	qvkCreateShaderModule = NULL;
	qvkDestroyBuffer = NULL;
	qvkDestroyCommandPool = NULL;
	qvkDestroyDescriptorPool = NULL;
	qvkDestroyDescriptorSetLayout = NULL;
	qvkDestroyDevice = NULL;
	qvkDestroyFence = NULL;
	qvkDestroyFramebuffer = NULL;
	qvkDestroyImage = NULL;
	qvkDestroyImageView = NULL;
	qvkDestroyPipeline = NULL;
	qvkDestroyPipelineCache = NULL;
	qvkDestroyPipelineLayout = NULL;
	qvkDestroyQueryPool = NULL;
	qvkDestroyRenderPass = NULL;
	qvkDestroySampler = NULL;
	qvkDestroySemaphore = NULL;
	qvkDestroyShaderModule = NULL;
	qvkDeviceWaitIdle = NULL;
	qvkEndCommandBuffer = NULL;
	qvkFlushMappedMemoryRanges = NULL;
	qvkFreeCommandBuffers = NULL;
	qvkFreeDescriptorSets = NULL;
	qvkFreeMemory = NULL;
	qvkGetBufferMemoryRequirements = NULL;
	qvkGetDeviceQueue = NULL;
	qvkGetImageMemoryRequirements = NULL;
	qvkGetImageSparseMemoryRequirements = NULL;
	qvkGetImageSubresourceLayout = NULL;
	qvkGetPipelineCacheData = NULL;
	qvkInvalidateMappedMemoryRanges = NULL;
	qvkMapMemory = NULL;
	qvkQueueSubmit = NULL;
	qvkQueueBindSparse = NULL;
	qvkQueueWaitIdle = NULL;
	qvkResetCommandBuffer = NULL;
	qvkResetDescriptorPool = NULL;
	qvkResetFences = NULL;
	qvkGetQueryPoolResults = NULL;
	qvkResetQueryPoolEXT = NULL;
	qvkUnmapMemory = NULL;
	qvkUpdateDescriptorSets = NULL;
	qvkWaitForFences = NULL;
	qvkAcquireNextImageKHR = NULL;
	qvkCreateSwapchainKHR = NULL;
	qvkDestroySwapchainKHR = NULL;
	qvkGetSwapchainImagesKHR = NULL;
	qvkQueuePresentKHR = NULL;

	qvkGetBufferMemoryRequirements2KHR = NULL;
	qvkGetImageMemoryRequirements2KHR = NULL;

	qvkDebugMarkerSetObjectNameEXT = NULL;
	qvkCmdDebugMarkerBeginEXT = NULL;
	qvkCmdDebugMarkerEndEXT = NULL;
	qvkCmdDebugMarkerInsertEXT = NULL;
	qvkCmdClearColorImage = NULL;

#ifdef USE_VULKAN_RTX
	qvkGetBufferDeviceAddress = NULL;
	qvkCreateAccelerationStructureKHR = NULL;
	qvkDestroyAccelerationStructureKHR = NULL;
	qvkGetAccelerationStructureBuildSizesKHR = NULL;
	qvkGetAccelerationStructureDeviceAddressKHR = NULL;
	qvkCmdBuildAccelerationStructuresKHR = NULL;
	qvkCreateRayTracingPipelinesKHR = NULL;
	qvkGetRayTracingShaderGroupHandlesKHR = NULL;
	qvkCmdTraceRaysKHR = NULL;
#endif
}
