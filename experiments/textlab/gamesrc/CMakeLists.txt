cmake_minimum_required(VERSION 3.31)
project(textlab_game_modules LANGUAGES C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Put artifacts next to the mod so tooling can copy them easily
set(TL_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/..")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${TL_OUTPUT_DIR}/vm")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${TL_OUTPUT_DIR}/vm")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${TL_OUTPUT_DIR}/vm")

# Engine headers live two levels up
get_filename_component(ENGINE_ROOT "${CMAKE_SOURCE_DIR}/../.." ABSOLUTE)
set(ENGINE_INCLUDE_DIRS
    "${ENGINE_ROOT}/src/qcommon"
    "${ENGINE_ROOT}/src/game"
    "${ENGINE_ROOT}/src/cgame"
    "${ENGINE_ROOT}/src/ui"
    "${ENGINE_ROOT}/src/renderercommon"
    "${ENGINE_ROOT}/src/client"
)

message(STATUS "ENGINE_ROOT: ${ENGINE_ROOT}")
message(STATUS "Library output: ${TL_OUTPUT_DIR}/vm")

include_directories(${ENGINE_INCLUDE_DIRS})

add_compile_options(-fPIC -fvisibility=hidden -Wall -Wextra)

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

if(NOT ARCH_SUFFIX)
    message(FATAL_ERROR "Unable to determine architecture suffix")
endif()

file(GLOB GAME_SOURCES "game/*.c")
file(GLOB CGAME_SOURCES "cgame/*.c")
file(GLOB UI_SOURCES "ui/*.c")

add_library(game_${ARCH_SUFFIX} SHARED ${GAME_SOURCES})
add_library(cgame_${ARCH_SUFFIX} SHARED ${CGAME_SOURCES})
add_library(ui_${ARCH_SUFFIX} SHARED ${UI_SOURCES})

set_target_properties(game_${ARCH_SUFFIX} PROPERTIES OUTPUT_NAME "game${ARCH_SUFFIX}" PREFIX "")
set_target_properties(cgame_${ARCH_SUFFIX} PROPERTIES OUTPUT_NAME "cgame${ARCH_SUFFIX}" PREFIX "")
set_target_properties(ui_${ARCH_SUFFIX} PROPERTIES OUTPUT_NAME "ui${ARCH_SUFFIX}" PREFIX "")

if(MSVC)
    target_compile_options(game_${ARCH_SUFFIX} PRIVATE /W4)
    target_compile_options(cgame_${ARCH_SUFFIX} PRIVATE /W4)
    target_compile_options(ui_${ARCH_SUFFIX} PRIVATE /W4)
    set_property(TARGET game_${ARCH_SUFFIX} PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set_property(TARGET cgame_${ARCH_SUFFIX} PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS ON)
    set_property(TARGET ui_${ARCH_SUFFIX} PROPERTY WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()
    target_link_libraries(game_${ARCH_SUFFIX} m)
    target_link_libraries(cgame_${ARCH_SUFFIX} m)
    target_link_libraries(ui_${ARCH_SUFFIX} m)
endif()

target_include_directories(game_${ARCH_SUFFIX} PRIVATE "${CMAKE_SOURCE_DIR}")
target_include_directories(cgame_${ARCH_SUFFIX} PRIVATE "${CMAKE_SOURCE_DIR}")
target_include_directories(ui_${ARCH_SUFFIX} PRIVATE "${CMAKE_SOURCE_DIR}")
