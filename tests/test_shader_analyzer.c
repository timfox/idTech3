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

    // Test shader performance analyzer functions
    void (*print_shader_perf)(void) = dlsym(lib_handle, "vk_print_shader_performance_stats");
    void (*print_shader_opt)(void) = dlsym(lib_handle, "vk_print_shader_optimization_suggestions");
    void (*print_shader_inst)(void) = dlsym(lib_handle, "vk_print_shader_instruction_analysis");
    void (*print_shader_regs)(void) = dlsym(lib_handle, "vk_print_shader_register_usage");

    if (print_shader_perf) {
        printf("✓ Found vk_print_shader_performance_stats\n");
    } else {
        printf("✗ vk_print_shader_performance_stats not found\n");
    }

    if (print_shader_opt) {
        printf("✓ Found vk_print_shader_optimization_suggestions\n");
    } else {
        printf("✗ vk_print_shader_optimization_suggestions not found\n");
    }

    if (print_shader_inst) {
        printf("✓ Found vk_print_shader_instruction_analysis\n");
    } else {
        printf("✗ vk_print_shader_instruction_analysis not found\n");
    }

    if (print_shader_regs) {
        printf("✓ Found vk_print_shader_register_usage\n");
    } else {
        printf("✗ vk_print_shader_register_usage not found\n");
    }

    dlclose(lib_handle);
    printf("Shader performance analyzer test completed\n");
    return 0;
}
