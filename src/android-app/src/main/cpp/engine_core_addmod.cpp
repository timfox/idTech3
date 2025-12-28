#include "engine_core.h"
#include <android/log.h>
#include <vector>
#include <string>

static std::vector<std::string> gRuntimeMods;

void engineCore_addModBytes(const unsigned char* data, int length, const char* modName) {
    __android_log_print(ANDROID_LOG_INFO, "EngineAndroid", "engineCore_addModBytes(%s, length=%d)", modName ? modName : "(unknown)", length);
    gRuntimeMods.push_back(std::string(modName ? modName : "(unknown)") + ":" + std::to_string(length));
}

