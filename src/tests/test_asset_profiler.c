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

    // Test asset loading profiler functions
    void (*print_asset_stats)(void) = dlsym(lib_handle, "vk_print_asset_loading_stats");
    void (*print_io_stats)(void) = dlsym(lib_handle, "vk_print_io_performance_stats");
    void (*print_streaming_stats)(void) = dlsym(lib_handle, "vk_print_streaming_stats");
    void (*print_asset_bottlenecks)(void) = dlsym(lib_handle, "vk_print_asset_loading_bottlenecks");

    if (print_asset_stats) {
        printf("✓ Found vk_print_asset_loading_stats\n");
    } else {
        printf("✗ vk_print_asset_loading_stats not found\n");
    }

    if (print_io_stats) {
        printf("✓ Found vk_print_io_performance_stats\n");
    } else {
        printf("✗ vk_print_io_performance_stats not found\n");
    }

    if (print_streaming_stats) {
        printf("✓ Found vk_print_streaming_stats\n");
    } else {
        printf("✗ vk_print_streaming_stats not found\n");
    }

    if (print_asset_bottlenecks) {
        printf("✓ Found vk_print_asset_loading_bottlenecks\n");
    } else {
        printf("✗ vk_print_asset_loading_bottlenecks not found\n");
    }

    dlclose(lib_handle);
    printf("Asset loading profiler test completed\n");
    return 0;
}
