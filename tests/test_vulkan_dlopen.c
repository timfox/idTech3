#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    void *handle = dlopen("./idtech3_vulkan_x86_64.so", RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "dlopen failed: %s\n", dlerror());
        return 1;
    }

    printf("Library loaded successfully!\n");

    // Try to get the GetRefAPI function
    void *func = dlsym(handle, "GetRefAPI");
    if (!func) {
        fprintf(stderr, "dlsym failed: %s\n", dlerror());
        dlclose(handle);
        return 1;
    }

    printf("GetRefAPI function found!\n");

    dlclose(handle);
    return 0;
}
