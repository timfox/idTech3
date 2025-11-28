<?php
/**
 * Cross-Platform Development - Platform Abstraction for id Tech 3
 */
$title = 'Cross-Platform Development - id Tech 3 Documentation';
$breadcrumbs = [
    '/platform' => 'Platform and Deployment',
    '/platform/cross-platform' => 'Cross-Platform Development'
];
?>

<h1>Cross-Platform Development - Modern Platform Abstraction</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Modern cross-platform development for id Tech 3 with PBR and Vulkan requires sophisticated abstraction layers to handle differences between Windows, Linux, macOS, Android, and console platforms. This guide focuses on practical implementations used in JKSunny's PBR port.</p>
    
    <div class="feature-list">
        <h3>Platform Abstraction Goals</h3>
        <ul>
            <li><strong>Vulkan API Compatibility:</strong> Handle driver differences and extensions</li>
            <li><strong>Window Management:</strong> Unified windowing across desktop and mobile</li>
            <li><strong>File System:</strong> Path handling, permissions, and sandboxing</li>
            <li><strong>Threading:</strong> Platform-specific threading models and performance</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Platform Detection and Configuration</h2>
    
    <h3>Platform Identification System</h3>
    <div class="code-block">
        <pre><code>// platform.h - Modern platform detection for PBR port
#ifndef PLATFORM_H
#define PLATFORM_H

// Primary platform detection
#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS 1
    #if defined(_WIN64)
        #define PLATFORM_64BIT 1
    #else
        #define PLATFORM_32BIT 1
    #endif
#elif defined(__ANDROID__)
    #define PLATFORM_ANDROID 1
    #define PLATFORM_MOBILE 1
    #define PLATFORM_64BIT 1  // Modern Android is 64-bit
#elif defined(__linux__)
    #define PLATFORM_LINUX 1
    #define PLATFORM_64BIT 1
#elif defined(__APPLE__)
    #include <TargetConditionals.h>
    #if TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR
        #define PLATFORM_IOS 1
        #define PLATFORM_MOBILE 1
    #else
        #define PLATFORM_MACOS 1
    #endif
    #define PLATFORM_64BIT 1  // Modern Apple is 64-bit only
#elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
    #define PLATFORM_BSD 1
    #define PLATFORM_64BIT 1
#endif

// Graphics API availability by platform
#if defined(PLATFORM_WINDOWS)
    #define VULKAN_AVAILABLE 1
    #define OPENGL_AVAILABLE 1
    #define D3D11_AVAILABLE 1
    #define D3D12_AVAILABLE 1
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_BSD)
    #define VULKAN_AVAILABLE 1
    #define OPENGL_AVAILABLE 1
#elif defined(PLATFORM_MACOS)
    #define VULKAN_AVAILABLE 1  // Via MoltenVK
    #define OPENGL_AVAILABLE 1  // Deprecated but available
    #define METAL_AVAILABLE 1
#elif defined(PLATFORM_IOS)
    #define VULKAN_AVAILABLE 1  // Via MoltenVK
    #define METAL_AVAILABLE 1
#elif defined(PLATFORM_ANDROID)
    #define VULKAN_AVAILABLE 1
    #define OPENGL_ES_AVAILABLE 1
#endif

// Platform-specific includes and definitions
#ifdef PLATFORM_WINDOWS
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <windowsx.h>
    #define PATH_SEPARATOR '\\'
    #define SHARED_LIB_EXT ".dll"
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_BSD)
    #include <unistd.h>
    #include <dlfcn.h>
    #include <sys/stat.h>
    #define PATH_SEPARATOR '/'
    #define SHARED_LIB_EXT ".so"
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_IOS)
    #include <unistd.h>
    #include <dlfcn.h>
    #include <sys/stat.h>
    #define PATH_SEPARATOR '/'
    #define SHARED_LIB_EXT ".dylib"
#elif defined(PLATFORM_ANDROID)
    #include <unistd.h>
    #include <dlfcn.h>
    #include <android/log.h>
    #include <android/asset_manager.h>
    #include <android/native_activity.h>
    #define PATH_SEPARATOR '/'
    #define SHARED_LIB_EXT ".so"
#endif

// Compiler-specific attributes
#ifdef __GNUC__
    #define PLATFORM_FORCEINLINE __attribute__((always_inline)) inline
    #define PLATFORM_NOINLINE __attribute__((noinline))
    #define PLATFORM_ALIGN(x) __attribute__((aligned(x)))
#elif defined(_MSC_VER)
    #define PLATFORM_FORCEINLINE __forceinline
    #define PLATFORM_NOINLINE __declspec(noinline)
    #define PLATFORM_ALIGN(x) __declspec(align(x))
#else
    #define PLATFORM_FORCEINLINE inline
    #define PLATFORM_NOINLINE
    #define PLATFORM_ALIGN(x)
#endif

#endif // PLATFORM_H</code></pre>
    </div>
    
    <h3>Platform Configuration Management</h3>
    <div class="code-block">
        <pre><code>// platform_config.h - Runtime platform configuration
typedef struct platformConfig_s {
    // Graphics capabilities
    qboolean vulkanSupported;
    qboolean rayTracingSupported;
    qboolean meshShadersSupported;
    qboolean variableRateShadingSupported;
    
    // Platform-specific paths
    char executablePath[MAX_OSPATH];
    char dataPath[MAX_OSPATH];
    char configPath[MAX_OSPATH];
    char cachePath[MAX_OSPATH];
    char tempPath[MAX_OSPATH];
    
    // Hardware information
    int cpuCores;
    int cpuThreads;
    size_t totalMemory;
    size_t availableMemory;
    
    // Display information
    int primaryDisplayWidth;
    int primaryDisplayHeight;
    int displayCount;
    float displayDPI;
    qboolean touchInput;
    
    // Platform-specific features
    qboolean steamDeckMode;        // Steam Deck detection
    qboolean consoleMode;          // Console platform
    qboolean mobileMode;           // Mobile platform
    qboolean sandboxed;            // Sandboxed environment
    
} platformConfig_t;

extern platformConfig_t platform;

// Platform initialization
qboolean Platform_Init(void) {
    memset(&platform, 0, sizeof(platform));
    
    // Detect Vulkan support
    platform.vulkanSupported = VK_DetectSupport();
    
    // Get hardware information
    Platform_GetHardwareInfo();
    
    // Set up platform-specific paths
    Platform_InitializePaths();
    
    // Detect special platforms
    Platform_DetectSpecialModes();
    
    Com_Printf("Platform: %s\n", Platform_GetName());
    Com_Printf("Vulkan: %s\n", platform.vulkanSupported ? "Available" : "Not Available");
    Com_Printf("CPU: %d cores, %d threads\n", platform.cpuCores, platform.cpuThreads);
    Com_Printf("Memory: %zu MB total, %zu MB available\n", 
               platform.totalMemory / (1024*1024), 
               platform.availableMemory / (1024*1024));
    
    return qtrue;
}

void Platform_GetHardwareInfo(void) {
#ifdef PLATFORM_WINDOWS
    SYSTEM_INFO sysInfo;
    MEMORYSTATUSEX memInfo;
    
    GetSystemInfo(&sysInfo);
    platform.cpuCores = sysInfo.dwNumberOfProcessors;
    platform.cpuThreads = sysInfo.dwNumberOfProcessors; // Simplified
    
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    platform.totalMemory = memInfo.ullTotalPhys;
    platform.availableMemory = memInfo.ullAvailPhys;
    
#elif defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
    platform.cpuCores = sysconf(_SC_NPROCESSORS_ONLN);
    platform.cpuThreads = platform.cpuCores; // Simplified
    
    long pages = sysconf(_SC_PHYS_PAGES);
    long pageSize = sysconf(_SC_PAGE_SIZE);
    platform.totalMemory = pages * pageSize;
    platform.availableMemory = platform.totalMemory; // Simplified
    
#elif defined(PLATFORM_ANDROID)
    // Android-specific hardware detection
    platform.cpuCores = android_getCpuCount();
    platform.cpuThreads = platform.cpuCores;
    platform.mobileMode = qtrue;
    platform.touchInput = qtrue;
#endif
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Vulkan Platform Abstraction</h2>
    
    <h3>Cross-Platform Vulkan Context</h3>
    <div class="code-block">
        <pre><code>// vk_platform.h - Vulkan platform abstraction for PBR port
typedef struct vkPlatform_s {
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    
    // Platform-specific surface creation
    union {
#ifdef PLATFORM_WINDOWS
        struct {
            HWND hwnd;
            HINSTANCE hinstance;
        } win32;
#elif defined(PLATFORM_LINUX)
        struct {
            Display* display;
            Window window;
        } xlib;
        struct {
            struct wl_display* display;
            struct wl_surface* surface;
        } wayland;
#elif defined(PLATFORM_MACOS)
        struct {
            void* nsView;  // NSView* - avoid Objective-C in C header
        } macos;
#elif defined(PLATFORM_ANDROID)
        struct {
            ANativeWindow* window;
        } android;
#endif
    } platformData;
    
    // Extension support
    qboolean swapchainSupported;
    qboolean debugReportSupported;
    qboolean validationLayersSupported;
    
    // Platform-specific extensions
    const char** instanceExtensions;
    uint32_t instanceExtensionCount;
    
} vkPlatform_t;

extern vkPlatform_t vkPlatform;

// Platform-specific Vulkan initialization
qboolean VK_Platform_Init(void) {
    // Get required instance extensions for platform
    if (!VK_Platform_GetInstanceExtensions()) {
        Com_Printf("^1Failed to get required Vulkan instance extensions\n");
        return qfalse;
    }
    
    // Create Vulkan instance
    if (!VK_CreateInstance()) {
        Com_Printf("^1Failed to create Vulkan instance\n");
        return qfalse;
    }
    
    // Create platform surface
    if (!VK_Platform_CreateSurface()) {
        Com_Printf("^1Failed to create Vulkan surface\n");
        return qfalse;
    }
    
    return qtrue;
}

qboolean VK_Platform_GetInstanceExtensions(void) {
    // Platform-specific required extensions
    static const char* requiredExtensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        
#ifdef PLATFORM_WINDOWS
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(PLATFORM_LINUX)
        VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
        // Also check for Wayland: VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME
#elif defined(PLATFORM_MACOS) || defined(PLATFORM_IOS)
        VK_MVK_MACOS_SURFACE_EXTENSION_NAME,
#elif defined(PLATFORM_ANDROID)
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME,
#endif
        
#ifdef _DEBUG
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
#endif
    };
    
    vkPlatform.instanceExtensionCount = ARRAY_LEN(requiredExtensions);
    vkPlatform.instanceExtensions = requiredExtensions;
    
    // Verify all required extensions are available
    uint32_t availableExtensionCount;
    vkEnumerateInstanceExtensionProperties(NULL, &availableExtensionCount, NULL);
    
    VkExtensionProperties* availableExtensions = 
        malloc(sizeof(VkExtensionProperties) * availableExtensionCount);
    vkEnumerateInstanceExtensionProperties(NULL, &availableExtensionCount, 
                                          availableExtensions);
    
    for (uint32_t i = 0; i < vkPlatform.instanceExtensionCount; i++) {
        qboolean found = qfalse;
        for (uint32_t j = 0; j < availableExtensionCount; j++) {
            if (strcmp(vkPlatform.instanceExtensions[i], 
                      availableExtensions[j].extensionName) == 0) {
                found = qtrue;
                break;
            }
        }
        
        if (!found) {
            Com_Printf("^1Required Vulkan extension not available: %s\n", 
                      vkPlatform.instanceExtensions[i]);
            free(availableExtensions);
            return qfalse;
        }
    }
    
    free(availableExtensions);
    return qtrue;
}

qboolean VK_Platform_CreateSurface(void) {
    VkResult result;
    
#ifdef PLATFORM_WINDOWS
    VkWin32SurfaceCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
        .hinstance = vkPlatform.platformData.win32.hinstance,
        .hwnd = vkPlatform.platformData.win32.hwnd
    };
    
    result = vkCreateWin32SurfaceKHR(vkPlatform.instance, &createInfo, 
                                    NULL, &vkPlatform.surface);
    
#elif defined(PLATFORM_LINUX)
    // X11/Xlib surface creation
    VkXlibSurfaceCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
        .dpy = vkPlatform.platformData.xlib.display,
        .window = vkPlatform.platformData.xlib.window
    };
    
    result = vkCreateXlibSurfaceKHR(vkPlatform.instance, &createInfo, 
                                   NULL, &vkPlatform.surface);
    
#elif defined(PLATFORM_MACOS)
    VkMacOSSurfaceCreateInfoMVK createInfo = {
        .sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK,
        .pView = vkPlatform.platformData.macos.nsView
    };
    
    result = vkCreateMacOSSurfaceMVK(vkPlatform.instance, &createInfo, 
                                    NULL, &vkPlatform.surface);
    
#elif defined(PLATFORM_ANDROID)
    VkAndroidSurfaceCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
        .window = vkPlatform.platformData.android.window
    };
    
    result = vkCreateAndroidSurfaceKHR(vkPlatform.instance, &createInfo, 
                                      NULL, &vkPlatform.surface);
#endif
    
    if (result != VK_SUCCESS) {
        Com_Printf("^1Failed to create Vulkan surface: %s\n", 
                  VK_ResultToString(result));
        return qfalse;
    }
    
    return qtrue;
}</code></pre>
    </div>
    
    <h3>PBR Shader Compilation Pipeline</h3>
    <div class="code-block">
        <pre><code>// shader_platform.h - Cross-platform shader compilation for PBR
typedef struct shaderCompiler_s {
    // Platform-specific shader compilation
    qboolean (*compileShader)(const char* source, shaderStage_t stage, 
                              const char* entryPoint, uint32_t** spirv, 
                              size_t* spirvSize);
    
    // Shader cache management
    qboolean (*loadFromCache)(const char* key, uint32_t** spirv, size_t* spirvSize);
    qboolean (*saveToCache)(const char* key, const uint32_t* spirv, size_t spirvSize);
    
    // Platform-specific optimizations
    qboolean (*optimizeSpirv)(uint32_t** spirv, size_t* spirvSize);
    
} shaderCompiler_t;

// Platform-specific shader compilation implementations
#ifdef PLATFORM_WINDOWS
// Use DXC for HLSL to SPIR-V compilation
qboolean Shader_CompileHLSL_DXC(const char* source, shaderStage_t stage,
                                const char* entryPoint, uint32_t** spirv, 
                                size_t* spirvSize) {
    IDxcCompiler* compiler = NULL;
    IDxcLibrary* library = NULL;
    IDxcBlobEncoding* sourceBlob = NULL;
    IDxcOperationResult* result = NULL;
    
    HRESULT hr = DxcCreateInstance(&CLSID_DxcCompiler, &IID_IDxcCompiler, 
                                  (void**)&compiler);
    if (FAILED(hr)) {
        Com_Printf("^1Failed to create DXC compiler\n");
        return qfalse;
    }
    
    hr = DxcCreateInstance(&CLSID_DxcLibrary, &IID_IDxcLibrary, (void**)&library);
    if (FAILED(hr)) {
        compiler->Release();
        Com_Printf("^1Failed to create DXC library\n");
        return qfalse;
    }
    
    // Create source blob
    hr = library->CreateBlobWithEncodingFromPinned(source, strlen(source), 
                                                   CP_UTF8, &sourceBlob);
    if (FAILED(hr)) {
        library->Release();
        compiler->Release();
        return qfalse;
    }
    
    // Set compilation arguments
    LPCWSTR args[] = {
        L"-spirv",                    // Generate SPIR-V
        L"-fspv-target-env=vulkan1.2", // Target Vulkan 1.2
        L"-O3",                       // Optimization level
        L"-Zpr",                      // Row-major matrices (preferred for Vulkan)
    };
    
    const wchar_t* targetProfile = GetTargetProfile(stage);
    wchar_t entryPointW[64];
    MultiByteToWideChar(CP_UTF8, 0, entryPoint, -1, entryPointW, 64);
    
    // Compile shader
    hr = compiler->Compile(sourceBlob, L"shader.hlsl", entryPointW, targetProfile,
                          args, ARRAY_LEN(args), NULL, 0, NULL, &result);
    
    // Get compilation result
    HRESULT compileResult;
    result->GetStatus(&compileResult);
    
    if (SUCCEEDED(compileResult)) {
        IDxcBlob* spirvBlob;
        result->GetResult(&spirvBlob);
        
        *spirvSize = spirvBlob->GetBufferSize();
        *spirv = malloc(*spirvSize);
        memcpy(*spirv, spirvBlob->GetBufferPointer(), *spirvSize);
        
        spirvBlob->Release();
    } else {
        // Get error messages
        IDxcBlobEncoding* errorBlob;
        result->GetErrorBuffer(&errorBlob);
        
        if (errorBlob) {
            Com_Printf("^1Shader compilation failed: %.*s\n",
                      (int)errorBlob->GetBufferSize(),
                      (char*)errorBlob->GetBufferPointer());
            errorBlob->Release();
        }
    }
    
    // Cleanup
    result->Release();
    sourceBlob->Release();
    library->Release();
    compiler->Release();
    
    return SUCCEEDED(compileResult);
}

#elif defined(PLATFORM_LINUX) || defined(PLATFORM_MACOS)
// Use glslangValidator or shaderc for GLSL to SPIR-V compilation
qboolean Shader_CompileGLSL_Shaderc(const char* source, shaderStage_t stage,
                                    const char* entryPoint, uint32_t** spirv,
                                    size_t* spirvSize) {
    shaderc_compiler_t compiler = shaderc_compiler_initialize();
    if (!compiler) {
        Com_Printf("^1Failed to initialize shaderc compiler\n");
        return qfalse;
    }
    
    shaderc_compile_options_t options = shaderc_compile_options_initialize();
    shaderc_compile_options_set_optimization_level(options, 
                                                   shaderc_optimization_level_performance);
    shaderc_compile_options_set_target_env(options, shaderc_target_env_vulkan, 
                                          shaderc_env_version_vulkan_1_2);
    
    // Convert stage to shaderc kind
    shaderc_shader_kind kind = GetShadercKind(stage);
    
    shaderc_compilation_result_t result = 
        shaderc_compile_into_spv(compiler, source, strlen(source), kind,
                                "shader.glsl", entryPoint, options);
    
    shaderc_compilation_status status = shaderc_result_get_compilation_status(result);
    
    if (status == shaderc_compilation_status_success) {
        *spirvSize = shaderc_result_get_length(result);
        *spirv = malloc(*spirvSize);
        memcpy(*spirv, shaderc_result_get_bytes(result), *spirvSize);
    } else {
        const char* errorMsg = shaderc_result_get_error_message(result);
        Com_Printf("^1Shader compilation failed: %s\n", errorMsg);
    }
    
    shaderc_result_release(result);
    shaderc_compile_options_release(options);
    shaderc_compiler_release(compiler);
    
    return status == shaderc_compilation_status_success;
}
#endif</code></pre>
    </div>
</div>

<div class="section">
    <h2>File System Abstraction</h2>
    
    <h3>Modern File System Interface</h3>
    <div class="code-block">
        <pre><code>// fs_platform.h - Cross-platform file system for PBR port
typedef struct fsHandle_s {
    union {
#ifdef PLATFORM_WINDOWS
        HANDLE winHandle;
#else
        int posixFd;
#endif
#ifdef PLATFORM_ANDROID
        AAsset* androidAsset;
#endif
    };
    
    qboolean isAndroidAsset;
    qboolean isMemoryMapped;
    void* mappedData;
    size_t mappedSize;
} fsHandle_t;

typedef struct fsStats_s {
    size_t size;
    time_t modificationTime;
    time_t creationTime;
    qboolean isDirectory;
    qboolean isReadOnly;
} fsStats_t;

// Cross-platform file operations
fsHandle_t FS_Platform_OpenFile(const char* path, const char* mode) {
    fsHandle_t handle = {0};
    
#ifdef PLATFORM_WINDOWS
    DWORD access = 0, shareMode = FILE_SHARE_READ, disposition = 0;
    
    if (strchr(mode, 'r')) access |= GENERIC_READ;
    if (strchr(mode, 'w')) {
        access |= GENERIC_WRITE;
        disposition = CREATE_ALWAYS;
    } else {
        disposition = OPEN_EXISTING;
    }
    
    // Convert UTF-8 path to wide char for Windows
    wchar_t widePath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, widePath, MAX_PATH);
    
    handle.winHandle = CreateFileW(widePath, access, shareMode, NULL, 
                                  disposition, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (handle.winHandle == INVALID_HANDLE_VALUE) {
        handle.winHandle = NULL;
    }
    
#elif defined(PLATFORM_ANDROID)
    // Check if it's an asset file first
    if (strncmp(path, "assets/", 7) == 0) {
        if (androidApp && androidApp->activity && androidApp->activity->assetManager) {
            handle.androidAsset = AAssetManager_open(
                androidApp->activity->assetManager,
                path + 7, // Skip "assets/" prefix
                AASSET_MODE_STREAMING
            );
            handle.isAndroidAsset = qtrue;
        }
    } else {
        // Regular file access
        int flags = 0;
        if (strchr(mode, 'r')) flags |= O_RDONLY;
        if (strchr(mode, 'w')) flags |= O_WRONLY | O_CREAT | O_TRUNC;
        
        handle.posixFd = open(path, flags, 0644);
        if (handle.posixFd == -1) {
            handle.posixFd = 0;
        }
    }
    
#else
    // POSIX platforms (Linux, macOS, iOS, BSD)
    handle.posixFd = open(path, 
                         strchr(mode, 'w') ? O_WRONLY | O_CREAT | O_TRUNC : O_RDONLY,
                         0644);
    if (handle.posixFd == -1) {
        handle.posixFd = 0;
    }
#endif
    
    return handle;
}

size_t FS_Platform_ReadFile(fsHandle_t handle, void* buffer, size_t size) {
#ifdef PLATFORM_WINDOWS
    if (!handle.winHandle) return 0;
    
    DWORD bytesRead;
    if (ReadFile(handle.winHandle, buffer, (DWORD)size, &bytesRead, NULL)) {
        return bytesRead;
    }
    return 0;
    
#elif defined(PLATFORM_ANDROID) && defined(handle.isAndroidAsset)
    if (handle.isAndroidAsset && handle.androidAsset) {
        return AAsset_read(handle.androidAsset, buffer, size);
    }
    // Fall through to POSIX
#endif
    
#ifndef PLATFORM_WINDOWS
    if (!handle.posixFd) return 0;
    
    ssize_t bytesRead = read(handle.posixFd, buffer, size);
    return bytesRead > 0 ? bytesRead : 0;
#endif
}

// Memory-mapped file support for large assets
qboolean FS_Platform_MapFile(fsHandle_t* handle, const char* path) {
#ifdef PLATFORM_WINDOWS
    HANDLE fileHandle = handle->winHandle;
    if (!fileHandle) return qfalse;
    
    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(fileHandle, &fileSize)) {
        return qfalse;
    }
    
    HANDLE mappingHandle = CreateFileMappingW(fileHandle, NULL, PAGE_READONLY, 
                                             fileSize.HighPart, fileSize.LowPart, NULL);
    if (!mappingHandle) {
        return qfalse;
    }
    
    handle->mappedData = MapViewOfFile(mappingHandle, FILE_MAP_READ, 0, 0, 0);
    handle->mappedSize = fileSize.QuadPart;
    handle->isMemoryMapped = qtrue;
    
    CloseHandle(mappingHandle);
    return handle->mappedData != NULL;
    
#else
    if (!handle->posixFd) return qfalse;
    
    struct stat st;
    if (fstat(handle->posixFd, &st) == -1) {
        return qfalse;
    }
    
    handle->mappedData = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, 
                             handle->posixFd, 0);
    if (handle->mappedData == MAP_FAILED) {
        handle->mappedData = NULL;
        return qfalse;
    }
    
    handle->mappedSize = st.st_size;
    handle->isMemoryMapped = qtrue;
    
    // Advise kernel about access pattern for large textures/models
    madvise(handle->mappedData, st.st_size, MADV_SEQUENTIAL);
    
    return qtrue;
#endif
}</code></pre>
    </div>
    
    <h3>Asset Pipeline Integration</h3>
    <div class="code-block">
        <pre><code>// asset_platform.h - Platform-specific asset loading for PBR
typedef struct assetLoader_s {
    // Platform-specific asset sources
    qboolean (*loadFromDisk)(const char* path, void** data, size_t* size);
    qboolean (*loadFromPak)(const char* path, void** data, size_t* size);
    qboolean (*loadFromAndroidAssets)(const char* path, void** data, size_t* size);
    qboolean (*loadFromiOSBundle)(const char* path, void** data, size_t* size);
    
    // Asset compression support
    qboolean (*decompressLZ4)(const void* compressed, size_t compressedSize,
                              void** decompressed, size_t* decompressedSize);
    qboolean (*decompressZSTD)(const void* compressed, size_t compressedSize,
                               void** decompressed, size_t* decompressedSize);
    
} assetLoader_t;

// PBR-specific asset loading
typedef struct pbrAssets_s {
    // Physically Based Rendering assets
    struct {
        char baseColorPath[MAX_QPATH];
        char normalPath[MAX_QPATH];
        char metallicRoughnessPath[MAX_QPATH];
        char occlusionPath[MAX_QPATH];
        char emissivePath[MAX_QPATH];
    } textures;
    
    // Material properties
    float metallicFactor;
    float roughnessFactor;
    vec3_t emissiveFactor;
    
    // Platform-specific optimizations
    qboolean useCompressedTextures;  // BC7, ASTC, etc.
    qboolean useAsyncLoading;
    qboolean useMipMapping;
    
} pbrAssets_t;

qboolean Asset_LoadPBRMaterial(const char* materialPath, pbrAssets_t* material) {
    char fullPath[MAX_OSPATH];
    
    // Build platform-specific asset path
#ifdef PLATFORM_ANDROID
    Com_sprintf(fullPath, sizeof(fullPath), "assets/materials/%s", materialPath);
#elif defined(PLATFORM_IOS)
    // iOS bundle path
    NSString* bundlePath = [[NSBundle mainBundle] resourcePath];
    Com_sprintf(fullPath, sizeof(fullPath), "%s/materials/%s", 
               [bundlePath UTF8String], materialPath);
#else
    // Desktop platforms
    Com_sprintf(fullPath, sizeof(fullPath), "baseq3/materials/%s", materialPath);
#endif
    
    // Load material definition file
    char* materialData;
    size_t materialSize;
    
    if (!FS_LoadFile(fullPath, (void**)&materialData, &materialSize)) {
        Com_Printf("^3Warning: Could not load PBR material %s\n", materialPath);
        return qfalse;
    }
    
    // Parse material definition (JSON or custom format)
    if (!Material_ParsePBR(materialData, materialSize, material)) {
        FS_FreeFile(materialData);
        return qfalse;
    }
    
    // Load textures asynchronously on supported platforms
    if (material->useAsyncLoading && platform.cpuThreads > 2) {
        Asset_LoadTexturesAsync(material);
    } else {
        Asset_LoadTexturesSync(material);
    }
    
    FS_FreeFile(materialData);
    return qtrue;
}

// Platform-specific texture format selection
textureFormat_t Asset_SelectOptimalFormat(textureType_t type) {
    switch (type) {
    case TEX_DIFFUSE:
#ifdef PLATFORM_WINDOWS
        return platform.vulkanSupported ? TEX_FORMAT_BC7_UNORM : TEX_FORMAT_RGBA8_UNORM;
#elif defined(PLATFORM_ANDROID)
        // Use ASTC on mobile if supported, otherwise ETC2
        return VK_CheckFormatSupport(VK_FORMAT_ASTC_4x4_UNORM_BLOCK) ? 
               TEX_FORMAT_ASTC_4x4 : TEX_FORMAT_ETC2_RGB8;
#elif defined(PLATFORM_IOS)
        return TEX_FORMAT_ASTC_4x4;  // ASTC is standard on iOS
#else
        return TEX_FORMAT_RGBA8_UNORM;  // Safe fallback
#endif
        
    case TEX_NORMAL:
#ifdef PLATFORM_WINDOWS
        return TEX_FORMAT_BC5_UNORM;  // Two-channel normal maps
#elif defined(PLATFORM_ANDROID) || defined(PLATFORM_IOS)
        return TEX_FORMAT_ASTC_4x4;
#else
        return TEX_FORMAT_RG8_UNORM;
#endif
        
    case TEX_METALLIC_ROUGHNESS:
        // Pack metallic (B) and roughness (G) in two channels
        return TEX_FORMAT_RG8_UNORM;
        
    default:
        return TEX_FORMAT_RGBA8_UNORM;
    }
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Window and Input Abstraction</h2>
    
    <h3>Modern Window Management</h3>
    <div class="code-block">
        <pre><code>// window_platform.h - Cross-platform windowing for Vulkan
typedef struct windowPlatform_s {
    // Platform-specific window handles
    union {
#ifdef PLATFORM_WINDOWS
        struct {
            HWND hwnd;
            HDC hdc;
        } win32;
#elif defined(PLATFORM_LINUX)
        struct {
            Display* display;
            Window window;
            Atom wmDeleteWindow;
        } x11;
#elif defined(PLATFORM_MACOS)
        struct {
            void* nsWindow;    // NSWindow*
            void* nsView;      // NSView*
        } cocoa;
#elif defined(PLATFORM_ANDROID)
        struct {
            ANativeWindow* nativeWindow;
            ANativeActivity* activity;
        } android;
#endif
    } handle;
    
    // Window properties
    int width, height;
    int refreshRate;
    qboolean fullscreen;
    qboolean vsync;
    qboolean hdr;
    
    // Input state
    qboolean mouseGrabbed;
    qboolean keyboardFocus;
    
    // Platform capabilities
    qboolean supportsHDR;
    qboolean supportsVariableRefreshRate;
    qboolean supportsBorderless;
    
} windowPlatform_t;

extern windowPlatform_t window;

qboolean Window_Create(const char* title, int width, int height, qboolean fullscreen) {
    memset(&window, 0, sizeof(window));
    window.width = width;
    window.height = height;
    window.fullscreen = fullscreen;
    
#ifdef PLATFORM_WINDOWS
    // Register window class
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = Window_WndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"Quake3e_PBR";
    
    if (!RegisterClassEx(&wc)) {
        Com_Printf("^1Failed to register window class\n");
        return qfalse;
    }
    
    // Calculate window size including borders
    RECT rect = {0, 0, width, height};
    DWORD style = fullscreen ? WS_POPUP : WS_OVERLAPPEDWINDOW;
    AdjustWindowRect(&rect, style, FALSE);
    
    // Convert title to wide char
    wchar_t wideTitle[256];
    MultiByteToWideChar(CP_UTF8, 0, title, -1, wideTitle, 256);
    
    // Create window
    window.handle.win32.hwnd = CreateWindowExW(
        0, L"Quake3e_PBR", wideTitle, style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        NULL, NULL, GetModuleHandle(NULL), NULL
    );
    
    if (!window.handle.win32.hwnd) {
        Com_Printf("^1Failed to create window\n");
        return qfalse;
    }
    
    window.handle.win32.hdc = GetDC(window.handle.win32.hwnd);
    ShowWindow(window.handle.win32.hwnd, SW_SHOW);
    SetForegroundWindow(window.handle.win32.hwnd);
    SetFocus(window.handle.win32.hwnd);
    
#elif defined(PLATFORM_LINUX)
    // X11 window creation
    window.handle.x11.display = XOpenDisplay(NULL);
    if (!window.handle.x11.display) {
        Com_Printf("^1Failed to open X display\n");
        return qfalse;
    }
    
    int screen = DefaultScreen(window.handle.x11.display);
    Window root = RootWindow(window.handle.x11.display, screen);
    
    XSetWindowAttributes attrs = {0};
    attrs.background_pixel = BlackPixel(window.handle.x11.display, screen);
    attrs.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask | 
                      ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                      StructureNotifyMask | FocusChangeMask;
    
    window.handle.x11.window = XCreateWindow(
        window.handle.x11.display, root,
        0, 0, width, height, 0,
        DefaultDepth(window.handle.x11.display, screen),
        InputOutput,
        DefaultVisual(window.handle.x11.display, screen),
        CWBackPixel | CWEventMask,
        &attrs
    );
    
    // Set window title
    XStoreName(window.handle.x11.display, window.handle.x11.window, title);
    
    // Handle window close button
    window.handle.x11.wmDeleteWindow = XInternAtom(window.handle.x11.display, 
                                                   "WM_DELETE_WINDOW", False);
    XSetWMProtocols(window.handle.x11.display, window.handle.x11.window, 
                   &window.handle.x11.wmDeleteWindow, 1);
    
    XMapWindow(window.handle.x11.display, window.handle.x11.window);
    XFlush(window.handle.x11.display);
    
#elif defined(PLATFORM_ANDROID)
    // Android window is provided by the system
    if (!androidApp || !androidApp->window) {
        Com_Printf("^1Android native window not available\n");
        return qfalse;
    }
    
    window.handle.android.nativeWindow = androidApp->window;
    window.handle.android.activity = androidApp->activity;
    
    // Get window dimensions
    window.width = ANativeWindow_getWidth(androidApp->window);
    window.height = ANativeWindow_getHeight(androidApp->window);
    
#endif
    
    // Store window handle for Vulkan surface creation
    vkPlatform.platformData = window.handle;
    
    return qtrue;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/platform/threading-concurrency">Threading and Concurrency</a></li>
        <li><a href="/platform/mobile-console">Mobile and Console Ports</a></li>
        <li><a href="/rendering/vulkan">Vulkan Renderer</a></li>
        <li><a href="/rendering/pbr">PBR Implementation</a></li>
        <li><a href="/modernization/build-systems">Build Systems</a></li>
    </ul>
</div>