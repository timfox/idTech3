/*
=============================================================================
Vulkan Instance and Device Management - C++23 Implementation
=============================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include <vector>
#include <string>
#include <algorithm>
#include <ranges>

#ifdef USE_VULKAN

// External Vulkan function pointers and global variables
extern "C" {
extern PFN_vkCreateInstance qvkCreateInstance;
extern PFN_vkEnumerateInstanceExtensionProperties qvkEnumerateInstanceExtensionProperties;
extern PFN_vkEnumeratePhysicalDevices qvkEnumeratePhysicalDevices;
extern PFN_vkGetPhysicalDeviceProperties qvkGetPhysicalDeviceProperties;
extern PFN_vkGetPhysicalDeviceFeatures qvkGetPhysicalDeviceFeatures;
extern PFN_vkGetPhysicalDeviceQueueFamilyProperties qvkGetPhysicalDeviceQueueFamilyProperties;
extern PFN_vkGetPhysicalDeviceMemoryProperties qvkGetPhysicalDeviceMemoryProperties;
extern PFN_vkCreateDevice qvkCreateDevice;
extern PFN_vkGetDeviceQueue qvkGetDeviceQueue;
extern PFN_vkDestroyInstance qvkDestroyInstance;
extern PFN_vkDestroyDevice qvkDestroyDevice;
extern PFN_vkEnumerateDeviceExtensionProperties qvkEnumerateDeviceExtensionProperties;
extern PFN_vkGetDeviceProcAddr qvkGetDeviceProcAddr;

// Global Vulkan instance and structures
extern VkInstance vk_instance;
extern Vk_Instance vk;
}

namespace instance_mgr {

// Physical device scoring structure
struct DeviceScore {
    int index;
    int score;
    std::string name;
    VkPhysicalDeviceType type;
    bool is_discrete;
    bool has_geometry_shader;
    bool has_tessellation_shader;
};

// Instance extension requirements
constexpr std::array<const char*, 3> required_instance_extensions = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME
};

// Device extension requirements
constexpr std::array<const char*, 2> required_device_extensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_MAINTENANCE1_EXTENSION_NAME
};

// Validation layers (if enabled)
#ifdef USE_VK_VALIDATION
constexpr std::array<const char*, 1> validation_layers = {
    "VK_LAYER_KHRONOS_validation"
};
#endif

// Check if instance extensions are available
static bool check_instance_extensions() {
    uint32_t extension_count = 0;
    VK_CHECK(qvkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr));

    std::vector<VkExtensionProperties> available_extensions(extension_count);
    VK_CHECK(qvkEnumerateInstanceExtensionProperties(nullptr, &extension_count, available_extensions.data()));

    ri.Printf(PRINT_ALL, "Vulkan: Checking %u instance extensions\n", extension_count);

    for (const char* required : required_instance_extensions) {
        bool found = std::ranges::any_of(available_extensions,
            [required](const VkExtensionProperties& ext) {
                return std::string(ext.extensionName) == required;
            });

        if (!found) {
            ri.Printf(PRINT_ERROR, "Vulkan: Required instance extension not found: %s\n", required);
            return false;
        }
        ri.Printf(PRINT_ALL, "Vulkan: ✓ %s\n", required);
    }

    return true;
}

// Create Vulkan instance
static VkResult create_vulkan_instance() {
    if (!check_instance_extensions()) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "idtech3";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 4, 0);
    app_info.pEngineName = "idtech3";
    app_info.engineVersion = VK_MAKE_VERSION(1, 4, 0);
    app_info.apiVersion = VK_MAKE_API_VERSION(0, 1, 4, 0);

    VkInstanceCreateInfo create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = required_instance_extensions.size();
    create_info.ppEnabledExtensionNames = required_instance_extensions.data();

#ifdef USE_VK_VALIDATION
    create_info.enabledLayerCount = validation_layers.size();
    create_info.ppEnabledLayerNames = validation_layers.data();
    ri.Printf(PRINT_ALL, "Vulkan: Validation layers enabled\n");
#else
    create_info.enabledLayerCount = 0;
#endif

    VkResult result = qvkCreateInstance(&create_info, nullptr, &vk_instance);
    if (result == VK_SUCCESS) {
        ri.Printf(PRINT_ALL, "Vulkan: Instance created successfully\n");
    }

    return result;
}

// Score a physical device based on its capabilities
static DeviceScore score_physical_device(VkPhysicalDevice device, int index) {
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures features;

    qvkGetPhysicalDeviceProperties(device, &properties);
    qvkGetPhysicalDeviceFeatures(device, &features);

    DeviceScore score = {};
    score.index = index;
    score.name = properties.deviceName;
    score.type = properties.deviceType;
    score.is_discrete = (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);
    score.has_geometry_shader = features.geometryShader;
    score.has_tessellation_shader = features.tessellationShader;

    // Base scoring
    if (score.is_discrete) {
        score.score += 1000; // Prefer discrete GPUs
    } else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        score.score += 500; // Integrated GPUs are still better than CPU
    }

    // Feature scoring
    if (score.has_geometry_shader) score.score += 100;
    if (score.has_tessellation_shader) score.score += 100;
    if (features.multiDrawIndirect) score.score += 50;
    if (features.drawIndirectFirstInstance) score.score += 25;

    // Memory scoring (rough estimate)
    VkPhysicalDeviceMemoryProperties memory_props;
    qvkGetPhysicalDeviceMemoryProperties(device, &memory_props);

    for (uint32_t i = 0; i < memory_props.memoryHeapCount; i++) {
        if (memory_props.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            // Score based on VRAM amount (MB)
            VkDeviceSize vram_mb = memory_props.memoryHeaps[i].size / (1024 * 1024);
            score.score += static_cast<int>(vram_mb / 100); // 1 point per 100MB
        }
    }

    return score;
}

// Enumerate and select physical device
static bool enumerate_physical_devices() {
    uint32_t device_count = 0;
    VkResult result = qvkEnumeratePhysicalDevices(vk_instance, &device_count, nullptr);

    if (result != VK_SUCCESS || device_count == 0) {
        ri.Printf(PRINT_ERROR, "Vulkan: Failed to enumerate physical devices\n");
        return false;
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    VK_CHECK(qvkEnumeratePhysicalDevices(vk_instance, &device_count, devices.data()));

    ri.Printf(PRINT_ALL, "Vulkan: Found %u physical device(s)\n", device_count);

    // Score all devices
    std::vector<DeviceScore> device_scores;
    for (uint32_t i = 0; i < device_count; i++) {
        device_scores.push_back(score_physical_device(devices[i], i));
    }

    // Sort by score (highest first)
    std::ranges::sort(device_scores, std::greater<>{},
        [](const DeviceScore& ds) { return ds.score; });

    // Print device information
    ri.Printf(PRINT_ALL, "Available physical devices:\n");
    for (const auto& score : device_scores) {
        const char* type_str = (score.type == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? "Discrete" :
                              (score.type == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) ? "Integrated" :
                              (score.type == VK_PHYSICAL_DEVICE_TYPE_CPU) ? "CPU" : "Other";

        ri.Printf(PRINT_ALL, " %d: %s (%s GPU) - Score: %d\n",
                 score.index, score.name.c_str(), type_str, score.score);
    }

    // Select the best device
    const DeviceScore& best_device = device_scores[0];
    vk.physical_device = devices[best_device.index];

    ri.Printf(PRINT_ALL, "Vulkan: Selected device %d (%s)\n",
             best_device.index, best_device.name.c_str());

    return true;
}

// Check device extension support
static bool check_device_extensions(VkPhysicalDevice device) {
    uint32_t extension_count;
    VK_CHECK(qvkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr));

    std::vector<VkExtensionProperties> available_extensions(extension_count);
    VK_CHECK(qvkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data()));

    for (const char* required : required_device_extensions) {
        bool found = std::ranges::any_of(available_extensions,
            [required](const VkExtensionProperties& ext) {
                return std::string(ext.extensionName) == required;
            });

        if (!found) {
            ri.Printf(PRINT_WARNING, "Vulkan: Device extension not available: %s\n", required);
            return false;
        }
    }

    return true;
}

// Find suitable queue family
static bool find_queue_family(VkPhysicalDevice device, uint32_t* queue_family) {
    uint32_t queue_family_count = 0;
    qvkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    qvkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    // Look for a queue family that supports graphics and presentation
    for (uint32_t i = 0; i < queue_family_count; i++) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            *queue_family = i;
            return true;
        }
    }

    return false;
}

// Create logical device
static VkResult create_logical_device() {
    if (!check_device_extensions(vk.physical_device)) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    uint32_t queue_family;
    if (!find_queue_family(vk.physical_device, &queue_family)) {
        ri.Printf(PRINT_ERROR, "Vulkan: No suitable queue family found\n");
        return VK_ERROR_FEATURE_NOT_PRESENT;
    }

    vk.queue_family_index = queue_family;

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {};
    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = queue_family;
    queue_create_info.queueCount = 1;
    queue_create_info.pQueuePriorities = &queue_priority;

    VkPhysicalDeviceFeatures device_features = {};
    device_features.geometryShader = VK_TRUE;
    device_features.tessellationShader = VK_TRUE;
    device_features.multiDrawIndirect = VK_TRUE;
    device_features.drawIndirectFirstInstance = VK_TRUE;
    device_features.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo device_create_info = {};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = 1;
    device_create_info.pQueueCreateInfos = &queue_create_info;
    device_create_info.enabledExtensionCount = required_device_extensions.size();
    device_create_info.ppEnabledExtensionNames = required_device_extensions.data();
    device_create_info.pEnabledFeatures = &device_features;

    VkResult result = qvkCreateDevice(vk.physical_device, &device_create_info, nullptr, &vk.device);
    if (result == VK_SUCCESS) {
        // Get the queue
        qvkGetDeviceQueue(vk.device, queue_family, 0, &vk.queue);
        ri.Printf(PRINT_ALL, "Vulkan: Logical device created successfully\n");
    }

    return result;
}

} // namespace instance_mgr

// Public interface functions
VkResult vk_create_instance(void) {
    return instance_mgr::create_vulkan_instance();
}

bool vk_enumerate_devices(void) {
    return instance_mgr::enumerate_physical_devices();
}

VkResult vk_create_device(void) {
    return instance_mgr::create_logical_device();
}

#endif // USE_VULKAN