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

    // Test parallel processing profiler functions
    void (*print_parallel)(void) = dlsym(lib_handle, "vk_print_parallel_stats");
    void (*print_thread)(void) = dlsym(lib_handle, "vk_print_thread_utilization");
    void (*print_sync)(void) = dlsym(lib_handle, "vk_print_synchronization_overhead");
    void (*print_efficiency)(void) = dlsym(lib_handle, "vk_print_parallel_efficiency");

    if (print_parallel) {
        printf("✓ Found vk_print_parallel_stats\n");
    } else {
        printf("✗ vk_print_parallel_stats not found\n");
    }

    if (print_thread) {
        printf("✓ Found vk_print_thread_utilization\n");
    } else {
        printf("✗ vk_print_thread_utilization not found\n");
    }

    if (print_sync) {
        printf("✓ Found vk_print_synchronization_overhead\n");
    } else {
        printf("✗ vk_print_synchronization_overhead not found\n");
    }

    if (print_efficiency) {
        printf("✓ Found vk_print_parallel_efficiency\n");
    } else {
        printf("✗ vk_print_parallel_efficiency not found\n");
    }

    dlclose(lib_handle);
    printf("Parallel processing profiler test completed\n");
    return 0;
}
