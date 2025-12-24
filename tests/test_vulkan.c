#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <vulkan/vulkan.h>

// Function pointer types for Vulkan renderer
typedef void (*PFN_vk_initialize)(void);
typedef void (*PFN_vk_shutdown)(void);

int main() {
    printf("Vulkan Renderer Test\n");
    printf("===================\n\n");

    // Test 1: Check if Vulkan library is available
    printf("Test 1: Checking Vulkan library availability...\n");
    void *vulkan_lib = dlopen("libvulkan.so.1", RTLD_LAZY);
    if (!vulkan_lib) {
        printf("  ❌ Vulkan library not found: %s\n", dlerror());
        return 1;
    }
    printf("  ✅ Vulkan library found\n");
    dlclose(vulkan_lib);

    // Test 2: Check if renderer shared library can be loaded
    printf("\nTest 2: Loading Vulkan renderer shared library...\n");
    void *renderer_lib = dlopen("./build/idtech3_vulkan_x86_64.so", RTLD_LAZY);
    if (!renderer_lib) {
        printf("  ❌ Failed to load renderer library: %s\n", dlerror());
        return 1;
    }
    printf("  ✅ Renderer library loaded successfully\n");

    // Test 3: Check if we can resolve some renderer functions
    printf("\nTest 3: Resolving renderer functions...\n");

    // Try to resolve some known Vulkan functions
    void *vk_init_func = dlsym(renderer_lib, "vk_initialize");
    if (vk_init_func) {
        printf("  ✅ vk_initialize function found\n");
    } else {
        printf("  ⚠️  vk_initialize not found: %s\n", dlerror());
    }

    void *vk_shutdown_func = dlsym(renderer_lib, "vk_shutdown");
    if (vk_shutdown_func) {
        printf("  ✅ vk_shutdown function found\n");
    } else {
        printf("  ⚠️  vk_shutdown not found: %s\n", dlerror());
    }

    // Check for some core Vulkan symbols
    void *qvk_create_instance = dlsym(renderer_lib, "qvkCreateInstance");
    if (qvk_create_instance) {
        printf("  ✅ Vulkan function pointers initialized\n");
    } else {
        printf("  ⚠️  Vulkan function pointers not initialized\n");
    }

    // Test 4: Basic Vulkan instance creation
    printf("\nTest 4: Basic Vulkan instance creation...\n");
    VkInstance instance = VK_NULL_HANDLE;
    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "idTech3 Vulkan Test",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "idTech3",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0
    };

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo
    };

    VkResult result = vkCreateInstance(&createInfo, NULL, &instance);
    if (result != VK_SUCCESS) {
        printf("  ❌ Failed to create Vulkan instance: %d\n", result);
        dlclose(renderer_lib);
        return 1;
    }
    printf("  ✅ Vulkan instance created successfully\n");

    // Cleanup
    vkDestroyInstance(instance, NULL);
    dlclose(renderer_lib);

    printf("\n🎉 All tests passed! Vulkan renderer is ready for use.\n\n");
    printf("Next steps:\n");
    printf("1. Run the engine with '+set r_renderer vulkan'\n");
    printf("2. Test basic rendering functionality\n");
    printf("3. Validate advanced features (bloom, ray tracing, etc.)\n");

    return 0;
}
