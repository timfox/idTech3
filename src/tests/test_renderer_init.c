// Lightweight render backend initialization test scaffold
// This is a non-invasive test to validate that Vulkan renderer library loads
// and basic symbols are available. It is not a full integration test.
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>

int main(void) {
    printf("Renderer Init Test: Vulkan backend loading\n");
    void *lib = dlopen("./idtech3_vulkan_x86_64.so", RTLD_LAZY);
    if (!lib) {
        printf("  ❌ Failed to load Vulkan renderer library: %s\n", dlerror());
        return 1;
    }
    printf("  ✅ Vulkan renderer library loaded\n");
    // Try to resolve a couple of symbols
    void *sym = dlsym(lib, "vk_initialize");
    if (sym) printf("  ✅ Found vk_initialize symbol\n"); else printf("  ⚠️  vk_initialize not found\n");
    sym = dlsym(lib, "vk_shutdown");
    if (sym) printf("  ✅ Found vk_shutdown symbol\n"); else printf("  ⚠️  vk_shutdown not found\n");
    dlclose(lib);
    printf("Renderer Init Test: Completed\n");
    return 0;
}

