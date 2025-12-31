#include <jni.h>
#include <string>
#include <vector>
#include <android/log.h>
// Engine core bridge (experimental)
#include "engine_core.h"

static int gEngineWidth = 0;
static int gEngineHeight = 0;
static std::vector<std::string> gLoadedMods;

extern "C" JNIEXPORT void JNICALL
Java_com_idtech3_MainActivity_engineInit(JNIEnv* env, jobject /* this */) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineInit() called (src/android-app)");
    // Initialize engine core
    engineCore_init();
    gLoadedMods.clear();
}

extern "C" JNIEXPORT void JNICALL
Java_com_idtech3_MainActivity_engineConfigureSurface(JNIEnv* env, jobject /* this */, jint width, jint height) {
    engineCore_configure(width, height);
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineConfigureSurface(%d,%d)", width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_idtech3_MainActivity_engineRender(JNIEnv* env, jobject /* this */) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineRender() tick (src/android-app)");
    engineCore_render();
}

extern "C" JNIEXPORT void JNICALL
Java_com_idtech3_MainActivity_engineLoadMod(JNIEnv* env, jobject /* this */, jstring modPath) {
    const char* path = env->GetStringUTFChars(modPath, nullptr);
    gLoadedMods.emplace_back(path);
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineLoadMod(%s) (src/android-app) loaded %zu mods", path, gLoadedMods.size());
    env->ReleaseStringUTFChars(modPath, path);
}

extern "C" JNIEXPORT void JNICALL
Java_com_idtech3_MainActivity_engineLoadModFromBytes(JNIEnv* env, jobject /* this */, jbyteArray data, jstring modName) {
    const char* name = env->GetStringUTFChars(modName, nullptr);
    jbyte* bytes = env->GetByteArrayElements(data, nullptr);
    int length = env->GetArrayLength(data);
    // Register via engine core and also keep runtime mod registry
    engineCore_addModBytes(reinterpret_cast<const unsigned char*>(bytes), length, name);
    gLoadedMods.emplace_back(std::string(name) + ":" + std::to_string(length));
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineLoadModFromBytes(%s, length=%d) registered (core)", name, length);
    env->ReleaseStringUTFChars(modName, name);
    env->ReleaseByteArrayElements(data, bytes, 0);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_idtech3_MainActivity_getRegistries(JNIEnv* env, jobject /* this */) {
    // Dump the current registries for CI visibility
    std::string out;
    out += "Textures: ";
    for (const auto& t : gTextureDescriptors) { out += t + " | "; }
    out += "\nMeshes: ";
    for (const auto& m : gMeshDescriptors) { out += m + " | "; }
    out += "\nResources: ";
    for (const auto& r : gResourceDescriptors) { out += r + " | "; }
    return env->NewStringUTF(out.c_str());
}
extern "C" JNIEXPORT void JNICALL
Java_com_idtech3_MainActivity_testIngestTextureBytes(JNIEnv* env, jobject /* this */, jbyteArray data, jstring texName) {
    const char* name = env->GetStringUTFChars(texName, nullptr);
    jbyte* bytes = env->GetByteArrayElements(data, nullptr);
    int length = env->GetArrayLength(data);
    engineCore_test_ingestTextureBytes(reinterpret_cast<const unsigned char*>(bytes), length, name);
    env->ReleaseStringUTFChars(texName, name);
    env->ReleaseByteArrayElements(data, bytes, 0);
}

extern "C" JNIEXPORT void JNICALL
Java_com_idtech3_MainActivity_testIngestMeshBytes(JNIEnv* env, jobject /* this */, jbyteArray data, jstring meshName) {
    const char* name = env->GetStringUTFChars(meshName, nullptr);
    jbyte* bytes = env->GetByteArrayElements(data, nullptr);
    int length = env->GetArrayLength(data);
    engineCore_test_ingestMeshBytes(reinterpret_cast<const unsigned char*>(bytes), length, name);
    env->ReleaseStringUTFChars(meshName, name);
    env->ReleaseByteArrayElements(data, bytes, 0);
}

extern "C" JNIEXPORT void JNICALL
Java_com_idtech3_MainActivity_testIngestResourceBytes(JNIEnv* env, jobject /* this */, jbyteArray data, jstring resName) {
    const char* name = env->GetStringUTFChars(resName, nullptr);
    jbyte* bytes = env->GetByteArrayElements(data, nullptr);
    int length = env->GetArrayLength(data);
    engineCore_test_ingestResourceBytes(reinterpret_cast<const unsigned char*>(bytes), length, name);
    env->ReleaseStringUTFChars(resName, name);
    env->ReleaseByteArrayElements(data, bytes, 0);
}

extern "C" JNIEXPORT void JNICALL
Java_com_idtech3_MainActivity_engineLoadTexture(JNIEnv* env, jobject /* this */, jstring texturePath) {
    const char* path = env->GetStringUTFChars(texturePath, nullptr);
    engineCore_loadTexture(path);
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineLoadTexture(%s) registered", path);
    env->ReleaseStringUTFChars(texturePath, path);
}

extern "C" JNIEXPORT void JNICALL
Java_com_idtech3_MainActivity_engineLoadTextureFromBytes(JNIEnv* env, jobject /* this */, jbyteArray data, jstring texName) {
    const char* name = env->GetStringUTFChars(texName, nullptr);
    jbyte* bytes = env->GetByteArrayElements(data, nullptr);
    int length = env->GetArrayLength(data);
    engineCore_loadTextureFromBytes(reinterpret_cast<const unsigned char*>(bytes), length, name);
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineLoadTextureFromBytes(%s, length=%d) registered", name, length);
    env->ReleaseStringUTFChars(texName, name);
    env->ReleaseByteArrayElements(data, bytes, 0);
}

extern "C" JNIEXPORT void JNICALL
Java_com_idtech3_MainActivity_engineLoadMeshFromBytes(JNIEnv* env, jobject /* this */, jbyteArray data, jstring meshName) {
    const char* name = env->GetStringUTFChars(meshName, nullptr);
    jbyte* bytes = env->GetByteArrayElements(data, nullptr);
    int length = env->GetArrayLength(data);
    engineCore_loadMeshFromBytes(reinterpret_cast<const unsigned char*>(bytes), length, name);
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineLoadMeshFromBytes(%s, length=%d) registered", name, length);
    env->ReleaseStringUTFChars(meshName, name);
    env->ReleaseByteArrayElements(data, bytes, 0);
}

extern "C" JNIEXPORT void JNICALL
Java_com_idtech3_MainActivity_engineLoadResourceFromBytes(JNIEnv* env, jobject /* this */, jbyteArray data, jstring resName) {
    const char* name = env->GetStringUTFChars(resName, nullptr);
    jbyte* bytes = env->GetByteArrayElements(data, nullptr);
    int length = env->GetArrayLength(data);
    engineCore_loadResourceFromBytes(reinterpret_cast<const unsigned char*>(bytes), length, name);
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineLoadResourceFromBytes(%s, length=%d) registered", name, length);
    env->ReleaseStringUTFChars(resName, name);
    env->ReleaseByteArrayElements(data, bytes, 0);
}
