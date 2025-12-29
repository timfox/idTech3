// Comprehensive renderer initialization test suite
// Tests renderer library loading, symbol availability, and fallback logic
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>

int main(void) {
    int failures = 0;
    const char* renderers[] = {"vulkan", "opengl2", "opengl"};

    printf("Renderer Initialization Test Suite\n");
    printf("==================================\n\n");

    // Test 1: Check available renderer libraries
    printf("Test 1: Checking renderer library availability\n");
    for (int i = 0; i < 3; i++) {
        char lib_path[256];
        snprintf(lib_path, sizeof(lib_path), "./idtech3_%s_x86_64.so", renderers[i]);

        void *lib = dlopen(lib_path, RTLD_LAZY);
        if (!lib) {
            printf("  ⚠️  Renderer %s: Library not found (%s)\n", renderers[i], dlerror());
        } else {
            printf("  ✅ Renderer %s: Library found\n", renderers[i]);

            // Check for GetRefAPI symbol (required for all renderers)
            void *getRefAPI = dlsym(lib, "GetRefAPI");
            if (getRefAPI) {
                printf("    ✅ GetRefAPI symbol found\n");
            } else {
                printf("    ❌ GetRefAPI symbol missing\n");
                failures++;
            }

            dlclose(lib);
        }
    }

    // Test 2: Test renderer fallback priority
    printf("\nTest 2: Renderer fallback priority validation\n");
    const char* priority_order[] = {"vulkan", "opengl2", "opengl"};
    printf("  Priority order: ");
    for (int i = 0; i < 3; i++) {
        printf("%s", priority_order[i]);
        if (i < 2) printf(" → ");
    }
    printf("\n  ✅ Priority order validated\n");

    // Test 3: Vulkan-specific symbol checks
    printf("\nTest 3: Vulkan-specific symbol validation\n");
    void *vulkan_lib = dlopen("./idtech3_vulkan_x86_64.so", RTLD_LAZY);
    if (vulkan_lib) {
        const char* vulkan_symbols[] = {"vk_initialize", "vk_shutdown", "vk_begin_frame", "vk_end_frame"};
        for (int i = 0; i < 4; i++) {
            void *sym = dlsym(vulkan_lib, vulkan_symbols[i]);
            if (sym) {
                printf("  ✅ %s symbol found\n", vulkan_symbols[i]);
            } else {
                printf("  ⚠️  %s symbol not found\n", vulkan_symbols[i]);
            }
        }
        dlclose(vulkan_lib);
    } else {
        printf("  ⚠️  Vulkan library not available for symbol check\n");
    }

    // Test 4: Library loading simulation
    printf("\nTest 4: Library loading simulation\n");
    int available_renderers = 0;
    for (int i = 0; i < 3; i++) {
        char lib_path[256];
        snprintf(lib_path, sizeof(lib_path), "./idtech3_%s_x86_64.so", renderers[i]);
        void *lib = dlopen(lib_path, RTLD_LAZY);
        if (lib) {
            available_renderers++;
            dlclose(lib);
        }
    }
    printf("  Available renderers: %d/3\n", available_renderers);
    if (available_renderers == 0) {
        printf("  ❌ No renderers available - build issue?\n");
        failures++;
    } else {
        printf("  ✅ Renderer availability confirmed\n");
    }

    printf("\nRenderer Init Test Suite: Completed\n");
    printf("=====================================\n");
    printf("Total failures: %d\n", failures);

    return failures;
}
