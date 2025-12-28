#include "engine_core.h"
#include <android/log.h>
#include <vector>
#include <string>

// Global registries for runtime introspection
std::vector<std::string> gTextureDescriptors;
std::vector<std::string> gMeshDescriptors;
std::vector<std::string> gResourceDescriptors;
struct LoaderItem { int kind; int length; std::string name; std::vector<uint8_t> data; };
static std::vector<LoaderItem> gLoaderQueue;

struct EngineState {
    bool initialized = false;
    int width = 0;
    int height = 0;
    int frame = 0;
    int textures = 0;
    int meshes = 0;
    int resources = 0;
    std::vector<std::string> mods;
} gEngine;

void engineCore_init() {
    gEngine.initialized = true;
    gEngine.width = 0;
    gEngine.height = 0;
    gEngine.frame = 0;
    gEngine.textures = 3;
    gEngine.meshes = 2;
    gEngine.resources = 4;
    gEngine.mods.clear();
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineCore_init: textures=%d meshes=%d resources=%d",
                        gEngine.textures, gEngine.meshes, gEngine.resources);
}

void engineCore_configure(int width, int height) {
    gEngine.width = width;
    gEngine.height = height;
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineCore_configure(%d,%d)", width, height);
}

void engineCore_render() {
    if (!gEngine.initialized) {
        engineCore_init();
    }
    gEngine.frame++;
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineCore_render frame=%d w=%d h=%d textures=%d meshes=%d resources=%d mods=%zu",
                        gEngine.frame, gEngine.width, gEngine.height, gEngine.textures, gEngine.meshes, gEngine.resources, gEngine.mods.size());
    // Run any pending loader tasks to ingest test data into the runtime
    engineCore_runLoaderCycle();
}

void engineCore_loadTexture(const char* path) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineCore_loadTexture(%s)", path ? path : "(null)");
    gEngine.textures++;
    gTextureDescriptors.push_back(std::string(path ? path : "(null)"));
}

// Removed legacy texture registry; use gTextureDescriptors instead
void engineCore_loadTextureFromBytes(const unsigned char* data, int length, const char* texName) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineCore_loadTextureFromBytes(%s, length=%d)", texName ? texName : "(unknown)", length);
    gEngine.textures++;
    gTextureDescriptors.push_back(std::string(texName ? texName : "(unknown)") + ":" + std::to_string(length));
}

void engineCore_loadMesh(const char* path) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineCore_loadMesh(%s)", path ? path : "(null)");
    gEngine.meshes++;
    gMeshDescriptors.push_back(std::string(path ? path : "(null)"));
}

void engineCore_loadMeshFromBytes(const unsigned char* data, int length, const char* meshName) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineCore_loadMeshFromBytes(%s, length=%d)", meshName ? meshName : "(unknown)", length);
    gEngine.meshes++;
    gMeshDescriptors.push_back(std::string(meshName ? meshName : "(unknown)") + ":" + std::to_string(length));
}

void engineCore_loadResource(const char* path) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineCore_loadResource(%s)", path ? path : "(null)");
    gEngine.resources++;
    gResourceDescriptors.push_back(std::string(path ? path : "(null)"));
}

void engineCore_loadResourceFromBytes(const unsigned char* data, int length, const char* resName) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineCore_loadResourceFromBytes(%s, length=%d)", resName ? resName : "(unknown)", length);
    gEngine.resources++;
    gResourceDescriptors.push_back(std::string(resName ? resName : "(unknown)") + ":" + std::to_string(length));
}

void engineCore_queueTextureBytes(const unsigned char* data, int length, const char* texName) {
    LoaderItem it; it.kind = 0; it.length = length; it.name = texName ? texName : "(unknown)";
    it.data.assign(data, data + (length>0?length:0));
    gLoaderQueue.push_back(it);
}

void engineCore_queueMeshBytes(const unsigned char* data, int length, const char* meshName) {
    LoaderItem it; it.kind = 1; it.length = length; it.name = meshName ? meshName : "(unknown)";
    it.data.assign(data, data + (length>0?length:0));
    gLoaderQueue.push_back(it);
}

void engineCore_queueResourceBytes(const unsigned char* data, int length, const char* resName) {
    LoaderItem it; it.kind = 2; it.length = length; it.name = resName ? resName : "(unknown)";
    it.data.assign(data, data + (length>0?length:0));
    gLoaderQueue.push_back(it);
}

void engineCore_runLoaderCycle() {
    for (const auto& it : gLoaderQueue) {
        switch (it.kind) {
            case 0:
                engineCore_loadTextureFromBytes(it.data.data(), it.length, it.name.c_str());
                break;
            case 1:
                engineCore_loadMeshFromBytes(it.data.data(), it.length, it.name.c_str());
                break;
            case 2:
                engineCore_loadResourceFromBytes(it.data.data(), it.length, it.name.c_str());
                break;
        }
    }
    gLoaderQueue.clear();
}

void engineCore_test_ingestTextureBytes(const unsigned char* data, int length, const char* texName) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineCore_test_ingestTextureBytes(%s, length=%d)", texName ? texName : "(unknown)", length);
    engineCore_loadTextureFromBytes(data, length, texName);
}

void engineCore_test_ingestMeshBytes(const unsigned char* data, int length, const char* meshName) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineCore_test_ingestMeshBytes(%s, length=%d)", meshName ? meshName : "(unknown)", length);
    engineCore_loadMeshFromBytes(data, length, meshName);
}

void engineCore_test_ingestResourceBytes(const unsigned char* data, int length, const char* resName) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineCore_test_ingestResourceBytes(%s, length=%d)", resName ? resName : "(unknown)", length);
    engineCore_loadResourceFromBytes(data, length, resName);
}
void engineCore_loadTexture(const char* path) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineCore_loadTexture(%s)", path ? path : "(null)");
    gEngine.textures++;
}

void engineCore_loadTextureFromBytes(const unsigned char* data, int length, const char* texName) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineCore_loadTextureFromBytes(%s, length=%d)", texName ? texName : "(unknown)", length);
    gEngine.textures++;
    gEngine.mods.push_back(std::string(texName ? texName : "(unknown)") + ":" + std::to_string(length));
}

void engineCore_loadMesh(const char* path) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineCore_loadMesh(%s)", path ? path : "(null)");
    gEngine.meshes++;
}

void engineCore_loadMeshFromBytes(const unsigned char* data, int length, const char* meshName) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineCore_loadMeshFromBytes(%s, length=%d)", meshName ? meshName : "(unknown)", length);
    gEngine.meshes++;
    gEngine.mods.push_back(std::string(meshName ? meshName : "(unknown)") + ":" + std::to_string(length));
}

void engineCore_loadResource(const char* path) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineCore_loadResource(%s)", path ? path : "(null)");
    gEngine.resources++;
}

void engineCore_loadResourceFromBytes(const unsigned char* data, int length, const char* resName) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineCore_loadResourceFromBytes(%s, length=%d)", resName ? resName : "(unknown)", length);
    gEngine.resources++;
    gEngine.mods.push_back(std::string(resName ? resName : "(unknown)") + ":" + std::to_string(length));
}
void engineCore_addModBytes(const unsigned char* data, int length, const char* modName) {
    // Basic stub: log and push to mods list
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineCore_addModBytes(%s, length=%d)", modName ? modName : "(unknown)", length);
    gEngine.mods.emplace_back(std::string(modName ? modName : "(unknown)") + ":" + std::to_string(length));
}
