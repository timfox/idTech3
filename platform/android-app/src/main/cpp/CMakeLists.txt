# This CMakeLists.txt is a minimal wrapper for the JNI bridge
# The main engine is built by the root CMakeLists.txt via Gradle's externalNativeBuild
cmake_minimum_required(VERSION 3.20)
project("native-lib")

# Build the JNI bridge library
add_library( native-lib SHARED 
    native-lib.cpp
    engine_core.cpp
)

find_library(log-lib log)
find_library(android-lib android)

target_link_libraries( native-lib 
    ${log-lib}
    ${android-lib}
)

# Include directories for engine_core
target_include_directories( native-lib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Note: The main engine libraries (libidtech3.so, libidtech3_vulkan.so)
# are built by the root CMakeLists.txt via Gradle's externalNativeBuild
# and will be automatically packaged by Gradle into the APK

