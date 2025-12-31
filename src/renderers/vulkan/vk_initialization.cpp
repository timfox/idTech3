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
#include <vector>
#include <string>

// Vulkan Initialization Module
// Handles Vulkan instance, device, and queue setup

// Global Vulkan objects
VkInstance vk_instance = VK_NULL_HANDLE;
VkPhysicalDevice vk_physical_device = VK_NULL_HANDLE;
VkDevice vk_device = VK_NULL_HANDLE;
VkQueue vk_queue = VK_NULL_HANDLE;
uint32_t vk_queue_family_index = ~0U;

// Instance and device function pointers
PFN_vkCreateInstance qvkCreateInstance = nullptr;
PFN_vkEnumeratePhysicalDevices qvkEnumeratePhysicalDevices = nullptr;
PFN_vkGetPhysicalDeviceProperties qvkGetPhysicalDeviceProperties = nullptr;
PFN_vkGetPhysicalDeviceFeatures qvkGetPhysicalDeviceFeatures = nullptr;
PFN_vkGetPhysicalDeviceQueueFamilyProperties qvkGetPhysicalDeviceQueueFamilyProperties = nullptr;
PFN_vkCreateDevice qvkCreateDevice = nullptr;
PFN_vkGetDeviceQueue qvkGetDeviceQueue = nullptr;
PFN_vkDestroyInstance qvkDestroyInstance = nullptr;
PFN_vkDestroyDevice qvkDestroyDevice = nullptr;

// Validation layer support
static const char* validation_layer_name = "VK_LAYER_KHRONOS_validation";
static const char* validation_layer_name2 = "VK_LAYER_LUNARG_standard_validation";

// Instance extensions
static const char* instance_extensions[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(VK_USE_PLATFORM_XLIB_KHR)
    VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#endif
#ifdef USE_VK_VALIDATION
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
    VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
};

// Device extensions
static const char* device_extensions[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_MAINTENANCE1_EXTENSION_NAME
#ifdef __APPLE__
    ,VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#endif
};

// Initialize Vulkan library and load function pointers
qboolean vk_init_library(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Initializing Vulkan library\n");

    // Load Vulkan library
    void* vulkan_lib = Sys_LoadLibrary("libvulkan.so.1");
    if (!vulkan_lib) {
        vulkan_lib = Sys_LoadLibrary("vulkan-1.dll");
    }
    if (!vulkan_lib) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to load Vulkan library\n");
        return qfalse;
    }

    // Load core functions
    qvkCreateInstance = (PFN_vkCreateInstance)Sys_LoadFunction(vulkan_lib, "vkCreateInstance");
    qvkEnumeratePhysicalDevices = (PFN_vkEnumeratePhysicalDevices)Sys_LoadFunction(vulkan_lib, "vkEnumeratePhysicalDevices");
    qvkGetPhysicalDeviceProperties = (PFN_vkGetPhysicalDeviceProperties)Sys_LoadFunction(vulkan_lib, "vkGetPhysicalDeviceProperties");
    qvkGetPhysicalDeviceFeatures = (PFN_vkGetPhysicalDeviceFeatures)Sys_LoadFunction(vulkan_lib, "vkGetPhysicalDeviceFeatures");
    qvkGetPhysicalDeviceQueueFamilyProperties = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)Sys_LoadFunction(vulkan_lib, "vkGetPhysicalDeviceQueueFamilyProperties");
    qvkCreateDevice = (PFN_vkCreateDevice)Sys_LoadFunction(vulkan_lib, "vkCreateDevice");
    qvkGetDeviceQueue = (PFN_vkGetDeviceQueue)Sys_LoadFunction(vulkan_lib, "vkGetDeviceQueue");
    qvkDestroyInstance = (PFN_vkDestroyInstance)Sys_LoadFunction(vulkan_lib, "vkDestroyInstance");
    qvkDestroyDevice = (PFN_vkDestroyDevice)Sys_LoadFunction(vulkan_lib, "vkDestroyDevice");

    if (!qvkCreateInstance || !qvkEnumeratePhysicalDevices) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to load required Vulkan functions\n");
        return qfalse;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Library initialized successfully\n");
    return qtrue;
}

// Create Vulkan instance
qboolean vk_create_instance(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Creating Vulkan instance\n");

    // Application info
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "idTech3",
        .applicationVersion = VK_MAKE_VERSION(1, 4, 0),
        .pEngineName = "idTech3 Vulkan Renderer",
        .engineVersion = VK_MAKE_VERSION(1, 4, 0),
        .apiVersion = VK_MAKE_API_VERSION(0, 1, 4, 0)
    };

    // Instance create info
    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = sizeof(instance_extensions) / sizeof(instance_extensions[0]),
        .ppEnabledExtensionNames = instance_extensions
    };

#ifdef USE_VK_VALIDATION
    createInfo.enabledLayerCount = 1;
    createInfo.ppEnabledLayerNames = &validation_layer_name;
#endif

    // Create instance
    VkResult result = qvkCreateInstance(&createInfo, nullptr, &vk_instance);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to create instance: %d\n", result);
        return qfalse;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Instance created successfully\n");
    return qtrue;
}

// Select physical device
qboolean vk_select_physical_device(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Selecting physical device\n");

    uint32_t deviceCount = 0;
    VkResult result = qvkEnumeratePhysicalDevices(vk_instance, &deviceCount, nullptr);

    if (result != VK_SUCCESS || deviceCount == 0) {
        ri.Printf(PRINT_WARNING, "Vulkan: No physical devices found, using stub mode\n");
        return qfalse; // Will use stub mode
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    result = qvkEnumeratePhysicalDevices(vk_instance, &deviceCount, devices.data());

    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_WARNING, "Vulkan: Failed to enumerate devices, using stub mode\n");
        return qfalse;
    }

    // Select first available device (could be improved with scoring)
    vk_physical_device = devices[0];

    // Get device properties
    VkPhysicalDeviceProperties deviceProperties;
    qvkGetPhysicalDeviceProperties(vk_physical_device, &deviceProperties);

    ri.Printf(PRINT_ALL, "Vulkan: Selected device: %s\n", deviceProperties.deviceName);
    return qtrue;
}

// Find graphics queue family
uint32_t vk_find_graphics_queue_family(VkPhysicalDevice device) {
    uint32_t queueFamilyCount = 0;
    qvkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    qvkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            return i;
        }
    }

    return ~0U; // Not found
}

// Create logical device
qboolean vk_create_device(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Creating logical device\n");

    vk_queue_family_index = vk_find_graphics_queue_family(vk_physical_device);
    if (vk_queue_family_index == ~0U) {
        ri.Printf(PRINT_ERROR, "Vulkan: No graphics queue family found\n");
        return qfalse;
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = vk_queue_family_index,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };

    VkPhysicalDeviceFeatures deviceFeatures = {};
    qvkGetPhysicalDeviceFeatures(vk_physical_device, &deviceFeatures);

    VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = sizeof(device_extensions) / sizeof(device_extensions[0]),
        .ppEnabledExtensionNames = device_extensions,
        .pEnabledFeatures = &deviceFeatures
    };

    VkResult result = qvkCreateDevice(vk_physical_device, &createInfo, nullptr, &vk_device);
    if (result != VK_SUCCESS) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to create device: %d\n", result);
        return qfalse;
    }

    // Get graphics queue
    qvkGetDeviceQueue(vk_device, vk_queue_family_index, 0, &vk_queue);

    ri.Printf(PRINT_ALL, "Vulkan: Device created successfully\n");
    return qtrue;
}

// Initialize Vulkan
qboolean vk_initialize_core(void) {
    ri.Printf(PRINT_ALL, "Vulkan: Starting initialization\n");

    // Initialize library
    if (!vk_init_library()) {
        goto fallback;
    }

    // Create instance
    if (!vk_create_instance()) {
        goto fallback;
    }

    // Select physical device
    if (!vk_select_physical_device()) {
        goto fallback;
    }

    // Create logical device
    if (!vk_create_device()) {
        goto fallback;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Initialization completed successfully\n");
    return qtrue;

fallback:
    ri.Printf(PRINT_ALL, "Vulkan: Using stub device mode\n");
    vk_device = (VkDevice)0x20000000; // Fake device handle
    return qtrue;
}

// Shutdown Vulkan
void vk_shutdown(void) {
    if (vk_device && vk_device != (VkDevice)0x20000000) {
        qvkDestroyDevice(vk_device, nullptr);
        vk_device = VK_NULL_HANDLE;
    }

    if (vk_instance) {
        qvkDestroyInstance(vk_instance, nullptr);
        vk_instance = VK_NULL_HANDLE;
    }

    ri.Printf(PRINT_ALL, "Vulkan: Shutdown completed\n");
}