<?php
/**
 * Vulkan Renderer Implementation - JKSunny's PBR Port
 */
$title = 'Vulkan Renderer Implementation - id Tech 3 Documentation';
$breadcrumbs = [
    '/renderer' => 'Renderer Deep Dive',
    '/renderer/vulkan-implementation' => 'Vulkan Renderer'
];
?>

<h1>Vulkan Renderer Implementation</h1>

<div class="section">
    <h2>Overview</h2>
    <p>The Vulkan renderer in JKSunny's PBR port replaces the original OpenGL renderer with a modern, explicit graphics API implementation. This documentation covers the actual implementation as it exists, focusing on the core systems that enable PBR rendering in id Tech 3.</p>
    
    <div class="feature-list">
        <h3>Implementation Features</h3>
        <ul>
            <li><strong>Command Buffer Management:</strong> Multi-threaded command recording</li>
            <li><strong>Descriptor Set Layout:</strong> Efficient resource binding for PBR materials</li>
            <li><strong>Pipeline State Objects:</strong> Pre-compiled render states</li>
            <li><strong>Memory Management:</strong> Vulkan memory allocator integration</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Core Vulkan Context</h2>
    
    <h3>Vulkan Instance and Device Setup</h3>
    <div class="code-block">
        <pre><code>// tr_vulkan.c - Core Vulkan context implementation
typedef struct {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkSurfaceKHR surface;
    
    // Queue families and queues
    uint32_t graphicsQueueFamily;
    uint32_t presentQueueFamily;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    
    // Swapchain
    VkSwapchainKHR swapchain;
    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;
    uint32_t swapchainImageCount;
    VkImage* swapchainImages;
    VkImageView* swapchainImageViews;
    
    // Synchronization objects
    VkSemaphore imageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore renderFinishedSemaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence inFlightFences[MAX_FRAMES_IN_FLIGHT];
    
    // Command pools and buffers
    VkCommandPool commandPool;
    VkCommandBuffer commandBuffers[MAX_FRAMES_IN_FLIGHT];
    
    // Current frame tracking
    uint32_t currentFrame;
    uint32_t currentImageIndex;
    
} vulkanContext_t;

static vulkanContext_t vk;

qboolean VK_Init(void) {
    Com_Printf("Initializing Vulkan renderer\n");
    
    // Create Vulkan instance
    if (!VK_CreateInstance()) {
        Com_Printf("^1Failed to create Vulkan instance\n");
        return qfalse;
    }
    
    // Create surface (platform-specific)
    if (!VK_CreateSurface()) {
        Com_Printf("^1Failed to create Vulkan surface\n");
        return qfalse;
    }
    
    // Select physical device
    if (!VK_SelectPhysicalDevice()) {
        Com_Printf("^1Failed to select suitable physical device\n");
        return qfalse;
    }
    
    // Create logical device
    if (!VK_CreateLogicalDevice()) {
        Com_Printf("^1Failed to create Vulkan logical device\n");
        return qfalse;
    }
    
    // Create swapchain
    if (!VK_CreateSwapchain()) {
        Com_Printf("^1Failed to create Vulkan swapchain\n");
        return qfalse;
    }
    
    // Create command pool and buffers
    if (!VK_CreateCommandPool()) {
        Com_Printf("^1Failed to create command pool\n");
        return qfalse;
    }
    
    // Create synchronization objects
    if (!VK_CreateSyncObjects()) {
        Com_Printf("^1Failed to create synchronization objects\n");
        return qfalse;
    }
    
    Com_Printf("Vulkan renderer initialized successfully\n");
    return qtrue;
}

qboolean VK_CreateInstance(void) {
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Quake3e PBR",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "id Tech 3 PBR",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_2
    };
    
    // Required extensions
    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(__linux__)
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#elif defined(__APPLE__)
        VK_MVK_MACOS_SURFACE_EXTENSION_NAME,
#endif
#ifdef _DEBUG
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#endif
    };
    
    // Validation layers (debug builds only)
    const char* validationLayers[] = {
        "VK_LAYER_KHRONOS_validation"
    };
    
    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = ARRAY_LEN(extensions),
        .ppEnabledExtensionNames = extensions,
#ifdef _DEBUG
        .enabledLayerCount = ARRAY_LEN(validationLayers),
        .ppEnabledLayerNames = validationLayers,
#else
        .enabledLayerCount = 0,
#endif
    };
    
    VkResult result = vkCreateInstance(&createInfo, NULL, &vk.instance);
    if (result != VK_SUCCESS) {
        Com_Printf("^1vkCreateInstance failed: %s\n", VK_ResultToString(result));
        return qfalse;
    }
    
    return qtrue;
}

qboolean VK_SelectPhysicalDevice(void) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(vk.instance, &deviceCount, NULL);
    
    if (deviceCount == 0) {
        Com_Printf("^1No Vulkan-capable devices found\n");
        return qfalse;
    }
    
    VkPhysicalDevice* devices = malloc(sizeof(VkPhysicalDevice) * deviceCount);
    vkEnumeratePhysicalDevices(vk.instance, &deviceCount, devices);
    
    // Score devices and pick the best one
    int bestScore = -1;
    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    
    for (uint32_t i = 0; i < deviceCount; i++) {
        int score = VK_ScorePhysicalDevice(devices[i]);
        if (score > bestScore) {
            bestScore = score;
            bestDevice = devices[i];
        }
    }
    
    free(devices);
    
    if (bestDevice == VK_NULL_HANDLE) {
        Com_Printf("^1No suitable Vulkan device found\n");
        return qfalse;
    }
    
    vk.physicalDevice = bestDevice;
    
    // Log selected device
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(vk.physicalDevice, &deviceProperties);
    Com_Printf("Selected Vulkan device: %s\n", deviceProperties.deviceName);
    
    return qtrue;
}

int VK_ScorePhysicalDevice(VkPhysicalDevice device) {
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
    
    vkGetPhysicalDeviceProperties(device, &deviceProperties);
    vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
    
    int score = 0;
    
    // Discrete GPUs have better performance
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 1000;
    }
    
    // Maximum possible size of textures affects graphics quality
    score += deviceProperties.limits.maxImageDimension2D;
    
    // Required features for PBR rendering
    if (!deviceFeatures.samplerAnisotropy) {
        return 0;  // Not suitable
    }
    
    if (!deviceFeatures.textureCompressionBC) {
        score -= 100;  // Prefer BC compression support
    }
    
    // Check queue family support
    if (!VK_FindQueueFamilies(device)) {
        return 0;  // Not suitable
    }
    
    // Check swapchain support
    if (!VK_CheckSwapchainSupport(device)) {
        return 0;  // Not suitable
    }
    
    return score;
}</code></pre>
    </div>
    
    <h3>Queue Family Management</h3>
    <div class="code-block">
        <pre><code>// Queue family detection and setup
typedef struct {
    uint32_t graphicsFamily;
    uint32_t presentFamily;
    qboolean graphicsFamilyFound;
    qboolean presentFamilyFound;
} queueFamilyIndices_t;

qboolean VK_FindQueueFamilies(VkPhysicalDevice device) {
    queueFamilyIndices_t indices = {0};
    
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);
    
    VkQueueFamilyProperties* queueFamilies = 
        malloc(sizeof(VkQueueFamilyProperties) * queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies);
    
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        // Check for graphics support
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
            indices.graphicsFamilyFound = qtrue;
        }
        
        // Check for present support
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, vk.surface, &presentSupport);
        
        if (presentSupport) {
            indices.presentFamily = i;
            indices.presentFamilyFound = qtrue;
        }
        
        // Early exit if we found everything
        if (indices.graphicsFamilyFound && indices.presentFamilyFound) {
            break;
        }
    }
    
    free(queueFamilies);
    
    if (indices.graphicsFamilyFound && indices.presentFamilyFound) {
        vk.graphicsQueueFamily = indices.graphicsFamily;
        vk.presentQueueFamily = indices.presentFamily;
        return qtrue;
    }
    
    return qfalse;
}

qboolean VK_CreateLogicalDevice(void) {
    // Queue create infos
    float queuePriority = 1.0f;
    
    VkDeviceQueueCreateInfo queueCreateInfos[2];
    uint32_t queueCreateInfoCount = 0;
    
    // Graphics queue
    queueCreateInfos[queueCreateInfoCount] = (VkDeviceQueueCreateInfo){
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = vk.graphicsQueueFamily,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };
    queueCreateInfoCount++;
    
    // Present queue (if different from graphics)
    if (vk.presentQueueFamily != vk.graphicsQueueFamily) {
        queueCreateInfos[queueCreateInfoCount] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = vk.presentQueueFamily,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        };
        queueCreateInfoCount++;
    }
    
    // Required device features
    VkPhysicalDeviceFeatures deviceFeatures = {
        .samplerAnisotropy = VK_TRUE,
        .textureCompressionBC = VK_TRUE,
        .fillModeNonSolid = VK_TRUE,  // For wireframe rendering
        .wideLines = VK_TRUE,         // For line width > 1.0
    };
    
    // Required device extensions
    const char* deviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    
    VkDeviceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = queueCreateInfoCount,
        .pQueueCreateInfos = queueCreateInfos,
        .pEnabledFeatures = &deviceFeatures,
        .enabledExtensionCount = ARRAY_LEN(deviceExtensions),
        .ppEnabledExtensionNames = deviceExtensions,
        .enabledLayerCount = 0,  // Device layers are deprecated
    };
    
    VkResult result = vkCreateDevice(vk.physicalDevice, &createInfo, NULL, &vk.device);
    if (result != VK_SUCCESS) {
        Com_Printf("^1vkCreateDevice failed: %s\n", VK_ResultToString(result));
        return qfalse;
    }
    
    // Get queue handles
    vkGetDeviceQueue(vk.device, vk.graphicsQueueFamily, 0, &vk.graphicsQueue);
    vkGetDeviceQueue(vk.device, vk.presentQueueFamily, 0, &vk.presentQueue);
    
    return qtrue;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Swapchain Management</h2>
    
    <h3>Swapchain Creation and Recreation</h3>
    <div class="code-block">
        <pre><code>// Swapchain management for double/triple buffering
typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR* formats;
    uint32_t formatCount;
    VkPresentModeKHR* presentModes;
    uint32_t presentModeCount;
} swapchainSupportDetails_t;

qboolean VK_CreateSwapchain(void) {
    swapchainSupportDetails_t swapchainSupport = VK_QuerySwapchainSupport(vk.physicalDevice);
    
    VkSurfaceFormatKHR surfaceFormat = VK_ChooseSwapSurfaceFormat(
        swapchainSupport.formats, swapchainSupport.formatCount);
    VkPresentModeKHR presentMode = VK_ChooseSwapPresentMode(
        swapchainSupport.presentModes, swapchainSupport.presentModeCount);
    VkExtent2D extent = VK_ChooseSwapExtent(&swapchainSupport.capabilities);
    
    // Request one more image than minimum for triple buffering
    uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
    if (swapchainSupport.capabilities.maxImageCount > 0 && 
        imageCount > swapchainSupport.capabilities.maxImageCount) {
        imageCount = swapchainSupport.capabilities.maxImageCount;
    }
    
    VkSwapchainCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = vk.surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = swapchainSupport.capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };
    
    // Handle queue family sharing
    uint32_t queueFamilyIndices[] = {vk.graphicsQueueFamily, vk.presentQueueFamily};
    
    if (vk.graphicsQueueFamily != vk.presentQueueFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    
    VkResult result = vkCreateSwapchainKHR(vk.device, &createInfo, NULL, &vk.swapchain);
    if (result != VK_SUCCESS) {
        Com_Printf("^1vkCreateSwapchainKHR failed: %s\n", VK_ResultToString(result));
        VK_CleanupSwapchainSupport(&swapchainSupport);
        return qfalse;
    }
    
    // Store swapchain details
    vk.swapchainImageFormat = surfaceFormat.format;
    vk.swapchainExtent = extent;
    
    // Get swapchain images
    vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.swapchainImageCount, NULL);
    vk.swapchainImages = malloc(sizeof(VkImage) * vk.swapchainImageCount);
    vkGetSwapchainImagesKHR(vk.device, vk.swapchain, &vk.swapchainImageCount, 
                           vk.swapchainImages);
    
    // Create image views
    if (!VK_CreateSwapchainImageViews()) {
        VK_CleanupSwapchainSupport(&swapchainSupport);
        return qfalse;
    }
    
    VK_CleanupSwapchainSupport(&swapchainSupport);
    
    Com_Printf("Created swapchain: %dx%d, %d images, format %d\n",
              extent.width, extent.height, vk.swapchainImageCount, 
              surfaceFormat.format);
    
    return qtrue;
}

VkSurfaceFormatKHR VK_ChooseSwapSurfaceFormat(VkSurfaceFormatKHR* availableFormats, 
                                             uint32_t formatCount) {
    // Prefer B8G8R8A8_UNORM with sRGB color space for PBR
    for (uint32_t i = 0; i < formatCount; i++) {
        if (availableFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM && 
            availableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormats[i];
        }
    }
    
    // Fallback to first available format
    return availableFormats[0];
}

VkPresentModeKHR VK_ChooseSwapPresentMode(VkPresentModeKHR* availablePresentModes, 
                                         uint32_t presentModeCount) {
    // Check for mailbox mode (triple buffering)
    for (uint32_t i = 0; i < presentModeCount; i++) {
        if (availablePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
            return VK_PRESENT_MODE_MAILBOX_KHR;
        }
    }
    
    // Check for immediate mode (no vsync)
    if (r_swapInterval->integer == 0) {
        for (uint32_t i = 0; i < presentModeCount; i++) {
            if (availablePresentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                return VK_PRESENT_MODE_IMMEDIATE_KHR;
            }
        }
    }
    
    // FIFO mode is guaranteed to be available (vsync)
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VK_ChooseSwapExtent(const VkSurfaceCapabilitiesKHR* capabilities) {
    if (capabilities->currentExtent.width != UINT32_MAX) {
        return capabilities->currentExtent;
    } else {
        // Get window size from platform layer
        int width, height;
        GLimp_GetWindowSize(&width, &height);
        
        VkExtent2D actualExtent = {
            .width = (uint32_t)width,
            .height = (uint32_t)height
        };
        
        // Clamp to supported range
        actualExtent.width = CLAMP(actualExtent.width, 
                                  capabilities->minImageExtent.width,
                                  capabilities->maxImageExtent.width);
        actualExtent.height = CLAMP(actualExtent.height,
                                   capabilities->minImageExtent.height,
                                   capabilities->maxImageExtent.height);
        
        return actualExtent;
    }
}

qboolean VK_CreateSwapchainImageViews(void) {
    vk.swapchainImageViews = malloc(sizeof(VkImageView) * vk.swapchainImageCount);
    
    for (uint32_t i = 0; i < vk.swapchainImageCount; i++) {
        VkImageViewCreateInfo createInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = vk.swapchainImages[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = vk.swapchainImageFormat,
            .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
            .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .subresourceRange.baseMipLevel = 0,
            .subresourceRange.levelCount = 1,
            .subresourceRange.baseArrayLayer = 0,
            .subresourceRange.layerCount = 1,
        };
        
        VkResult result = vkCreateImageView(vk.device, &createInfo, NULL, 
                                          &vk.swapchainImageViews[i]);
        if (result != VK_SUCCESS) {
            Com_Printf("^1Failed to create image view %d: %s\n", i, 
                      VK_ResultToString(result));
            return qfalse;
        }
    }
    
    return qtrue;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Command Buffer Management</h2>
    
    <h3>Command Pool and Buffer Allocation</h3>
    <div class="code-block">
        <pre><code>// Command buffer recording and submission
qboolean VK_CreateCommandPool(void) {
    VkCommandPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = vk.graphicsQueueFamily,
    };
    
    VkResult result = vkCreateCommandPool(vk.device, &poolInfo, NULL, &vk.commandPool);
    if (result != VK_SUCCESS) {
        Com_Printf("^1vkCreateCommandPool failed: %s\n", VK_ResultToString(result));
        return qfalse;
    }
    
    // Allocate command buffers
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = vk.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = MAX_FRAMES_IN_FLIGHT,
    };
    
    result = vkAllocateCommandBuffers(vk.device, &allocInfo, vk.commandBuffers);
    if (result != VK_SUCCESS) {
        Com_Printf("^1vkAllocateCommandBuffers failed: %s\n", VK_ResultToString(result));
        return qfalse;
    }
    
    return qtrue;
}

void VK_BeginCommandBuffer(VkCommandBuffer commandBuffer) {
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0,
        .pInheritanceInfo = NULL,
    };
    
    VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        Com_Error(ERR_FATAL, "vkBeginCommandBuffer failed: %s", VK_ResultToString(result));
    }
}

void VK_EndCommandBuffer(VkCommandBuffer commandBuffer) {
    VkResult result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) {
        Com_Error(ERR_FATAL, "vkEndCommandBuffer failed: %s", VK_ResultToString(result));
    }
}

void VK_SubmitCommandBuffer(VkCommandBuffer commandBuffer) {
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vk.imageAvailableSemaphores[vk.currentFrame],
        .pWaitDstStageMask = (VkPipelineStageFlags[]){VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT},
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &vk.renderFinishedSemaphores[vk.currentFrame],
    };
    
    VkResult result = vkQueueSubmit(vk.graphicsQueue, 1, &submitInfo, 
                                   vk.inFlightFences[vk.currentFrame]);
    if (result != VK_SUCCESS) {
        Com_Error(ERR_FATAL, "vkQueueSubmit failed: %s", VK_ResultToString(result));
    }
}</code></pre>
    </div>
    
    <h3>Frame Synchronization</h3>
    <div class="code-block">
        <pre><code>// Synchronization objects for frame pacing
qboolean VK_CreateSyncObjects(void) {
    VkSemaphoreCreateInfo semaphoreInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    
    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };
    
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkResult result;
        
        result = vkCreateSemaphore(vk.device, &semaphoreInfo, NULL, 
                                  &vk.imageAvailableSemaphores[i]);
        if (result != VK_SUCCESS) {
            Com_Printf("^1Failed to create imageAvailable semaphore: %s\n", 
                      VK_ResultToString(result));
            return qfalse;
        }
        
        result = vkCreateSemaphore(vk.device, &semaphoreInfo, NULL, 
                                  &vk.renderFinishedSemaphores[i]);
        if (result != VK_SUCCESS) {
            Com_Printf("^1Failed to create renderFinished semaphore: %s\n", 
                      VK_ResultToString(result));
            return qfalse;
        }
        
        result = vkCreateFence(vk.device, &fenceInfo, NULL, &vk.inFlightFences[i]);
        if (result != VK_SUCCESS) {
            Com_Printf("^1Failed to create in-flight fence: %s\n", 
                      VK_ResultToString(result));
            return qfalse;
        }
    }
    
    return qtrue;
}

// Main frame rendering loop
void VK_BeginFrame(void) {
    // Wait for previous frame to finish
    vkWaitForFences(vk.device, 1, &vk.inFlightFences[vk.currentFrame], 
                   VK_TRUE, UINT64_MAX);
    
    // Acquire next swapchain image
    VkResult result = vkAcquireNextImageKHR(vk.device, vk.swapchain, UINT64_MAX,
                                           vk.imageAvailableSemaphores[vk.currentFrame],
                                           VK_NULL_HANDLE, &vk.currentImageIndex);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Swapchain is out of date, recreate it
        VK_RecreateSwapchain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        Com_Error(ERR_FATAL, "vkAcquireNextImageKHR failed: %s", VK_ResultToString(result));
    }
    
    // Reset fence for this frame
    vkResetFences(vk.device, 1, &vk.inFlightFences[vk.currentFrame]);
    
    // Reset and begin command buffer
    vkResetCommandBuffer(vk.commandBuffers[vk.currentFrame], 0);
    VK_BeginCommandBuffer(vk.commandBuffers[vk.currentFrame]);
}

void VK_EndFrame(void) {
    // End command buffer recording
    VK_EndCommandBuffer(vk.commandBuffers[vk.currentFrame]);
    
    // Submit command buffer
    VK_SubmitCommandBuffer(vk.commandBuffers[vk.currentFrame]);
    
    // Present the image
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &vk.renderFinishedSemaphores[vk.currentFrame],
        .swapchainCount = 1,
        .pSwapchains = &vk.swapchain,
        .pImageIndices = &vk.currentImageIndex,
        .pResults = NULL,
    };
    
    VkResult result = vkQueuePresentKHR(vk.presentQueue, &presentInfo);
    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        VK_RecreateSwapchain();
    } else if (result != VK_SUCCESS) {
        Com_Error(ERR_FATAL, "vkQueuePresentKHR failed: %s", VK_ResultToString(result));
    }
    
    // Advance to next frame
    vk.currentFrame = (vk.currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VK_WaitIdle(void) {
    vkDeviceWaitIdle(vk.device);
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Error Handling and Debugging</h2>
    
    <h3>Vulkan Error Translation</h3>
    <div class="code-block">
        <pre><code>// Vulkan error handling and debugging support
const char* VK_ResultToString(VkResult result) {
    switch (result) {
    case VK_SUCCESS: return "VK_SUCCESS";
    case VK_NOT_READY: return "VK_NOT_READY";
    case VK_TIMEOUT: return "VK_TIMEOUT";
    case VK_EVENT_SET: return "VK_EVENT_SET";
    case VK_EVENT_RESET: return "VK_EVENT_RESET";
    case VK_INCOMPLETE: return "VK_INCOMPLETE";
    case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTED_POOL: return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
    case VK_ERROR_INVALID_SHADER_NV: return "VK_ERROR_INVALID_SHADER_NV";
    default: return "UNKNOWN_VK_RESULT";
    }
}

#ifdef _DEBUG
// Debug callback for validation layers
VKAPI_ATTR VkBool32 VKAPI_CALL VK_DebugCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT objectType,
    uint64_t object,
    size_t location,
    int32_t messageCode,
    const char* pLayerPrefix,
    const char* pMessage,
    void* pUserData) {
    
    const char* type = "UNKNOWN";
    
    if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
        type = "ERROR";
    } else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
        type = "WARNING";
    } else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
        type = "PERFORMANCE";
    } else if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
        type = "INFO";
    } else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
        type = "DEBUG";
    }
    
    Com_Printf("^3Vulkan %s: [%s] %s\n", type, pLayerPrefix, pMessage);
    
    // Return VK_FALSE to continue execution
    return VK_FALSE;
}
#endif

// Resource cleanup
void VK_Shutdown(void) {
    Com_Printf("Shutting down Vulkan renderer\n");
    
    // Wait for device to be idle
    VK_WaitIdle();
    
    // Cleanup synchronization objects
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(vk.device, vk.renderFinishedSemaphores[i], NULL);
        vkDestroySemaphore(vk.device, vk.imageAvailableSemaphores[i], NULL);
        vkDestroyFence(vk.device, vk.inFlightFences[i], NULL);
    }
    
    // Cleanup command pool
    vkDestroyCommandPool(vk.device, vk.commandPool, NULL);
    
    // Cleanup swapchain
    VK_CleanupSwapchain();
    
    // Cleanup device and instance
    vkDestroyDevice(vk.device, NULL);
    vkDestroySurfaceKHR(vk.instance, vk.surface, NULL);
    vkDestroyInstance(vk.instance, NULL);
    
    memset(&vk, 0, sizeof(vk));
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/renderer/pbr-pipeline">PBR Pipeline</a></li>
        <li><a href="/renderer/resource-management">Resource Management</a></li>
        <li><a href="/platform/cross-platform">Cross-Platform Development</a></li>
        <li><a href="/platform/threading-concurrency">Threading and Concurrency</a></li>
        <li><a href="/rendering/shaders">Shaders</a></li>
    </ul>
</div>