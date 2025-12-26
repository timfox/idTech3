#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

int main() {
    void *lib_handle = dlopen("./idtech3_vulkan_x86_64.so", RTLD_LAZY);
    if (!lib_handle) {
        fprintf(stderr, "Failed to load Vulkan renderer: %s\n", dlerror());
        return 1;
    }

    printf("Successfully loaded Vulkan renderer library\n");

    // Test memory bandwidth profiler functions
    void (*print_bandwidth)(void) = dlsym(lib_handle, "vk_print_memory_bandwidth_stats");
    void (*print_cache)(void) = dlsym(lib_handle, "vk_print_cache_performance_stats");
    void (*print_layout)(void) = dlsym(lib_handle, "vk_print_layout_optimization_recommendations");

    if (print_bandwidth) {
        printf("✓ Found vk_print_memory_bandwidth_stats\n");
    } else {
        printf("✗ vk_print_memory_bandwidth_stats not found\n");
    }

    if (print_cache) {
        printf("✓ Found vk_print_cache_performance_stats\n");
    } else {
        printf("✗ vk_print_cache_performance_stats not found\n");
    }

    if (print_layout) {
        printf("✓ Found vk_print_layout_optimization_recommendations\n");
    } else {
        printf("✗ vk_print_layout_optimization_recommendations not found\n");
    }

    dlclose(lib_handle);
    printf("Memory bandwidth profiler test completed\n");
    return 0;
}
