#include <jni.h>
#include <string>

// Android native engine hooks with lightweight logging for visibility
#include <android/log.h>

extern "C" JNIEXPORT void JNICALL
Java_com_idtech3_MainActivity_engineInit(JNIEnv* env, jobject /* this */) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineInit() called");
    // Placeholder for engine initialization on Android.
}
extern "C" JNIEXPORT void JNICALL
Java_com_idtech3_MainActivity_engineLoadMod(JNIEnv* env, jobject /* this */, jstring modPath) {
    const char* path = env->GetStringUTFChars(modPath, nullptr);
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineLoadMod(%s)", path);
    env->ReleaseStringUTFChars(modPath, path);
}

extern "C" JNIEXPORT void JNICALL
Java_com_idtech3_MainActivity_engineRender(JNIEnv* env, jobject /* this */) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineRender() tick");
    // Placeholder for per-frame engine rendering on Android.
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_idtech3_MainActivity_stringFromJNI(JNIEnv* env, jobject /* this */) {
    const char* hello = "Hello from native Android!";
    return env->NewStringUTF(hello);
}

