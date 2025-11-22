# CMakeLists.txt for building native game modules
# This allows you to use modern C features instead of QVM's limited C compiler

cmake_minimum_required(VERSION 3.12)
project(mymod_game_modules)

# Set C standard to C11 for modern features
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Build as shared libraries (DLLs/.so files)
set(BUILD_SHARED_LIBS ON)

# Output directory - libraries go in vm/ directory
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../vm)

# Include directories - you'll need to point to engine headers
# Adjust these paths to match your engine source location
# CMAKE_SOURCE_DIR is mymod/gamesrc/, so go up 2 levels to get to idtech3 root
get_filename_component(ENGINE_ROOT "${CMAKE_SOURCE_DIR}/../.." ABSOLUTE)
set(ENGINE_INCLUDE_DIRS
    ${ENGINE_ROOT}/src/qcommon
    ${ENGINE_ROOT}/src/game
    ${ENGINE_ROOT}/src/cgame
    ${ENGINE_ROOT}/src/ui
)

message(STATUS "ENGINE_ROOT: ${ENGINE_ROOT}")
message(STATUS "ENGINE_INCLUDE_DIRS: ${ENGINE_INCLUDE_DIRS}")

include_directories(${ENGINE_INCLUDE_DIRS})

# Compiler flags
add_compile_options(-fPIC)
add_compile_options(-fvisibility=hidden)
add_compile_options(-Wall -Wextra)

# Platform-specific settings
if(UNIX)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        set(ARCH_SUFFIX "x86_64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i386|i686|x86")
        set(ARCH_SUFFIX "i386")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(ARCH_SUFFIX "arm64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
        set(ARCH_SUFFIX "arm")
    endif()
elseif(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ARCH_SUFFIX "x64")
    else()
        set(ARCH_SUFFIX "x86")
    endif()
endif()

# Game module (server-side) - OpenArena source
file(GLOB GAME_SOURCES "game/*.c")
add_library(game_${ARCH_SUFFIX} SHARED ${GAME_SOURCES})

# Rename output to match engine expectations: gamex86_64.so format (no dot!)
# Engine expects: {name}{ARCH_STRING}{DLL_EXT}
set_target_properties(game_${ARCH_SUFFIX} PROPERTIES
    OUTPUT_NAME "game${ARCH_SUFFIX}"
    PREFIX ""
)

# CGame module (client-side) - OpenArena source
file(GLOB CGAME_SOURCES "cgame/*.c")
add_library(cgame_${ARCH_SUFFIX} SHARED ${CGAME_SOURCES})

set_target_properties(cgame_${ARCH_SUFFIX} PROPERTIES
    OUTPUT_NAME "cgame${ARCH_SUFFIX}"
    PREFIX ""
)

# UI module - OpenArena source
file(GLOB UI_SOURCES "ui/*.c")
add_library(ui_${ARCH_SUFFIX} SHARED ${UI_SOURCES})

set_target_properties(ui_${ARCH_SUFFIX} PROPERTIES
    OUTPUT_NAME "ui${ARCH_SUFFIX}"
    PREFIX ""
)

# Link math library if needed
target_link_libraries(game_${ARCH_SUFFIX} m)
target_link_libraries(cgame_${ARCH_SUFFIX} m)
target_link_libraries(ui_${ARCH_SUFFIX} m)

