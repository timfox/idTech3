# CMakeLists.txt for building native game modules
# This allows you to use modern C features instead of QVM's limited C compiler

cmake_minimum_required(VERSION 3.20)
project(mymod_game_modules)

# Set C standard to C23 for modern features
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set C++ standard for C++ gamecode helpers (use C++23 features)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build as shared libraries by default (DLLs/.so files)
# Can be overridden with -DBUILD_SHARED_LIBS=OFF for static libraries
# Monolithic builds must be static to avoid runtime dependencies on module .so files.
if(COMBINED_MONOLITH)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libs" FORCE)
elseif(NOT DEFINED BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS ON)
endif()

# Output directory - libraries go in vm/ directory
# Set both library and archive output directories for shared and static libs
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../vm)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/../vm)

# Include directories - ensure we locate the engine tree correctly.
# When built as a subproject (monolithic build), CMAKE_SOURCE_DIR points to the root engine dir,
# so compute from the current source dir instead.
if(NOT DEFINED ENGINE_ROOT)
	get_filename_component(ENGINE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
endif()
set(ENGINE_INCLUDE_DIRS
    ${ENGINE_ROOT}/src/qcommon
    ${ENGINE_ROOT}/src/game
    ${ENGINE_ROOT}/src/cgame
    ${ENGINE_ROOT}/src/ui
    ${ENGINE_ROOT}/src/renderercommon
)

message(STATUS "ENGINE_ROOT: ${ENGINE_ROOT}")
message(STATUS "ENGINE_INCLUDE_DIRS: ${ENGINE_INCLUDE_DIRS}")

include_directories(${ENGINE_INCLUDE_DIRS})

# Compiler flags
add_compile_options(-fPIC)
add_compile_options(-fvisibility=hidden)
add_compile_options(-Wall -Wextra)

# Suppress warnings common in legacy game code
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    # Suppress strict-aliasing warnings (common in legacy C code with type punning)
    add_compile_options(-Wno-strict-aliasing)
    # Suppress array-bounds warnings (legacy code may access arrays at MAX_PERSISTANT)
    add_compile_options(-Wno-array-bounds)
    # Suppress stringop-overflow warnings (false positives with matrix operations)
    add_compile_options(-Wno-stringop-overflow)
    # Suppress format-truncation warnings (legacy code uses fixed-size buffers)
    add_compile_options(-Wno-format-truncation)
    # Suppress uninitialized variable warnings (legacy code patterns)
    add_compile_options(-Wno-uninitialized)
endif()

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

# Shared gameplay sources used across modules
file(GLOB BG_SHARED_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/game/bg_*.c")

# Game module (server-side) - OpenArena source
file(GLOB GAME_SOURCES "game/*.c")
# Remove shared bg_* from the per-module source list; they will be linked once
list(FILTER GAME_SOURCES EXCLUDE REGEX ".*/bg_.*\\.c")
file(GLOB GAME_CPP_SOURCES "game/*.cpp")
if(GAME_CPP_SOURCES)
	list(APPEND GAME_SOURCES ${GAME_CPP_SOURCES})
endif()

# Build shared gameplay logic once
add_library(bg_shared STATIC ${BG_SHARED_SOURCES})
if(COMBINED_MONOLITH)
    target_compile_definitions(bg_shared PRIVATE COMBINED_MONOLITH)
endif()
# Make sure shared code sees the same includes as the modules (propagate to dependents)
target_include_directories(bg_shared PUBLIC ${ENGINE_INCLUDE_DIRS})
# Use BUILD_SHARED_LIBS to determine library type
if(BUILD_SHARED_LIBS)
    add_library(game_${ARCH_SUFFIX} SHARED ${GAME_SOURCES})
else()
    add_library(game_${ARCH_SUFFIX} STATIC ${GAME_SOURCES})
    if(COMBINED_MONOLITH)
        target_compile_definitions(game_${ARCH_SUFFIX} PRIVATE COMBINED_MONOLITH)
    endif()
endif()

# Rename output to match engine expectations: game.x86_64.so format (with dot!)
# Engine expects: {name}.{ARCH_STRING}{DLL_EXT} (VM_LoadLib uses "%s." ARCH_STRING DLL_EXT)
set_target_properties(game_${ARCH_SUFFIX} PROPERTIES
    OUTPUT_NAME "game.${ARCH_SUFFIX}"
    PREFIX ""
)

# CGame module (client-side) - OpenArena source
file(GLOB CGAME_SOURCES "cgame/*.c")
# Exclude cg_newdraw.c as it requires MISSIONPACK UI components not available
list(REMOVE_ITEM CGAME_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/cgame/cg_newdraw.c")

# Use BUILD_SHARED_LIBS to determine library type
if(BUILD_SHARED_LIBS)
    add_library(cgame_${ARCH_SUFFIX} SHARED ${CGAME_SOURCES})
else()
    add_library(cgame_${ARCH_SUFFIX} STATIC ${CGAME_SOURCES})
    # When building as static library for monolithic build, define COMBINED_MONOLITH
    if(COMBINED_MONOLITH)
        target_compile_definitions(cgame_${ARCH_SUFFIX} PRIVATE COMBINED_MONOLITH)
    endif()
endif()

set_target_properties(cgame_${ARCH_SUFFIX} PROPERTIES
    OUTPUT_NAME "cgame.${ARCH_SUFFIX}"
    PREFIX ""
)

# UI module - OpenArena source
# UI depends on game module for bg_itemlist and other shared symbols
file(GLOB UI_SOURCES "ui/*.c")
# Use BUILD_SHARED_LIBS to determine library type
if(BUILD_SHARED_LIBS)
    add_library(ui_${ARCH_SUFFIX} SHARED ${UI_SOURCES})
else()
    add_library(ui_${ARCH_SUFFIX} STATIC ${UI_SOURCES})
    # When building as static library for monolithic build, define COMBINED_MONOLITH
    if(COMBINED_MONOLITH)
        target_compile_definitions(ui_${ARCH_SUFFIX} PRIVATE COMBINED_MONOLITH)
    endif()
endif()

set_target_properties(ui_${ARCH_SUFFIX} PROPERTIES
    OUTPUT_NAME "ui.${ARCH_SUFFIX}"
    PREFIX ""
)

# Link shared gameplay logic once; UI still links game for other shared data
target_link_libraries(bg_shared m)
target_link_libraries(game_${ARCH_SUFFIX} bg_shared m)
target_link_libraries(cgame_${ARCH_SUFFIX} bg_shared m)
target_link_libraries(ui_${ARCH_SUFFIX} bg_shared game_${ARCH_SUFFIX} m)

# EnTT ECS support
option(USE_ENTT "Enable EnTT ECS system" OFF)
if(USE_ENTT)
	# Add EnTT include directory
	include_directories(${ENGINE_ROOT}/libs/entt/src)
	# Add compile definition
	target_compile_definitions(game_${ARCH_SUFFIX} PRIVATE USE_ENTT)
	# Link C++ standard library for C++ files
	target_link_libraries(game_${ARCH_SUFFIX} stdc++fs)
endif()

# Link math library if needed
# Legacy math links are handled above alongside bg_shared

