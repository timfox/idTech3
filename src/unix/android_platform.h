#ifndef __ANDROID_PLATFORM_H__
#define __ANDROID_PLATFORM_H__

#include <jni.h>
#include <android/native_activity.h>
#include <android/native_window.h>

// Platform-specific functions for Android
qboolean Android_Initialize(struct android_app* app);
void Android_Shutdown(void);
void Android_RunMainLoop(void);

// Input and interaction
void Android_ShowKeyboard(void);
void Android_HideKeyboard(void);
void Android_Vibrate(int milliseconds);

// Display and window management
void Android_GetDisplayMetrics(void);
ANativeWindow* Android_GetNativeWindow(void);

// Platform detection
qboolean Android_IsRunning(void);

// File system paths
const char* Android_GetDataDir(void);
const char* Android_GetCacheDir(void);

// Main entry point for Android
void android_main(struct android_app* app);

// Display metrics structure
typedef struct {
    int width_pixels;
    int height_pixels;
    float density;
    float scaled_density;
    int density_dpi;
} android_display_metrics_t;

extern android_display_metrics_t android_display_metrics;

#endif // __ANDROID_PLATFORM_H__
