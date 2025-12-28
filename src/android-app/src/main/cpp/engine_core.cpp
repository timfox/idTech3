#include "engine_core.h"
#include <android/log.h>

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
}

