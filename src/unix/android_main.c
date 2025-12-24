#include "../common/qcommon.h"
#include "../client/client.h"
#include "../renderercommon/tr_public.h"
#include "android_platform.h"
#include <jni.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <memory>
#include <thread>
#include <functional>
#include <atomic>
#include <array>
#include <string_view>
#include <mutex>
#include <condition_variable>

// Android logging with C++23 features
namespace android {
    constexpr std::string_view LOG_TAG = "Quake3Android";

    template<typename... Args>
    constexpr void log_info(const char* fmt, Args&&... args) {
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG.data(), fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    constexpr void log_warn(const char* fmt, Args&&... args) {
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG.data(), fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    constexpr void log_error(const char* fmt, Args&&... args) {
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG.data(), fmt, std::forward<Args>(args)...);
    }
}

// Global Android app state
static struct android_app* android_app = NULL;
static ANativeWindow* android_window = NULL;
static JavaVM* android_jvm = NULL;
static jclass android_activity_class = NULL;
static jobject android_activity_object = NULL;

// Engine thread
static pthread_t engine_thread;
static volatile qboolean engine_initialized = qfalse;
static volatile qboolean engine_shutdown_requested = qfalse;

// Touch input state
#define MAX_TOUCH_POINTS 10
static struct {
    int id;
    float x, y;
    qboolean pressed;
} touch_points[MAX_TOUCH_POINTS];

// JNI method IDs
static jmethodID show_keyboard_method = NULL;
static jmethodID hide_keyboard_method = NULL;
static jmethodID vibrate_method = NULL;
static jmethodID get_display_metrics_method = NULL;

// Forward declarations
static void android_handle_cmd(struct android_app* app, int32_t cmd);
static int32_t android_handle_input(struct android_app* app, AInputEvent* event);
static void* android_engine_thread(void* arg);
static void android_init_jni(JNIEnv* env);
static void android_process_touch_events(void);

// Display metrics
static struct {
    int width_pixels;
    int height_pixels;
    float density;
    float scaled_density;
    int density_dpi;
} display_metrics;

// Initialize Android platform
qboolean Android_Initialize(struct android_app* app) {
    LOGI("Initializing Android platform support");

    android_app = app;
    android_app->onAppCmd = android_handle_cmd;
    android_app->onInputEvent = android_handle_input;

    // Initialize JNI
    android_init_jni(app->activity->env);

    // Store JVM reference
    app->activity->vm->AttachCurrentThread(&app->activity->env, NULL);
    (*app->activity->vm)->GetJavaVM(app->activity->vm, &android_jvm);

    // Get display metrics
    Android_GetDisplayMetrics();

    LOGI("Android platform initialized successfully");
    return qtrue;
}

// Shutdown Android platform
void Android_Shutdown(void) {
    LOGI("Shutting down Android platform");

    if (engine_initialized) {
        engine_shutdown_requested = qtrue;

        // Wait for engine thread to finish
        pthread_join(engine_thread, NULL);
    }

    if (android_jvm) {
        (*android_jvm)->DetachCurrentThread(android_jvm);
    }

    android_app = NULL;
    android_window = NULL;
    engine_initialized = qfalse;
}

// Main Android event loop
void Android_RunMainLoop(void) {
    LOGI("Starting Android main loop");

    while (1) {
        int ident;
        int events;
        struct android_poll_source* source;

        // Poll for events
        while ((ident = ALooper_pollAll(0, NULL, &events, (void**)&source)) >= 0) {
            if (source != NULL) {
                source->process(android_app, source);
            }

            // Check for exit conditions
            if (android_app->destroyRequested) {
                LOGI("Android app destroy requested");
                return;
            }
        }

        // Process touch input
        android_process_touch_events();

        // Small sleep to prevent busy waiting
        usleep(1000); // 1ms
    }
}

// Handle Android app commands
static void android_handle_cmd(struct android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            LOGI("Android: APP_CMD_INIT_WINDOW");
            android_window = app->window;

            if (android_window != NULL && !engine_initialized) {
                // Start engine thread
                pthread_create(&engine_thread, NULL, android_engine_thread, NULL);
            }
            break;

        case APP_CMD_TERM_WINDOW:
            LOGI("Android: APP_CMD_TERM_WINDOW");
            android_window = NULL;
            break;

        case APP_CMD_GAINED_FOCUS:
            LOGI("Android: APP_CMD_GAINED_FOCUS");
            // Resume audio, etc.
            break;

        case APP_CMD_LOST_FOCUS:
            LOGI("Android: APP_CMD_LOST_FOCUS");
            // Pause audio, etc.
            break;

        case APP_CMD_LOW_MEMORY:
            LOGI("Android: APP_CMD_LOW_MEMORY");
            // Free memory, reduce quality
            break;

        case APP_CMD_DESTROY:
            LOGI("Android: APP_CMD_DESTROY");
            Android_Shutdown();
            break;

        default:
            LOGI("Android: Unhandled command %d", cmd);
            break;
    }
}

// Handle Android input events
static int32_t android_handle_input(struct android_app* app, AInputEvent* event) {
    int32_t event_type = AInputEvent_getType(event);

    if (event_type == AINPUT_EVENT_TYPE_MOTION) {
        int32_t action = AMotionEvent_getAction(event);
        int32_t pointer_index = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
        int32_t masked_action = action & AMOTION_EVENT_ACTION_MASK;

        float x = AMotionEvent_getX(event, pointer_index);
        float y = AMotionEvent_getY(event, pointer_index);
        int id = AMotionEvent_getPointerId(event, pointer_index);

        // Convert to normalized coordinates
        float normalized_x = x / display_metrics.width_pixels;
        float normalized_y = y / display_metrics.height_pixels;

        switch (masked_action) {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
                if (id < MAX_TOUCH_POINTS) {
                    touch_points[id].id = id;
                    touch_points[id].x = normalized_x;
                    touch_points[id].y = normalized_y;
                    touch_points[id].pressed = qtrue;
                    LOGI("Touch down: id=%d, x=%.3f, y=%.3f", id, normalized_x, normalized_y);
                }
                break;

            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
                if (id < MAX_TOUCH_POINTS) {
                    touch_points[id].pressed = qfalse;
                    LOGI("Touch up: id=%d", id);
                }
                break;

            case AMOTION_EVENT_ACTION_MOVE:
                // Handle all pointers for move events
                int pointer_count = AMotionEvent_getPointerCount(event);
                for (int i = 0; i < pointer_count && i < MAX_TOUCH_POINTS; i++) {
                    int pid = AMotionEvent_getPointerId(event, i);
                    if (pid < MAX_TOUCH_POINTS && touch_points[pid].pressed) {
                        float px = AMotionEvent_getX(event, i) / display_metrics.width_pixels;
                        float py = AMotionEvent_getY(event, i) / display_metrics.height_pixels;
                        touch_points[pid].x = px;
                        touch_points[pid].y = py;
                    }
                }
                break;
        }

        return 1; // Event consumed
    }

    return 0; // Event not consumed
}

// Engine thread function
static void* android_engine_thread(void* arg) {
    LOGI("Starting engine thread");

    // Initialize engine
    char* argv[] = {"quake3e", "+set", "cl_renderer", "vulkan", "+set", "r_mode", "-1", "+set", "r_customwidth", "1920", "+set", "r_customheight", "1080"};
    int argc = sizeof(argv) / sizeof(argv[0]);

    Com_Init(argc, argv);

    engine_initialized = qtrue;
    LOGI("Engine initialized successfully");

    // Main engine loop
    while (!engine_shutdown_requested) {
        Com_Frame();
        usleep(1000); // ~60 FPS
    }

    LOGI("Engine thread shutting down");
    return NULL;
}

// Initialize JNI
static void android_init_jni(JNIEnv* env) {
    LOGI("Initializing JNI");

    // Get activity class
    jclass activity_class = (*env)->FindClass(env, "android/app/NativeActivity");
    if (activity_class == NULL) {
        LOGE("Failed to find NativeActivity class");
        return;
    }

    android_activity_class = (*env)->NewGlobalRef(env, activity_class);
    android_activity_object = (*env)->NewGlobalRef(env, android_app->activity->clazz);

    // Get method IDs for platform features
    show_keyboard_method = (*env)->GetMethodID(env, android_activity_class, "showKeyboard", "()V");
    hide_keyboard_method = (*env)->GetMethodID(env, android_activity_class, "hideKeyboard", "()V");
    vibrate_method = (*env)->GetMethodID(env, android_activity_class, "vibrate", "(I)V");
    get_display_metrics_method = (*env)->GetMethodID(env, android_activity_class, "getDisplayMetrics", "()Landroid/util/DisplayMetrics;");

    LOGI("JNI initialized successfully");
}

// Process touch events for engine
static void android_process_touch_events(void) {
    for (int i = 0; i < MAX_TOUCH_POINTS; i++) {
        if (touch_points[i].pressed) {
            // Send touch event to engine
            // This would integrate with the client's input system
            // For now, just log the events
            static int frame_count = 0;
            if (frame_count++ % 60 == 0) { // Log every ~1 second
                LOGI("Active touch: id=%d, x=%.3f, y=%.3f", touch_points[i].id, touch_points[i].x, touch_points[i].y);
            }
        }
    }
}

// Platform-specific functions
void Android_ShowKeyboard(void) {
    if (android_app && android_app->activity->env && show_keyboard_method) {
        JNIEnv* env = android_app->activity->env;
        (*env)->CallVoidMethod(env, android_activity_object, show_keyboard_method);
    }
}

void Android_HideKeyboard(void) {
    if (android_app && android_app->activity->env && hide_keyboard_method) {
        JNIEnv* env = android_app->activity->env;
        (*env)->CallVoidMethod(env, android_activity_object, hide_keyboard_method);
    }
}

void Android_Vibrate(int milliseconds) {
    if (android_app && android_app->activity->env && vibrate_method) {
        JNIEnv* env = android_app->activity->env;
        (*env)->CallVoidMethod(env, android_activity_object, vibrate_method, milliseconds);
    }
}

void Android_GetDisplayMetrics(void) {
    if (!android_app || !android_app->activity->env || !get_display_metrics_method) {
        // Fallback values
        display_metrics.width_pixels = 1920;
        display_metrics.height_pixels = 1080;
        display_metrics.density = 1.0f;
        display_metrics.scaled_density = 1.0f;
        display_metrics.density_dpi = 160;
        return;
    }

    JNIEnv* env = android_app->activity->env;
    jobject metrics = (*env)->CallObjectMethod(env, android_activity_object, get_display_metrics_method);

    if (metrics) {
        jclass metrics_class = (*env)->GetObjectClass(env, metrics);

        jfieldID width_field = (*env)->GetFieldID(env, metrics_class, "widthPixels", "I");
        jfieldID height_field = (*env)->GetFieldID(env, metrics_class, "heightPixels", "I");
        jfieldID density_field = (*env)->GetFieldID(env, metrics_class, "density", "F");
        jfieldID scaled_density_field = (*env)->GetFieldID(env, metrics_class, "scaledDensity", "F");
        jfieldID density_dpi_field = (*env)->GetFieldID(env, metrics_class, "densityDpi", "I");

        display_metrics.width_pixels = (*env)->GetIntField(env, metrics, width_field);
        display_metrics.height_pixels = (*env)->GetIntField(env, metrics, height_pixels_field);
        display_metrics.density = (*env)->GetFloatField(env, metrics, density_field);
        display_metrics.scaled_density = (*env)->GetFloatField(env, metrics, scaled_density_field);
        display_metrics.density_dpi = (*env)->GetIntField(env, metrics, density_dpi_field);

        (*env)->DeleteLocalRef(env, metrics);

        LOGI("Display metrics: %dx%d, density=%.2f, dpi=%d",
             display_metrics.width_pixels, display_metrics.height_pixels,
             display_metrics.density, display_metrics.density_dpi);
    }
}

// Get Android window for rendering
ANativeWindow* Android_GetNativeWindow(void) {
    return android_window;
}

// Check if running on Android
qboolean Android_IsRunning(void) {
    return android_app != NULL;
}

// Get platform-specific paths
const char* Android_GetDataDir(void) {
    static char data_dir[PATH_MAX] = {0};

    if (android_app && android_app->activity->internalDataPath) {
        Q_strncpyz(data_dir, android_app->activity->internalDataPath, sizeof(data_dir));
        return data_dir;
    }

    return "/sdcard/Android/data/com.quake3e/files";
}

const char* Android_GetCacheDir(void) {
    static char cache_dir[PATH_MAX] = {0};

    if (android_app && android_app->activity->internalDataPath) {
        Com_sprintf(cache_dir, sizeof(cache_dir), "%s/cache", android_app->activity->internalDataPath);
        return cache_dir;
    }

    return "/sdcard/Android/data/com.quake3e/cache";
}

// Android-specific main function
void android_main(struct android_app* app) {
    LOGI("Starting Quake 3 Android");

    // Initialize Android platform
    if (!Android_Initialize(app)) {
        LOGE("Failed to initialize Android platform");
        return;
    }

    // Run main event loop
    Android_RunMainLoop();

    LOGI("Quake 3 Android shutting down");
}
