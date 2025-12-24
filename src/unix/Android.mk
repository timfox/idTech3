# Android.mk for Quake 3 Engine Android Build

LOCAL_PATH := $(call my-dir)

# Main engine shared library
include $(CLEAR_VARS)

LOCAL_MODULE := idtech3

# Source files
LOCAL_SRC_FILES := \
    android_main.cpp \
    linux_glimp.c \
    linux_qgl.c \
    linux_qvk.c \
    linux_local.h \
    linux_signals.c \
    linux_joystick.c \
    linux_snd.c \
    macos_integration.c \
    ios_appdelegate.mm \
    ios_integration.mm \
    x11_dga.c \
    x11_randr.c \
    x11_vidmode.c

# Include paths
LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../client \
    $(LOCAL_PATH)/../server \
    $(LOCAL_PATH)/../common \
    $(LOCAL_PATH)/../renderers \
    $(LOCAL_PATH)/../botlib \
    $(LOCAL_PATH)/../cgame \
    $(LOCAL_PATH)/../game \
    $(LOCAL_PATH)/../ui \
    $(LOCAL_PATH)/../asm \
    $(LOCAL_PATH)/../../libs/zstd \
    $(LOCAL_PATH)/../../libs/enet/include \
    $(LOCAL_PATH)/../../libs/cjson \
    $(LOCAL_PATH)/../../libs/curl/include \
    $(LOCAL_PATH)/../../libs/freetype/include \
    $(LOCAL_PATH)/../../libs/jpeg \
    $(LOCAL_PATH)/../../libs/ogg/include \
    $(LOCAL_PATH)/../../libs/vorbis/include \
    $(LOCAL_PATH)/../../libs/sdl/include \
    $(LOCAL_PATH)/../../libs/entt/single_include \
    $(LOCAL_PATH)/../../libs/vma

# Compiler flags
LOCAL_CFLAGS := \
    -DUSE_VULKAN \
    -DUSE_OPENAL \
    -DUSE_CURL \
    -DUSE_FREETYPE \
    -DUSE_ZSTD \
    -DUSE_ENET \
    -DUSE_CJSON \
    -DUSE_OPENEXR \
    -DUSE_NANOSVG \
    -DUSE_ASSIMP \
    -DUSE_CIMGUI \
    -DUSE_OPENAL \
    -DUSE_BULLET \
    -DUSE_JOBSYSTEM \
    -DUSE_THEORA \
    -DUSE_VPX \
    -DUSE_DAV1D \
    -DUSE_STB_TRUETYPE \
    -DANDROID \
    -Wno-unused-parameter \
    -Wno-unused-variable \
    -Wno-unused-function \
    -Wno-missing-field-initializers \
    -Wno-sign-compare \
    -Wno-pointer-sign \
    -fvisibility=hidden \
    -ffast-math \
    -O3

LOCAL_CPPFLAGS := \
    -std=c++17 \
    -fexceptions \
    -frtti \
    -Wno-reorder \
    -Wno-unused-parameter

# Libraries
LOCAL_LDLIBS := \
    -llog \
    -landroid \
    -lEGL \
    -lGLESv3 \
    -lOpenSLES \
    -lz \
    -lm \
    -ldl

# Static libraries
LOCAL_STATIC_LIBRARIES := \
    idtech3_common \
    idtech3_client \
    idtech3_server \
    idtech3_renderer_vulkan \
    idtech3_botlib \
    idtech3_cgame \
    idtech3_game \
    idtech3_ui

include $(BUILD_SHARED_LIBRARY)

# Common library
include $(CLEAR_VARS)
LOCAL_MODULE := idtech3_common
LOCAL_SRC_FILES := ../common/*.c
LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES)
LOCAL_CFLAGS := $(LOCAL_CFLAGS)
include $(BUILD_STATIC_LIBRARY)

# Client library
include $(CLEAR_VARS)
LOCAL_MODULE := idtech3_client
LOCAL_SRC_FILES := ../client/*.c
LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES)
LOCAL_CFLAGS := $(LOCAL_CFLAGS)
include $(BUILD_STATIC_LIBRARY)

# Server library
include $(CLEAR_VARS)
LOCAL_MODULE := idtech3_server
LOCAL_SRC_FILES := ../server/*.c ../server/*.cpp
LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES)
LOCAL_CFLAGS := $(LOCAL_CFLAGS)
LOCAL_CPPFLAGS := $(LOCAL_CPPFLAGS)
include $(BUILD_STATIC_LIBRARY)

# Vulkan renderer library
include $(CLEAR_VARS)
LOCAL_MODULE := idtech3_renderer_vulkan
LOCAL_SRC_FILES := ../renderers/vulkan/*.c ../renderers/vulkan/*.cpp
LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES)
LOCAL_CFLAGS := $(LOCAL_CFLAGS)
LOCAL_CPPFLAGS := $(LOCAL_CPPFLAGS)
include $(BUILD_STATIC_LIBRARY)

# Bot library
include $(CLEAR_VARS)
LOCAL_MODULE := idtech3_botlib
LOCAL_SRC_FILES := ../botlib/*.c
LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES)
LOCAL_CFLAGS := $(LOCAL_CFLAGS)
include $(BUILD_STATIC_LIBRARY)

# CGAME library
include $(CLEAR_VARS)
LOCAL_MODULE := idtech3_cgame
LOCAL_SRC_FILES := ../cgame/*.c
LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES)
LOCAL_CFLAGS := $(LOCAL_CFLAGS)
include $(BUILD_STATIC_LIBRARY)

# Game library
include $(CLEAR_VARS)
LOCAL_MODULE := idtech3_game
LOCAL_SRC_FILES := ../game/*.c
LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES)
LOCAL_CFLAGS := $(LOCAL_CFLAGS)
include $(BUILD_STATIC_LIBRARY)

# UI library
include $(CLEAR_VARS)
LOCAL_MODULE := idtech3_ui
LOCAL_SRC_FILES := ../ui/*.cpp
LOCAL_C_INCLUDES := $(LOCAL_C_INCLUDES)
LOCAL_CFLAGS := $(LOCAL_CFLAGS)
LOCAL_CPPFLAGS := $(LOCAL_CPPFLAGS)
include $(BUILD_STATIC_LIBRARY)
