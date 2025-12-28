#include <jni.h>
#include <string>
#include <android/log.h>

extern "C" JNIEXPORT void JNICALL
Java_com_idtech3_MainActivity_engineInit(JNIEnv* env, jobject /* this */) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineInit() called (src/android-app)");
}

extern "C" JNIEXPORT void JNICALL
Java_com_idtech3_MainActivity_engineRender(JNIEnv* env, jobject /* this */) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineRender() tick (src/android-app)");
}

extern "C" JNIEXPORT void JNICALL
Java_com_idtech3_MainActivity_engineLoadMod(JNIEnv* env, jobject /* this */, jstring modPath) {
    const char* path = env->GetStringUTFChars(modPath, nullptr);
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineLoadMod(%s) (src/android-app)", path);
    env->ReleaseStringUTFChars(modPath, path);
}

