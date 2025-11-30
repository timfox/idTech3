# CMakeLists.txt for building native game modules
# This allows you to use modern C features instead of QVM's limited C compiler

cmake_minimum_required(VERSION 3.20)
project(mymod_game_modules)

# Set C standard to C23 for modern features
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set C++ standard for ECS code (EnTT requires C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
    ${ENGINE_ROOT}/src/renderercommon
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
file(GLOB GAME_CPP_SOURCES "game/*.cpp")
if(GAME_CPP_SOURCES)
	list(APPEND GAME_SOURCES ${GAME_CPP_SOURCES})
endif()
add_library(game_${ARCH_SUFFIX} SHARED ${GAME_SOURCES})

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
add_library(cgame_${ARCH_SUFFIX} SHARED ${CGAME_SOURCES})

set_target_properties(cgame_${ARCH_SUFFIX} PROPERTIES
    OUTPUT_NAME "cgame.${ARCH_SUFFIX}"
    PREFIX ""
)

# UI module - OpenArena source
# UI depends on game module for bg_itemlist and other shared symbols
file(GLOB UI_SOURCES "ui/*.c")
add_library(ui_${ARCH_SUFFIX} SHARED ${UI_SOURCES})

set_target_properties(ui_${ARCH_SUFFIX} PROPERTIES
    OUTPUT_NAME "ui.${ARCH_SUFFIX}"
    PREFIX ""
)

# Link UI against game module to resolve bg_itemlist and other shared symbols
target_link_libraries(ui_${ARCH_SUFFIX} game_${ARCH_SUFFIX} m)

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
target_link_libraries(game_${ARCH_SUFFIX} m)
target_link_libraries(cgame_${ARCH_SUFFIX} m)
target_link_libraries(ui_${ARCH_SUFFIX} m)

