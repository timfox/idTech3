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
    constexpr std::string_view LOG_TAG = "IdTech3Android";

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

// Modern C++23 Android platform implementation
namespace android {

    // RAII wrapper for JNI global references
    class JniGlobalRef {
    private:
        JNIEnv* env_;
        jobject ref_;

    public:
        JniGlobalRef(JNIEnv* env, jobject obj) : env_(env), ref_(env->NewGlobalRef(obj)) {}
        ~JniGlobalRef() {
            if (ref_ && env_) {
                env_->DeleteGlobalRef(ref_);
            }
        }

        JniGlobalRef(const JniGlobalRef&) = delete;
        JniGlobalRef& operator=(const JniGlobalRef&) = delete;

        JniGlobalRef(JniGlobalRef&& other) noexcept : env_(other.env_), ref_(other.ref_) {
            other.ref_ = nullptr;
        }

        JniGlobalRef& operator=(JniGlobalRef&& other) noexcept {
            if (this != &other) {
                env_ = other.env_;
                ref_ = other.ref_;
                other.ref_ = nullptr;
            }
            return *this;
        }

        jobject get() const { return ref_; }
    };

    // Touch point structure with modern C++ features
    struct TouchPoint {
        int id;
        float x, y;
        bool pressed;

        constexpr TouchPoint() : id(-1), x(0.0f), y(0.0f), pressed(false) {}
        constexpr void reset() { id = -1; pressed = false; }
    };

    // Android platform state with RAII
    class AndroidPlatform {
    private:
        struct android_app* app_ = nullptr;
        ANativeWindow* window_ = nullptr;
        JavaVM* jvm_ = nullptr;
        std::unique_ptr<JniGlobalRef> activity_ref_;
        jclass activity_class_ = nullptr;

        // Engine threading with modern C++23
        std::jthread engine_thread_;
        std::atomic<bool> engine_initialized_{false};
        std::atomic<bool> shutdown_requested_{false};

        // Touch input with fixed-size array
        static constexpr size_t MAX_TOUCH_POINTS = 10;
        std::array<TouchPoint, MAX_TOUCH_POINTS> touch_points_;

        // JNI method IDs
        jmethodID show_keyboard_method_ = nullptr;
        jmethodID hide_keyboard_method_ = nullptr;
        jmethodID vibrate_method_ = nullptr;
        jmethodID get_display_metrics_method_ = nullptr;

        // Synchronization primitives
        std::mutex touch_mutex_;
        std::condition_variable cv_;

    public:
        AndroidPlatform() = default;
        ~AndroidPlatform() { shutdown(); }

        // Deleted copy/move operations for singleton-like behavior
        AndroidPlatform(const AndroidPlatform&) = delete;
        AndroidPlatform& operator=(const AndroidPlatform&) = delete;
        AndroidPlatform(AndroidPlatform&&) = delete;
        AndroidPlatform& operator=(AndroidPlatform&&) = delete;

        // Core lifecycle methods
        bool initialize(struct android_app* app);
        void shutdown();

        // Main event loop
        void run_main_loop();

        // Platform-specific features
        void show_keyboard();
        void hide_keyboard();
        void vibrate(int milliseconds);

        // Display metrics
        android_display_metrics_t get_display_metrics();

        // Touch input processing
        void process_touch_events();

        // Window management
        ANativeWindow* get_native_window() const { return window_; }
        bool has_window() const { return window_ != nullptr; }

        // Platform detection
        bool is_running() const { return app_ != nullptr; }

        // File system paths
        std::string get_data_dir() const;
        std::string get_cache_dir() const;

    private:
        // Event handlers
        static void handle_cmd(struct android_app* app, int32_t cmd);
        static int32_t handle_input(struct android_app* app, AInputEvent* event);

        // JNI initialization
        bool init_jni(JNIEnv* env);

        // Engine thread function
        void engine_thread_func();

        // Helper methods
        void on_app_cmd(int32_t cmd);
        void on_input_event(AInputEvent* event);
    };

    // Global platform instance
    static std::unique_ptr<AndroidPlatform> g_platform;

    // Implementation of AndroidPlatform class
    bool AndroidPlatform::initialize(struct android_app* app) {
        log_info("Initializing Android platform support");

        if (!app) {
            log_error("Android app is null");
            return false;
        }

        app_ = app;
        app_->onAppCmd = handle_cmd;
        app_->onInputEvent = handle_input;
        app_->userData = this;

        // Initialize JNI
        if (!init_jni(app_->activity->env)) {
            log_error("Failed to initialize JNI");
            return false;
        }

        // Store JVM reference
        app_->activity->vm->AttachCurrentThread(&app_->activity->env, nullptr);
        app_->activity->vm->GetJavaVM(&jvm_);

        log_info("Android platform initialized successfully");
        return true;
    }

    void AndroidPlatform::shutdown() {
        log_info("Shutting down Android platform");

        if (engine_thread_.joinable()) {
            shutdown_requested_ = true;
            engine_thread_.join();
        }

        if (jvm_) {
            jvm_->DetachCurrentThread();
            jvm_ = nullptr;
        }

        window_ = nullptr;
        app_ = nullptr;
        activity_ref_.reset();

        log_info("Android platform shut down");
    }

    void AndroidPlatform::run_main_loop() {
        log_info("Starting Android main loop");

        while (true) {
            int ident;
            int events;
            struct android_poll_source* source;

            // Poll for events with timeout
            while ((ident = ALooper_pollAll(0, nullptr, &events, reinterpret_cast<void**>(&source))) >= 0) {
                if (source != nullptr) {
                    source->process(app_, source);
                }

                // Check for exit conditions
                if (app_->destroyRequested) {
                    log_info("Android app destroy requested");
                    return;
                }
            }

            // Process touch input
            process_touch_events();

            // Small sleep to prevent busy waiting
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // Static callback functions for android_app
    void AndroidPlatform::handle_cmd(struct android_app* app, int32_t cmd) {
        auto* platform = static_cast<AndroidPlatform*>(app->userData);
        if (platform) {
            platform->on_app_cmd(cmd);
        }
    }

    int32_t AndroidPlatform::handle_input(struct android_app* app, AInputEvent* event) {
        auto* platform = static_cast<AndroidPlatform*>(app->userData);
        if (platform) {
            platform->on_input_event(event);
            return 1; // Event consumed
        }
        return 0; // Event not consumed
    }

    void AndroidPlatform::on_app_cmd(int32_t cmd) {
        switch (cmd) {
            case APP_CMD_INIT_WINDOW:
                log_info("Android: APP_CMD_INIT_WINDOW");
                window_ = app_->window;

                if (window_ && !engine_initialized_) {
                    // Start engine thread
                    engine_thread_ = std::jthread([this](std::stop_token token) {
                        engine_thread_func();
                    });
                }
                break;

            case APP_CMD_TERM_WINDOW:
                log_info("Android: APP_CMD_TERM_WINDOW");
                window_ = nullptr;
                break;

            case APP_CMD_GAINED_FOCUS:
                log_info("Android: APP_CMD_GAINED_FOCUS");
                // Resume audio, etc.
                break;

            case APP_CMD_LOST_FOCUS:
                log_info("Android: APP_CMD_LOST_FOCUS");
                // Pause audio, etc.
                break;

            case APP_CMD_LOW_MEMORY:
                log_info("Android: APP_CMD_LOW_MEMORY");
                // Free memory, reduce quality
                break;

            case APP_CMD_DESTROY:
                log_info("Android: APP_CMD_DESTROY");
                shutdown_requested_ = true;
                break;

            default:
                log_info("Android: Unhandled command %d", cmd);
                break;
        }
    }

    void AndroidPlatform::on_input_event(AInputEvent* event) {
        if (!event) return;

        int32_t event_type = AInputEvent_getType(event);

        if (event_type == AINPUT_EVENT_TYPE_MOTION) {
            int32_t action = AMotionEvent_getAction(event);
            int32_t pointer_index = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >>
                                   AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
            int32_t masked_action = action & AMOTION_EVENT_ACTION_MASK;

            float x = AMotionEvent_getX(event, pointer_index);
            float y = AMotionEvent_getY(event, pointer_index);
            int id = AMotionEvent_getPointerId(event, pointer_index);

            // Convert to normalized coordinates
            android_display_metrics_t metrics = get_display_metrics();
            float normalized_x = x / static_cast<float>(metrics.width_pixels);
            float normalized_y = y / static_cast<float>(metrics.height_pixels);

            std::lock_guard<std::mutex> lock(touch_mutex_);

            switch (masked_action) {
                case AMOTION_EVENT_ACTION_DOWN:
                case AMOTION_EVENT_ACTION_POINTER_DOWN:
                    if (static_cast<size_t>(id) < touch_points_.size()) {
                        touch_points_[id].id = id;
                        touch_points_[id].x = normalized_x;
                        touch_points_[id].y = normalized_y;
                        touch_points_[id].pressed = true;
                        log_info("Touch down: id=%d, x=%.3f, y=%.3f", id, normalized_x, normalized_y);
                    }
                    break;

                case AMOTION_EVENT_ACTION_UP:
                case AMOTION_EVENT_ACTION_POINTER_UP:
                    if (static_cast<size_t>(id) < touch_points_.size()) {
                        touch_points_[id].pressed = false;
                        log_info("Touch up: id=%d", id);
                    }
                    break;

                case AMOTION_EVENT_ACTION_MOVE:
                    // Handle all pointers for move events
                    int pointer_count = AMotionEvent_getPointerCount(event);
                    for (int i = 0; i < pointer_count && static_cast<size_t>(i) < touch_points_.size(); i++) {
                        int pid = AMotionEvent_getPointerId(event, i);
                        if (static_cast<size_t>(pid) < touch_points_.size() && touch_points_[pid].pressed) {
                            float px = AMotionEvent_getX(event, i) / static_cast<float>(metrics.width_pixels);
                            float py = AMotionEvent_getY(event, i) / static_cast<float>(metrics.height_pixels);
                            touch_points_[pid].x = px;
                            touch_points_[pid].y = py;
                        }
                    }
                    break;
            }
        }
    }

} // namespace android

// Legacy C interface functions for backward compatibility
extern "C" {

qboolean Android_Initialize(struct android_app* app) {
    android::g_platform = std::make_unique<android::AndroidPlatform>();
    return android::g_platform->initialize(app) ? qtrue : qfalse;
}

void Android_Shutdown(void) {
    android::g_platform.reset();
}

void Android_RunMainLoop(void) {
    if (android::g_platform) {
        android::g_platform->run_main_loop();
    }
}

void Android_ShowKeyboard(void) {
    if (android::g_platform) {
        android::g_platform->show_keyboard();
    }
}

void Android_HideKeyboard(void) {
    if (android::g_platform) {
        android::g_platform->hide_keyboard();
    }
}

void Android_Vibrate(int milliseconds) {
    if (android::g_platform) {
        android::g_platform->vibrate(milliseconds);
    }
}

android_display_metrics_t Android_GetDisplayMetrics(void) {
    if (android::g_platform) {
        return android::g_platform->get_display_metrics();
    }
    return {1920, 1080, 1.0f, 1.0f, 160}; // Default fallback
}

ANativeWindow* Android_GetNativeWindow(void) {
    if (android::g_platform) {
        return android::g_platform->get_native_window();
    }
    return nullptr;
}

qboolean Android_IsRunning(void) {
    return android::g_platform && android::g_platform->is_running() ? qtrue : qfalse;
}

const char* Android_GetDataDir(void) {
    static std::string data_dir;
    if (android::g_platform) {
        data_dir = android::g_platform->get_data_dir();
        return data_dir.c_str();
    }
    return "/sdcard/Android/data/com.idtech3.engine/files";
}

const char* Android_GetCacheDir(void) {
    static std::string cache_dir;
    if (android::g_platform) {
        cache_dir = android::g_platform->get_cache_dir();
        return cache_dir.c_str();
    }
    return "/sdcard/Android/data/com.idtech3.engine/cache";
}

// Main Android entry point
void android_main(struct android_app* app) {
    android::log_info("Starting idTech3 Android (C++23)");

    // Initialize Android platform
    if (!Android_Initialize(app)) {
        android::log_error("Failed to initialize Android platform");
        return;
    }

    // Run main event loop
    Android_RunMainLoop();

    android::log_info("idTech3 Android shutting down");
}

} // extern "C"

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

// Legacy C-style global variables for backward compatibility
static struct android_app* android_app = nullptr;
static ANativeWindow* android_window = nullptr;

// Display metrics (legacy C-style for compatibility)
static android_display_metrics_t display_metrics = {1920, 1080, 1.0f, 1.0f, 160};
