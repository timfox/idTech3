#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

typedef struct {
    void *handle;
    int (*R_Init)(void);
    void (*R_Shutdown)(void);
    void (*vk_print_render_profiler_stats)(void);
} renderer_api_t;

int main() {
    void *lib_handle = dlopen("./idtech3_vulkan_x86_64.so", RTLD_LAZY);
    if (!lib_handle) {
        fprintf(stderr, "Failed to load Vulkan renderer: %s\n", dlerror());
        return 1;
    }

    printf("Successfully loaded Vulkan renderer library\n");

    // Try to get the profiler function
    void (*print_stats)(void) = dlsym(lib_handle, "vk_print_render_profiler_stats");
    if (print_stats) {
        printf("Found vk_print_render_profiler_stats function\n");
        // Call it (though it might not work without full initialization)
        // print_stats();
    } else {
        fprintf(stderr, "vk_print_render_profiler_stats not found: %s\n", dlerror());
    }

    dlclose(lib_handle);
    printf("Test completed\n");
    return 0;
}
