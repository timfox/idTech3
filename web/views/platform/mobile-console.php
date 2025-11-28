<?php
/**
 * Mobile and Console Ports - Platform-Specific Considerations
 */
$title = 'Mobile and Console Ports - id Tech 3 Documentation';
$breadcrumbs = [
    '/platform' => 'Platform and Deployment',
    '/platform/mobile-console' => 'Mobile and Console Ports'
];
?>

<h1>Mobile and Console Ports - Platform-Specific Deployment</h1>

<div class="section">
    <h2>Overview</h2>
    <p>Deploying JKSunny's PBR port to mobile and console platforms requires careful optimization and platform-specific adaptations. This guide covers practical implementations for Android, iOS, Nintendo Switch, and Steam Deck deployment with Vulkan and PBR rendering.</p>
    
    <div class="feature-list">
        <h3>Platform-Specific Challenges</h3>
        <ul>
            <li><strong>Mobile Constraints:</strong> Limited memory, thermal throttling, touch controls</li>
            <li><strong>Console Requirements:</strong> Certification processes, platform APIs, specific hardware</li>
            <li><strong>PBR Scaling:</strong> Quality settings adaptation for different performance targets</li>
            <li><strong>Vulkan Compatibility:</strong> Driver variations and extension support</li>
        </ul>
    </div>
</div>

<div class="section">
    <h2>Android Deployment</h2>
    
    <h3>Android NDK Integration</h3>
    <div class="code-block">
        <pre><code>// android_main.c - Android native activity integration for PBR port
#include <android/native_activity.h>
#include <android/asset_manager.h>
#include <android/configuration.h>
#include <android/looper.h>
#include <android/sensor.h>

typedef struct androidApp_s {
    ANativeActivity* activity;
    AConfiguration* config;
    AAssetManager* assetManager;
    ANativeWindow* window;
    ALooper* looper;
    
    // Input handling
    AInputQueue* inputQueue;
    ASensorManager* sensorManager;
    ASensorEventQueue* sensorEventQueue;
    
    // Application state
    int32_t activityState;
    qboolean focused;
    qboolean hasWindow;
    
    // Display properties
    int32_t width, height;
    int32_t density;
    int32_t orientation;
    
    // Performance monitoring
    qboolean thermalThrottled;
    float cpuUsage;
    float gpuUsage;
    
} androidApp_t;

static androidApp_t* g_app = NULL;

// Android activity lifecycle callbacks
void android_main(struct android_app* state) {
    g_app = (androidApp_t*)state;
    
    // Initialize Android-specific systems
    Android_InitializeLogging();
    Android_InitializeAssets();
    Android_InitializeInput();
    Android_InitializePerformanceMonitoring();
    
    // Set up Vulkan for Android
    if (!VK_Android_Initialize()) {
        ALOGE("Failed to initialize Vulkan on Android");
        return;
    }
    
    // Initialize engine with Android-specific paths
    Com_Init_Android();
    
    // Main loop
    while (!g_app->destroyRequested) {
        // Poll Android events
        Android_PollEvents();
        
        // Update performance monitoring
        Android_UpdatePerformanceMetrics();
        
        // Run frame if we have focus and window
        if (g_app->focused && g_app->hasWindow) {
            Com_Frame();
            
            // Handle thermal throttling
            if (g_app->thermalThrottled) {
                Android_ReduceQuality();
            }
        }
    }
    
    // Cleanup
    Com_Shutdown();
    Android_Cleanup();
}

qboolean VK_Android_Initialize(void) {
    // Load Vulkan library dynamically
    void* vulkanLib = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!vulkanLib) {
        ALOGE("Failed to load libvulkan.so");
        return qfalse;
    }
    
    // Get Vulkan function pointers
    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = 
        (PFN_vkGetInstanceProcAddr)dlsym(vulkanLib, "vkGetInstanceProcAddr");
    
    if (!vkGetInstanceProcAddr) {
        ALOGE("Failed to get vkGetInstanceProcAddr");
        return qfalse;
    }
    
    // Check for required Android extensions
    uint32_t extensionCount;
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);
    
    VkExtensionProperties* extensions = malloc(sizeof(VkExtensionProperties) * extensionCount);
    vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, extensions);
    
    qboolean androidSurfaceSupported = qfalse;
    for (uint32_t i = 0; i < extensionCount; i++) {
        if (strcmp(extensions[i].extensionName, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME) == 0) {
            androidSurfaceSupported = qtrue;
            break;
        }
    }
    
    free(extensions);
    
    if (!androidSurfaceSupported) {
        ALOGE("VK_KHR_android_surface not supported");
        return qfalse;
    }
    
    return qtrue;
}

// Android-specific asset loading
qboolean Android_LoadAsset(const char* filename, void** data, size_t* size) {
    if (!g_app || !g_app->assetManager) {
        return qfalse;
    }
    
    AAsset* asset = AAssetManager_open(g_app->assetManager, filename, AASSET_MODE_BUFFER);
    if (!asset) {
        ALOGW("Failed to open asset: %s", filename);
        return qfalse;
    }
    
    *size = AAsset_getLength(asset);
    *data = malloc(*size);
    
    if (AAsset_read(asset, *data, *size) != *size) {
        ALOGE("Failed to read asset: %s", filename);
        free(*data);
        AAsset_close(asset);
        return qfalse;
    }
    
    AAsset_close(asset);
    return qtrue;
}

// Android-specific PBR quality settings
typedef struct androidPBRSettings_s {
    // Texture quality
    int maxTextureSize;        // 1024, 2048, 4096
    qboolean useASTC;          // ASTC compression
    qboolean generateMipmaps;  // Mipmap generation
    
    // Lighting quality
    int shadowMapSize;         // 512, 1024, 2048
    int maxShadowCascades;     // 1, 2, 4
    qboolean useIBL;           // Image-based lighting
    int iblResolution;         // 128, 256, 512
    
    // Rendering quality
    int renderScale;           // 50%, 75%, 100%
    qboolean useBloom;
    qboolean useSSAO;
    qboolean useTAA;           // Temporal anti-aliasing
    
    // Performance targets
    int targetFPS;             // 30, 60
    float thermalLimit;        // 0.7 = reduce quality at 70% thermal
    
} androidPBRSettings_t;

void Android_AdaptPBRSettings(androidPBRSettings_t* settings) {
    // Get device capabilities
    char manufacturer[PROP_VALUE_MAX];
    char model[PROP_VALUE_MAX];
    
    __system_property_get("ro.product.manufacturer", manufacturer);
    __system_property_get("ro.product.model", model);
    
    // Detect device tier
    int deviceTier = Android_GetDeviceTier(manufacturer, model);
    
    switch (deviceTier) {
    case DEVICE_TIER_LOW:
        // Budget devices
        settings->maxTextureSize = 1024;
        settings->shadowMapSize = 512;
        settings->maxShadowCascades = 1;
        settings->useIBL = qfalse;
        settings->renderScale = 50;
        settings->useBloom = qfalse;
        settings->useSSAO = qfalse;
        settings->useTAA = qfalse;
        settings->targetFPS = 30;
        break;
        
    case DEVICE_TIER_MID:
        // Mid-range devices
        settings->maxTextureSize = 2048;
        settings->shadowMapSize = 1024;
        settings->maxShadowCascades = 2;
        settings->useIBL = qtrue;
        settings->iblResolution = 256;
        settings->renderScale = 75;
        settings->useBloom = qtrue;
        settings->useSSAO = qfalse;
        settings->useTAA = qtrue;
        settings->targetFPS = 60;
        break;
        
    case DEVICE_TIER_HIGH:
        // Flagship devices
        settings->maxTextureSize = 4096;
        settings->shadowMapSize = 2048;
        settings->maxShadowCascades = 4;
        settings->useIBL = qtrue;
        settings->iblResolution = 512;
        settings->renderScale = 100;
        settings->useBloom = qtrue;
        settings->useSSAO = qtrue;
        settings->useTAA = qtrue;
        settings->targetFPS = 60;
        break;
    }
    
    // Apply thermal throttling settings
    settings->thermalLimit = 0.8f;  // Reduce quality at 80% thermal
}

// Thermal throttling management
void Android_UpdatePerformanceMetrics(void) {
    static int lastUpdate = 0;
    int currentTime = Sys_Milliseconds();
    
    if (currentTime - lastUpdate < 1000) {
        return; // Update every second
    }
    
    // Check thermal state (Android 7.0+)
    #ifdef __ANDROID_API_24__
    if (android_get_device_api_level() >= 24) {
        int thermalState = AThermo_getCurrentThermalStatus();
        
        switch (thermalState) {
        case ATHERMO_STATUS_NONE:
        case ATHERMO_STATUS_LIGHT:
            g_app->thermalThrottled = qfalse;
            break;
        case ATHERMO_STATUS_MODERATE:
        case ATHERMO_STATUS_SEVERE:
        case ATHERMO_STATUS_CRITICAL:
            g_app->thermalThrottled = qtrue;
            break;
        }
    }
    #endif
    
    lastUpdate = currentTime;
}

void Android_ReduceQuality(void) {
    static int reductionLevel = 0;
    
    if (g_app->thermalThrottled && reductionLevel < 3) {
        reductionLevel++;
        
        switch (reductionLevel) {
        case 1:
            // Reduce resolution by 25%
            Cvar_Set("r_customwidth", va("%d", (int)(glConfig.vidWidth * 0.75f)));
            Cvar_Set("r_customheight", va("%d", (int)(glConfig.vidHeight * 0.75f)));
            break;
        case 2:
            // Disable expensive effects
            Cvar_Set("r_bloom", "0");
            Cvar_Set("r_ssao", "0");
            break;
        case 3:
            // Reduce texture quality
            Cvar_Set("r_picmip", "2");
            break;
        }
        
        ALOGI("Thermal throttling: reduced quality level %d", reductionLevel);
    } else if (!g_app->thermalThrottled && reductionLevel > 0) {
        // Gradually restore quality when thermal state improves
        reductionLevel = max(0, reductionLevel - 1);
        
        if (reductionLevel == 0) {
            // Restore original settings
            Android_RestoreQuality();
        }
    }
}</code></pre>
    </div>
    
    <h3>Android Build Configuration</h3>
    <div class="code-block">
        <pre><code># Android.mk - NDK build configuration for PBR port
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := quake3e_pbr
LOCAL_CFLAGS := -DANDROID -DUSE_VULKAN -DUSE_PBR -O3 -ffast-math
LOCAL_CPPFLAGS := -std=c++17 -fexceptions -frtti

# Source files
LOCAL_SRC_FILES := \
    android_main.c \
    android_input.c \
    android_sound.c \
    android_vulkan.c \
    ../shared/bg_misc.c \
    ../shared/bg_pmove.c \
    ../shared/q_math.c \
    ../shared/q_shared.c \
    ../client/cl_cgame.c \
    ../client/cl_cin.c \
    ../client/cl_console.c \
    ../client/cl_input.c \
    ../client/cl_keys.c \
    ../client/cl_main.c \
    ../client/cl_net_chan.c \
    ../client/cl_parse.c \
    ../client/cl_scrn.c \
    ../client/cl_ui.c \
    ../client/snd_dma.c \
    ../client/snd_mem.c \
    ../client/snd_mix.c \
    ../qcommon/cmd.c \
    ../qcommon/common.c \
    ../qcommon/cvar.c \
    ../qcommon/files.c \
    ../qcommon/huffman.c \
    ../qcommon/md4.c \
    ../qcommon/msg.c \
    ../qcommon/net_chan.c \
    ../qcommon/unzip.c \
    ../renderer/tr_animation.c \
    ../renderer/tr_backend.c \
    ../renderer/tr_bsp.c \
    ../renderer/tr_cmds.c \
    ../renderer/tr_curve.c \
    ../renderer/tr_flares.c \
    ../renderer/tr_font.c \
    ../renderer/tr_image.c \
    ../renderer/tr_init.c \
    ../renderer/tr_light.c \
    ../renderer/tr_main.c \
    ../renderer/tr_marks.c \
    ../renderer/tr_mesh.c \
    ../renderer/tr_model.c \
    ../renderer/tr_noise.c \
    ../renderer/tr_scene.c \
    ../renderer/tr_shade.c \
    ../renderer/tr_shade_calc.c \
    ../renderer/tr_shader.c \
    ../renderer/tr_shadows.c \
    ../renderer/tr_sky.c \
    ../renderer/tr_surface.c \
    ../renderer/tr_world.c \
    ../renderer/tr_vulkan.c \
    ../renderer/tr_pbr.c

# Include paths
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../shared \
    $(LOCAL_PATH)/../client \
    $(LOCAL_PATH)/../qcommon \
    $(LOCAL_PATH)/../renderer \
    $(LOCAL_PATH)/vulkan/include \
    $(LOCAL_PATH)/astc/include

# Libraries
LOCAL_LDLIBS := -llog -landroid -lEGL -lGLESv3 -lOpenSLES -lvulkan
LOCAL_STATIC_LIBRARIES := android_native_app_glue astc_codec

# Enable Vulkan validation layers in debug builds
ifeq ($(APP_OPTIM),debug)
    LOCAL_CFLAGS += -DVULKAN_DEBUG
endif

include $(BUILD_SHARED_LIBRARY)

# ASTC texture compression library
include $(CLEAR_VARS)
LOCAL_MODULE := astc_codec
LOCAL_SRC_FILES := astc/astc_codec.c
include $(BUILD_STATIC_LIBRARY)

$(call import-module,android/native_app_glue)</code></pre>
    </div>
</div>

<div class="section">
    <h2>iOS Deployment</h2>
    
    <h3>iOS Metal/Vulkan Integration</h3>
    <div class="code-block">
        <pre><code>// ios_main.m - iOS integration with MoltenVK for PBR rendering
#import <UIKit/UIKit.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>

@interface GameViewController : UIViewController <MTKViewDelegate>
@property (nonatomic, strong) MTKView *mtkView;
@property (nonatomic, strong) id<MTLDevice> device;
@property (nonatomic, assign) BOOL gameInitialized;
@end

@implementation GameViewController

- (void)viewDidLoad {
    [super viewDidLoad];
    
    // Initialize Metal device
    self.device = MTLCreateSystemDefaultDevice();
    if (!self.device) {
        NSLog(@"Metal is not supported on this device");
        return;
    }
    
    // Create Metal view
    self.mtkView = [[MTKView alloc] initWithFrame:self.view.bounds device:self.device];
    self.mtkView.delegate = self;
    self.mtkView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    self.mtkView.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
    self.mtkView.preferredFramesPerSecond = 60;
    
    // Enable HDR if supported (iPhone X and newer)
    if (@available(iOS 16.0, *)) {
        if ([self.device supportsFamily:MTLGPUFamilyApple4]) {
            self.mtkView.colorPixelFormat = MTLPixelFormatRGBA16Float;
            self.mtkView.wantsExtendedDynamicRangeContent = YES;
        }
    }
    
    [self.view addSubview:self.mtkView];
    
    // Initialize MoltenVK
    [self initializeMoltenVK];
    
    // Initialize game engine
    [self initializeGame];
}

- (void)initializeMoltenVK {
    // MoltenVK configuration for optimal performance
    setenv("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS", "1", 1);
    setenv("MVK_CONFIG_SHADER_CONVERSION_FLIP_VERTEX_Y", "1", 1);
    setenv("MVK_CONFIG_SYNCHRONOUS_QUEUE_SUBMITS", "0", 1);
    setenv("MVK_CONFIG_PREFILL_METAL_COMMAND_BUFFERS", "1", 1);
    
    // Performance settings for iOS
    setenv("MVK_CONFIG_PERFORMANCE_TRACKING", "0", 1);  // Disable in release
    setenv("MVK_CONFIG_ACTIVITY_PERFORMANCE_LOGGING_STYLE", "0", 1);
    
    // Memory management
    setenv("MVK_CONFIG_SHOULD_MAXIMIZE_CONCURRENT_COMPILATION", "1", 1);
    setenv("MVK_CONFIG_FAST_MATH_ENABLED", "1", 1);
}

- (void)initializeGame {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
        // Get iOS-specific paths
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, 
                                                            NSUserDomainMask, YES);
        NSString *documentsPath = [paths objectAtIndex:0];
        NSString *bundlePath = [[NSBundle mainBundle] resourcePath];
        
        // Set up engine paths
        const char* docs = [documentsPath UTF8String];
        const char* bundle = [bundlePath UTF8String];
        
        iOS_SetPaths(docs, bundle);
        
        // Initialize engine with iOS-specific configuration
        Com_Init_iOS();
        
        dispatch_async(dispatch_get_main_queue(), ^{
            self.gameInitialized = YES;
        });
    });
}

// MTKViewDelegate methods
- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
    // Handle screen rotation and size changes
    if (self.gameInitialized) {
        iOS_HandleSizeChange(size.width, size.height);
    }
}

- (void)drawInMTKView:(MTKView *)view {
    if (!self.gameInitialized) {
        return;
    }
    
    @autoreleasepool {
        // Update iOS-specific systems
        iOS_UpdatePerformanceMetrics();
        iOS_HandleMemoryWarnings();
        
        // Run game frame
        Com_Frame();
    }
}

@end

// iOS-specific C interface functions
void iOS_SetPaths(const char* documentsPath, const char* bundlePath) {
    // Set up file system paths for iOS sandbox
    Cvar_Set("fs_homepath", documentsPath);
    Cvar_Set("fs_basepath", bundlePath);
    
    // Create necessary directories
    char configPath[MAX_OSPATH];
    Com_sprintf(configPath, sizeof(configPath), "%s/baseq3", documentsPath);
    Sys_Mkdir(configPath);
}

void iOS_HandleSizeChange(float width, float height) {
    // Update screen resolution
    Cvar_SetValue("r_customwidth", width);
    Cvar_SetValue("r_customheight", height);
    Cvar_Set("r_mode", "-1");  // Custom resolution mode
    
    // Force renderer restart to apply new resolution
    Cbuf_AddText("vid_restart\n");
}

void iOS_UpdatePerformanceMetrics(void) {
    static int lastUpdate = 0;
    int currentTime = Sys_Milliseconds();
    
    if (currentTime - lastUpdate < 1000) {
        return;
    }
    
    // Monitor memory usage (iOS aggressively manages memory)
    vm_size_t memoryUsage = iOS_GetMemoryUsage();
    vm_size_t memoryLimit = iOS_GetMemoryLimit();
    
    float memoryRatio = (float)memoryUsage / memoryLimit;
    
    if (memoryRatio > 0.8f) {
        // Approaching memory limit, reduce quality
        iOS_ReduceMemoryUsage();
    }
    
    lastUpdate = currentTime;
}

vm_size_t iOS_GetMemoryUsage(void) {
    struct task_basic_info info;
    mach_msg_type_number_t size = sizeof(info);
    kern_return_t kerr = task_info(mach_task_self(), TASK_BASIC_INFO, 
                                  (task_info_t)&info, &size);
    
    if (kerr == KERN_SUCCESS) {
        return info.resident_size;
    }
    
    return 0;
}

void iOS_ReduceMemoryUsage(void) {
    // Progressive memory reduction strategies
    static int reductionLevel = 0;
    
    if (reductionLevel < 4) {
        switch (reductionLevel) {
        case 0:
            // Reduce texture cache
            R_PurgeUnusedTextures();
            break;
        case 1:
            // Reduce texture quality
            Cvar_Set("r_picmip", "1");
            break;
        case 2:
            // Disable expensive post-processing
            Cvar_Set("r_bloom", "0");
            Cvar_Set("r_ssao", "0");
            break;
        case 3:
            // Reduce shadow quality
            Cvar_Set("r_shadowMapSize", "512");
            break;
        }
        
        reductionLevel++;
        NSLog(@"iOS memory warning: reduced quality level %d", reductionLevel);
    }
}

// iOS PBR quality profiles
typedef struct iosPBRProfile_s {
    const char* name;
    int textureSize;
    int shadowMapSize;
    qboolean useHDR;
    qboolean useIBL;
    qboolean useBloom;
    qboolean useSSAO;
    int targetFPS;
} iosPBRProfile_t;

static iosPBRProfile_t iosPBRProfiles[] = {
    // iPhone SE, older devices
    {"Low", 1024, 512, qfalse, qfalse, qfalse, qfalse, 30},
    
    // iPhone 12, 13 base models
    {"Medium", 2048, 1024, qtrue, qtrue, qtrue, qfalse, 60},
    
    // iPhone 13 Pro, 14 Pro with ProMotion
    {"High", 4096, 2048, qtrue, qtrue, qtrue, qtrue, 120},
    
    // iPhone 14 Pro Max, 15 Pro with A17 Pro
    {"Ultra", 4096, 4096, qtrue, qtrue, qtrue, qtrue, 120}
};

void iOS_SelectPBRProfile(void) {
    // Detect device capabilities
    NSString* deviceModel = [[UIDevice currentDevice] model];
    NSString* systemVersion = [[UIDevice currentDevice] systemVersion];
    
    // Get GPU family for capability detection
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    int profileIndex = 0;  // Default to low
    
    if (@available(iOS 13.0, *)) {
        if ([device supportsFamily:MTLGPUFamilyApple7]) {
            // A15 Bionic and newer
            profileIndex = 3;  // Ultra
        } else if ([device supportsFamily:MTLGPUFamilyApple6]) {
            // A14 Bionic
            profileIndex = 2;  // High
        } else if ([device supportsFamily:MTLGPUFamilyApple5]) {
            // A13 Bionic
            profileIndex = 1;  // Medium
        }
    }
    
    // Apply selected profile
    iosPBRProfile_t* profile = &iosPBRProfiles[profileIndex];
    
    Cvar_SetValue("r_maxTextureSize", profile->textureSize);
    Cvar_SetValue("r_shadowMapSize", profile->shadowMapSize);
    Cvar_SetValue("r_hdr", profile->useHDR ? 1 : 0);
    Cvar_SetValue("r_ibl", profile->useIBL ? 1 : 0);
    Cvar_SetValue("r_bloom", profile->useBloom ? 1 : 0);
    Cvar_SetValue("r_ssao", profile->useSSAO ? 1 : 0);
    Cvar_SetValue("com_maxfps", profile->targetFPS);
    
    NSLog(@"Selected iOS PBR profile: %s", profile->name);
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Steam Deck Optimization</h2>
    
    <h3>Steam Deck Specific Adaptations</h3>
    <div class="code-block">
        <pre><code>// steamdeck.c - Steam Deck specific optimizations for PBR port
#include "steamdeck.h"

typedef struct steamDeckConfig_s {
    // Hardware detection
    qboolean isSteamDeck;
    qboolean isDocked;
    
    // Performance profiles
    enum {
        STEAMDECK_PROFILE_BATTERY,    // 30 FPS, lower quality
        STEAMDECK_PROFILE_BALANCED,   // 40 FPS, medium quality  
        STEAMDECK_PROFILE_PERFORMANCE // 60 FPS, high quality
    } profile;
    
    // Display settings
    int nativeWidth;         // 1280
    int nativeHeight;        // 800
    int refreshRate;         // 60Hz or 40Hz
    
    // Power management
    float batteryLevel;
    qboolean powerSaveMode;
    int thermalState;
    
    // Controller settings
    qboolean useGyroscope;
    qboolean useTrackpads;
    qboolean useBackPaddles;
    
} steamDeckConfig_t;

static steamDeckConfig_t steamDeck;

qboolean SteamDeck_Detect(void) {
    // Check for Steam Deck hardware identifiers
    FILE* f = fopen("/sys/devices/virtual/dmi/id/product_name", "r");
    if (f) {
        char product[256];
        if (fgets(product, sizeof(product), f)) {
            if (strstr(product, "Jupiter") || strstr(product, "Steam Deck")) {
                steamDeck.isSteamDeck = qtrue;
                Com_Printf("Steam Deck detected\n");
            }
        }
        fclose(f);
    }
    
    if (!steamDeck.isSteamDeck) {
        return qfalse;
    }
    
    // Initialize Steam Deck specific settings
    steamDeck.nativeWidth = 1280;
    steamDeck.nativeHeight = 800;
    steamDeck.refreshRate = 60;
    steamDeck.profile = STEAMDECK_PROFILE_BALANCED;
    
    // Check if docked (external display connected)
    steamDeck.isDocked = SteamDeck_CheckDockedMode();
    
    return qtrue;
}

void SteamDeck_InitializePBRSettings(void) {
    if (!steamDeck.isSteamDeck) {
        return;
    }
    
    Com_Printf("Configuring PBR settings for Steam Deck\n");
    
    // Base Steam Deck PBR configuration
    Cvar_Set("r_customwidth", "1280");
    Cvar_Set("r_customheight", "800");
    Cvar_Set("r_mode", "-1");
    
    // Apply profile-specific settings
    SteamDeck_ApplyProfile(steamDeck.profile);
    
    // Steam Deck specific optimizations
    Cvar_Set("r_fsr", "1");              // Enable FidelityFX Super Resolution
    Cvar_Set("r_fsr_sharpening", "0.2"); // Moderate sharpening
    Cvar_Set("r_vrs", "1");              // Variable Rate Shading
    Cvar_Set("r_asyncCompute", "1");     // Async compute for AMD GPU
    
    // Optimize for AMD RDNA2 architecture
    Cvar_Set("r_amdOptimized", "1");
    Cvar_Set("r_preferredFormat", "B8G8R8A8_UNORM"); // AMD preferred format
}

void SteamDeck_ApplyProfile(int profile) {
    switch (profile) {
    case STEAMDECK_PROFILE_BATTERY:
        // Battery saving profile (2-4 hours gameplay)
        Com_Printf("Applying Steam Deck Battery profile\n");
        
        Cvar_Set("com_maxfps", "30");
        Cvar_Set("r_renderScale", "80");     // 80% render scale with FSR upscaling
        Cvar_Set("r_shadowMapSize", "1024");
        Cvar_Set("r_maxShadowCascades", "2");
        Cvar_Set("r_bloom", "0");
        Cvar_Set("r_ssao", "0");
        Cvar_Set("r_taa", "1");              // TAA helps with lower render scale
        Cvar_Set("r_picmip", "1");
        Cvar_Set("r_iblResolution", "256");
        
        // GPU frequency limiting
        SteamDeck_SetGPUClockLimit(800);     // 800 MHz instead of 1600 MHz
        break;
        
    case STEAMDECK_PROFILE_BALANCED:
        // Balanced profile (1.5-2.5 hours gameplay)
        Com_Printf("Applying Steam Deck Balanced profile\n");
        
        Cvar_Set("com_maxfps", "40");        // 40 FPS sweet spot for Steam Deck
        Cvar_Set("r_renderScale", "90");
        Cvar_Set("r_shadowMapSize", "1024");
        Cvar_Set("r_maxShadowCascades", "3");
        Cvar_Set("r_bloom", "1");
        Cvar_Set("r_ssao", "1");
        Cvar_Set("r_taa", "1");
        Cvar_Set("r_picmip", "0");
        Cvar_Set("r_iblResolution", "512");
        
        // Moderate GPU frequency
        SteamDeck_SetGPUClockLimit(1200);
        break;
        
    case STEAMDECK_PROFILE_PERFORMANCE:
        // Performance profile (1-1.5 hours gameplay)
        Com_Printf("Applying Steam Deck Performance profile\n");
        
        Cvar_Set("com_maxfps", "60");
        Cvar_Set("r_renderScale", "100");
        Cvar_Set("r_shadowMapSize", "2048");
        Cvar_Set("r_maxShadowCascades", "4");
        Cvar_Set("r_bloom", "1");
        Cvar_Set("r_ssao", "1");
        Cvar_Set("r_taa", "1");
        Cvar_Set("r_picmip", "0");
        Cvar_Set("r_iblResolution", "512");
        
        // Maximum GPU performance
        SteamDeck_SetGPUClockLimit(1600);
        break;
    }
    
    steamDeck.profile = profile;
}

void SteamDeck_UpdatePerformanceMetrics(void) {
    static int lastUpdate = 0;
    int currentTime = Sys_Milliseconds();
    
    if (currentTime - lastUpdate < 5000) {
        return; // Update every 5 seconds
    }
    
    // Read battery level
    steamDeck.batteryLevel = SteamDeck_GetBatteryLevel();
    
    // Check thermal state
    steamDeck.thermalState = SteamDeck_GetThermalState();
    
    // Auto-adjust profile based on conditions
    if (steamDeck.batteryLevel < 0.2f && steamDeck.profile != STEAMDECK_PROFILE_BATTERY) {
        Com_Printf("Low battery detected, switching to battery profile\n");
        SteamDeck_ApplyProfile(STEAMDECK_PROFILE_BATTERY);
    } else if (steamDeck.thermalState > 80 && steamDeck.profile == STEAMDECK_PROFILE_PERFORMANCE) {
        Com_Printf("High temperature detected, reducing performance\n");
        SteamDeck_ApplyProfile(STEAMDECK_PROFILE_BALANCED);
    }
    
    lastUpdate = currentTime;
}

float SteamDeck_GetBatteryLevel(void) {
    FILE* f = fopen("/sys/class/power_supply/BAT1/capacity", "r");
    if (f) {
        int capacity;
        if (fscanf(f, "%d", &capacity) == 1) {
            fclose(f);
            return capacity / 100.0f;
        }
        fclose(f);
    }
    return 1.0f; // Assume full battery if can't read
}

int SteamDeck_GetThermalState(void) {
    FILE* f = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (f) {
        int temp;
        if (fscanf(f, "%d", &temp) == 1) {
            fclose(f);
            return temp / 1000; // Convert millidegrees to degrees
        }
        fclose(f);
    }
    return 50; // Assume normal temperature
}

void SteamDeck_SetGPUClockLimit(int clockMHz) {
    // Use Steam Deck's power management interface
    char command[256];
    Com_sprintf(command, sizeof(command), 
               "echo %d > /sys/class/drm/card0/device/pp_od_clk_voltage", clockMHz);
    
    // Note: This requires root access or proper permissions
    // In practice, Steam's power management handles this
    system(command);
}

// Steam Deck controller optimization
void SteamDeck_InitializeControls(void) {
    if (!steamDeck.isSteamDeck) {
        return;
    }
    
    // Enable Steam Deck specific input features
    Cvar_Set("in_steamdeck", "1");
    Cvar_Set("in_gyroscope", "1");
    Cvar_Set("in_trackpads", "1");
    Cvar_Set("in_backpaddles", "1");
    
    // Optimize control scheme for handheld
    Cvar_Set("cl_mousesensitivity", "3.0");  // Higher sensitivity for trackpads
    Cvar_Set("cl_gyrosensitivity", "1.5");   // Gyroscope fine-tuning
    
    // Steam Input API integration
    SteamDeck_InitializeSteamInput();
}

// FSR (FidelityFX Super Resolution) integration for Steam Deck
qboolean SteamDeck_InitializeFSR(void) {
    // FSR 2.0 integration for better upscaling quality
    if (!VK_CheckExtensionSupport("VK_AMD_fsr")) {
        Com_Printf("FSR extension not available\n");
        return qfalse;
    }
    
    // Create FSR context
    FsrContextDescription contextDesc = {
        .maxRenderSize = {1280, 800},
        .displaySize = {1280, 800},
        .flags = FSR_ENABLE_HIGH_DYNAMIC_RANGE | FSR_ENABLE_AUTO_EXPOSURE
    };
    
    if (ffxFsrContextCreate(&fsrContext, &contextDesc) != FFX_OK) {
        Com_Printf("Failed to create FSR context\n");
        return qfalse;
    }
    
    Com_Printf("FSR 2.0 initialized for Steam Deck\n");
    return qtrue;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Console Certification</h2>
    
    <h3>Console-Specific Requirements</h3>
    <div class="code-block">
        <pre><code>// console_cert.h - Console certification requirements for PBR port

// Nintendo Switch specific requirements
typedef struct switchCertification_s {
    // Performance requirements
    qboolean maintainTargetFPS;     // Must maintain 30/60 FPS
    qboolean handleSuspendResume;   // Proper suspend/resume handling
    qboolean supportHandheldDock;   // Seamless handheld/docked switching
    
    // Memory requirements  
    size_t maxMemoryUsage;          // 3.2GB limit for games
    qboolean properMemoryCleanup;   // No memory leaks
    
    // Input requirements
    qboolean supportAllControllers; // Pro Controller, Joy-Cons, etc.
    qboolean supportTouchScreen;    // Handheld touch input
    qboolean handleControllerDisconnect; // Graceful controller handling
    
    // System integration
    qboolean supportUserProfiles;   // Nintendo Account integration
    qboolean supportScreenshots;    // Built-in screenshot/video
    qboolean supportAchievements;   // Nintendo achievements system
    
} switchCertification_t;

// PlayStation/Xbox certification requirements
typedef struct consoleCertification_s {
    // Performance standards
    struct {
        int targetFPS;               // 30, 60, or 120 FPS
        float frameTimeVariance;     // < 2ms variance
        int loadingTimeMax;          // Maximum loading times
        qboolean support4K;          // 4K rendering support
        qboolean supportHDR;         // HDR10 support
    } performance;
    
    // Platform integration
    struct {
        qboolean achievements;       // Platform achievements
        qboolean cloudSaves;         // Cloud save integration
        qboolean socialFeatures;    // Friends, messaging, etc.
        qboolean accessibility;     // Accessibility features
    } platform;
    
    // Content requirements
    struct {
        qboolean ageRating;          // ESRB/PEGI rating compliance
        qboolean contentFiltering;  // Parental controls
        qboolean languageSupport;   // Localization requirements
    } content;
    
} consoleCertification_t;

// Console-specific initialization
qboolean Console_InitializePlatform(consolePlatform_t platform) {
    switch (platform) {
    case CONSOLE_NINTENDO_SWITCH:
        return Console_InitializeSwitch();
    case CONSOLE_PLAYSTATION_5:
        return Console_InitializePS5();
    case CONSOLE_XBOX_SERIES:
        return Console_InitializeXboxSeries();
    default:
        Com_Printf("^1Unknown console platform: %d\n", platform);
        return qfalse;
    }
}

qboolean Console_InitializeSwitch(void) {
    // Nintendo Switch initialization
    Com_Printf("Initializing for Nintendo Switch\n");
    
    // Set Switch-specific graphics limits
    Cvar_Set("r_maxTextureSize", "2048");    // Memory constraints
    Cvar_Set("r_shadowMapSize", "1024");     // Performance constraints
    Cvar_Set("r_maxShadowCascades", "2");    // GPU limitations
    
    // Switch supports both handheld (720p) and docked (1080p) modes
    if (Switch_IsDockedMode()) {
        Cvar_Set("r_customwidth", "1920");
        Cvar_Set("r_customheight", "1080");
        Cvar_Set("com_maxfps", "60");
    } else {
        Cvar_Set("r_customwidth", "1280");
        Cvar_Set("r_customheight", "720");
        Cvar_Set("com_maxfps", "30");        // Battery conservation
    }
    
    // Enable Nintendo-specific features
    Cvar_Set("nintendo_accounts", "1");
    Cvar_Set("nintendo_screenshots", "1");
    
    // Register dock/undock callback
    Switch_RegisterDockCallback(Console_HandleSwitchDockChange);
    
    return qtrue;
}

qboolean Console_InitializePS5(void) {
    // PlayStation 5 initialization
    Com_Printf("Initializing for PlayStation 5\n");
    
    // PS5 has powerful hardware - enable high-quality settings
    Cvar_Set("r_customwidth", "3840");      // 4K support
    Cvar_Set("r_customheight", "2160");
    Cvar_Set("r_maxTextureSize", "8192");   // High-res textures
    Cvar_Set("r_shadowMapSize", "4096");    // High-quality shadows
    Cvar_Set("r_maxShadowCascades", "4");
    Cvar_Set("r_hdr", "1");                 // HDR10 support
    Cvar_Set("r_rayTracing", "1");          // RT if supported
    Cvar_Set("com_maxfps", "60");
    
    // PS5-specific features
    Cvar_Set("ps5_dualsense", "1");         // DualSense controller features
    Cvar_Set("ps5_haptic", "1");            // Haptic feedback
    Cvar_Set("ps5_adaptive_triggers", "1"); // Adaptive triggers
    Cvar_Set("ps5_3d_audio", "1");          // Tempest 3D AudioTech
    
    // PlayStation Network integration
    Cvar_Set("psn_trophies", "1");
    Cvar_Set("psn_activities", "1");
    
    return qtrue;
}

qboolean Console_InitializeXboxSeries(void) {
    // Xbox Series X/S initialization
    Com_Printf("Initializing for Xbox Series X/S\n");
    
    // Detect Series X vs Series S
    if (Xbox_IsSeriesX()) {
        // Series X - full 4K gaming
        Cvar_Set("r_customwidth", "3840");
        Cvar_Set("r_customheight", "2160");
        Cvar_Set("r_maxTextureSize", "8192");
        Cvar_Set("r_shadowMapSize", "4096");
        Cvar_Set("com_maxfps", "60");
    } else {
        // Series S - 1440p optimized
        Cvar_Set("r_customwidth", "2560");
        Cvar_Set("r_customheight", "1440");
        Cvar_Set("r_maxTextureSize", "4096");
        Cvar_Set("r_shadowMapSize", "2048");
        Cvar_Set("com_maxfps", "60");
    }
    
    // Common Xbox Series features
    Cvar_Set("r_hdr", "1");
    Cvar_Set("r_vrs", "1");                 // Variable Rate Shading
    Cvar_Set("r_directStorage", "1");       // DirectStorage API
    Cvar_Set("xbox_smart_delivery", "1");   // Smart Delivery support
    Cvar_Set("xbox_quick_resume", "1");     // Quick Resume support
    Cvar_Set("xbox_auto_hdr", "1");         // Auto HDR if game doesn't have native HDR
    
    // Xbox Live integration
    Cvar_Set("xbox_achievements", "1");
    Cvar_Set("xbox_game_bar", "1");
    
    return qtrue;
}

// Handle platform-specific events
void Console_HandleSwitchDockChange(qboolean docked) {
    Com_Printf("Switch dock state changed: %s\n", docked ? "docked" : "handheld");
    
    if (docked) {
        // Switching to docked mode - increase quality
        Cvar_Set("r_customwidth", "1920");
        Cvar_Set("r_customheight", "1080");
        Cvar_Set("com_maxfps", "60");
        Cvar_Set("r_renderScale", "100");
    } else {
        // Switching to handheld - reduce for battery life
        Cvar_Set("r_customwidth", "1280");
        Cvar_Set("r_customheight", "720");
        Cvar_Set("com_maxfps", "30");
        Cvar_Set("r_renderScale", "90");
    }
    
    // Restart renderer with new settings
    Cbuf_AddText("vid_restart\n");
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Cross-Platform Asset Pipeline</h2>
    
    <h3>Platform-Optimized Asset Generation</h3>
    <div class="code-block">
        <pre><code>// asset_pipeline.h - Cross-platform asset optimization for PBR
typedef struct platformAssets_s {
    struct {
        textureFormat_t baseColor;
        textureFormat_t normal;
        textureFormat_t metallicRoughness;
        textureFormat_t occlusion;
        int maxResolution;
        qboolean generateMipmaps;
        qboolean useCompression;
    } textures;
    
    struct {
        audioFormat_t format;
        int sampleRate;
        int bitDepth;
        qboolean useCompression;
        float compressionQuality;
    } audio;
    
    struct {
        int maxVertices;
        int maxTriangles;
        qboolean useLOD;
        int lodLevels;
        float lodDistances[4];
    } models;
    
} platformAssets_t;

// Platform-specific asset configurations
static platformAssets_t platformConfigs[] = {
    // Android
    {
        .textures = {
            .baseColor = TEX_FORMAT_ASTC_4x4,
            .normal = TEX_FORMAT_ASTC_4x4,
            .metallicRoughness = TEX_FORMAT_ASTC_4x4,
            .occlusion = TEX_FORMAT_ASTC_4x4,
            .maxResolution = 2048,
            .generateMipmaps = qtrue,
            .useCompression = qtrue
        },
        .audio = {
            .format = AUDIO_FORMAT_OGG,
            .sampleRate = 44100,
            .bitDepth = 16,
            .useCompression = qtrue,
            .compressionQuality = 0.7f
        },
        .models = {
            .maxVertices = 32768,
            .maxTriangles = 65536,
            .useLOD = qtrue,
            .lodLevels = 3,
            .lodDistances = {50.0f, 150.0f, 500.0f, 0.0f}
        }
    },
    
    // iOS
    {
        .textures = {
            .baseColor = TEX_FORMAT_ASTC_4x4,
            .normal = TEX_FORMAT_ASTC_4x4,
            .metallicRoughness = TEX_FORMAT_ASTC_4x4,
            .occlusion = TEX_FORMAT_ASTC_4x4,
            .maxResolution = 4096,
            .generateMipmaps = qtrue,
            .useCompression = qtrue
        },
        .audio = {
            .format = AUDIO_FORMAT_AAC,
            .sampleRate = 48000,
            .bitDepth = 16,
            .useCompression = qtrue,
            .compressionQuality = 0.8f
        },
        .models = {
            .maxVertices = 65536,
            .maxTriangles = 131072,
            .useLOD = qtrue,
            .lodLevels = 4,
            .lodDistances = {25.0f, 75.0f, 200.0f, 500.0f}
        }
    },
    
    // Steam Deck
    {
        .textures = {
            .baseColor = TEX_FORMAT_BC7_UNORM,
            .normal = TEX_FORMAT_BC5_UNORM,
            .metallicRoughness = TEX_FORMAT_BC7_UNORM,
            .occlusion = TEX_FORMAT_BC4_UNORM,
            .maxResolution = 2048,
            .generateMipmaps = qtrue,
            .useCompression = qtrue
        },
        .audio = {
            .format = AUDIO_FORMAT_OGG,
            .sampleRate = 48000,
            .bitDepth = 16,
            .useCompression = qtrue,
            .compressionQuality = 0.8f
        },
        .models = {
            .maxVertices = 65536,
            .maxTriangles = 131072,
            .useLOD = qtrue,
            .lodLevels = 3,
            .lodDistances = {40.0f, 120.0f, 300.0f, 0.0f}
        }
    }
};

// Asset build pipeline
qboolean AssetPipeline_BuildForPlatform(const char* sourceDir, const char* outputDir, 
                                       platformType_t platform) {
    platformAssets_t* config = &platformConfigs[platform];
    
    Com_Printf("Building assets for platform %d\n", platform);
    
    // Process textures
    if (!AssetPipeline_ProcessTextures(sourceDir, outputDir, &config->textures)) {
        Com_Printf("^1Failed to process textures\n");
        return qfalse;
    }
    
    // Process models
    if (!AssetPipeline_ProcessModels(sourceDir, outputDir, &config->models)) {
        Com_Printf("^1Failed to process models\n");
        return qfalse;
    }
    
    // Process audio
    if (!AssetPipeline_ProcessAudio(sourceDir, outputDir, &config->audio)) {
        Com_Printf("^1Failed to process audio\n");
        return qfalse;
    }
    
    Com_Printf("Asset pipeline completed successfully\n");
    return qtrue;
}

qboolean AssetPipeline_ProcessTextures(const char* sourceDir, const char* outputDir,
                                      const textureConfig_t* config) {
    // Find all texture files
    char searchPath[MAX_OSPATH];
    Com_sprintf(searchPath, sizeof(searchPath), "%s/**/*.png", sourceDir);
    Com_sprintf(searchPath, sizeof(searchPath), "%s/**/*.jpg", sourceDir);
    Com_sprintf(searchPath, sizeof(searchPath), "%s/**/*.tga", sourceDir);
    
    // Process each texture based on type and platform requirements
    char** fileList = FS_ListFiles(searchPath, NULL, NULL);
    
    for (int i = 0; fileList[i]; i++) {
        textureType_t type = Texture_DetectType(fileList[i]);
        textureFormat_t format = Texture_SelectFormat(type, config);
        
        // Resize if necessary
        int targetWidth, targetHeight;
        Texture_CalculateTargetSize(fileList[i], config->maxResolution, 
                                   &targetWidth, &targetHeight);
        
        // Convert and compress
        if (!Texture_ConvertAndCompress(fileList[i], outputDir, format,
                                       targetWidth, targetHeight, 
                                       config->generateMipmaps)) {
            Com_Printf("^3Warning: Failed to process texture %s\n", fileList[i]);
        }
    }
    
    FS_FreeFileList(fileList);
    return qtrue;
}</code></pre>
    </div>
</div>

<div class="section">
    <h2>Related Topics</h2>
    <ul>
        <li><a href="/platform/cross-platform">Cross-Platform Development</a></li>
        <li><a href="/platform/threading-concurrency">Threading and Concurrency</a></li>
        <li><a href="/rendering/vulkan">Vulkan Renderer</a></li>
        <li><a href="/rendering/pbr">PBR Implementation</a></li>
        <li><a href="/modernization/build-systems">Build Systems</a></li>
    </ul>
</div>