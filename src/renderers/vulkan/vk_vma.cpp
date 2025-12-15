// VMA (Vulkan Memory Allocator) implementation
// This must be compiled as C++ because VMA uses C++ headers internally
#ifdef USE_VMA

// Suppress warnings from third-party VMA library code
#if defined(__GNUC__) || defined(__clang__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wunused-parameter"
	#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

// VMA configuration (must match vk.h)
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#define VMA_VULKAN_VERSION 1000000 // Vulkan 1.0

// Include Vulkan header
#include "../renderercommon/vulkan/vulkan.h"

// Now include VMA implementation (this will include C++ headers)
#define VMA_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#include "vk_mem_alloc.h"
#pragma GCC diagnostic pop

// Restore warnings
#if defined(__GNUC__) || defined(__clang__)
	#pragma GCC diagnostic pop
#endif

#endif // USE_VMA
