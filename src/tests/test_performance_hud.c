#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "src/common/q_shared.h"

int main() {
    void *lib_handle = dlopen("./idtech3_vulkan_x86_64.so", RTLD_LAZY);
    if (!lib_handle) {
        fprintf(stderr, "Failed to load Vulkan renderer: %s\n", dlerror());
        return 1;
    }

    printf("Successfully loaded Vulkan renderer library\n");

    // Test performance HUD functions
    void (*toggle_hud)(void) = dlsym(lib_handle, "vk_toggle_performance_hud");
    qboolean (*is_enabled)(void) = dlsym(lib_handle, "vk_is_performance_hud_enabled");

    if (toggle_hud) {
        printf("✓ Found vk_toggle_performance_hud\n");
    } else {
        printf("✗ vk_toggle_performance_hud not found\n");
    }

    if (is_enabled) {
        printf("✓ Found vk_is_performance_hud_enabled\n");
    } else {
        printf("✗ vk_is_performance_hud_enabled not found\n");
    }

    dlclose(lib_handle);
    printf("Performance HUD test completed\n");
    return 0;
}
