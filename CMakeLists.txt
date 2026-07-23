CMAKE_MINIMUM_REQUIRED(VERSION 3.24)

PROJECT(idtech3 VERSION 1.0.0 LANGUAGES C CXX)

# Android: optional persistent cache for Lua tarball + FetchContent (FreeType). CI sets -DANDROID_DEPS_CACHE=...
if(ANDROID)
  set(ANDROID_DEPS_CACHE "" CACHE PATH
    "If set, reuse this directory for Android third-party downloads (Lua ExternalProject, FetchContent). Speeds CI and clean rebuilds.")
  if(ANDROID_DEPS_CACHE AND NOT ANDROID_DEPS_CACHE STREQUAL "")
    file(MAKE_DIRECTORY "${ANDROID_DEPS_CACHE}")
    set(FETCHCONTENT_BASE_DIR "${ANDROID_DEPS_CACHE}/fetchcontent" CACHE PATH "FetchContent base directory (Android deps cache)" FORCE)
    message(STATUS "ANDROID_DEPS_CACHE: ${ANDROID_DEPS_CACHE}")
  endif()
endif()

if(NOT ANDROID)
  find_package(Threads REQUIRED)
endif()

if("c_std_23" IN_LIST CMAKE_C_COMPILE_FEATURES)
    set(CMAKE_C_STANDARD 23)
else()
    set(CMAKE_C_STANDARD 17)
    if(MSVC)
        message(WARNING "MSVC C23 mode is not available with this toolchain. Falling back to C17.")
    else()
        message(WARNING "C23 mode is not available with this toolchain. Falling back to C17.")
    endif()
endif()
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if("cxx_std_20" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
  set(IDTECH3_CXX_STANDARD 20)
else()
  set(IDTECH3_CXX_STANDARD 17)
  if(NOT MSVC)
    message(WARNING "C++20 not available; falling back to C++17 for C++ targets.")
  endif()
endif()
set(CMAKE_CXX_STANDARD ${IDTECH3_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
message(STATUS "Engine C++ standard: C++${IDTECH3_CXX_STANDARD}")

include( ${CMAKE_SOURCE_DIR}/cmake/IdTech3Cpp20.cmake )

option(C_STANDARD_STRICT "Enable the stricter warning set for engine builds" ON)
option(CI_BUILD "Enable CI-specific behaviors such as warnings-as-errors" OFF)
option(USE_FLUX "Enable FLUX.2 image generation support" ON)
option(USE_TRELLIS "Enable Microsoft TRELLIS.2 external 3D generation hook" ON)
option(USE_GENETIC_GAN "Enable genetic genome + external GAN body decode API" ON)
option(ENABLE_FORTIFY_SOURCE "Enable _FORTIFY_SOURCE=2 buffer overflow protection (GCC/Clang)" ON)
option(ENABLE_ASAN "Enable AddressSanitizer for memory error detection (Debug builds)" OFF)
option(ENABLE_LTO "Enable IPO/LTO for Release and RelWithDebInfo (GCC/Clang; longer links, smaller/faster binaries)" OFF)
option(BUILD_SAMPLES_DEMO_GAME "Build samples/demo_game (idtech3_demo.pk3 + helper)" OFF)
option(BUILD_EXAMPLE_DEMO_GAME "Deprecated alias for BUILD_SAMPLES_DEMO_GAME" OFF)
if(BUILD_EXAMPLE_DEMO_GAME AND NOT BUILD_SAMPLES_DEMO_GAME)
  set(BUILD_SAMPLES_DEMO_GAME ON CACHE BOOL "Build samples/demo_game (idtech3_demo.pk3 + helper)" FORCE)
  message(WARNING "BUILD_EXAMPLE_DEMO_GAME is deprecated; use BUILD_SAMPLES_DEMO_GAME (samples/ → examples/ symlink)")
endif()
# FreeUSD is desktop-oriented (CMake Linux/macOS/MinGW). Default OFF on Android NDK.
if(ANDROID)
	set(_IDTECH3_FREEUSD_DEFAULT OFF)
else()
	set(_IDTECH3_FREEUSD_DEFAULT ON)
endif()
option(USE_FREEUSD "Link FreeUSD for USDA mesh import and USD scene tools (GPL-2.0-or-later)" ${_IDTECH3_FREEUSD_DEFAULT})
option(USE_IDTECH3_BACKEND "Require idtech3backend submodule (timfox/idtech3backend); wiring TBD" OFF)
option(USE_IDTECH3_EMULATOR "Require idTech3-Emulator submodule (QEMU fork); in-engine bridge TBD" OFF)

set(IDTECH3_PROFILE "game" CACHE STRING "Build profile: core|game|full|research")
set_property(CACHE IDTECH3_PROFILE PROPERTY STRINGS core game full research)
option(USE_RESEARCH_EXTENSIONS "Research/paper-repro qcommon modules (RadiusFPS, DaX, GCC-FER, ...)" ON)
option(USE_OPEN_WORLD "Open-world, districts, sector graph, fog biology, genetic GAN core" ON)
option(USE_ARC_BLANC "Arc Blanc real-time ocean (Tessendorf FFT + solid-fluid coupling)" OFF)
option(USE_GAME_AI_MIDDLEWARE "Director/GOAP/Horde/BT gameplay middleware (g_director, g_goap, …)" ON)
option(USE_UBERTOOLS_COMPAT "Clean-room ÜberTools (Babble/TIKI) compatibility stack; no Miles" ON)
option(USE_BABBLE "Clean-room Babble dialogue graphs (requires USE_UBERTOOLS_COMPAT)" ON)
option(USE_TIKI "Clean-room TIKI/TAN model defs (requires USE_UBERTOOLS_COMPAT)" ON)

include(${CMAKE_SOURCE_DIR}/cmake/IdTech3Backend.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/IdTech3Emulator.cmake)

if(C_STANDARD_STRICT)
  if(MSVC)
    add_compile_options(/W4)
  else()
    # Flags valid for both C and C++
    set(C_CXX_STRICT_WARNINGS
      -Wall
      -Wextra
      -Wpedantic
      -Wshadow
      -Wformat=2
      -Wundef
      -Wcast-qual
      -Wpointer-arith
      -Wvla
    )
    add_compile_options(${C_CXX_STRICT_WARNINGS})
    # C-only flags (not valid for C++) - apply per-flag so generator expr expands
    add_compile_options(
      $<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>
      $<$<COMPILE_LANGUAGE:C>:-Wmissing-prototypes>
      $<$<COMPILE_LANGUAGE:C>:-Wold-style-definition>
    )
  endif()
endif()

if(CI_BUILD)
  if(MSVC)
    # Keep CI diagnostics broad; first-party-only targets opt into /WX below.
    add_compile_options(/W4 /wd4100 /wd4505)
  else()
    # Keep CI diagnostics broad; first-party-only targets opt into -Werror below.
    add_compile_options(-Wno-unused-function -Wno-shadow -Wno-cast-qual -Wno-vla)
  endif()
endif()

function(idtech3_first_party_warnings_as_errors target_name)
  if(NOT TARGET ${target_name})
    return()
  endif()
  # Per-target C++20 policy (no global -fno-exceptions — FreeUSD/third_party need exceptions).
  idtech3_cpp20_apply_target_flags(${target_name})
  if(NOT CI_BUILD)
    return()
  endif()
  if(MSVC)
    target_compile_options(${target_name} PRIVATE /WX)
  else()
    target_compile_options(${target_name} PRIVATE -Werror)
  endif()
endfunction()

# Security: buffer overflow protection (GCC/Clang; not MSVC)
if(ENABLE_FORTIFY_SOURCE AND NOT MSVC)
  add_compile_options(-D_FORTIFY_SOURCE=2)
  message(STATUS "FORTIFY_SOURCE=2 enabled (buffer overflow protection)")
endif()

# Sanitizers: AddressSanitizer for memory error detection (Debug only)
if(ENABLE_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC)
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address)
  message(STATUS "AddressSanitizer enabled (memory error detection)")
endif()

# Optional link-time optimization (IPO/LTO): off by default; use for shipping builds on GCC/Clang.
if(ENABLE_LTO)
  if(MSVC)
    message(WARNING "ENABLE_LTO: not wired for MSVC in this option; ignoring.")
  elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "ENABLE_LTO ignored (Debug build)")
  else()
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _ipo_ok OUTPUT _ipo_err)
    if(_ipo_ok)
      set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
      message(STATUS "IPO/LTO enabled (CMAKE_INTERPROCEDURAL_OPTIMIZATION=ON)")
    else()
      message(WARNING "ENABLE_LTO requested but not supported by this toolchain: ${_ipo_err}")
    endif()
  endif()
endif()

# --- Begin FindSDL3
# Prefer official CMake package config (SDL3::SDL3). FetchContent fallback runs after USE_SDL option.
set(SDL3_FOUND FALSE)
find_package(SDL3 QUIET CONFIG COMPONENTS SDL3)
if(TARGET SDL3::SDL3)
  set(SDL3_FOUND TRUE)
  message(STATUS "SDL3: found system/config package")
endif()
# --- End FindSDL3

# --- Begin FindOpenAL.cmake
find_path(OPENAL_INCLUDE_DIR
  NAMES AL/al.h OpenAL/al.h
  PATH_SUFFIXES include
  PATHS ${CMAKE_PREFIX_PATH} /usr/local /usr /opt/local /opt /opt/homebrew
)

find_library(OPENAL_LIBRARY
  NAMES openal OpenAL32
  PATH_SUFFIXES lib64 lib
  PATHS ${CMAKE_PREFIX_PATH} /usr/local /usr /opt/local /opt /opt/homebrew
)

if(OPENAL_INCLUDE_DIR AND OPENAL_LIBRARY)
  set(OPENAL_FOUND TRUE)
  if(NOT TARGET OpenAL::OpenAL)
    add_library(OpenAL::OpenAL UNKNOWN IMPORTED)
    set_target_properties(OpenAL::OpenAL PROPERTIES
      IMPORTED_LOCATION "${OPENAL_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${OPENAL_INCLUDE_DIR}")
  endif()
else()
  set(OPENAL_FOUND FALSE)
endif()
# --- End FindOpenAL.cmake

if(NOT DEFINED USE_OPUS)
  option(USE_OPUS "Enable Opus codec support" ON)
endif()
if(NOT DEFINED USE_FLAC)
  option(USE_FLAC "Enable FLAC codec support" ON)
endif()
if(NOT DEFINED USE_WEBM)
  option(USE_WEBM "Enable WebM audio support" ON)
endif()
if(NOT DEFINED USE_LUA)
  option(USE_LUA "Enable Lua scripting support" ON)
endif()
if(NOT DEFINED USE_DUKTAPE)
  option(USE_DUKTAPE "Enable Duktape JavaScript scripting support" ON)
endif()
if(NOT DEFINED USE_SYSTEM_DUKTAPE)
  option(USE_SYSTEM_DUKTAPE "Use system-installed Duktape instead of vendored internal copy" OFF)
endif()
if(NOT DEFINED USE_CSHARP)
  option(USE_CSHARP "Enable Mono C# scripting support" OFF)
endif()
if(NOT DEFINED USE_PYTHON)
  option(USE_PYTHON "Enable embedded CPython scripting (Infernux-style batch bridge)" OFF)
endif()
if(NOT DEFINED USE_SQLITE)
  option(USE_SQLITE "Enable native SQLite gameplay/profile database support" ON)
endif()

set(LUA_FOUND FALSE)
set(LUA_LINK_LIBRARIES "")
set(LUA_VERSION_NUM 0)
set(LUA_VERSION_STRING "")
set(LUA_EXTRA_LIBRARIES "")
set(DUKTAPE_FOUND FALSE)
set(DUKTAPE_LINK_LIBRARIES "")
set(DUKTAPE_EXTRA_LIBRARIES "")
set(CSHARP_FOUND FALSE)
set(CSHARP_LINK_LIBRARIES "")
set(CSHARP_INCLUDE_DIRS "")
set(PYTHON_FOUND FALSE)
set(PYTHON_LINK_LIBRARIES "")
set(PYTHON_INCLUDE_DIRS "")
set(SQLITE_FOUND FALSE)

if(USE_LUA)
if(NOT ANDROID)
  set(_LUA_HINT_PATHS
    $ENV{LUA_DIR}
    $ENV{LUA_PATH}
    ${CMAKE_PREFIX_PATH}
    ${CMAKE_INSTALL_PREFIX}
    /usr/local
    /usr
    /opt/local
    /opt
    /opt/homebrew
    /sw
  )
  if(WIN32)
    list(APPEND _LUA_HINT_PATHS
      "C:/msys64/mingw64"
      "C:/msys64/ucrt64"
      "C:/msys64/mingw32"
      "C:/msys64/usr"
      "C:/Lua"
      "C:/Program Files/Lua"
      "C:/Program Files (x86)/Lua"
    )
  endif()

  find_path(LUA_INCLUDE_DIR
    NAMES lua.h
    PATH_SUFFIXES
      include
      include/lua
      include/lua5.5
      include/lua-5.5
      include/lua5.4
      include/lua-5.4
      include/lua5.3
      include/lua-5.3
      include/lua5.2
      include/lua-5.2
      include/lua5.1
      include/lua-5.1
    PATHS ${_LUA_HINT_PATHS}
  )

  set(_LUA_HEADER_PATH "")
  if(LUA_INCLUDE_DIR AND EXISTS "${LUA_INCLUDE_DIR}/lua.h")
    set(_LUA_HEADER_PATH "${LUA_INCLUDE_DIR}/lua.h")
  endif()

  if(_LUA_HEADER_PATH)
    file(STRINGS "${_LUA_HEADER_PATH}" LUA_VERSION_MAJOR_LINE REGEX "^#define[ \t]+LUA_VERSION_MAJOR_N[ \t]+[0-9]+" LIMIT_COUNT 1)
    file(STRINGS "${_LUA_HEADER_PATH}" LUA_VERSION_MINOR_LINE REGEX "^#define[ \t]+LUA_VERSION_MINOR_N[ \t]+[0-9]+" LIMIT_COUNT 1)
    if(LUA_VERSION_MAJOR_LINE AND LUA_VERSION_MINOR_LINE)
      string(REGEX REPLACE ".*LUA_VERSION_MAJOR_N[ \t]+([0-9]+).*" "\\1" LUA_VERSION_MAJOR "${LUA_VERSION_MAJOR_LINE}")
      string(REGEX REPLACE ".*LUA_VERSION_MINOR_N[ \t]+([0-9]+).*" "\\1" LUA_VERSION_MINOR "${LUA_VERSION_MINOR_LINE}")
      math(EXPR LUA_VERSION_NUM "${LUA_VERSION_MAJOR} * 100 + ${LUA_VERSION_MINOR}")
      set(LUA_VERSION_STRING "${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}")
    else()
      file(STRINGS "${_LUA_HEADER_PATH}" LUA_VERSION_NUM_LINE REGEX "^#define[ \t]+LUA_VERSION_NUM[ \t]+[0-9]+" LIMIT_COUNT 1)
      if(LUA_VERSION_NUM_LINE)
        string(REGEX REPLACE ".*LUA_VERSION_NUM[ \t]+([0-9]+).*" "\\1" LUA_VERSION_NUM "${LUA_VERSION_NUM_LINE}")
        math(EXPR LUA_VERSION_MAJOR "${LUA_VERSION_NUM} / 100")
        math(EXPR LUA_VERSION_MINOR "${LUA_VERSION_NUM} % 100")
        set(LUA_VERSION_STRING "${LUA_VERSION_MAJOR}.${LUA_VERSION_MINOR}")
      endif()
    endif()
  endif()

  set(_LUA_LIB_NAMES lua5.5 lua-5.5 lua55 lua5.4 lua-5.4 lua54 lua5.3 lua-5.3 lua53 lua5.2 lua-5.2 lua52 lua5.1 lua-5.1 lua51 lua)
  if(LUA_VERSION_STRING)
    set(_LUA_LIB_NAMES lua${LUA_VERSION_STRING} lua-${LUA_VERSION_STRING} lua${LUA_VERSION_MAJOR}${LUA_VERSION_MINOR} ${_LUA_LIB_NAMES})
    list(REMOVE_DUPLICATES _LUA_LIB_NAMES)
  endif()

  set(_LUA_LIB_HINTS "")
  if(LUA_INCLUDE_DIR)
    get_filename_component(_LUA_INCLUDE_PARENT "${LUA_INCLUDE_DIR}" DIRECTORY)
    list(APPEND _LUA_LIB_HINTS
      "${LUA_INCLUDE_DIR}/../lib"
      "${LUA_INCLUDE_DIR}/../lib64"
      "${_LUA_INCLUDE_PARENT}/lib"
      "${_LUA_INCLUDE_PARENT}/lib64"
    )
    list(REMOVE_DUPLICATES _LUA_LIB_HINTS)
  endif()

  if(_LUA_LIB_HINTS)
    find_library(LUA_LIBRARY
      NAMES ${_LUA_LIB_NAMES}
      HINTS ${_LUA_LIB_HINTS}
      PATH_SUFFIXES "" lib lib64
      NO_DEFAULT_PATH
    )
  endif()

  if(NOT LUA_LIBRARY)
    find_library(LUA_LIBRARY
      NAMES ${_LUA_LIB_NAMES}
      PATH_SUFFIXES lib64 lib
      PATHS ${_LUA_HINT_PATHS}
    )
  endif()

  if((NOT LUA_INCLUDE_DIR OR NOT LUA_LIBRARY) AND NOT WIN32)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
      set(_LUA_PKG_NAMES ${_LUA_LIB_NAMES})
      list(REMOVE_DUPLICATES _LUA_PKG_NAMES)
      if(_LUA_PKG_NAMES)
        set(_LUA_PKG_DETECTED FALSE)
        foreach(_LUA_PKG_NAME ${_LUA_PKG_NAMES})
          pkg_search_module(LUA_PKG QUIET ${_LUA_PKG_NAME})
          if(LUA_PKG_FOUND)
            set(_LUA_PKG_DETECTED TRUE)
            break()
          endif()
        endforeach()
        if(_LUA_PKG_DETECTED)
          if(NOT LUA_INCLUDE_DIR AND LUA_PKG_INCLUDE_DIRS)
            list(GET LUA_PKG_INCLUDE_DIRS 0 LUA_INCLUDE_DIR)
          endif()
          if(NOT LUA_LIBRARY)
            if(LUA_PKG_LIBRARIES)
              list(GET LUA_PKG_LIBRARIES 0 LUA_LIBRARY)
              list(REMOVE_AT LUA_PKG_LIBRARIES 0)
              set(LUA_EXTRA_LIBRARIES ${LUA_PKG_LIBRARIES})
            elseif(LUA_PKG_LINK_LIBRARIES)
              list(GET LUA_PKG_LINK_LIBRARIES 0 LUA_LIBRARY)
              list(REMOVE_AT LUA_PKG_LINK_LIBRARIES 0)
              set(LUA_EXTRA_LIBRARIES ${LUA_PKG_LINK_LIBRARIES})
            endif()
          endif()
        endif()
      endif()
    endif()
  endif()

  if(NOT LUA_INCLUDE_DIR OR NOT LUA_LIBRARY)
    message(WARNING "Lua headers/library not found; Lua scripting support will be disabled.")
  else()
    set(LUA_FOUND TRUE)

    if(LUA_VERSION_NUM GREATER 0 AND LUA_VERSION_NUM LESS 501)
      message(FATAL_ERROR "Lua 5.1+ is required.")
    endif()

    if(LUA_VERSION_STRING)
      message(STATUS "Lua support enabled: ${LUA_VERSION_STRING} (${LUA_LIBRARY})")
    else()
      message(STATUS "Lua support enabled: ${LUA_LIBRARY}")
    endif()

    list(APPEND LUA_LINK_LIBRARIES ${LUA_LIBRARY})
    if(LUA_EXTRA_LIBRARIES)
      list(APPEND LUA_LINK_LIBRARIES ${LUA_EXTRA_LIBRARIES})
    endif()
  endif()
endif()

if(USE_SQLITE)
  find_package(SQLite3 QUIET)
  if(SQLite3_FOUND)
    set(SQLITE_FOUND TRUE)
  else()
    message(WARNING "USE_SQLITE=ON but SQLite3 not found; database service disabled.")
  endif()
endif()
endif()

# Android NDK: build static Lua from upstream source (no system pkg-config)
set(LUA_ANDROID_BUILT FALSE)
if(ANDROID AND USE_LUA AND NOT LUA_FOUND)
  include(ExternalProject)
  # Lua upstream ships a GNU Makefile; CMAKE_MAKE_PROGRAM is often Ninja on Android - use make.
  find_program(LUA_ANDROID_MAKE_EXE NAMES make gmake)
  if(NOT LUA_ANDROID_MAKE_EXE)
    message(FATAL_ERROR "GNU make (make) is required to build Lua for Android but was not found in PATH.")
  endif()
  set(_LUA_ANDROID_VER "5.4.7")
  set(_LUA_ANDROID_SHA "9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30")
  # One prefix per ABI - Gradle builds arm64 and armeabi-v7a in the same CMake binary dir; a shared
  # "lua-android/" would leave the wrong-arch liblua.a from whichever ABI configured first.
  set(_LUA_ANDROID_PREFIX "${CMAKE_BINARY_DIR}/lua-android-${ANDROID_ABI}")
  set(_LUA_EP_EXTRA "")
  if(ANDROID_DEPS_CACHE AND NOT ANDROID_DEPS_CACHE STREQUAL "")
    set(_LUA_DL_DIR "${ANDROID_DEPS_CACHE}/externalproject-downloads")
    file(MAKE_DIRECTORY "${_LUA_DL_DIR}")
    list(APPEND _LUA_EP_EXTRA DOWNLOAD_DIR "${_LUA_DL_DIR}")
  endif()
  # Lua src/Makefile sets "AR= ar rcu" and invokes $(AR) target objs - AR must be "llvm-ar rcu", not bare llvm-ar.
  # ANDROID may set CMAKE_C_COMPILER to bare NDK clang (no --target); without it, objects are host x86_64.
  if(DEFINED ANDROID_PLATFORM_LEVEL AND NOT ANDROID_PLATFORM_LEVEL STREQUAL "")
    set(_LUA_API "${ANDROID_PLATFORM_LEVEL}")
  elseif(ANDROID_PLATFORM MATCHES "android-([0-9]+)")
    set(_LUA_API "${CMAKE_MATCH_1}")
  else()
    set(_LUA_API "26")
  endif()
  if(ANDROID_ABI STREQUAL "arm64-v8a")
    set(_LUA_CLANG_TARGET "aarch64-none-linux-android${_LUA_API}")
  elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
    set(_LUA_CLANG_TARGET "armv7-none-linux-androideabi${_LUA_API}")
  elseif(ANDROID_ABI STREQUAL "x86")
    set(_LUA_CLANG_TARGET "i686-none-linux-android${_LUA_API}")
  elseif(ANDROID_ABI STREQUAL "x86_64")
    set(_LUA_CLANG_TARGET "x86_64-none-linux-android${_LUA_API}")
  else()
    set(_LUA_CLANG_TARGET "aarch64-none-linux-android${_LUA_API}")
  endif()
  set(_LUA_CC_WRAPPER "${CMAKE_C_COMPILER} --target=${_LUA_CLANG_TARGET}")
  if(CMAKE_SYSROOT)
    string(APPEND _LUA_CC_WRAPPER " --sysroot=${CMAKE_SYSROOT}")
  endif()
  set(_LUA_MAKE_CC "CC=${_LUA_CC_WRAPPER}")
  set(_LUA_MAKE_AR "AR=${CMAKE_AR} rcu")
  set(_LUA_MAKE_RANLIB "RANLIB=${CMAKE_RANLIB}")
  ExternalProject_Add(lua_android_ep
    URL "https://www.lua.org/ftp/lua-${_LUA_ANDROID_VER}.tar.gz"
    URL_HASH "SHA256=${_LUA_ANDROID_SHA}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    ${_LUA_EP_EXTRA}
    CONFIGURE_COMMAND ""
    # Lua src/Makefile sets "CC= gcc" (not ?=); pass overrides on the make line (one argv each).
    BUILD_COMMAND ${LUA_ANDROID_MAKE_EXE} -C <SOURCE_DIR>/src
      ${_LUA_MAKE_CC} ${_LUA_MAKE_AR} ${_LUA_MAKE_RANLIB} liblua.a MYCFLAGS=-fPIC
    INSTALL_COMMAND ${CMAKE_COMMAND} -E make_directory "${_LUA_ANDROID_PREFIX}/lib" "${_LUA_ANDROID_PREFIX}/include"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different <SOURCE_DIR>/src/liblua.a "${_LUA_ANDROID_PREFIX}/lib/liblua.a"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different <SOURCE_DIR>/src/lua.h "${_LUA_ANDROID_PREFIX}/include/"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different <SOURCE_DIR>/src/luaconf.h "${_LUA_ANDROID_PREFIX}/include/"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different <SOURCE_DIR>/src/lauxlib.h "${_LUA_ANDROID_PREFIX}/include/"
      COMMAND ${CMAKE_COMMAND} -E copy_if_different <SOURCE_DIR>/src/lualib.h "${_LUA_ANDROID_PREFIX}/include/"
    BUILD_BYPRODUCTS "${_LUA_ANDROID_PREFIX}/lib/liblua.a"
  )
  set(LUA_FOUND TRUE)
  set(LUA_INCLUDE_DIR "${_LUA_ANDROID_PREFIX}/include")
  set(LUA_LIBRARY "${_LUA_ANDROID_PREFIX}/lib/liblua.a")
  set(LUA_LINK_LIBRARIES "${LUA_LIBRARY}")
  set(LUA_VERSION_STRING "${_LUA_ANDROID_VER}")
  set(LUA_VERSION_NUM 504)
  set(LUA_ANDROID_BUILT TRUE)
  message(STATUS "Lua for Android: will build static ${_LUA_ANDROID_VER} via ExternalProject (first build downloads tarball)")
endif()

if(USE_DUKTAPE)
  set(DUKTAPE_VENDORED_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/third_party/duktape/duktape.c")
  set(DUKTAPE_VENDORED_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/third_party/duktape/duktape.h")

  if((NOT USE_SYSTEM_DUKTAPE) AND EXISTS "${DUKTAPE_VENDORED_SOURCE}" AND EXISTS "${DUKTAPE_VENDORED_HEADER}")
    add_library(js_runtime_internal STATIC ${DUKTAPE_VENDORED_SOURCE})
    target_include_directories(js_runtime_internal PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/duktape")
    set_property(TARGET js_runtime_internal PROPERTY POSITION_INDEPENDENT_CODE ON)
    if(NOT MSVC)
      target_compile_options(js_runtime_internal PRIVATE -fno-strict-aliasing -Wno-format-nonliteral -Wno-unused-but-set-variable)
    endif()

    set(DUKTAPE_FOUND TRUE)
    set(DUKTAPE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/duktape")
    list(APPEND DUKTAPE_LINK_LIBRARIES js_runtime_internal)
    message(STATUS "Duktape support enabled: vendored internal static library")
  else()
    set(_DUKTAPE_HINT_PATHS
      $ENV{DUKTAPE_DIR}
      $ENV{DUKTAPE_PATH}
      ${CMAKE_PREFIX_PATH}
      ${CMAKE_INSTALL_PREFIX}
      /usr/local
      /usr
      /opt/local
      /opt
      /opt/homebrew
      /sw
    )
    if(WIN32)
      list(APPEND _DUKTAPE_HINT_PATHS
        "C:/msys64/mingw64"
        "C:/msys64/ucrt64"
        "C:/msys64/mingw32"
        "C:/msys64/usr"
      )
    endif()

    find_path(DUKTAPE_INCLUDE_DIR
      NAMES duktape.h
      PATH_SUFFIXES include
      PATHS ${_DUKTAPE_HINT_PATHS}
    )

    find_library(DUKTAPE_LIBRARY
      NAMES duktape
      PATH_SUFFIXES lib64 lib
      PATHS ${_DUKTAPE_HINT_PATHS}
    )

    if((NOT DUKTAPE_INCLUDE_DIR OR NOT DUKTAPE_LIBRARY) AND NOT WIN32)
      find_package(PkgConfig QUIET)
      if(PKG_CONFIG_FOUND)
        pkg_search_module(DUKTAPE_PKG QUIET duktape)
        if(DUKTAPE_PKG_FOUND)
          if(NOT DUKTAPE_INCLUDE_DIR AND DUKTAPE_PKG_INCLUDE_DIRS)
            list(GET DUKTAPE_PKG_INCLUDE_DIRS 0 DUKTAPE_INCLUDE_DIR)
          endif()
          if(NOT DUKTAPE_LIBRARY)
            if(DUKTAPE_PKG_LIBRARIES)
              list(GET DUKTAPE_PKG_LIBRARIES 0 DUKTAPE_LIBRARY)
              list(REMOVE_AT DUKTAPE_PKG_LIBRARIES 0)
              set(DUKTAPE_EXTRA_LIBRARIES ${DUKTAPE_PKG_LIBRARIES})
            elseif(DUKTAPE_PKG_LINK_LIBRARIES)
              list(GET DUKTAPE_PKG_LINK_LIBRARIES 0 DUKTAPE_LIBRARY)
              list(REMOVE_AT DUKTAPE_PKG_LINK_LIBRARIES 0)
              set(DUKTAPE_EXTRA_LIBRARIES ${DUKTAPE_PKG_LINK_LIBRARIES})
            endif()
          endif()
        endif()
      endif()
    endif()

    if(NOT DUKTAPE_INCLUDE_DIR OR NOT DUKTAPE_LIBRARY)
      if(USE_SYSTEM_DUKTAPE)
        message(WARNING "System Duktape requested but headers/library not found; JavaScript scripting support will be disabled.")
      else()
        message(WARNING "Vendored Duktape not found and system headers/library not found; JavaScript scripting support will be disabled.")
      endif()
    else()
      set(DUKTAPE_FOUND TRUE)
      message(STATUS "Duktape support enabled: system library ${DUKTAPE_LIBRARY}")
      list(APPEND DUKTAPE_LINK_LIBRARIES ${DUKTAPE_LIBRARY})
      if(DUKTAPE_EXTRA_LIBRARIES)
        list(APPEND DUKTAPE_LINK_LIBRARIES ${DUKTAPE_EXTRA_LIBRARIES})
      endif()
    endif()
  endif()
endif()

if(USE_CSHARP AND NOT ANDROID)
  find_path(MONO_INCLUDE_DIR
    NAMES mono/jit/jit.h
    PATH_SUFFIXES include include/mono-2.0
    PATHS
      $ENV{MONO_PREFIX}
      /usr
      /usr/local
      /opt/homebrew
      /opt/local
  )
  find_library(MONO_LIBRARY
    NAMES mono-2.0-sgen mono-2.0
    PATH_SUFFIXES lib64 lib
    PATHS
      $ENV{MONO_PREFIX}
      /usr
      /usr/local
      /opt/homebrew
      /opt/local
  )
  if(MONO_INCLUDE_DIR AND MONO_LIBRARY)
    set(CSHARP_FOUND TRUE)
    set(CSHARP_INCLUDE_DIRS "${MONO_INCLUDE_DIR}")
    list(APPEND CSHARP_LINK_LIBRARIES "${MONO_LIBRARY}")
    message(STATUS "C# scripting enabled: Mono ${MONO_LIBRARY}")
  else()
    message(WARNING "USE_CSHARP=ON but Mono not found (install libmono-2.0-dev mono-devel); C# scripting disabled.")
  endif()
endif()

if(USE_PYTHON AND NOT ANDROID)
  find_package(Python3 COMPONENTS Development)
  if(Python3_Development_FOUND)
    set(PYTHON_FOUND TRUE)
    set(PYTHON_INCLUDE_DIRS "${Python3_INCLUDE_DIRS}")
    list(APPEND PYTHON_LINK_LIBRARIES Python3::Python)
    message(STATUS "Python scripting enabled: ${Python3_VERSION} (${Python3_EXECUTABLE})")
  else()
    message(WARNING "USE_PYTHON=ON but Python3 Development not found (install python3-dev); Python scripting disabled.")
  endif()
endif()

set(OPUS_FOUND FALSE)
set(FLAC_FOUND FALSE)
set(WEBM_FOUND FALSE)
set(OPUS_INCLUDE_DIR "")
set(OPUSFILE_INCLUDE_DIR "")
set(OPUSFILE_LIBRARY "")
set(OGG_LIBRARY "")
set(OGG_INCLUDE_DIR "")
set(FLAC_INCLUDE_DIR "")
set(FLAC_LIBRARY "")
set(NESTEGG_INCLUDE_DIR "")
set(NESTEGG_LIBRARY "")

# Prefer vendored Ogg/Opus/Opusfile/FLAC/Nestegg when present.
if(USE_OPUS OR USE_FLAC OR USE_WEBM)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/ogg/src/bitwise.c")
    if(NOT TARGET ogg)
      add_library(ogg STATIC
        third_party/src/ogg/src/bitwise.c
        third_party/src/ogg/src/framing.c
      )
      target_include_directories(ogg PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/ogg/include)
      target_compile_definitions(ogg PRIVATE INCLUDE_INTTYPES_H=1 INCLUDE_SYS_TYPES_H=1)
      if(NOT MSVC)
        target_compile_options(ogg PRIVATE -Wno-undef)
      endif()
    endif()
    set(OGG_LIBRARY ogg)
    set(OGG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/ogg/include)
  endif()
endif()

if(USE_OPUS)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/opus/CMakeLists.txt")
    set(OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
    set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(OPUS_BUILD_FRAMEWORK OFF CACHE BOOL "" FORCE)
    set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
    add_subdirectory(third_party/src/opus ${CMAKE_BINARY_DIR}/external/opus)
    set(SKIP_INSTALL_ALL OFF CACHE BOOL "" FORCE)
    set(OPUS_LIBRARY opus)
    set(OPUS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/opus/include)
    if(TARGET opus)
      target_compile_definitions(opus PRIVATE EMBEDDED_ARM=0)
      if(NOT MSVC)
        target_compile_options(opus PRIVATE
          -Wno-vla
          -Wno-cast-align
          -Wno-undef
          -Wno-cast-qual
          -Wno-missing-prototypes
          -Wno-sign-compare
        )
        if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
          target_compile_options(opus PRIVATE -Wno-stringop-overflow)
        endif()
      endif()
    endif()
  endif()
endif()

if(USE_OPUS)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/opusfile/src/opusfile.c")
    if(NOT TARGET opusfile)
      add_library(opusfile STATIC
        third_party/src/opusfile/src/info.c
        third_party/src/opusfile/src/internal.c
        third_party/src/opusfile/src/opusfile.c
        third_party/src/opusfile/src/stream.c
      )
      target_compile_definitions(opusfile PRIVATE OP_DISABLE_HTTP INCLUDE_INTTYPES_H=1 INCLUDE_SYS_TYPES_H=1)
      target_include_directories(opusfile PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/opusfile/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/opusfile
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/ogg/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/opus/include
      )
      if(NOT MSVC)
        target_compile_options(opusfile PRIVATE
          -Wno-undef
          -Wno-cast-qual
        )
        if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
          target_compile_options(opusfile PRIVATE -Wno-maybe-uninitialized)
        endif()
      endif()
      if(OGG_LIBRARY)
        target_link_libraries(opusfile PRIVATE ${OGG_LIBRARY})
      endif()
      if(OPUS_LIBRARY)
        target_link_libraries(opusfile PRIVATE ${OPUS_LIBRARY})
      endif()
    endif()
    set(OPUSFILE_LIBRARY opusfile)
    set(OPUSFILE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/opusfile/include)
  endif()
endif()

if(USE_FLAC)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/flac/CMakeLists.txt")
    set(BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(BUILD_CXXLIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(WITH_OGG OFF CACHE BOOL "" FORCE)
    set(INSTALL_MANPAGES OFF CACHE BOOL "" FORCE)
    set(INSTALL_PKGCONFIG_MODULES OFF CACHE BOOL "" FORCE)
    set(INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
    set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
    add_subdirectory(third_party/src/flac ${CMAKE_BINARY_DIR}/external/flac)
    set(SKIP_INSTALL_ALL OFF CACHE BOOL "" FORCE)
    if(TARGET FLAC::FLAC)
      set(FLAC_LIBRARY FLAC::FLAC)
    elseif(TARGET FLAC)
      set(FLAC_LIBRARY FLAC)
    endif()
    if(TARGET FLAC AND NOT MSVC)
      target_compile_options(FLAC PRIVATE -Wno-cast-qual -Wno-format-nonliteral -Wno-static-in-inline -Wno-gnu-zero-variadic-macro-arguments)
    endif()
    if(TARGET grabbag AND NOT MSVC)
      target_compile_options(grabbag PRIVATE -Wno-cast-qual -Wno-format-nonliteral)
    endif()
    if(TARGET getopt AND NOT MSVC)
      target_compile_options(getopt PRIVATE -Wno-cast-qual -Wno-gnu-zero-variadic-macro-arguments)
    endif()
    set(FLAC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/flac/include)
    if(TARGET benchmark_residual)
      set_property(TARGET benchmark_residual PROPERTY EXCLUDE_FROM_ALL TRUE)
      if(NOT MSVC)
        target_compile_options(benchmark_residual PRIVATE -Wno-vla -Wno-cast-qual)
      endif()
    endif()
    if(TARGET utf8)
      if(NOT MSVC)
        target_compile_options(utf8 PRIVATE -Wno-pedantic -Wno-cast-qual)
      endif()
    endif()
  endif()
endif()

if(USE_WEBM)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/nestegg/src/nestegg.c")
    if(NOT TARGET nestegg)
      add_library(nestegg STATIC
        third_party/src/nestegg/src/nestegg.c
        third_party/src/nestegg/halloc/src/halloc.c
      )
      target_include_directories(nestegg PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/nestegg
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/nestegg/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/nestegg/halloc
      )
      if(NOT MSVC)
        target_compile_options(nestegg PRIVATE -Wno-unused-function)
      endif()
    endif()
    set(NESTEGG_LIBRARY nestegg)
    set(NESTEGG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/nestegg/include)
  endif()
endif()

#
if(USE_OPUS AND NOT OPUS_LIBRARY)
  find_path(OPUSFILE_INCLUDE_DIR
    NAMES opus/opusfile.h
    PATH_SUFFIXES include
    PATHS ${CMAKE_PREFIX_PATH} /usr/local /usr /opt/local /opt /opt/homebrew
  )
  find_library(OPUSFILE_LIBRARY
    NAMES opusfile
    PATH_SUFFIXES lib64 lib
    PATHS ${CMAKE_PREFIX_PATH} /usr/local /usr /opt/local /opt /opt/homebrew
  )
  find_library(OPUS_LIBRARY
    NAMES opus
    PATH_SUFFIXES lib64 lib
    PATHS ${CMAKE_PREFIX_PATH} /usr/local /usr /opt/local /opt /opt/homebrew
  )
  find_library(OGG_LIBRARY
    NAMES ogg
    PATH_SUFFIXES lib64 lib
    PATHS ${CMAKE_PREFIX_PATH} /usr/local /usr /opt/local /opt /opt/homebrew
  )
endif()
if(USE_OPUS AND OPUSFILE_INCLUDE_DIR AND OPUSFILE_LIBRARY AND OPUS_LIBRARY AND OGG_LIBRARY)
  set(OPUS_FOUND TRUE)
endif()

if(USE_OPUS OR USE_WEBM)
  find_path(OPUS_INCLUDE_DIR
    NAMES opus/opus.h
    PATH_SUFFIXES include
    PATHS ${CMAKE_PREFIX_PATH} /usr/local /usr /opt/local /opt /opt/homebrew
  )
  if(NOT OPUS_INCLUDE_DIR AND OPUSFILE_INCLUDE_DIR)
    set(OPUS_INCLUDE_DIR ${OPUSFILE_INCLUDE_DIR})
  endif()
endif()

if(USE_FLAC AND NOT FLAC_LIBRARY)
  find_path(FLAC_INCLUDE_DIR
    NAMES FLAC/stream_decoder.h
    PATH_SUFFIXES include
    PATHS ${CMAKE_PREFIX_PATH} /usr/local /usr /opt/local /opt /opt/homebrew
  )
  find_library(FLAC_LIBRARY
    NAMES FLAC flac
    PATH_SUFFIXES lib64 lib
    PATHS ${CMAKE_PREFIX_PATH} /usr/local /usr /opt/local /opt /opt/homebrew
  )
endif()
if(USE_FLAC AND FLAC_INCLUDE_DIR AND FLAC_LIBRARY)
  set(FLAC_FOUND TRUE)
endif()

if(USE_WEBM AND NOT NESTEGG_LIBRARY)
  find_path(NESTEGG_INCLUDE_DIR
    NAMES nestegg/nestegg.h
    PATH_SUFFIXES include
    PATHS ${EXTERNAL_SRC}/nestegg ${EXTERNAL_INCLUDE}
  )
  find_library(NESTEGG_LIBRARY
    NAMES nestegg
    PATH_SUFFIXES lib64 lib
    PATHS ${EXTERNAL_SRC}/nestegg ${EXTERNAL_ROOT}
  )
endif()
if(USE_WEBM AND NESTEGG_INCLUDE_DIR AND NESTEGG_LIBRARY AND USE_OPUS AND OPUS_LIBRARY AND OPUS_INCLUDE_DIR)
  set(WEBM_FOUND TRUE)
endif()

# If vendored targets were created, mark codecs as found.
if(USE_OPUS AND TARGET opus AND TARGET opusfile AND TARGET ogg)
  set(OPUS_LIBRARY opus)
  set(OPUSFILE_LIBRARY opusfile)
  set(OGG_LIBRARY ogg)
  if(NOT OPUS_INCLUDE_DIR)
    set(OPUS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/opus/include)
  endif()
  if(NOT OPUSFILE_INCLUDE_DIR)
    set(OPUSFILE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/opusfile/include)
  endif()
  if(NOT OGG_INCLUDE_DIR)
    set(OGG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/ogg/include)
  endif()
  set(OPUS_FOUND TRUE)
endif()

if(USE_FLAC AND (TARGET FLAC::FLAC OR TARGET FLAC))
  if(NOT FLAC_LIBRARY)
    if(TARGET FLAC::FLAC)
      set(FLAC_LIBRARY FLAC::FLAC)
    else()
      set(FLAC_LIBRARY FLAC)
    endif()
  endif()
  if(NOT FLAC_INCLUDE_DIR)
    set(FLAC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/flac/include)
  endif()
  set(FLAC_FOUND TRUE)
endif()

if(USE_WEBM AND TARGET nestegg AND USE_OPUS AND OPUS_LIBRARY AND OPUS_INCLUDE_DIR)
  if(NOT NESTEGG_LIBRARY)
    set(NESTEGG_LIBRARY nestegg)
  endif()
  if(NOT NESTEGG_INCLUDE_DIR)
    set(NESTEGG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/src/nestegg/include)
  endif()
  set(WEBM_FOUND TRUE)
endif()

SET(CNAME "idtech3")
SET(DNAME "idtech3_server")

SET(RENDERER_PREFIX ${CNAME})
SET(RENDERER_DEFAULT vulkan CACHE STRING "Renderer backend; Vulkan is the only supported renderer.")
if(NOT RENDERER_DEFAULT STREQUAL "vulkan")
  message(FATAL_ERROR "OpenGL has been removed; set RENDERER_DEFAULT=vulkan.")
endif()
add_compile_definitions(RENDERER_PREFIX="${RENDERER_PREFIX}" RENDERER_DEFAULT="${RENDERER_DEFAULT}")

OPTION(USE_SDL "" ON)
# When no system SDL3 is detected, download and build SDL3 from the official release tarball.
option(USE_BUNDLED_SDL3 "Build SDL3 from source if system SDL3 is not found (requires network on first configure)" ON)
OPTION(USE_OPENAL "Enable OpenAL audio backend" ON)
OPTION(USE_CURL "" ON)
OPTION(USE_LIBTORRENT "Enable optional peer-assisted package delivery through libtorrent-rasterbar" OFF)
OPTION(USE_LOCAL_HEADERS "" ON)

# libcurl (HTTP/HTTPS downloads). Linked into client when found.
set(CURL_FOUND FALSE)
set(CURL_LIBRARIES "")
set(CURL_INCLUDE_DIRS "")
if(USE_CURL)
  if(ANDROID)
    # Restrict search to the NDK sysroot only. NO_CMAKE_FIND_ROOT_PATH would search the
    # host (e.g. /usr/include) and pull in glibc headers while compiling for Android.
    find_path(CURL_INCLUDE_DIR
      NAMES curl/curl.h
      PATHS "${CMAKE_SYSROOT}/usr/include"
      NO_DEFAULT_PATH)
    if(CURL_INCLUDE_DIR)
      set(_CURL_ABI_DIR "")
      if(ANDROID_ABI STREQUAL "arm64-v8a")
        set(_CURL_ABI_DIR "aarch64-linux-android")
      elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
        set(_CURL_ABI_DIR "arm-linux-androideabi")
      endif()
      set(_CURL_LIB_HINTS "")
      if(_CURL_ABI_DIR AND DEFINED ANDROID_PLATFORM_LEVEL)
        list(APPEND _CURL_LIB_HINTS
          "${CMAKE_SYSROOT}/usr/lib/${_CURL_ABI_DIR}/${ANDROID_PLATFORM_LEVEL}"
          "${CMAKE_SYSROOT}/usr/lib/${_CURL_ABI_DIR}")
      endif()
      find_library(CURL_LIBRARY
        NAMES curl libcurl
        PATHS ${_CURL_LIB_HINTS}
        NO_DEFAULT_PATH)
    endif()
    if(CURL_INCLUDE_DIR AND CURL_LIBRARY)
      set(CURL_FOUND TRUE)
      set(CURL_INCLUDE_DIRS "${CURL_INCLUDE_DIR}")
      set(CURL_LIBRARIES "${CURL_LIBRARY}")
      message(STATUS "libcurl (Android): ${CURL_LIBRARY}")
    else()
      message(WARNING "USE_CURL is ON but libcurl was not found under the NDK sysroot. Install NDK curl or disable USE_CURL.")
    endif()
  else()
    find_package(CURL QUIET)
    if(CURL_FOUND)
      set(CURL_INCLUDE_DIRS ${CURL_INCLUDE_DIRS})
      set(CURL_LIBRARIES ${CURL_LIBRARIES})
      message(STATUS "libcurl: ${CURL_LIBRARIES}")
    else()
      message(WARNING "USE_CURL is ON but CMake could not find CURL. Install libcurl development packages or disable USE_CURL.")
    endif()
  endif()
endif()

# libtorrent-rasterbar (optional peer-assisted content delivery). The client
# still builds the control/stub path when the dependency is not present.
set(LIBTORRENT_FOUND FALSE)
set(LIBTORRENT_TARGET "")
if(USE_LIBTORRENT)
  find_package(LibtorrentRasterbar QUIET CONFIG)
  if(TARGET LibtorrentRasterbar::torrent-rasterbar)
    set(LIBTORRENT_FOUND TRUE)
    set(LIBTORRENT_TARGET LibtorrentRasterbar::torrent-rasterbar)
  elseif(TARGET libtorrent-rasterbar::torrent-rasterbar)
    set(LIBTORRENT_FOUND TRUE)
    set(LIBTORRENT_TARGET libtorrent-rasterbar::torrent-rasterbar)
  else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
      pkg_check_modules(LIBTORRENT_RASTERBAR QUIET IMPORTED_TARGET libtorrent-rasterbar)
      if(TARGET PkgConfig::LIBTORRENT_RASTERBAR)
        set(LIBTORRENT_FOUND TRUE)
        set(LIBTORRENT_TARGET PkgConfig::LIBTORRENT_RASTERBAR)
      endif()
    endif()
  endif()
  if(LIBTORRENT_FOUND)
    message(STATUS "libtorrent-rasterbar: ${LIBTORRENT_TARGET}")
  else()
    message(WARNING "USE_LIBTORRENT is ON but libtorrent-rasterbar was not found; peer-assisted package delivery will build as controls/stub only.")
  endif()
endif()
OPTION(USE_VULKAN "" ON)
if(NOT USE_VULKAN)
  message(FATAL_ERROR "USE_VULKAN=OFF is no longer supported; Vulkan is the only renderer backend.")
endif()
OPTION(USE_MP3 "Enable MP3 codec support" ON)
#
OPTION(USE_ZLIB "Enable zlib support (vendored)" ON)
OPTION(USE_LIBPNG "Enable libpng support (vendored)" ON)
OPTION(USE_GLM "Enable glm headers (vendored)" ON)
OPTION(USE_SYSTEM_JPEG "" OFF)
OPTION(USE_RENDERER_DLOPEN "" ON)
if(WIN32 AND USE_RENDERER_DLOPEN)
  message(STATUS "Renderer dlopen disabled on Windows targets; switching to static renderer builds")
  set(USE_RENDERER_DLOPEN OFF CACHE BOOL "" FORCE)
endif()
if(USE_RENDERER_DLOPEN)
  add_compile_definitions(USE_RENDERER_DLOPEN=1)
else()
  add_compile_definitions(USE_RENDERER_DLOPEN=0)
endif()
if(USE_SDL)
  add_compile_definitions(USE_SDL=1)
else()
  add_compile_definitions(USE_SDL=0)
endif()

# Set when FetchContent-built SDL3 is used.
set(IDTECH3_USING_FETCHED_SDL3 OFF)
if(USE_SDL AND NOT SDL3_FOUND AND USE_BUNDLED_SDL3 AND NOT ANDROID AND NOT MSVC)
  set(_IDTECH3_SDL3_VER "3.2.22")
  set(_IDTECH3_SDL3_SHA256 "f29d00cbcee273c0a54f3f32f86bf5c595e8823a96b1d92a145aac40571ebfcc")
  include(FetchContent)
  FetchContent_Declare(
    idtech3_sdl3
    URL "https://github.com/libsdl-org/SDL/releases/download/release-${_IDTECH3_SDL3_VER}/SDL3-${_IDTECH3_SDL3_VER}.tar.gz"
    URL_HASH SHA256=${_IDTECH3_SDL3_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
  # Prefer shared SDL3: avoids MatrixMultiply clash with engine q_math.c and
  # allows PIC-safe linking into idtech3_vulkan.so (renderer dlopen).
  set(SDL_SHARED ON CACHE BOOL "" FORCE)
  set(SDL_STATIC OFF CACHE BOOL "" FORCE)
  set(SDL_GPU OFF CACHE BOOL "" FORCE)
  message(STATUS "SDL3: system library not found; building SDL ${_IDTECH3_SDL3_VER} via FetchContent (one-time download)")
  FetchContent_MakeAvailable(idtech3_sdl3)
  if(NOT TARGET SDL3::SDL3)
    message(FATAL_ERROR "SDL3 FetchContent: expected target SDL3::SDL3 after building bundled SDL3.")
  endif()
  set(SDL3_FOUND TRUE)
  set(IDTECH3_USING_FETCHED_SDL3 ON)
  FetchContent_GetProperties(idtech3_sdl3 SOURCE_DIR _idtech3_sdl3_src)
  message(STATUS "SDL3: bundled build enabled (from ${_idtech3_sdl3_src})")
endif()

OPTION(SKIP_IDPAK_CHECK "Skip checking for original Quake III pak files (pak0.pk3)" ON)
OPTION(BUILD_FREETYPE "Enable FreeType-powered font rendering / font generation" ON)
if(BUILD_FREETYPE)
	include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/DownloadInterFonts.cmake)
endif()
OPTION(USE_DTLS "Enable DTLS encryption for game network traffic (OpenSSL)" ON)
OPTION(USE_STEAM "Enable Steam API (achievements, overlay, rich presence, Deck detection)" ON)
OPTION(USE_STEAM_NETWORKING "Enable Steam Datagram Relay (SDR) transport" OFF)
# Default SDK root for this machine; override with -DSTEAMWORKS_SDK= or env. Empty is fine on CI.
if(DEFINED ENV{STEAMWORKS_SDK} AND NOT "$ENV{STEAMWORKS_SDK}" STREQUAL "")
	set(_STEAMWORKS_SDK_DEFAULT "$ENV{STEAMWORKS_SDK}")
elseif(EXISTS "/home/tim/SteamWorks/sdk/public/steam/steam_api.h")
	set(_STEAMWORKS_SDK_DEFAULT "/home/tim/SteamWorks/sdk")
else()
	set(_STEAMWORKS_SDK_DEFAULT "")
endif()
set(STEAMWORKS_SDK "${_STEAMWORKS_SDK_DEFAULT}" CACHE PATH "Steamworks SDK root (contains public/steam and redistributable_bin)")
unset(_STEAMWORKS_SDK_DEFAULT)
OPTION(USE_VULKAN_RTX "Enable Vulkan ray tracing (VK_KHR_ray_tracing_*); requires RT-capable GPU" OFF)
OPTION(USE_EXPERIMENTAL_RENDERERS "Neural/scaffold renderer paths (NIV, RenderFormer, VkSplat, MGS, WPT, etc.)" ON)
OPTION(USE_VUDA "CUDA-Vulkan spatial multiplexing interop (Linux + NVIDIA; KHR external memory fd)" OFF)
OPTION(USE_MIMIR_CUDA "Mímir CUDA/Vulkan interop for r_mimir_cuda (Linux + NVIDIA)" OFF)
OPTION(USE_RADIUSFPS_CUDA "RadiusFPS-G CUDA farthest point sampling (Linux + NVIDIA)" OFF)
if(USE_VUDA)
  if(APPLE OR WIN32)
    message(WARNING "USE_VUDA: primary target is Linux+NVIDIA; disabling on this platform.")
    set(USE_VUDA OFF CACHE BOOL "" FORCE)
  endif()
endif()
if(USE_MIMIR_CUDA)
  if(APPLE OR WIN32)
    message(WARNING "USE_MIMIR_CUDA: primary target is Linux+NVIDIA; disabling on this platform.")
    set(USE_MIMIR_CUDA OFF CACHE BOOL "" FORCE)
  endif()
endif()
if(USE_RADIUSFPS_CUDA)
  if(APPLE OR WIN32)
    message(WARNING "USE_RADIUSFPS_CUDA: primary target is Linux+NVIDIA; disabling on this platform.")
    set(USE_RADIUSFPS_CUDA OFF CACHE BOOL "" FORCE)
  endif()
endif()

include(${CMAKE_SOURCE_DIR}/cmake/IdTech3Layout.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/IdTech3Profile.cmake)
option(IDTECH3_EXPORT_MSVC_MANIFEST "Write build/msvc_source_manifest.json from target SOURCES (Phase 5d)" OFF)
if(EXISTS "${CMAKE_SOURCE_DIR}/cmake/profiles/${IDTECH3_PROFILE}.cmake")
  include("${CMAKE_SOURCE_DIR}/cmake/profiles/${IDTECH3_PROFILE}.cmake")
endif()

OPTION(USE_METAL_RENDERER "Build native Metal renderer (Apple only)" OFF)
OPTION(USE_DXR_RENDERER "Build DirectX 12 + DXR renderer (Windows only)" OFF)

if(USE_METAL_RENDERER)
  if(NOT APPLE)
    message(WARNING "USE_METAL_RENDERER is Apple-only; disabling.")
    set(USE_METAL_RENDERER OFF CACHE BOOL "" FORCE)
  else()
    message(STATUS "Metal roadmap scaffold enabled (idtech3_metal plugin; not shippable — see docs/METAL_RENDERER.md)")
  endif()
endif()
if(USE_DXR_RENDERER)
  if(NOT WIN32)
    message(WARNING "USE_DXR_RENDERER is Windows-only; disabling.")
    set(USE_DXR_RENDERER OFF CACHE BOOL "" FORCE)
  else()
    message(STATUS "DXR roadmap scaffold enabled (idtech3_dxr plugin; not shippable — see docs/DXR_RENDERER.md)")
  endif()
endif()

set(EXTERNAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
set(EXTERNAL_SRC "${EXTERNAL_ROOT}/src")
set(EXTERNAL_INCLUDE "${EXTERNAL_ROOT}/include")
# Legacy vendored SDL2 headers are not used with SDL3 (system or FetchContent).
set(EXTERNAL_SDL_INCLUDE_DIR "")
set(EXTERNAL_CURL_WINDOWS_INCLUDE_DIR "${EXTERNAL_SRC}/curl/windows/include")
set(EXTERNAL_JPEG_SRC_DIR "${EXTERNAL_SRC}/jpeg")
set(EXTERNAL_ZLIB_SRC "${EXTERNAL_SRC}/zlib")
set(EXTERNAL_LIBPNG_SRC "${EXTERNAL_SRC}/libpng")
set(EXTERNAL_GLM_INCLUDE "${EXTERNAL_SRC}/glm")

if(UNIX AND NOT APPLE AND NOT ANDROID)
  find_package(X11 REQUIRED)
  if(TARGET X11::X11)
    set(IDTECH3_X11_LIBS X11::X11)
  else()
    set(IDTECH3_X11_LIBS ${X11_LIBRARIES})
  endif()
  set(IDTECH3_X11_INCLUDE ${X11_INCLUDE_DIR})
else()
  set(IDTECH3_X11_LIBS "")
  set(IDTECH3_X11_INCLUDE "")
endif()

# Add cflux2 external library
if(USE_FLUX)
	add_subdirectory(third_party/src/cflux2)
endif()

if(USE_ZLIB)
  set(ZLIB_BUILD_SHARED OFF CACHE BOOL "" FORCE)
  set(ZLIB_BUILD_STATIC ON CACHE BOOL "" FORCE)
  set(ZLIB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
  if(EXISTS "${EXTERNAL_ZLIB_SRC}/CMakeLists.txt")
    add_subdirectory(${EXTERNAL_ZLIB_SRC} ${CMAKE_BINARY_DIR}/external/zlib)
    if(NOT TARGET ZLIB::ZLIB)
      add_library(ZLIB::ZLIB ALIAS zlibstatic)
    endif()
    if(TARGET zlibstatic)
      target_compile_definitions(zlibstatic PRIVATE ZLIB_CONST)
      target_compile_definitions(zlibstatic PRIVATE _LFS64_LARGEFILE=1 _LARGEFILE64_SOURCE=1)
      set_property(TARGET zlibstatic PROPERTY POSITION_INDEPENDENT_CODE ON)
      if(NOT MSVC)
        target_compile_options(zlibstatic PRIVATE -Wno-format-nonliteral)
      endif()
    endif()
    if(TARGET zlib)
      target_compile_definitions(zlib PRIVATE ZLIB_CONST)
      target_compile_definitions(zlib PRIVATE _LFS64_LARGEFILE=1 _LARGEFILE64_SOURCE=1)
      set_property(TARGET zlib PROPERTY POSITION_INDEPENDENT_CODE ON)
    endif()
  endif()
  set(SKIP_INSTALL_ALL OFF CACHE BOOL "" FORCE)
endif()

if(USE_LIBPNG)
  if(NOT USE_ZLIB)
    message(FATAL_ERROR "libpng requires zlib; enable USE_ZLIB.")
  endif()
  set(PNG_SHARED OFF CACHE BOOL "" FORCE)
  set(PNG_STATIC ON CACHE BOOL "" FORCE)
  set(PNG_TESTS OFF CACHE BOOL "" FORCE)
  set(PNG_TOOLS OFF CACHE BOOL "" FORCE)
  set(PNG_BUILD_ZLIB OFF CACHE BOOL "" FORCE)
  set(ZLIB_ROOT "${CMAKE_BINARY_DIR}/external/zlib" CACHE PATH "" FORCE)
  set(ZLIB_INCLUDE_DIR "${EXTERNAL_ZLIB_SRC}" CACHE PATH "" FORCE)
  set(ZLIB_INCLUDE_DIRS "${EXTERNAL_ZLIB_SRC};${CMAKE_BINARY_DIR}/external/zlib" CACHE PATH "" FORCE)
  set(ZLIB_LIBRARY "${CMAKE_BINARY_DIR}/external/zlib/${CMAKE_STATIC_LIBRARY_PREFIX}z${CMAKE_STATIC_LIBRARY_SUFFIX}" CACHE FILEPATH "" FORCE)
  set(ZLIB_LIBRARY_RELEASE "${CMAKE_BINARY_DIR}/external/zlib/${CMAKE_STATIC_LIBRARY_PREFIX}z${CMAKE_STATIC_LIBRARY_SUFFIX}" CACHE FILEPATH "" FORCE)
  set(ZLIB_LIBRARY_DEBUG "${CMAKE_BINARY_DIR}/external/zlib/${CMAKE_STATIC_LIBRARY_PREFIX}z${CMAKE_STATIC_LIBRARY_SUFFIX}" CACHE FILEPATH "" FORCE)
  set(ZLIB_LIBRARIES "${CMAKE_BINARY_DIR}/external/zlib/${CMAKE_STATIC_LIBRARY_PREFIX}z${CMAKE_STATIC_LIBRARY_SUFFIX}" CACHE FILEPATH "" FORCE)
  set(ZLIB_FOUND TRUE CACHE BOOL "" FORCE)
  set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
  set(SKIP_INSTALL_EXPORT ON CACHE BOOL "" FORCE)
  if(EXISTS "${EXTERNAL_LIBPNG_SRC}/CMakeLists.txt")
    add_subdirectory(${EXTERNAL_LIBPNG_SRC} ${CMAKE_BINARY_DIR}/external/libpng)
    if(TARGET png_static)
      target_include_directories(png_static PRIVATE ${EXTERNAL_ZLIB_SRC})
      target_link_libraries(png_static PRIVATE zlibstatic)
      set_property(TARGET png_static PROPERTY POSITION_INDEPENDENT_CODE ON)
    endif()
  endif()
  set(SKIP_INSTALL_EXPORT OFF CACHE BOOL "" FORCE)
  set(SKIP_INSTALL_ALL OFF CACHE BOOL "" FORCE)
endif()

if(BUILD_FREETYPE)
  if(ANDROID)
    find_package(Freetype QUIET)
    if(NOT Freetype_FOUND)
      include(FetchContent)
      set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
      set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
      set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
      set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
      set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
      FetchContent_Declare(freetype_android_fc
        GIT_REPOSITORY https://github.com/freetype/freetype.git
        GIT_TAG VER-2-13-2
        GIT_SHALLOW TRUE
      )
      FetchContent_MakeAvailable(freetype_android_fc)
    endif()
  else()
    find_package(Freetype REQUIRED)
  endif()
  add_library(idtech3_freetype INTERFACE)
  target_compile_definitions(idtech3_freetype INTERFACE BUILD_FREETYPE)
  if(TARGET Freetype::Freetype)
    target_link_libraries(idtech3_freetype INTERFACE Freetype::Freetype)
  elseif(TARGET freetype)
    target_link_libraries(idtech3_freetype INTERFACE freetype)
  else()
    target_include_directories(idtech3_freetype INTERFACE ${FREETYPE_INCLUDE_DIRS})
    target_link_libraries(idtech3_freetype INTERFACE ${FREETYPE_LIBRARIES})
  endif()
endif()

include(${CMAKE_SOURCE_DIR}/cmake/EngineQcommonSources.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/engine/AsmSources.cmake)
idtech3_init_qcommon_sources()
include(${CMAKE_SOURCE_DIR}/cmake/IdTech3Extension.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/IdTech3IncludeDirs.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/IdTech3QcommonExtensions.cmake)
idtech3_append_research_qcommon_sources()
idtech3_append_open_world_qcommon_sources()
# exclude platform-dependent QVM JIT compilers; add the active backend below
list(FILTER QCOMMON_SRCS EXCLUDE REGEX ".*vm_(aarch64|armv7l|powerpc|x86)\\.c")
# exclude optional network modules; add when options enabled
list(FILTER QCOMMON_SRCS EXCLUDE REGEX ".*net_dtls\\.c")
list(FILTER QCOMMON_SRCS EXCLUDE REGEX ".*net_sdr\\.c")

# USE_DTLS: DTLS encryption (requires OpenSSL)
set(USE_DTLS_FOUND FALSE)
set(USE_DTLS_ANDROID_BUILT FALSE)
set(OPENSSL_ANDROID_INTERFACE_TARGET "")
if(USE_DTLS)
  find_package(OpenSSL QUIET)
  if(OpenSSL_FOUND)
    list(APPEND QCOMMON_SRCS engine/core/net_dtls.c)
    set(USE_DTLS_FOUND TRUE)
    message(STATUS "DTLS: enabled (OpenSSL ${OPENSSL_VERSION})")
  elseif(ANDROID)
    option(OPENSSL_ANDROID_AUTOBUILD "Build static OpenSSL from source for Android when find_package fails" ON)
    if(OPENSSL_ANDROID_AUTOBUILD)
      find_program(OPENSSL_ANDROID_MAKE_EXE NAMES make gmake)
      find_program(OPENSSL_ANDROID_PERL_EXE NAMES perl)
      if(NOT OPENSSL_ANDROID_MAKE_EXE OR NOT OPENSSL_ANDROID_PERL_EXE)
        message(WARNING "USE_DTLS on Android needs Perl + GNU make to build OpenSSL; install or set OPENSSL_ROOT_DIR. DTLS disabled.")
      else()
        include(ExternalProject)
        set(_OSSL_VER "3.0.15")
        set(_OSSL_SHA "23c666d0edf20f14249b3d8f0368acaee9ab585b09e1de82107c66e1f3ec9533")
        if(ANDROID_ABI STREQUAL "arm64-v8a")
          set(_OSSL_OS "android-arm64")
        elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
          set(_OSSL_OS "android-arm")
        else()
          set(_OSSL_OS "android-${ANDROID_ABI}")
        endif()
        set(_OSSL_PREFIX "${CMAKE_BINARY_DIR}/openssl-android-${ANDROID_ABI}")
        set(_OSSL_API "${ANDROID_PLATFORM_LEVEL}")
        if(NOT _OSSL_API OR _OSSL_API STREQUAL "")
          set(_OSSL_API 26)
        endif()
        set(_OSSL_DL_EXTRA "")
        if(ANDROID_DEPS_CACHE AND NOT ANDROID_DEPS_CACHE STREQUAL "")
          set(_OSSL_DL "${ANDROID_DEPS_CACHE}/externalproject-downloads")
          file(MAKE_DIRECTORY "${_OSSL_DL}")
          set(_OSSL_DL_EXTRA DOWNLOAD_DIR "${_OSSL_DL}")
        endif()
        set(_NDK_ROOT "$ENV{ANDROID_NDK}")
        if(NOT _NDK_ROOT AND DEFINED ANDROID_NDK)
          set(_NDK_ROOT "${ANDROID_NDK}")
        endif()
        if(NOT _NDK_ROOT OR _NDK_ROOT STREQUAL "")
          message(WARNING "USE_DTLS on Android: ANDROID_NDK not set; OpenSSL Configure may fail.")
        endif()
        # OpenSSL 3 Configure expects NDK clang/gcc on PATH (NOTES-ANDROID.md). CMake's
        # ANDROID_NDK_HOME / toolchain file do not put toolchains/llvm/prebuilt/.../bin on PATH.
        set(_OSSL_PATH "$ENV{PATH}")
        if(_NDK_ROOT AND IS_DIRECTORY "${_NDK_ROOT}/toolchains/llvm/prebuilt")
          file(GLOB _ndk_llvm_prebuilt "${_NDK_ROOT}/toolchains/llvm/prebuilt/*")
          list(LENGTH _ndk_llvm_prebuilt _ndk_pb_len)
          if(_ndk_pb_len GREATER 0)
            list(GET _ndk_llvm_prebuilt 0 _ndk_llvm_hostdir)
            set(_ndk_llvm_bin "${_ndk_llvm_hostdir}/bin")
            if(EXISTS "${_ndk_llvm_bin}")
              if(CMAKE_HOST_WIN32)
                set(_OSSL_PATH "${_ndk_llvm_bin};$ENV{PATH}")
              else()
                set(_OSSL_PATH "${_ndk_llvm_bin}:$ENV{PATH}")
              endif()
            endif()
          endif()
        endif()
        ExternalProject_Add(openssl_android_ep
          URL "https://www.openssl.org/source/openssl-${_OSSL_VER}.tar.gz"
          URL_HASH "SHA256=${_OSSL_SHA}"
          DOWNLOAD_EXTRACT_TIMESTAMP TRUE
          INSTALL_DIR "${_OSSL_PREFIX}"
          ${_OSSL_DL_EXTRA}
          CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env
            "ANDROID_NDK_ROOT=${_NDK_ROOT}"
            "PATH=${_OSSL_PATH}"
            "CC=${CMAKE_C_COMPILER}"
            "AR=${CMAKE_AR}"
            "RANLIB=${CMAKE_RANLIB}"
            ${CMAKE_COMMAND} -E chdir <SOURCE_DIR>
            ${OPENSSL_ANDROID_PERL_EXE} ./Configure ${_OSSL_OS}
            no-shared no-tests
            -D__ANDROID_API__=${_OSSL_API}
            --prefix=<INSTALL_DIR>
            --openssldir=<INSTALL_DIR>/ssl
          BUILD_COMMAND ${CMAKE_COMMAND} -E env
            "ANDROID_NDK_ROOT=${_NDK_ROOT}"
            "PATH=${_OSSL_PATH}"
            ${CMAKE_COMMAND} -E chdir <SOURCE_DIR>
            ${OPENSSL_ANDROID_MAKE_EXE} build_libs
          INSTALL_COMMAND ${CMAKE_COMMAND} -E env
            "PATH=${_OSSL_PATH}"
            ${CMAKE_COMMAND} -E chdir <SOURCE_DIR>
            ${OPENSSL_ANDROID_MAKE_EXE} install_sw
          BUILD_BYPRODUCTS
            "${_OSSL_PREFIX}/lib/libssl.a"
            "${_OSSL_PREFIX}/lib/libcrypto.a"
        )
        add_library(openssl_android_iface INTERFACE)
        target_include_directories(openssl_android_iface INTERFACE "${_OSSL_PREFIX}/include")
        target_link_libraries(openssl_android_iface INTERFACE
          "${_OSSL_PREFIX}/lib/libssl.a"
          "${_OSSL_PREFIX}/lib/libcrypto.a"
          dl)
        add_dependencies(openssl_android_iface openssl_android_ep)
        list(APPEND QCOMMON_SRCS engine/core/net_dtls.c)
        set(USE_DTLS_FOUND TRUE)
        set(USE_DTLS_ANDROID_BUILT TRUE)
        set(OPENSSL_ANDROID_INTERFACE_TARGET openssl_android_iface)
        set(OPENSSL_INCLUDE_DIR "${_OSSL_PREFIX}/include")
        message(STATUS "DTLS: will build static OpenSSL ${_OSSL_VER} for Android (${_OSSL_OS}, ExternalProject)")
      endif()
    else()
      message(WARNING "USE_DTLS is ON but OpenSSL was not found for Android and OPENSSL_ANDROID_AUTOBUILD=OFF. Set OPENSSL_ROOT_DIR or enable autobuild.")
    endif()
  elseif(NOT ANDROID AND NOT OpenSSL_FOUND)
    message(WARNING "USE_DTLS is ON but OpenSSL was not found (install libssl-dev / OpenSSL SDK). DTLS disabled for this configure.")
  endif()
endif()

# USE_STEAM / USE_STEAM_NETWORKING: Steam API and SDR (requires Steamworks SDK)
set(STEAMWORKS_FOUND FALSE)
if(USE_STEAM OR USE_STEAM_NETWORKING)
  if(USE_STEAM_NETWORKING)
    list(APPEND QCOMMON_SRCS engine/core/net_sdr.c)
  endif()
  # Prefer CMake cache -DSTEAMWORKS_SDK=, then environment, then common layouts.
  if(NOT STEAMWORKS_SDK AND DEFINED ENV{STEAMWORKS_SDK})
    set(STEAMWORKS_SDK "$ENV{STEAMWORKS_SDK}" CACHE PATH "Steamworks SDK root" FORCE)
  endif()
  find_path(STEAMWORKS_INCLUDE_DIR steam/steam_api.h
    HINTS
      ${STEAMWORKS_SDK}
      ${STEAMWORKS_SDK}/public
      $ENV{STEAMWORKS_SDK}
      $ENV{STEAMWORKS_SDK}/public
    PATH_SUFFIXES include public)
  if(STEAMWORKS_INCLUDE_DIR)
    find_library(STEAMWORKS_LIBRARY steam_api
      HINTS
        ${STEAMWORKS_SDK}/redistributable_bin
        ${STEAMWORKS_SDK}
        $ENV{STEAMWORKS_SDK}/redistributable_bin
        $ENV{STEAMWORKS_SDK}
      PATH_SUFFIXES linux64 linux32 win64 osx)
    if(STEAMWORKS_LIBRARY)
      set(STEAMWORKS_FOUND TRUE)
      message(STATUS "Steamworks SDK found (${STEAMWORKS_INCLUDE_DIR}, ${STEAMWORKS_LIBRARY})")
    endif()
  endif()
  if(NOT STEAMWORKS_FOUND)
    if(USE_STEAM_NETWORKING)
      message(WARNING "USE_STEAM_NETWORKING requested but Steamworks SDK not found. Set STEAMWORKS_SDK. SDR will use stub (no-op).")
    endif()
    if(USE_STEAM)
      message(WARNING "USE_STEAM requested but Steamworks SDK not found. Set STEAMWORKS_SDK.")
    endif()
  endif()
  if(STEAMWORKS_FOUND)
    set(STEAMWORKS_AVAILABLE 1)
    # Steamworks headers are C++; compile the Steam sources as C++ with C ABI.
    set_source_files_properties(
      ${CMAKE_SOURCE_DIR}/engine/core/steam_shared.c
      ${CMAKE_SOURCE_DIR}/runtime/client/platform/cl_steam.c
      PROPERTIES LANGUAGE CXX)
    # Ship redistributable next to the binary so dlopen/rpath finds it at runtime.
    file(COPY "${STEAMWORKS_LIBRARY}" DESTINATION "${CMAKE_BINARY_DIR}")
    if(EXISTS "${CMAKE_SOURCE_DIR}/steam_appid.txt")
      file(COPY "${CMAKE_SOURCE_DIR}/steam_appid.txt" DESTINATION "${CMAKE_BINARY_DIR}")
    endif()
  else()
    set(STEAMWORKS_AVAILABLE 0)
  endif()
endif()

# add platform-dependent QVM bytecode compilers (unknown CPU = interpreter-only, NO_VM_COMPILED)
set(NO_VM_COMPILED ON)
if(CMAKE_SYSTEM_PROCESSOR MATCHES AMD64|x86|i*86)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES AMD64)
		if(WIN32)
			set(BINEXT .x64)
		else()
			set(BINEXT .x86_64)
		endif()
		set(RENDEXT _x86_64)
	else()
		#set(BINEXT .x86)
		set(RENDEXT _x86)
	endif()
	list(APPEND QCOMMON_SRCS engine/core/vm_x86.c)
	set(NO_VM_COMPILED OFF)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES aarch64|arm64)
	set(BINEXT .aarch64)
	set(RENDEXT _aarch64)
	list(APPEND QCOMMON_SRCS engine/core/vm_aarch64.c)
	set(NO_VM_COMPILED OFF)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES arm*)
	set(BINEXT .arm)
	set(RENDEXT _arm)
	list(APPEND QCOMMON_SRCS engine/core/vm_armv7l.c)
	set(NO_VM_COMPILED OFF)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64le|ppc64")
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64le")
		set(BINEXT .ppc64le)
		set(RENDEXT _ppc64le)
	else()
		set(BINEXT .ppc64)
		set(RENDEXT _ppc64)
	endif()
	list(APPEND QCOMMON_SRCS engine/core/vm_powerpc.c)
	set(NO_VM_COMPILED OFF)
endif()

# Normalize a simple arch identifier for renderer filenames (x86_64, x86, arm64, arm)
if(CMAKE_SYSTEM_PROCESSOR MATCHES AMD64|x86_64|X86_64)
  set(TARGET_ARCH_NAME "x86_64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES i.86|i386|i486|i586|i686)
  set(TARGET_ARCH_NAME "x86")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES aarch64|arm64)
  set(TARGET_ARCH_NAME "arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES arm*)
  set(TARGET_ARCH_NAME "arm")
else()
  set(TARGET_ARCH_NAME "")
endif()

# botlib
include(${CMAKE_SOURCE_DIR}/cmake/modules/BotlibSources.cmake)
idtech3_init_botlib_sources()
ADD_LIBRARY(botlib OBJECT ${BOTLIB_SRCS})
TARGET_COMPILE_DEFINITIONS(botlib PRIVATE BOTLIB)
target_include_directories(botlib PRIVATE ${IDTECH3_DIR_ENGINE_CORE})
idtech3_first_party_warnings_as_errors(botlib)
if(USE_GLM)
  target_include_directories(botlib PRIVATE ${EXTERNAL_GLM_INCLUDE})
endif()

include(${CMAKE_SOURCE_DIR}/cmake/server/ServerSources.cmake)
idtech3_init_server_sources()
include(${CMAKE_SOURCE_DIR}/cmake/server/ServerExtensionSources.cmake)
idtech3_strip_server_extension_sources()
idtech3_append_server_extension_sources()

# universal binary
ADD_LIBRARY(qcommon OBJECT ${QCOMMON_SRCS} ${SERVER_SRCS})
idtech3_first_party_warnings_as_errors(qcommon)
if(USE_GLM)
  target_include_directories(qcommon PRIVATE ${EXTERNAL_GLM_INCLUDE})
endif()
target_include_directories(qcommon PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/engine/platform
  ${CMAKE_SOURCE_DIR}/engine/platform/win32
  ${CMAKE_SOURCE_DIR}/engine/platform/win32
  ${IDTECH3_DIR_ENGINE_CORE}
  ${IDTECH3_DIR_RUNTIME_CLIENT}
  ${CMAKE_SOURCE_DIR}/modules/audio
)
idtech3_apply_research_include_dirs(qcommon)
idtech3_apply_research_compile_defs(qcommon)
set_property(TARGET qcommon PROPERTY POSITION_INDEPENDENT_CODE ON)
if(USE_LUA AND LUA_FOUND)
  target_compile_definitions(qcommon PRIVATE USE_LUA)
  target_include_directories(qcommon PRIVATE ${LUA_INCLUDE_DIR})
endif()
if(LUA_ANDROID_BUILT)
  add_dependencies(qcommon lua_android_ep)
endif()
if(USE_DUKTAPE AND DUKTAPE_FOUND)
  target_compile_definitions(qcommon PRIVATE USE_DUKTAPE)
  target_include_directories(qcommon PRIVATE ${DUKTAPE_INCLUDE_DIR})
endif()
if(USE_RADIUSFPS_CUDA)
  target_compile_definitions(qcommon PRIVATE RADIUSFPS_HAVE_CUDA=1)
endif()
if(USE_CSHARP AND CSHARP_FOUND)
  target_compile_definitions(qcommon PRIVATE USE_CSHARP IDTECH3_CSHARP_API_PATH="${CMAKE_SOURCE_DIR}/engine/core/csharp/IdTech3.Engine.cs")
  target_include_directories(qcommon PRIVATE ${CSHARP_INCLUDE_DIRS})
endif()
if(USE_PYTHON AND PYTHON_FOUND)
  target_compile_definitions(qcommon PRIVATE USE_PYTHON)
  target_include_directories(qcommon PRIVATE ${PYTHON_INCLUDE_DIRS})
endif()
if(USE_SQLITE AND SQLITE_FOUND)
  target_compile_definitions(qcommon PRIVATE USE_SQLITE)
  target_link_libraries(qcommon PRIVATE SQLite::SQLite3)
endif()
if(USE_DTLS_FOUND)
  target_compile_definitions(qcommon PRIVATE USE_DTLS=1)
  target_include_directories(qcommon PRIVATE ${OPENSSL_INCLUDE_DIR})
  if(USE_DTLS_ANDROID_BUILT AND OPENSSL_ANDROID_INTERFACE_TARGET)
    target_link_libraries(qcommon PRIVATE ${OPENSSL_ANDROID_INTERFACE_TARGET})
    add_dependencies(qcommon openssl_android_ep)
  else()
    target_link_libraries(qcommon PRIVATE OpenSSL::SSL OpenSSL::Crypto)
  endif()
endif()
if(USE_STEAM_NETWORKING)
  target_compile_definitions(qcommon PRIVATE USE_STEAM_NETWORKING=1 STEAMWORKS_AVAILABLE=${STEAMWORKS_AVAILABLE})
  if(STEAMWORKS_FOUND)
    target_include_directories(qcommon PRIVATE ${STEAMWORKS_INCLUDE_DIR})
  endif()
endif()
if(USE_STEAM AND STEAMWORKS_FOUND)
  target_compile_definitions(qcommon PRIVATE USE_STEAM=1 STEAMWORKS_AVAILABLE=${STEAMWORKS_AVAILABLE})
  target_include_directories(qcommon PRIVATE ${STEAMWORKS_INCLUDE_DIR})
endif()

# dedicated server
ADD_LIBRARY(qcommon_ded OBJECT ${QCOMMON_SRCS} ${SERVER_SRCS})
TARGET_COMPILE_DEFINITIONS(qcommon_ded PUBLIC DEDICATED)
idtech3_first_party_warnings_as_errors(qcommon_ded)
if(USE_GLM)
  target_include_directories(qcommon_ded PRIVATE ${EXTERNAL_GLM_INCLUDE})
endif()
target_include_directories(qcommon_ded PRIVATE
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/engine/platform
  ${CMAKE_SOURCE_DIR}/engine/platform/win32
  ${CMAKE_SOURCE_DIR}/engine/platform/win32
  ${IDTECH3_DIR_ENGINE_CORE}
  ${IDTECH3_DIR_RUNTIME_CLIENT}
  ${CMAKE_SOURCE_DIR}/modules/audio
)
idtech3_apply_research_include_dirs(qcommon_ded)
idtech3_apply_research_compile_defs(qcommon_ded)
set_property(TARGET qcommon_ded PROPERTY POSITION_INDEPENDENT_CODE ON)
if(USE_LUA AND LUA_FOUND)
  target_compile_definitions(qcommon_ded PRIVATE USE_LUA)
  target_include_directories(qcommon_ded PRIVATE ${LUA_INCLUDE_DIR})
endif()
if(LUA_ANDROID_BUILT)
  add_dependencies(qcommon_ded lua_android_ep)
endif()
if(USE_DUKTAPE AND DUKTAPE_FOUND)
  target_compile_definitions(qcommon_ded PRIVATE USE_DUKTAPE)
  target_include_directories(qcommon_ded PRIVATE ${DUKTAPE_INCLUDE_DIR})
endif()
if(USE_RADIUSFPS_CUDA)
  target_compile_definitions(qcommon_ded PRIVATE RADIUSFPS_HAVE_CUDA=1)
endif()
if(USE_CSHARP AND CSHARP_FOUND)
  target_compile_definitions(qcommon_ded PRIVATE USE_CSHARP IDTECH3_CSHARP_API_PATH="${CMAKE_SOURCE_DIR}/engine/core/csharp/IdTech3.Engine.cs")
  target_include_directories(qcommon_ded PRIVATE ${CSHARP_INCLUDE_DIRS})
endif()
if(USE_PYTHON AND PYTHON_FOUND)
  target_compile_definitions(qcommon_ded PRIVATE USE_PYTHON)
  target_include_directories(qcommon_ded PRIVATE ${PYTHON_INCLUDE_DIRS})
endif()
if(USE_SQLITE AND SQLITE_FOUND)
  target_compile_definitions(qcommon_ded PRIVATE USE_SQLITE)
  target_link_libraries(qcommon_ded PRIVATE SQLite::SQLite3)
endif()
if(USE_DTLS_FOUND)
  target_compile_definitions(qcommon_ded PRIVATE USE_DTLS=1)
  target_include_directories(qcommon_ded PRIVATE ${OPENSSL_INCLUDE_DIR})
  if(USE_DTLS_ANDROID_BUILT AND OPENSSL_ANDROID_INTERFACE_TARGET)
    target_link_libraries(qcommon_ded PRIVATE ${OPENSSL_ANDROID_INTERFACE_TARGET})
    add_dependencies(qcommon_ded openssl_android_ep)
  else()
    target_link_libraries(qcommon_ded PRIVATE OpenSSL::SSL OpenSSL::Crypto)
  endif()
endif()
if(USE_STEAM_NETWORKING)
  target_compile_definitions(qcommon_ded PRIVATE USE_STEAM_NETWORKING=1 STEAMWORKS_AVAILABLE=${STEAMWORKS_AVAILABLE})
  if(STEAMWORKS_FOUND)
    target_include_directories(qcommon_ded PRIVATE ${STEAMWORKS_INCLUDE_DIR})
  endif()
endif()

# Apply SKIP_IDPAK_CHECK to qcommon targets that compile files.c
if(SKIP_IDPAK_CHECK)
  TARGET_COMPILE_DEFINITIONS(qcommon PRIVATE SKIP_IDPAK_CHECK=1)
  TARGET_COMPILE_DEFINITIONS(qcommon_ded PRIVATE SKIP_IDPAK_CHECK=1)
endif()

if(NO_VM_COMPILED)
	target_compile_definitions(qcommon PRIVATE NO_VM_COMPILED=1)
	target_compile_definitions(qcommon_ded PRIVATE NO_VM_COMPILED=1)
endif()

idtech3_backend_apply_compile_defs(qcommon)
idtech3_backend_apply_compile_defs(qcommon_ded)

# client sources (explicit domain manifest — see cmake/client/ClientSources.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/client/ClientSources.cmake)
idtech3_init_client_sources()
list(APPEND CLIENT_SRCS
  engine/content/torrent_manifest.cpp
  modules/audio/snd_main.c
  modules/audio/mix/snd_dma.c
  modules/audio/mix/snd_mem.c
  modules/audio/mix/snd_mix.c
  modules/audio/mix/snd_mix_fallback.c
  modules/audio/mix/snd_wavelet.c
  modules/audio/mix/snd_adpcm.c
  modules/audio/codecs/snd_codec.c
  modules/audio/codecs/snd_codec_mp3.c
  modules/audio/codecs/snd_codec_ogg.c
  modules/audio/codecs/snd_codec_wav.c
  modules/audio/codecs/snd_codec_opus.c
  modules/audio/codecs/snd_codec_flac.c
  modules/audio/codecs/snd_codec_webm.c
  modules/audio/backends/snd_backend_null.c
)
if(USE_OPENAL)
  list(APPEND CLIENT_SRCS modules/audio/backends/snd_backend_openal.c)
  list(APPEND CLIENT_SRCS modules/audio/effects/snd_efx.c)
endif()
if(USE_SDL)
  list(APPEND CLIENT_SRCS modules/audio/backends/snd_backend_sdl.c)
endif()
list(APPEND CLIENT_SRCS
  modules/audio/mixer/snd_mixer.c
  modules/audio/snd_music_adaptive.c
  runtime/game/g_director.c
  runtime/game/g_response.c
  runtime/game/g_choreography.c
  runtime/game/g_dismember.c
  runtime/game/g_facial.c
  runtime/game/g_horde.c
  runtime/game/g_goap.c
  runtime/game/g_aiml.c
  runtime/game/g_eda.c
  runtime/game/g_engine_systems.c
  runtime/game/g_bt.c
  runtime/game/g_entity_bridge.c
  runtime/game/g_lua_bindings.c
)
include(${CMAKE_SOURCE_DIR}/cmake/modules/ClientGameAiSources.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/modules/UberToolsSources.cmake)
idtech3_strip_game_ai_middleware_sources()
idtech3_strip_client_extension_sources()
idtech3_append_client_extension_sources()
idtech3_append_game_ai_middleware_sources()
idtech3_append_ubertools_client_sources()
# Architecture-specific assembly (snd_mix etc). Unix x86_64 only; Windows uses C fallbacks.
idtech3_init_asm_sources()
if(ASM_SRCS)
	list(APPEND CLIENT_SRCS ${ASM_SRCS})
	enable_language(ASM)
	set_source_files_properties(${ASM_SRCS} PROPERTIES LANGUAGE ASM)
	set(ASM_FLAGS "-x assembler-with-cpp")
	if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
		set(ASM_FLAGS "${ASM_FLAGS} -masm=intel")
	endif()
	set_source_files_properties(${ASM_SRCS} PROPERTIES COMPILE_FLAGS "${ASM_FLAGS}")
	add_compile_definitions(USE_ASM)
endif()
IF(NOT USE_CURL OR NOT CURL_FOUND)
	list(REMOVE_ITEM CLIENT_SRCS runtime/client/platform/cl_curl.c)
ENDIF()
IF(USE_CURL AND NOT CURL_FOUND)
	message(WARNING "USE_CURL is ON but libcurl was not found; cl_curl disabled.")
ENDIF()
IF(NOT USE_MP3)
	list(REMOVE_ITEM CLIENT_SRCS runtime/client/snd_codec_mp3.c)
	list(REMOVE_ITEM CLIENT_SRCS modules/audio/codecs/snd_codec_mp3.c)
ENDIF()
include(${CMAKE_SOURCE_DIR}/cmake/modules/AudioMp3Sources.cmake)
idtech3_append_mp3_client_sources()

# Opus/FLAC sources already listed above; remove if not found.
if(NOT USE_OPUS OR NOT OPUS_FOUND)
  list(REMOVE_ITEM CLIENT_SRCS modules/audio/codecs/snd_codec_opus.c)
endif()
if(NOT USE_FLAC OR NOT FLAC_FOUND)
  list(REMOVE_ITEM CLIENT_SRCS modules/audio/codecs/snd_codec_flac.c)
endif()
if(NOT USE_WEBM OR NOT WEBM_FOUND)
  list(REMOVE_ITEM CLIENT_SRCS modules/audio/codecs/snd_codec_webm.c)
endif()

# --- Modern video codec support ---
option(USE_FFMPEG "Enable FFmpeg video codec support (requires system libavcodec)" ON)
option(USE_DAV1D "Enable dav1d AV1 video codec support (requires system libdav1d)" ON)
option(USE_DAV2D "Enable dav2d AV2 video codec support (requires system libdav2d)" ON)
option(USE_VVDEC "Enable vvdec VVC video codec support (requires system libvvdec)" ON)
option(USE_VPX "Enable libvpx VP8/VP9 video codec support (requires system libvpx)" ON)
option(USE_THEORA "Enable Theora video codec support (requires system libtheora)" ON)

set(FFMPEG_FOUND FALSE)
set(DAV1D_FOUND FALSE)
set(DAV2D_FOUND FALSE)
set(VVDEC_FOUND FALSE)
set(VPX_FOUND FALSE)
set(THEORA_FOUND FALSE)

if(USE_FFMPEG)
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(FFMPEG_PKG QUIET libavcodec libavformat libavutil libswscale libswresample)
    if(FFMPEG_PKG_FOUND)
      set(FFMPEG_FOUND TRUE)
      message(STATUS "FFmpeg video codec: enabled (${FFMPEG_PKG_LINK_LIBRARIES})")
    endif()
  endif()
  if(NOT FFMPEG_FOUND)
    message(STATUS "FFmpeg video codec: disabled (pkg-config could not find libavcodec/libavformat/libavutil/libswscale/libswresample)")
    list(REMOVE_ITEM CLIENT_SRCS runtime/client/media/cl_cin_ffmpeg.c)
  endif()
else()
  list(REMOVE_ITEM CLIENT_SRCS runtime/client/media/cl_cin_ffmpeg.c)
endif()

if(USE_DAV1D)
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(DAV1D_PKG QUIET dav1d)
    if(DAV1D_PKG_FOUND)
      set(DAV1D_FOUND TRUE)
      message(STATUS "dav1d AV1 codec: enabled (${DAV1D_PKG_LINK_LIBRARIES})")
    endif()
  endif()
  if(NOT DAV1D_FOUND)
    message(STATUS "dav1d AV1 codec: disabled (dav1d not found)")
    list(REMOVE_ITEM CLIENT_SRCS runtime/client/media/cl_cin_dav1d.c)
  endif()
else()
  list(REMOVE_ITEM CLIENT_SRCS runtime/client/media/cl_cin_dav1d.c)
endif()

if(USE_DAV2D)
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(DAV2D_PKG QUIET dav2d)
    if(DAV2D_PKG_FOUND)
      set(DAV2D_FOUND TRUE)
      message(STATUS "dav2d AV2 codec: enabled (${DAV2D_PKG_LINK_LIBRARIES})")
    endif()
  endif()
  if(NOT DAV2D_FOUND)
    message(STATUS "dav2d AV2 codec: disabled (dav2d not found)")
    list(REMOVE_ITEM CLIENT_SRCS runtime/client/media/cl_cin_dav2d.c)
  endif()
else()
  list(REMOVE_ITEM CLIENT_SRCS runtime/client/media/cl_cin_dav2d.c)
endif()

if(USE_VVDEC)
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(VVDEC_PKG QUIET libvvdec)
    if(NOT VVDEC_PKG_FOUND)
      pkg_check_modules(VVDEC_PKG QUIET vvdec)
    endif()
    if(VVDEC_PKG_FOUND)
      set(VVDEC_FOUND TRUE)
      message(STATUS "vvdec VVC codec: enabled (${VVDEC_PKG_LINK_LIBRARIES})")
    endif()
  endif()
  if(NOT VVDEC_FOUND)
    message(STATUS "vvdec VVC codec: disabled (pkg-config could not find libvvdec/vvdec)")
    list(REMOVE_ITEM CLIENT_SRCS runtime/client/media/cl_cin_vvdec.c)
  endif()
else()
  list(REMOVE_ITEM CLIENT_SRCS runtime/client/media/cl_cin_vvdec.c)
endif()

if(USE_VPX)
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(VPX_PKG QUIET vpx)
    if(VPX_PKG_FOUND)
      set(VPX_FOUND TRUE)
      message(STATUS "libvpx VP8/VP9 codec: enabled (${VPX_PKG_LINK_LIBRARIES})")
    endif()
  endif()
  if(NOT VPX_FOUND)
    message(STATUS "libvpx VP8/VP9 codec: disabled (vpx not found)")
    list(REMOVE_ITEM CLIENT_SRCS runtime/client/media/cl_cin_vpx.c)
  endif()
else()
  list(REMOVE_ITEM CLIENT_SRCS runtime/client/media/cl_cin_vpx.c)
endif()

if(USE_THEORA)
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(THEORA_PKG QUIET theoradec)
    if(THEORA_PKG_FOUND)
      set(THEORA_FOUND TRUE)
      message(STATUS "Theora codec: enabled (${THEORA_PKG_LINK_LIBRARIES})")
    endif()
  endif()
  if(NOT THEORA_FOUND)
    message(STATUS "Theora codec: disabled (libtheoradec not found)")
    list(REMOVE_ITEM CLIENT_SRCS runtime/client/media/cl_cin_theora.c)
  endif()
else()
  list(REMOVE_ITEM CLIENT_SRCS runtime/client/media/cl_cin_theora.c)
endif()

# pkg-config resolves *host* FFmpeg/dav1d/vpx/Theora; never use those include paths for NDK targets.
if(ANDROID)
  set(FFMPEG_FOUND FALSE)
  set(DAV1D_FOUND FALSE)
  set(DAV2D_FOUND FALSE)
  set(VVDEC_FOUND FALSE)
  set(VPX_FOUND FALSE)
  set(THEORA_FOUND FALSE)
  list(REMOVE_ITEM CLIENT_SRCS runtime/client/media/cl_cin_ffmpeg.c)
  list(REMOVE_ITEM CLIENT_SRCS runtime/client/media/cl_cin_dav1d.c)
  list(REMOVE_ITEM CLIENT_SRCS runtime/client/media/cl_cin_dav2d.c)
  list(REMOVE_ITEM CLIENT_SRCS runtime/client/media/cl_cin_vvdec.c)
  list(REMOVE_ITEM CLIENT_SRCS runtime/client/media/cl_cin_vpx.c)
  list(REMOVE_ITEM CLIENT_SRCS runtime/client/media/cl_cin_theora.c)
  message(STATUS "Android: host pkg-config video codecs disabled (FFmpeg/dav1d/dav2d/vvdec/vpx/Theora); use vendor/NDK builds to re-enable")
endif()
# --- End modern video codec support ---

if(USE_VUDA AND USE_RESEARCH_EXTENSIONS)
  list(APPEND CLIENT_SRCS
    runtime/client/shell/cl_vuda.c
    extensions/research/vuda/vuda_cuda.c
  )
endif()

#ADD_LIBRARY(client OBJECT ${CLIENT_SRCS})

# jpeg
IF(USE_SYSTEM_JPEG)
	ADD_LIBRARY(client OBJECT ${CLIENT_SRCS})
	find_package(JPEG REQUIRED)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE -DUSE_SYSTEM_JPEG)
	target_include_directories(client PRIVATE ${CMAKE_SOURCE_DIR}/runtime/game)
	target_include_directories(client PRIVATE ${CMAKE_SOURCE_DIR}/extensions/generative)
	if(USE_OPUS)
		target_compile_definitions(client PRIVATE INCLUDE_INTTYPES_H=1 INCLUDE_SYS_TYPES_H=1)
	endif()
	if(USE_GLM)
		target_include_directories(client PRIVATE ${EXTERNAL_GLM_INCLUDE})
	endif()
	if(USE_CURL AND CURL_FOUND)
		target_include_directories(client PRIVATE ${CURL_INCLUDE_DIRS})
		target_link_libraries(client PRIVATE ${CURL_LIBRARIES})
	endif()
	TARGET_LINK_LIBRARIES(client ${JPEG_LIBRARIES})
ELSE()
	AUX_SOURCE_DIRECTORY(${EXTERNAL_JPEG_SRC_DIR} JPEG_SRCS)
	#ADD_LIBRARY(jpeg ${JPEG_SRCS})
	#TARGET_LINK_LIBRARIES(client jpeg)
	ADD_LIBRARY(client OBJECT ${CLIENT_SRCS} ${JPEG_SRCS})
	# Ensure client can find qcommon headers needed by some client-side modules (eg. mp3)
	target_include_directories(client PRIVATE ${IDTECH3_DIR_ENGINE_CORE} ${EXTERNAL_JPEG_SRC_DIR})
	# ECS (EnTT) - alternative to gentity
	target_include_directories(client PRIVATE ${CMAKE_SOURCE_DIR}/runtime/game)
	# Audio module headers (moved from client/)
	target_include_directories(client PRIVATE
		${CMAKE_SOURCE_DIR}/modules/audio
		${CMAKE_SOURCE_DIR}/modules/audio/codecs
		${IDTECH3_DIR_RUNTIME_CLIENT}
		${CMAKE_SOURCE_DIR}/extensions/generative
	)
	if(USE_OPUS)
		target_compile_definitions(client PRIVATE INCLUDE_INTTYPES_H=1 INCLUDE_SYS_TYPES_H=1)
	endif()
	if(USE_GLM)
		target_include_directories(client PRIVATE ${EXTERNAL_GLM_INCLUDE})
	endif()
	if(USE_CURL AND CURL_FOUND)
		target_include_directories(client PRIVATE ${CURL_INCLUDE_DIRS})
		target_link_libraries(client PRIVATE ${CURL_LIBRARIES})
	endif()
ENDIF()
# The default client object also compiles vendored JPEG sources. CI enforces
# client first-party warnings with scripts/check_first_party_warnings.sh.
idtech3_first_party_warnings_as_errors(client)
idtech3_client_include_domains(client)
target_include_directories(client PRIVATE ${CMAKE_SOURCE_DIR})
# Steam client defs must run after ADD_LIBRARY(client) above.
if(USE_STEAM_NETWORKING AND STEAMWORKS_FOUND)
	target_compile_definitions(client PRIVATE USE_STEAM_NETWORKING=1 USE_STEAM=1)
	target_include_directories(client PRIVATE ${STEAMWORKS_INCLUDE_DIR})
	message(STATUS "Steam: full API + SDR enabled")
elseif(USE_STEAM AND STEAMWORKS_FOUND)
	target_compile_definitions(client PRIVATE USE_STEAM=1)
	target_include_directories(client PRIVATE ${STEAMWORKS_INCLUDE_DIR})
	message(STATUS "Steam: API enabled (achievements, overlay, rich presence, Deck)")
endif()
# g_lua_bindings.c / cl_main include lua_compat.h (and duktape) before feature guards;
# headers live under versioned dirs on Linux (e.g. /usr/include/lua5.4), so wire include paths.
if(USE_LUA AND LUA_FOUND)
	target_include_directories(client PRIVATE ${LUA_INCLUDE_DIR})
endif()
if(USE_DUKTAPE AND DUKTAPE_FOUND)
	target_include_directories(client PRIVATE ${DUKTAPE_INCLUDE_DIR})
endif()
if(USE_LIBTORRENT AND LIBTORRENT_FOUND)
	target_compile_definitions(client PRIVATE USE_LIBTORRENT=1)
	target_link_libraries(client PRIVATE ${LIBTORRENT_TARGET})
endif()
idtech3_apply_game_ai_middleware_defs(client)
idtech3_apply_ubertools_defs(client)

if(USE_FLUX)
	target_compile_definitions(client PRIVATE USE_FLUX)
	target_include_directories(client PRIVATE ${EXTERNAL_ROOT}/src/cflux2)
endif()

if(USE_TRELLIS)
	target_compile_definitions(client PRIVATE USE_TRELLIS)
endif()

if(USE_GENETIC_GAN)
	target_compile_definitions(client PRIVATE USE_GENETIC_GAN)
endif()

idtech3_apply_research_compile_defs(client)

if(USE_OPEN_WORLD)
	target_compile_definitions(qcommon PRIVATE USE_OPEN_WORLD=1)
	target_compile_definitions(qcommon_ded PRIVATE USE_OPEN_WORLD=1)
	target_compile_definitions(client PRIVATE USE_OPEN_WORLD=1)
endif()
if(USE_ARC_BLANC)
	target_compile_definitions(qcommon PRIVATE USE_ARC_BLANC=1)
	target_compile_definitions(qcommon_ded PRIVATE USE_ARC_BLANC=1)
	target_compile_definitions(client PRIVATE USE_ARC_BLANC=1)
endif()

# renderers
set(RENDERER_COMMON_SRCS
	renderers/common/tr_font_stub.c  # placeholder, will be replaced below
	renderers/common/tr_image_bmp.c
	renderers/common/tr_image_jpg.c
	renderers/common/tr_image_pcx.c
	renderers/common/tr_image_png.c
	renderers/common/tr_image_tga.c
	renderers/common/tr_image_hdr.c
	renderers/common/tr_image_qoi.c
	renderers/common/tr_image_dds.c
	renderers/common/tr_image_svg.c
	renderers/common/tr_model_mesh_import.c
	renderers/common/tr_vox_parse.c
	renderers/common/tr_model_freeusd_register.c
	renderers/vulkan/tr_model_gltf.c
	renderers/vulkan/tr_gltf_topo.c
	renderers/vulkan/tr_model_obj.c
	renderers/vulkan/tr_model_md5.c
	renderers/common/tr_image_ktx2.c
	renderers/common/tr_noise.c
	renderers/common/tr_bsp30_triangulate.c
)
include(${CMAKE_SOURCE_DIR}/cmake/modules/UberToolsSources.cmake)
idtech3_append_ubertools_renderer_sources()
IF(BUILD_FREETYPE)
	list(REMOVE_ITEM RENDERER_COMMON_SRCS renderers/common/tr_font_stub.c)
	list(REMOVE_ITEM RENDERER_COMMON_SRCS renderers/common/tr_vector_font_stub.c)
	list(REMOVE_ITEM RENDERER_COMMON_SRCS renderers/common/tr_vector_font_glyphlet_stub.c)
	list(APPEND RENDERER_COMMON_SRCS renderers/common/tr_font.c)
	list(APPEND RENDERER_COMMON_SRCS renderers/common/tr_vector_font.c)
	list(APPEND RENDERER_COMMON_SRCS renderers/common/tr_vector_font_glyphlet.c)
ELSE()
	list(REMOVE_ITEM RENDERER_COMMON_SRCS renderers/common/tr_vector_font.c)
	list(REMOVE_ITEM RENDERER_COMMON_SRCS renderers/common/tr_vector_font_glyphlet.c)
	list(APPEND RENDERER_COMMON_SRCS renderers/common/tr_vector_font_stub.c)
	list(APPEND RENDERER_COMMON_SRCS renderers/common/tr_vector_font_glyphlet_stub.c)
ENDIF()

if(USE_FREEUSD)
	include(${CMAKE_SOURCE_DIR}/cmake/FreeUSD.cmake)
	list(APPEND RENDERER_COMMON_SRCS renderers/common/tr_model_freeusd.cpp)
	target_compile_definitions(client PRIVATE USE_FREEUSD=1)
	target_link_libraries(client PRIVATE idtech3_freeusd freeusd::c)
	message(STATUS "FreeUSD: client tools (usd_info, usd_assess, usd_entities, usd_shaders, usd_load)")
else()
	message(STATUS "FreeUSD: disabled (cmake -DUSE_FREEUSD=OFF); usd_* commands unavailable")
endif()

include(${CMAKE_SOURCE_DIR}/cmake/renderers/VulkanCoreSources.cmake)
idtech3_init_vulkan_core_sources()
idtech3_log_vulkan_core_manifest()
include(${CMAKE_SOURCE_DIR}/cmake/renderers/VulkanExtensionSources.cmake)
idtech3_apply_vulkan_extension_sources()
list(APPEND RENDERER_VK_SRCS
    renderers/vulkan/utils/mikktspace/mikktspace.c
)
set(VK_SHADER_DIR "${CMAKE_SOURCE_DIR}/renderers/vulkan/shaders")
set(VK_SPIRV_DIR "${VK_SHADER_DIR}/spirv")
set(VK_GEN_DIR "${VK_SPIRV_DIR}/generated")

set(VK_SHADER_DATA_GEN "${VK_GEN_DIR}/shader_data.c")
set(VK_SHADER_BINDING_GEN "${VK_GEN_DIR}/shader_binding.c")
set(VK_SHADER_GENERATED_SRCS ${VK_SHADER_DATA_GEN} ${VK_SHADER_BINDING_GEN})

file(GLOB_RECURSE VK_SHADER_GLSL_FILES
  "${VK_SHADER_DIR}/glsl/*.frag"
  "${VK_SHADER_DIR}/glsl/*.vert"
  "${VK_SHADER_DIR}/glsl/*.geom"
  "${VK_SHADER_DIR}/glsl/*.comp"
  "${VK_SHADER_DIR}/glsl/*.tmpl"
)

set(VK_SHADER_TOOL_SOURCES
  "${VK_SHADER_DIR}/tools/bin2hex.c"
  "${VK_SHADER_DIR}/tools/bindshader.c"
)

option(SKIP_SHADER_REGEN "Skip shader regeneration, use pre-committed generated shaders" OFF)

if(SKIP_SHADER_REGEN)
  message(STATUS "Shader regeneration: SKIPPED (using pre-committed shaders)")
  add_custom_target(vk_shaders_autogen)
else()
  add_custom_command(
    OUTPUT ${VK_SHADER_GENERATED_SRCS}
    COMMAND "${CMAKE_SOURCE_DIR}/scripts/compile_shaders.sh"
            --generated-dir "${VK_GEN_DIR}"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    DEPENDS
      "${CMAKE_SOURCE_DIR}/scripts/compile_shaders.sh"
      ${VK_SHADER_GLSL_FILES}
      ${VK_SHADER_TOOL_SOURCES}
    COMMENT "Generating Vulkan shader blobs"
    VERBATIM
  )
  add_custom_target(vk_shaders_autogen
    DEPENDS ${VK_SHADER_GENERATED_SRCS}
  )
endif()
# Suppress shadow warnings for MikkTSpace third-party code
if(MSVC)
    set_source_files_properties(renderers/vulkan/utils/mikktspace/mikktspace.c PROPERTIES
        COMPILE_OPTIONS "/wd4456;/wd4457;/wd4458;/wd4459"
    )
else()
    set_source_files_properties(renderers/vulkan/utils/mikktspace/mikktspace.c PROPERTIES
        COMPILE_OPTIONS "-Wno-shadow;-Wno-unused-but-set-variable"
    )
endif()

SET(AUX_SRCS)
if(NOT USE_RENDERER_DLOPEN)
	list(APPEND AUX_SRCS
		engine/core/q_shared.c
		engine/core/q_math.c)
	if(NOT USE_LIBPNG)
		list(APPEND AUX_SRCS engine/core/puff.c)
	endif()
endif()
IF(USE_RENDERER_DLOPEN)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_RENDERER_DLOPEN RENDERER_PREFIX="${RENDERER_PREFIX}")

	ADD_LIBRARY(${RENDERER_PREFIX}_vulkan${RENDEXT} SHARED ${RENDERER_VK_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
	idtech3_first_party_warnings_as_errors(${RENDERER_PREFIX}_vulkan${RENDEXT})
	TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_RENDERER_DLOPEN RENDERER_VULKAN)
	idtech3_apply_ubertools_defs(${RENDERER_PREFIX}_vulkan${RENDEXT})
	if(USE_VULKAN_RTX)
	  target_compile_definitions(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_VULKAN_RTX=1)
	  message(STATUS "Vulkan RTX: ray tracing extensions will be requested when available")
	endif()
	if(USE_EXPERIMENTAL_RENDERERS)
	  target_compile_definitions(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_EXPERIMENTAL_RENDERERS=1)
	  message(STATUS "Experimental renderers: compile define USE_EXPERIMENTAL_RENDERERS=1")
	endif()
	if(USE_VUDA)
	  target_compile_definitions(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_VUDA=1)
	  message(STATUS "VUDA: CUDA-Vulkan interop enabled (r_vuda 1 + cl_vuda 1 + vid_restart)")
	endif()
	if(USE_ARC_BLANC)
	  target_compile_definitions(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_ARC_BLANC=1)
	  message(STATUS "Arc Blanc: ocean height upload enabled (r_arcBlanc 1)")
	endif()
	if(USE_MIMIR_CUDA)
	  target_compile_definitions(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_MIMIR_CUDA=1)
	  message(STATUS "Mímir: CUDA interop enabled (r_mimir 1 + r_mimir_cuda 1 + vid_restart)")
	endif()
	if(USE_IDTECH3_EMULATOR)
	  idtech3_emulator_apply_compile_defs(${RENDERER_PREFIX}_vulkan${RENDEXT})
	  target_compile_definitions(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_IDTECH3_EMULATOR=1)
	  message(STATUS "idTech3-Emulator: *emulator_screen texture path (r_emulatorScreen 1)")
	endif()
	add_dependencies(${RENDERER_PREFIX}_vulkan${RENDEXT} vk_shaders_autogen)
	IF(BUILD_FREETYPE)
		target_link_libraries(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE idtech3_freetype)
	ENDIF()
	target_include_directories(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${CMAKE_SOURCE_DIR})
	# engine/ so #include "platform/android/..." resolves without relying only on the repo-root platform symlink
	target_include_directories(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${CMAKE_SOURCE_DIR}/engine)
	target_include_directories(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${IDTECH3_DIR_ENGINE_CORE})
	idtech3_vulkan_extension_include_dirs(${RENDERER_PREFIX}_vulkan${RENDEXT})
	idtech3_apply_research_include_dirs(${RENDERER_PREFIX}_vulkan${RENDEXT})
	if(USE_FREEUSD)
		target_link_libraries(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE idtech3_freeusd freeusd::c)
	endif()
	if(WIN32)
		target_link_libraries(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE qcommon m ${CMAKE_DL_LIBS})
	else()
		target_link_libraries(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE m ${CMAKE_DL_LIBS})
	endif()
	IF(APPLE)
		target_link_options(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE "-undefined" "dynamic_lookup")
	ENDIF()
	if(UNIX AND NOT APPLE)
		target_link_options(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE "-Wl,--allow-shlib-undefined")
	endif()
	# Normalize target filenames for dlopen-friendly plugin names:
	# set OUTPUT_NAME and remove default 'lib' PREFIX so filenames become:
	#   idtech3_vulkan_x86_64.so  (instead of libidtech3_vulkan_x86_64.so)
	# Use simple names by default, architecture-specific names only when explicitly needed
	set(RENDERER_OUT_Vulkan "idtech3_vulkan")
	set_target_properties(${RENDERER_PREFIX}_vulkan${RENDEXT} PROPERTIES
		OUTPUT_NAME ${RENDERER_OUT_Vulkan} PREFIX ""
		CXX_STANDARD ${IDTECH3_CXX_STANDARD} CXX_STANDARD_REQUIRED ON)
	# MinGW/PE: without this, renderer DLLs can link but export no symbols (GetRefAPI missing) - breaks dlopen on Windows.
	if(WIN32)
		set_target_properties(${RENDERER_PREFIX}_vulkan${RENDEXT}
			PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
	endif()

	# Roadmap-only renderer plugins (Metal / DXR scaffolds — not shippable).
	function(idtech3_add_platform_renderer_stub STUB_SHORT STUB_DEFINE)
		set(_stub_target "${RENDERER_PREFIX}_${STUB_SHORT}${RENDEXT}")
		add_library(${_stub_target} SHARED renderers/common/tr_platform_renderer_stub.c)
		target_compile_definitions(${_stub_target} PRIVATE USE_RENDERER_DLOPEN ${STUB_DEFINE}=1)
		target_include_directories(${_stub_target} PRIVATE ${CMAKE_SOURCE_DIR})
		if(WIN32)
			target_link_libraries(${_stub_target} PRIVATE m ${CMAKE_DL_LIBS})
			set_target_properties(${_stub_target} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
		else()
			target_link_libraries(${_stub_target} PRIVATE m ${CMAKE_DL_LIBS})
		endif()
		if(APPLE)
			target_link_options(${_stub_target} PRIVATE "-undefined" "dynamic_lookup")
		endif()
		if(UNIX AND NOT APPLE)
			target_link_options(${_stub_target} PRIVATE "-Wl,--allow-shlib-undefined")
		endif()
		set_target_properties(${_stub_target} PROPERTIES OUTPUT_NAME "idtech3_${STUB_SHORT}" PREFIX "")
	endfunction()
	if(USE_METAL_RENDERER)
		idtech3_add_platform_renderer_stub(metal RENDERER_PLATFORM_STUB_METAL)
	endif()
	if(USE_DXR_RENDERER)
		idtech3_add_platform_renderer_stub(dxr RENDERER_PLATFORM_STUB_DXR)
	endif()
ELSE()
	ADD_LIBRARY(${RENDERER_PREFIX}_vulkan OBJECT ${RENDERER_VK_SRCS} ${RENDERER_COMMON_SRCS})
	idtech3_first_party_warnings_as_errors(${RENDERER_PREFIX}_vulkan)
	TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan PRIVATE RENDERER_VULKAN)
	if(USE_VULKAN_RTX)
	  target_compile_definitions(${RENDERER_PREFIX}_vulkan PRIVATE USE_VULKAN_RTX=1)
	endif()
	if(USE_EXPERIMENTAL_RENDERERS)
	  target_compile_definitions(${RENDERER_PREFIX}_vulkan PRIVATE USE_EXPERIMENTAL_RENDERERS=1)
	endif()
	if(USE_VUDA)
	  target_compile_definitions(${RENDERER_PREFIX}_vulkan PRIVATE USE_VUDA=1)
	endif()
	if(USE_ARC_BLANC)
	  target_compile_definitions(${RENDERER_PREFIX}_vulkan PRIVATE USE_ARC_BLANC=1)
	endif()
	if(USE_MIMIR_CUDA)
	  target_compile_definitions(${RENDERER_PREFIX}_vulkan PRIVATE USE_MIMIR_CUDA=1)
	endif()
	if(USE_IDTECH3_EMULATOR)
	  idtech3_emulator_apply_compile_defs(${RENDERER_PREFIX}_vulkan)
	  target_compile_definitions(${RENDERER_PREFIX}_vulkan PRIVATE USE_IDTECH3_EMULATOR=1)
	endif()
	add_dependencies(${RENDERER_PREFIX}_vulkan vk_shaders_autogen)
	IF(BUILD_FREETYPE)
		target_link_libraries(${RENDERER_PREFIX}_vulkan PRIVATE idtech3_freetype)
	ENDIF()
	target_include_directories(${RENDERER_PREFIX}_vulkan PRIVATE ${CMAKE_SOURCE_DIR})
	target_include_directories(${RENDERER_PREFIX}_vulkan PRIVATE ${CMAKE_SOURCE_DIR}/engine)
	target_include_directories(${RENDERER_PREFIX}_vulkan PRIVATE ${IDTECH3_DIR_ENGINE_CORE})
	idtech3_vulkan_extension_include_dirs(${RENDERER_PREFIX}_vulkan)
	idtech3_apply_research_include_dirs(${RENDERER_PREFIX}_vulkan)
	if(USE_FREEUSD)
		target_link_libraries(${RENDERER_PREFIX}_vulkan PRIVATE idtech3_freeusd freeusd::c)
	endif()
	set_target_properties(${RENDERER_PREFIX}_vulkan PROPERTIES
		CXX_STANDARD ${IDTECH3_CXX_STANDARD} CXX_STANDARD_REQUIRED ON)
ENDIF()

# libpng/zlib for renderer PNG loader
if(USE_LIBPNG)
  if(TARGET png_static)
    set(LIBPNG_TARGET png_static)
  elseif(TARGET png_shared)
    set(LIBPNG_TARGET png_shared)
  elseif(TARGET png)
    set(LIBPNG_TARGET png)
  else()
    set(LIBPNG_TARGET "")
  endif()

  if(LIBPNG_TARGET)
    if(NOT MSVC)
      target_compile_options(${LIBPNG_TARGET} PRIVATE -Wno-cast-qual)
    endif()
	    if(USE_RENDERER_DLOPEN)
	      target_link_libraries(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${LIBPNG_TARGET})
	      target_compile_definitions(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_LIBPNG USE_ZLIB)
	    else()
	      target_link_libraries(${RENDERER_PREFIX}_vulkan PRIVATE ${LIBPNG_TARGET})
	      target_compile_definitions(${RENDERER_PREFIX}_vulkan PRIVATE USE_LIBPNG USE_ZLIB)
	    endif()
  endif()
endif()

	if(USE_GLM)
	  if(USE_RENDERER_DLOPEN)
	    target_include_directories(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${EXTERNAL_GLM_INCLUDE})
	  else()
	    target_include_directories(${RENDERER_PREFIX}_vulkan PRIVATE ${EXTERNAL_GLM_INCLUDE})
	  endif()
	endif()

# EXR image loading via tinyexr -- built as separate static lib to avoid
# duplicate TINYEXR_IMPLEMENTATION symbols when both renderers are loaded
enable_language(CXX)
add_library(exr_loader STATIC renderers/common/tr_image_exr.cpp)
target_include_directories(exr_loader PRIVATE ${EXTERNAL_INCLUDE}/tinyexr ${CMAKE_SOURCE_DIR} ${IDTECH3_DIR_ENGINE_CORE})
target_include_directories(exr_loader PRIVATE ${EXTERNAL_ZLIB_SRC})
set_property(TARGET exr_loader PROPERTY POSITION_INDEPENDENT_CODE ON)
set_target_properties(exr_loader PROPERTIES
  CXX_STANDARD ${IDTECH3_CXX_STANDARD} CXX_STANDARD_REQUIRED ON)
if(NOT MSVC)
  target_compile_options(exr_loader PRIVATE -Wno-old-style-cast -Wno-sign-compare -Wno-cast-qual -Wno-calloc-transposed-args)
endif()
if(TARGET ZLIB::ZLIB)
  target_link_libraries(exr_loader PRIVATE ZLIB::ZLIB)
endif()
if(USE_RENDERER_DLOPEN)
  target_link_libraries(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE exr_loader)
else()
  target_link_libraries(${RENDERER_PREFIX}_vulkan PRIVATE exr_loader)
endif()

# cgltf include for glTF 2.0 model loading
if(USE_RENDERER_DLOPEN)
  target_include_directories(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${EXTERNAL_INCLUDE}/cgltf)
else()
  target_include_directories(${RENDERER_PREFIX}_vulkan PRIVATE ${EXTERNAL_INCLUDE}/cgltf)
endif()

# --- ImGui Inspector (optional, requires C++) ---
option(USE_IMGUI "Enable ImGui inspector overlay" ON)
# Disable on macOS: imgui C++ headers get pulled into C compilation units, causing "unknown type name 'constexpr'" errors.
if(USE_IMGUI AND NOT APPLE)
  enable_language(CXX)
  set(IMGUI_SRCS
    renderers/vulkan/inspector/vk_imgui.cpp
    renderers/vulkan/inspector/vk_imgui_chrome_panels.cpp
    renderers/vulkan/inspector/vk_imgui_helpers.cpp
    renderers/vulkan/inspector/vk_imgui_fonts.cpp
    renderers/vulkan/inspector/vk_imgui_studio_panels.cpp
    renderers/vulkan/inspector/vk_imgui_oscar_panels.cpp
    renderers/vulkan/inspector/vk_imgui_postfx_panels.cpp
    renderers/vulkan/inspector/vk_imgui_physics_panels.cpp
    renderers/vulkan/inspector/vk_imgui_volumetrics_panels.cpp
    renderers/vulkan/inspector/vk_imgui_ocean_panels.cpp
    renderers/vulkan/inspector/vk_imgui_sim_render_panels.cpp
    renderers/vulkan/inspector/vk_imgui_scene_panels.cpp
    renderers/vulkan/inspector/vk_imgui_scene.cpp
    renderers/vulkan/inspector/vk_imgui_vulkan.cpp
    ${EXTERNAL_SRC}/cimgui/imgui/imgui.cpp
    ${EXTERNAL_SRC}/cimgui/imgui/imgui_draw.cpp
    ${EXTERNAL_SRC}/cimgui/imgui/imgui_tables.cpp
    ${EXTERNAL_SRC}/cimgui/imgui/imgui_widgets.cpp
    ${EXTERNAL_SRC}/cimgui/imgui/backends/imgui_impl_vulkan.cpp
  )
  set_source_files_properties(${EXTERNAL_SRC}/cimgui/imgui/backends/imgui_impl_vulkan.cpp PROPERTIES
    COMPILE_DEFINITIONS "VK_NO_PROTOTYPES")
  set(IMGUI_INCLUDE_DIRS
    ${EXTERNAL_SRC}/cimgui/imgui
    ${EXTERNAL_SRC}/cimgui/imgui/backends
    ${EXTERNAL_SRC}/cimgui
    ${IDTECH3_DIR_RENDERERS}/common
  )
  if(USE_RENDERER_DLOPEN)
    target_sources(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${IMGUI_SRCS})
    target_include_directories(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${IMGUI_INCLUDE_DIRS})
    target_compile_definitions(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_IMGUI USE_VULKAN)
    if(USE_SDL AND TARGET SDL3::SDL3)
      target_link_libraries(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE SDL3::SDL3)
    endif()
  else()
    if(USE_VULKAN)
      target_sources(${RENDERER_PREFIX}_vulkan PRIVATE ${IMGUI_SRCS})
      target_include_directories(${RENDERER_PREFIX}_vulkan PRIVATE ${IMGUI_INCLUDE_DIRS})
      target_compile_definitions(${RENDERER_PREFIX}_vulkan PRIVATE USE_IMGUI USE_VULKAN)
      if(USE_SDL AND TARGET SDL3::SDL3)
        target_link_libraries(${RENDERER_PREFIX}_vulkan PRIVATE SDL3::SDL3)
      endif()
    endif()
  endif()
  message(STATUS "ImGui Inspector: enabled")
  if(NOT MSVC)
    set_source_files_properties(
      renderers/vulkan/inspector/vk_imgui_scene.cpp
      PROPERTIES COMPILE_OPTIONS "-fno-operator-names")
  endif()
endif()

# common function, window system
IF(ANDROID)
	SET(Q3_SRCS
		engine/platform/unix/unix_shared.c
		engine/platform/android/android_stubs.c
		engine/platform/android/android_main.c
		engine/platform/android/android_asset_bootstrap.c
		engine/platform/android/android_logcat.c
		engine/platform/android/android_touch_overlay.c)
	SET(Q3_UI_SRCS "")
ELSEIF(UNIX)
	SET (Q3_SRCS
		engine/platform/unix/unix_main.c
		engine/platform/unix/unix_shared.c
		engine/platform/unix/linux_signals.c)

	IF(USE_SDL)
		list(APPEND Q3_UI_SRCS
			engine/platform/sdl/sdl_gamma.c
			engine/platform/sdl/sdl_glimp.c
			engine/platform/sdl/sdl_input.c)
	ELSE()
		SET(Q3_UI_SRCS
			engine/platform/unix/linux_glimp.c
			engine/platform/unix/linux_snd.c
			engine/platform/unix/x11_dga.c
			engine/platform/unix/x11_randr.c
			engine/platform/unix/x11_vidmode.c)
		IF(USE_VULKAN)
			list(APPEND Q3_UI_SRCS engine/platform/unix/linux_qvk.c)
		ENDIF()
	ENDIF()

	IF(APPLE)
		SET(EXE_TYPE MACOSX_BUNDLE)
		list(APPEND Q3_SRCS engine/platform/unix/quake3_flat.icns)
		set_source_files_properties(engine/platform/unix/quake3_flat.icns PROPERTIES
			MACOSX_PACKAGE_LOCATION Resources)
		SET(MACOSX_BUNDLE_ICON_FILE quake3_flat.icns)
	ENDIF()
ELSEIF(WIN32)
	SET(EXE_TYPE WIN32)
	IF(MSVC)
		SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /MANIFEST:NO")
		ADD_COMPILE_DEFINITIONS(_CRT_SECURE_NO_WARNINGS) # v3.12+
	ENDIF()

	SET(Q3_SRCS
		engine/platform/win32/win_main.c
		engine/platform/win32/win_shared.c
		engine/platform/win32/win_syscon.c
		engine/platform/win32/win_resource.rc)
	
	# Set include directory for resource compiler to find icon and manifest files
	if(MINGW)
		set_source_files_properties(engine/platform/win32/win_resource.rc PROPERTIES
			COMPILE_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/engine/platform/win32")
	endif()

	IF(USE_SDL)
		SET(Q3_UI_SRCS
			engine/platform/sdl/sdl_gamma.c
			engine/platform/sdl/sdl_glimp.c
			engine/platform/sdl/sdl_input.c
			engine/platform/win32/win_minimize.c)
	ELSE()
		SET(Q3_UI_SRCS
			engine/platform/win32/win_gamma.c
			engine/platform/win32/win_glimp.c
			engine/platform/win32/win_input.c
			engine/platform/win32/win_minimize.c
			engine/platform/win32/win_wndproc.c
			engine/platform/win32/win_snd.c)
	ENDIF()
		IF(USE_VULKAN AND NOT USE_SDL)
			list(APPEND Q3_UI_SRCS engine/platform/win32/win_qvk.c)
		ENDIF()
ENDIF()

# Audio backends/effects are built with the client module only.

IF(Q3_UI_SRCS)
ADD_LIBRARY(q3ui OBJECT ${Q3_UI_SRCS})
idtech3_first_party_warnings_as_errors(q3ui)
target_include_directories(q3ui PRIVATE
	${IDTECH3_DIR_ENGINE_CORE}
	${CMAKE_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/engine/platform
	${CMAKE_SOURCE_DIR}/runtime/client
	${CMAKE_SOURCE_DIR}/modules/audio)
ENDIF()

if(USE_LOCAL_HEADERS AND WIN32)
	target_include_directories(client PRIVATE ${EXTERNAL_CURL_WINDOWS_INCLUDE_DIR})
endif()

IF(NOT MSVC)
	IF(USE_SDL)
		if(NOT SDL3_FOUND OR NOT TARGET SDL3::SDL3)
		  message(FATAL_ERROR "SDL3 could not be found. Install SDL3 dev packages, set SDL3_DIR, or enable USE_BUNDLED_SDL3=ON (default) for an automatic source build.")
		endif()
		if(TARGET q3ui)
			TARGET_LINK_LIBRARIES(q3ui SDL3::SDL3)
		endif()
		TARGET_LINK_LIBRARIES(client PRIVATE SDL3::SDL3)
	ELSEIF(UNIX AND NOT ANDROID)
		find_package (X11 REQUIRED)
		if(TARGET q3ui)
			TARGET_LINK_LIBRARIES(q3ui ${X11_LIBRARIES})
		endif()
	ENDIF()
ENDIF()

if(TARGET q3ui)
	target_include_directories(q3ui PRIVATE ${CMAKE_SOURCE_DIR}/modules/audio)
endif()

IF(USE_OPENAL)
	if(NOT OPENAL_FOUND)
	  message(FATAL_ERROR "OpenAL could not be found. Please install or set OPENAL paths.")
	endif()
	if(TARGET q3ui)
		TARGET_COMPILE_DEFINITIONS(q3ui PRIVATE USE_OPENAL)
		TARGET_COMPILE_DEFINITIONS(q3ui PRIVATE USE_OPENAL_EFX)
		target_include_directories(q3ui PRIVATE ${OPENAL_INCLUDE_DIR})
	endif()
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_OPENAL)
	target_include_directories(client PRIVATE ${OPENAL_INCLUDE_DIR})
ENDIF()

if(USE_OPUS)
  if(OPUS_FOUND)
    TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_OPUS)
    target_include_directories(client PRIVATE ${OPUSFILE_INCLUDE_DIR} ${OPUS_INCLUDE_DIR})
    target_link_libraries(client PRIVATE ${OPUSFILE_LIBRARY} ${OPUS_LIBRARY} ${OGG_LIBRARY})
  else()
    message(WARNING "Opus requested but libopusfile/libopus/ogg not found; Opus codec disabled.")
  endif()
endif()

if(USE_FLAC)
  if(FLAC_FOUND)
    TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_FLAC)
    target_include_directories(client PRIVATE ${FLAC_INCLUDE_DIR})
    target_link_libraries(client PRIVATE ${FLAC_LIBRARY})
  else()
    message(WARNING "FLAC requested but libFLAC not found; FLAC codec disabled.")
  endif()
endif()

if(USE_WEBM)
  if(WEBM_FOUND)
    TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_WEBM)
    target_include_directories(client PRIVATE ${NESTEGG_INCLUDE_DIR} ${OPUS_INCLUDE_DIR})
    target_link_libraries(client PRIVATE ${NESTEGG_LIBRARY} ${OPUS_LIBRARY})
  else()
    message(WARNING "WebM requested but libnestegg/libopus not found; WebM codec disabled.")
  endif()
endif()

if(TARGET q3ui)
	TARGET_COMPILE_DEFINITIONS(q3ui PRIVATE USE_VULKAN_API)
endif()
TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_VULKAN_API)

if(USE_VUDA)
  target_compile_definitions(client PRIVATE USE_VUDA=1)
  if(USE_RENDERER_DLOPEN)
    target_link_libraries(client PRIVATE ${CMAKE_DL_LIBS})
  endif()
  message(STATUS "VUDA client: cl_vuda + vuda_cuda (dlopen libcudart)")
endif()

if(USE_IDTECH3_EMULATOR)
  idtech3_emulator_apply_compile_defs(client)
  target_compile_definitions(client PRIVATE USE_IDTECH3_EMULATOR=1)
  target_include_directories(client PRIVATE ${CMAKE_SOURCE_DIR}/extensions/emulator)
  message(STATUS "idTech3-Emulator client: cl_emulator + QEMU guest bridge (cl_emulator 1 + r_emulatorScreen 1)")
endif()

if(USE_IMGUI AND NOT APPLE)
	if(TARGET client)
		target_compile_definitions(client PRIVATE USE_IMGUI)
	endif()
endif()

IF(USE_MP3)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_MP3)
ENDIF()

# --- Modern video codec compile definitions and link libraries ---
if(FFMPEG_FOUND)
  TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_FFMPEG)
  target_include_directories(client PRIVATE ${FFMPEG_PKG_INCLUDE_DIRS})
  target_link_libraries(client PRIVATE ${FFMPEG_PKG_LINK_LIBRARIES})
endif()

if(DAV1D_FOUND)
  TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_DAV1D)
  target_include_directories(client PRIVATE ${DAV1D_PKG_INCLUDE_DIRS})
  target_link_libraries(client PRIVATE ${DAV1D_PKG_LINK_LIBRARIES})
endif()

if(DAV2D_FOUND)
  TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_DAV2D)
  target_include_directories(client PRIVATE ${DAV2D_PKG_INCLUDE_DIRS})
  target_link_libraries(client PRIVATE ${DAV2D_PKG_LINK_LIBRARIES})
endif()

if(VVDEC_FOUND)
  TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_VVDEC)
  target_include_directories(client PRIVATE ${VVDEC_PKG_INCLUDE_DIRS})
  target_link_libraries(client PRIVATE ${VVDEC_PKG_LINK_LIBRARIES})
endif()

if(VPX_FOUND)
  TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_VPX)
  target_include_directories(client PRIVATE ${VPX_PKG_INCLUDE_DIRS})
  target_link_libraries(client PRIVATE ${VPX_PKG_LINK_LIBRARIES})
endif()

if(THEORA_FOUND)
  TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_THEORA)
  target_include_directories(client PRIVATE ${THEORA_PKG_INCLUDE_DIRS})
  target_link_libraries(client PRIVATE ${THEORA_PKG_LINK_LIBRARIES})
endif()

# Ensure GNU-style --export-dynamic is not present in global linker flags on macOS
if(APPLE)
  if(DEFINED CMAKE_EXE_LINKER_FLAGS)
    string(REPLACE "-Wl,--export-dynamic" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
  endif()
  if(DEFINED CMAKE_SHARED_LINKER_FLAGS)
    string(REPLACE "-Wl,--export-dynamic" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
  endif()
endif()

# --- Recast Navigation module ---
option(USE_RECAST_NAV "Enable Recast/Detour navigation mesh" ON)
if(USE_RECAST_NAV)
  enable_language(CXX)
  file(GLOB RECAST_SRCS
    third_party/src/recast/Recast/Source/*.cpp
    third_party/src/recast/Detour/Source/*.cpp
    third_party/src/recast/DetourCrowd/Source/*.cpp
    third_party/src/recast/DetourTileCache/Source/*.cpp
  )
  ADD_LIBRARY(recast_nav STATIC ${RECAST_SRCS} modules/navigation/nav_recast.cpp modules/navigation/nav_bsp_extract.cpp)
  target_include_directories(recast_nav PRIVATE
    ${CMAKE_SOURCE_DIR}/third_party/src/recast/Recast/Include
    ${CMAKE_SOURCE_DIR}/third_party/src/recast/Detour/Include
    ${CMAKE_SOURCE_DIR}/third_party/src/recast/DetourCrowd/Include
    ${CMAKE_SOURCE_DIR}/third_party/src/recast/DetourTileCache/Include
    ${CMAKE_SOURCE_DIR}/modules/navigation
    ${IDTECH3_DIR_ENGINE_CORE}
  )
  set_property(TARGET recast_nav PROPERTY POSITION_INDEPENDENT_CODE ON)
  set_target_properties(recast_nav PROPERTIES
    CXX_STANDARD ${IDTECH3_CXX_STANDARD} CXX_STANDARD_REQUIRED ON)
  if(NOT MSVC)
    target_compile_options(recast_nav PRIVATE -Wno-unused-parameter -Wno-sign-compare -Wno-cast-qual -Wno-shadow)
  endif()
  message(STATUS "Recast Navigation: enabled")
else()
  ADD_LIBRARY(recast_nav STATIC modules/navigation/nav_recast_stub.c)
  target_include_directories(recast_nav PRIVATE ${CMAKE_SOURCE_DIR}/modules/navigation ${IDTECH3_DIR_ENGINE_CORE})
  set_property(TARGET recast_nav PROPERTY POSITION_INDEPENDENT_CODE ON)
  message(STATUS "Recast Navigation: stub (USE_RECAST_NAV=OFF)")
endif()

# --- ECS (EnTT) module - alternative to gentity ---
enable_language(CXX)
ADD_LIBRARY(ecs_module STATIC runtime/game/ecs.cpp)
set_target_properties(ecs_module PROPERTIES
  CXX_STANDARD ${IDTECH3_CXX_STANDARD}
  CXX_STANDARD_REQUIRED ON
)
target_include_directories(ecs_module PRIVATE
  ${EXTERNAL_INCLUDE}/entt/single_include
  ${CMAKE_SOURCE_DIR}/runtime/game
  ${IDTECH3_DIR_ENGINE_CORE}
)
if(NOT MSVC)
  target_compile_options(ecs_module PRIVATE -Wno-unused-parameter)
endif()
message(STATUS "ECS (EnTT): enabled")

# --- Physics module (swappable substrate: box3d default, bullet / jolt optional) ---
set(IDTECH3_PHYSICS_BACKEND "box3d" CACHE STRING "Physics substrate: box3d | bullet | jolt | none")
set_property(CACHE IDTECH3_PHYSICS_BACKEND PROPERTY STRINGS box3d bullet jolt none)
# Legacy alias: USE_BULLET_PHYSICS=ON with backend=bullet still works
option(USE_BULLET_PHYSICS "Legacy: prefer Bullet when IDTECH3_PHYSICS_BACKEND is unset/auto" OFF)

set(PHYSICS_SRCS
  modules/physics/phys_bullet.c
  modules/physics/phys_character.c
  modules/physics/phys_procedural_anim.c
  modules/physics/phys_dmm.c
  modules/physics/phys_ik.c
  modules/physics/phys_dmm_materials.c
  modules/physics/phys_cloth.c
  modules/physics/phys_solvers.c
  modules/physics/phys_particles.c
  modules/physics/phys_softblob.c
  modules/physics/phys_fluid.c
  modules/physics/phys_events.c
  modules/physics/phys_materials.c
  modules/physics/phys_motor.c
  modules/physics/phys_middleware.c
  modules/physics/phys_props.c
  modules/physics/phys_volumes.c
  modules/physics/phys_debugdraw.c
  modules/physics/phys_ragdoll_bind.c
)

set(IDTECH3_PHYSICS_BACKEND_EFFECTIVE "${IDTECH3_PHYSICS_BACKEND}")
if(USE_BULLET_PHYSICS AND IDTECH3_PHYSICS_BACKEND STREQUAL "box3d")
  # Explicit legacy opt-in from older scripts: force bullet if requested via USE_BULLET_PHYSICS
  # only when user passes -DUSE_BULLET_PHYSICS=ON alongside default box3d — honor bullet.
  # Prefer IDTECH3_PHYSICS_BACKEND; compile_engine.sh sets both explicitly.
endif()

set(BULLET_FOUND FALSE)
set(BOX3D_FOUND FALSE)
set(JOLT_FOUND FALSE)

if(IDTECH3_PHYSICS_BACKEND_EFFECTIVE STREQUAL "box3d")
  set(_IDTECH3_BOX3D_DIR "${CMAKE_SOURCE_DIR}/third_party/box3d")
  if(EXISTS "${_IDTECH3_BOX3D_DIR}/CMakeLists.txt")
    set(BOX3D_VALIDATE OFF CACHE BOOL "Box3D heavy validation" FORCE)
    set(BOX3D_UNIT_TESTS OFF CACHE BOOL "Box3D unit tests" FORCE)
    set(BOX3D_SAMPLES OFF CACHE BOOL "Box3D samples" FORCE)
    add_subdirectory("${_IDTECH3_BOX3D_DIR}" "${CMAKE_BINARY_DIR}/third_party/box3d" EXCLUDE_FROM_ALL)
    list(APPEND PHYSICS_SRCS modules/physics/phys_box3d_impl.c)
    set(BOX3D_FOUND TRUE)
    message(STATUS "Physics backend: Box3D (default, third_party/box3d)")
  else()
    message(WARNING "Physics backend box3d requested but third_party/box3d missing — run: git submodule update --init third_party/box3d")
    set(IDTECH3_PHYSICS_BACKEND_EFFECTIVE "none")
  endif()
endif()

if(IDTECH3_PHYSICS_BACKEND_EFFECTIVE STREQUAL "bullet")
  if(NOT ANDROID)
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
      pkg_check_modules(BULLET_PKG QUIET bullet)
      if(BULLET_PKG_FOUND)
        set(BULLET_FOUND TRUE)
        list(APPEND PHYSICS_SRCS modules/physics/phys_bullet_impl.cpp)
        message(STATUS "Physics backend: Bullet (C++ libbullet)")
      endif()
    endif()
  endif()
  if(NOT BULLET_FOUND)
    message(WARNING "Physics backend bullet requested but libbullet not found — substrate stubs only")
    set(IDTECH3_PHYSICS_BACKEND_EFFECTIVE "none")
  endif()
endif()

if(IDTECH3_PHYSICS_BACKEND_EFFECTIVE STREQUAL "jolt")
  set(_IDTECH3_JOLT_DIR "${CMAKE_SOURCE_DIR}/third_party/JoltPhysics")
  if(EXISTS "${_IDTECH3_JOLT_DIR}/Build/CMakeLists.txt")
    set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "JoltPhysics warnings in parent build" FORCE)
    set(ENABLE_INSTALL OFF CACHE BOOL "JoltPhysics install targets" FORCE)
    set(JPH_USE_DX12 OFF CACHE BOOL "JoltPhysics DX12 compute backend" FORCE)
    set(JPH_USE_VK OFF CACHE BOOL "JoltPhysics Vulkan compute backend" FORCE)
    set(JPH_USE_MTL OFF CACHE BOOL "JoltPhysics Metal compute backend" FORCE)
    set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "JoltPhysics compute fallback" FORCE)
    set(TARGET_UNIT_TESTS OFF CACHE BOOL "JoltPhysics unit tests" FORCE)
    set(TARGET_HELLO_WORLD OFF CACHE BOOL "JoltPhysics hello world" FORCE)
    set(TARGET_PERFORMANCE_TEST OFF CACHE BOOL "JoltPhysics performance test" FORCE)
    set(TARGET_SAMPLES OFF CACHE BOOL "JoltPhysics samples" FORCE)
    set(TARGET_VIEWER OFF CACHE BOOL "JoltPhysics viewer" FORCE)
    add_subdirectory("${_IDTECH3_JOLT_DIR}/Build" "${CMAKE_BINARY_DIR}/third_party/JoltPhysics" EXCLUDE_FROM_ALL)
    list(APPEND PHYSICS_SRCS modules/physics/phys_jolt_impl.cpp)
    set(JOLT_FOUND TRUE)
    message(STATUS "Physics backend: Jolt (optional, third_party/JoltPhysics)")
  else()
    message(WARNING "Physics backend jolt requested but third_party/JoltPhysics missing — run: git submodule update --init third_party/JoltPhysics")
    set(IDTECH3_PHYSICS_BACKEND_EFFECTIVE "none")
  endif()
endif()

if(IDTECH3_PHYSICS_BACKEND_EFFECTIVE STREQUAL "none")
  message(STATUS "Physics backend: none (middleware APIs present, no rigid-body substrate)")
endif()

ADD_LIBRARY(phys_module OBJECT ${PHYSICS_SRCS})
idtech3_first_party_warnings_as_errors(phys_module)
set_target_properties(phys_module PROPERTIES
  CXX_STANDARD ${IDTECH3_CXX_STANDARD} CXX_STANDARD_REQUIRED ON)
target_include_directories(phys_module PRIVATE
  ${CMAKE_SOURCE_DIR}/modules/physics
  ${IDTECH3_DIR_ENGINE_CORE}
)
if(NOT MSVC)
  target_compile_options(phys_module PRIVATE -Wno-unused-parameter)
endif()
if(BOX3D_FOUND)
  target_compile_definitions(phys_module PRIVATE USE_BOX3D_PHYSICS_IMPL)
  target_include_directories(phys_module PRIVATE ${CMAKE_SOURCE_DIR}/third_party/box3d/include)
  target_link_libraries(phys_module PUBLIC box3d)
endif()
if(BULLET_FOUND)
  target_compile_definitions(phys_module PRIVATE USE_BULLET_PHYSICS_IMPL)
  target_include_directories(phys_module PRIVATE ${BULLET_PKG_INCLUDE_DIRS})
  target_link_libraries(phys_module PUBLIC ${BULLET_PKG_LINK_LIBRARIES})
  enable_language(CXX)
endif()
if(JOLT_FOUND)
  target_compile_definitions(phys_module PRIVATE USE_JOLT_PHYSICS_IMPL)
  target_link_libraries(phys_module PUBLIC Jolt)
  enable_language(CXX)
endif()
# OpenAL/Bullet DSOs need libstdc++ with CXXABI_1.3.15+. CUDA may inject -L.../gcc/13,
# so resolve the host C++ compiler's libstdc++.so and link it last on Linux executables.
set(IDTECH3_LIBSTDCXX_USE_EXPLICIT FALSE)
if(UNIX AND NOT MSVC AND NOT APPLE AND NOT ANDROID)
  if(NOT CMAKE_CXX_COMPILER)
    set(_IDTECH3_LIBSTDCXX_CXX "${CMAKE_C_COMPILER}")
  else()
    set(_IDTECH3_LIBSTDCXX_CXX "${CMAKE_CXX_COMPILER}")
  endif()
  execute_process(
    COMMAND ${_IDTECH3_LIBSTDCXX_CXX} -print-file-name=libstdc++.so
    OUTPUT_VARIABLE IDTECH3_LIBSTDCXX
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(IDTECH3_LIBSTDCXX AND NOT IDTECH3_LIBSTDCXX MATCHES "^libstdc\\+\\+\\.so$")
    set(IDTECH3_LIBSTDCXX_USE_EXPLICIT TRUE)
    message(STATUS "Link libstdc++ (after OpenAL/Bullet): ${IDTECH3_LIBSTDCXX}")
  endif()
endif()

# client executable / shared library

IF(ANDROID)
	ADD_LIBRARY(${CNAME}${BINEXT} SHARED ${Q3_SRCS})
ELSE()
	ADD_EXECUTABLE(${CNAME}${BINEXT} ${EXE_TYPE} ${Q3_SRCS})
ENDIF()
idtech3_first_party_warnings_as_errors(${CNAME}${BINEXT})
if(LUA_ANDROID_BUILT)
	add_dependencies(${CNAME}${BINEXT} lua_android_ep)
endif()
if(ANDROID AND USE_DTLS_ANDROID_BUILT)
	add_dependencies(${CNAME}${BINEXT} openssl_android_ep)
endif()
if(TARGET q3ui)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE qcommon botlib client q3ui phys_module)
else()
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE qcommon botlib client phys_module)
endif()
# unix_main/win_main (Q3_SRCS) include SDL_main.h; link SDL3 so includes propagate to the exe TU.
if(USE_SDL AND TARGET SDL3::SDL3)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE SDL3::SDL3)
endif()
# Platform entry sources need engine headers (OBJECT libs keep includes PRIVATE).
target_include_directories(${CNAME}${BINEXT} PRIVATE
	${IDTECH3_DIR_ENGINE_CORE}
	${CMAKE_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/engine/platform
	${CMAKE_SOURCE_DIR}/runtime/client)
if(TARGET Threads::Threads)
  TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE Threads::Threads)
endif()
if(TARGET recast_nav)
  TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE recast_nav)
endif()
TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ecs_module)
if(USE_FLUX)
	TARGET_COMPILE_DEFINITIONS(${CNAME}${BINEXT} PRIVATE USE_FLUX)
	if(TARGET cflux2)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE cflux2)
	endif()
endif()
# Link OpenAL only when enabled
IF(USE_OPENAL)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE OpenAL::OpenAL)
ENDIF()
#TARGET_LINK_OPTIONS(${CNAME} PRIVATE -fno-pie) # v3.13+

if(UNIX AND NOT MSVC AND NOT APPLE)
  # Ensure main executable exports symbols so dlopen'd renderer plugins can resolve engine symbols.
  # --export-dynamic is a GNU ld option and should not be used on macOS (Darwin) where the linker rejects it.
  target_link_options(${CNAME}${BINEXT} PRIVATE "-Wl,--export-dynamic")
endif()

IF(NOT USE_RENDERER_DLOPEN)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${RENDERER_PREFIX}_vulkan)
ENDIF()
if(USE_RADIUSFPS_CUDA)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE CUDA::cudart)
endif()

# dedicated server executable (not built on Android)

IF(NOT ANDROID)
# Dedicated server must be a console subsystem on Windows so CI smoke tests can read stdout.
IF(WIN32)
ADD_EXECUTABLE(${DNAME}${BINEXT} ${Q3_SRCS})
ELSE()
ADD_EXECUTABLE(${DNAME}${BINEXT} ${EXE_TYPE} ${Q3_SRCS})
ENDIF()
idtech3_first_party_warnings_as_errors(${DNAME}${BINEXT})
TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE qcommon_ded botlib phys_module)
if(TARGET Threads::Threads)
  TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE Threads::Threads)
endif()
# Platform entry (unix_main / win_main) needs q_shared.h / qcommon.h.
target_include_directories(${DNAME}${BINEXT} PRIVATE
	${IDTECH3_DIR_ENGINE_CORE}
	${CMAKE_SOURCE_DIR}
	${CMAKE_SOURCE_DIR}/engine/platform)
#TARGET_LINK_OPTIONS(${DNAME} PRIVATE -fno-pie) # v3.13+

if(UNIX AND NOT MSVC AND NOT APPLE)
  target_link_options(${DNAME}${BINEXT} PRIVATE "-Wl,--export-dynamic")
endif()
ENDIF() # NOT ANDROID (server target block)

if(USE_LUA AND LUA_FOUND)
  TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${LUA_LINK_LIBRARIES})
  if(NOT ANDROID)
    TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE ${LUA_LINK_LIBRARIES})
  endif()
endif()
if(USE_DUKTAPE AND DUKTAPE_FOUND)
  TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${DUKTAPE_LINK_LIBRARIES})
  if(NOT ANDROID)
    TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE ${DUKTAPE_LINK_LIBRARIES})
  endif()
endif()
if(USE_CSHARP AND CSHARP_FOUND)
  TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${CSHARP_LINK_LIBRARIES})
  if(NOT ANDROID)
    TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE ${CSHARP_LINK_LIBRARIES})
  endif()
endif()
if(USE_PYTHON AND PYTHON_FOUND)
  TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${PYTHON_LINK_LIBRARIES})
  if(NOT ANDROID)
    TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE ${PYTHON_LINK_LIBRARIES})
  endif()
endif()
if(USE_RADIUSFPS_CUDA AND NOT ANDROID)
  TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE CUDA::cudart)
endif()
if((USE_STEAM OR USE_STEAM_NETWORKING) AND STEAMWORKS_FOUND)
  TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${STEAMWORKS_LIBRARY})
  if(UNIX AND NOT APPLE AND NOT ANDROID)
    # Find libsteam_api.so next to the executable (copied into build/ and release/).
    set_property(TARGET ${CNAME}${BINEXT} APPEND PROPERTY BUILD_RPATH "$ORIGIN")
    set_property(TARGET ${CNAME}${BINEXT} APPEND PROPERTY INSTALL_RPATH "$ORIGIN")
  endif()
  if(USE_STEAM_NETWORKING AND NOT ANDROID)
    TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE ${STEAMWORKS_LIBRARY})
    if(UNIX AND NOT APPLE)
      set_property(TARGET ${DNAME}${BINEXT} APPEND PROPERTY BUILD_RPATH "$ORIGIN")
      set_property(TARGET ${DNAME}${BINEXT} APPEND PROPERTY INSTALL_RPATH "$ORIGIN")
    endif()
  endif()
endif()

IF(WIN32)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE winmm comctl32 ws2_32)
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE winmm comctl32 ws2_32)
ELSEIF(NOT ANDROID)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE m ${CMAKE_DL_LIBS})
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE m ${CMAKE_DL_LIBS})
ELSE()
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE m ${CMAKE_DL_LIBS} log android OpenSLES aaudio)
ENDIF()

if(UNIX AND NOT APPLE AND NOT ANDROID AND IDTECH3_X11_LIBS)
	if(IDTECH3_X11_INCLUDE)
		target_include_directories(${CNAME}${BINEXT} PRIVATE ${IDTECH3_X11_INCLUDE})
		target_include_directories(${DNAME}${BINEXT} PRIVATE ${IDTECH3_X11_INCLUDE})
	endif()
	target_link_libraries(${CNAME}${BINEXT} PRIVATE ${IDTECH3_X11_LIBS})
	target_link_libraries(${DNAME}${BINEXT} PRIVATE ${IDTECH3_X11_LIBS})
endif()

if(UNIX AND NOT MSVC AND NOT APPLE AND NOT ANDROID)
  if(IDTECH3_LIBSTDCXX_USE_EXPLICIT)
    target_link_libraries(${CNAME}${BINEXT} PRIVATE
      "-Wl,--push-state" "-Wl,--no-as-needed" "${IDTECH3_LIBSTDCXX}" "-Wl,--pop-state")
    target_link_libraries(${DNAME}${BINEXT} PRIVATE
      "-Wl,--push-state" "-Wl,--no-as-needed" "${IDTECH3_LIBSTDCXX}" "-Wl,--pop-state")
  else()
    target_link_libraries(${CNAME}${BINEXT} PRIVATE
      "-Wl,--push-state" "-Wl,--no-as-needed" stdc++ "-Wl,--pop-state")
    target_link_libraries(${DNAME}${BINEXT} PRIVATE
      "-Wl,--push-state" "-Wl,--no-as-needed" stdc++ "-Wl,--pop-state")
  endif()
endif()

# Deterministic Vulkan shader codegen/check tooling.
# Usage:
#   cmake --build <build-dir> --target vk_shader_regen
#   cmake --build <build-dir> --target vk_shader_check
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
	add_custom_target(vk_shader_regen
		COMMAND ${CMAKE_SOURCE_DIR}/scripts/compile_shaders.sh --apply
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMENT "Regenerating Vulkan shader_data.c and shader_binding.c")

	add_custom_target(vk_shader_check
		COMMAND ${CMAKE_SOURCE_DIR}/scripts/compile_shaders.sh --check
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMENT "Checking Vulkan generated shader blobs are up-to-date with GLSL")
else()
	message(STATUS "Python3 not found: vk_shader_regen/vk_shader_check targets are unavailable.")
endif()

# --- CTest validation ---
enable_testing()
add_test(NAME smoke_test
	COMMAND ${CMAKE_SOURCE_DIR}/scripts/smoke_test.sh ${CMAKE_SOURCE_DIR}/release
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

if(USE_FREEUSD)
	add_test(NAME test_freeusd_smoke
		COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_freeusd_smoke.sh ${CMAKE_SOURCE_DIR}/release
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
	set_tests_properties(test_freeusd_smoke PROPERTIES
		LABELS "validation;freeusd"
		REQUIRED_FILES "${CMAKE_SOURCE_DIR}/release/idtech3_vulkan.so")
endif()
set_tests_properties(smoke_test PROPERTIES
	LABELS "validation;smoke"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/release/idtech3_server"
	# Avoid parallel runs with renderer_regression_check (both hammer glslang; server startup is CPU-sensitive).
	RESOURCE_LOCK "idtech3_headless_validation")

add_test(NAME renderer_regression_check
	COMMAND ${CMAKE_SOURCE_DIR}/scripts/renderer_regression_check.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(renderer_regression_check PROPERTIES
	LABELS "validation;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/scripts/renderer_regression_check.sh"
	RESOURCE_LOCK "idtech3_headless_validation")

add_test(NAME gpu_golden_compare
	COMMAND ${CMAKE_SOURCE_DIR}/scripts/gpu_golden_capture.sh --compare
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(gpu_golden_compare PROPERTIES
	LABELS "validation;renderer;golden"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/scripts/gpu_golden_capture.sh")

add_test(NAME check_artifacts
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/check_artifacts.sh ${CMAKE_SOURCE_DIR}/release
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(check_artifacts PROPERTIES
	LABELS "validation;artifacts"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/release/idtech3_server")

add_test(NAME test_run_vulkan_script
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_run_vulkan.sh ${CMAKE_SOURCE_DIR}/scripts/run_vulkan.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_run_vulkan_script PROPERTIES
	LABELS "unit;scripts;validation")

add_test(NAME test_demo_game_pk3
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_demo_game_pk3.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_demo_game_pk3 PROPERTIES
	LABELS "scripts;validation;examples"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_demo_game_pk3.sh")

add_test(NAME test_demo_openworld_pk3
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_demo_openworld_pk3.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_demo_openworld_pk3 PROPERTIES
	LABELS "scripts;validation;examples;openworld;sector_stream"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_demo_openworld_pk3.sh;${CMAKE_SOURCE_DIR}/examples/demo_game/mod/demo_openworld.cfg;${CMAKE_SOURCE_DIR}/scripts/bake_staged_openworld_nav.sh")

add_test(NAME test_check_loc
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_check_loc.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_check_loc PROPERTIES
	LABELS "scripts;validation;i18n"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_check_loc.sh;${CMAKE_SOURCE_DIR}/scripts/check_loc_keys.sh")

add_test(NAME test_ubertools_clean_room
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_ubertools_clean_room.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_ubertools_clean_room PROPERTIES
	LABELS "scripts;validation;ubertools"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_ubertools_clean_room.sh;${CMAKE_SOURCE_DIR}/docs/UBERTOOLS_CLEAN_ROOM.md")

add_test(NAME test_validate_assets
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_validate_assets.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_validate_assets PROPERTIES
	LABELS "scripts;validation;assets"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_validate_assets.sh;${CMAKE_SOURCE_DIR}/scripts/validate_assets.sh")

add_test(NAME test_asset_pipeline
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_asset_pipeline.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_asset_pipeline PROPERTIES
	LABELS "scripts;validation;assets"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_asset_pipeline.sh;${CMAKE_SOURCE_DIR}/scripts/asset_pipeline.sh;${CMAKE_SOURCE_DIR}/examples/demo_game/asset_pipeline.conf")

add_test(NAME test_game_starter_template
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_game_starter_template.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_game_starter_template PROPERTIES
	LABELS "scripts;validation;templates"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_game_starter_template.sh;${CMAKE_SOURCE_DIR}/scripts/create_starter_game.sh;${CMAKE_SOURCE_DIR}/examples/templates/game.starter/game.idproj")

add_test(NAME test_sqlite_service
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_sqlite_service.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_sqlite_service PROPERTIES
	LABELS "scripts;validation;database"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_sqlite_service.sh;${IDTECH3_DIR_ENGINE_CORE}/engine_db.c;${CMAKE_SOURCE_DIR}/engine/core/csharp/IdTech3.Engine.cs")

add_test(NAME test_crash_report
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_crash_report.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_crash_report PROPERTIES
	LABELS "scripts;validation;crash"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_crash_report.sh")

add_test(NAME test_engine_save
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_engine_save.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_engine_save PROPERTIES
	LABELS "scripts;validation;engine"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_engine_save.sh;${CMAKE_SOURCE_DIR}/runtime/game/g_engine_systems.c")

add_test(NAME test_squeezeme
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_squeezeme.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_squeezeme PROPERTIES
	LABELS "scripts;validation;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_squeezeme.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/extensions/splats/vk_squeezeme.c")

add_test(NAME test_hybrid1
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_hybrid1.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_hybrid1 PROPERTIES
	LABELS "scripts;validation;renderer;rtx"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_hybrid1.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/extensions/rtx/vk_hybrid1.c")

add_test(NAME test_voxel_sprites
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_voxel_sprites.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_voxel_sprites PROPERTIES
	LABELS "scripts;validation;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_voxel_sprites.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/tr_model_vox.c;${CMAKE_SOURCE_DIR}/docs/VOXEL_SPRITES.md")

add_test(NAME test_unified_clustered
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_unified_clustered.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_unified_clustered PROPERTIES
	LABELS "scripts;validation;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_unified_clustered.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/tr_render_mode_vk.c;${CMAKE_SOURCE_DIR}/docs/UNIFIED_CLUSTERED_RENDERER.md")

add_test(NAME test_visibility_buffer
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_visibility_buffer.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_visibility_buffer PROPERTIES
	LABELS "scripts;validation;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_visibility_buffer.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/vk_visibility_buffer.c;${CMAKE_SOURCE_DIR}/docs/RENDERER_2027.md")

add_test(NAME test_mboit_stochastic
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_mboit_stochastic.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_mboit_stochastic PROPERTIES
	LABELS "scripts;validation;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_mboit_stochastic.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/shaders/glsl/oit_moments.frag;${CMAKE_SOURCE_DIR}/docs/MOMENT_OIT_STOCHASTIC_ALPHA.md")

add_test(NAME test_upscale
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_upscale.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_upscale PROPERTIES
	LABELS "scripts;validation;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_upscale.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/vk_upscale.c")

add_test(NAME test_vt
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_vt.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_vt PROPERTIES
	LABELS "scripts;validation;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_vt.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/vk_vt.c")

add_test(NAME test_vdb_woodcock
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_vdb_woodcock.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_vdb_woodcock PROPERTIES
	LABELS "scripts;validation;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_vdb_woodcock.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/vk_vdb.c;${CMAKE_SOURCE_DIR}/examples/demo_game/bootstrap_media/vdb/fog_2cubed.nvdb")

add_test(NAME test_nanovdb_decode
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_nanovdb_decode.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_nanovdb_decode PROPERTIES
	LABELS "scripts;validation;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_nanovdb_decode.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/vk_nanovdb_decode.c;${CMAKE_SOURCE_DIR}/tests/data/fog_2cubed.nvdb")

add_test(NAME test_sdl3_features
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_sdl3_features.sh all
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_sdl3_features PROPERTIES
	LABELS "scripts;validation;client;sdl"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_sdl3_features.sh;${CMAKE_SOURCE_DIR}/engine/platform/sdl/sdl_input.c")

add_test(NAME test_module_include_canonical
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_module_include_canonical.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_module_include_canonical PROPERTIES
	LABELS "scripts;validation;layout"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_module_include_canonical.sh;${CMAKE_SOURCE_DIR}/modules/physics/phys_middleware.h;${CMAKE_SOURCE_DIR}/modules/navigation/nav_recast.h")

add_test(NAME test_meshlets
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_meshlets.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_meshlets PROPERTIES
	LABELS "scripts;validation;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_meshlets.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/vk_meshlets.c")

add_test(NAME test_dressi
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_dressi.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_dressi PROPERTIES
	LABELS "scripts;validation;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_dressi.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/extensions/scaffold/vk_dressi.c")

add_test(NAME test_raygun
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_raygun.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_raygun PROPERTIES
	LABELS "scripts;validation;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_raygun.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/extensions/rtx/vk_raygun.c")

add_test(NAME test_aiwc
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_aiwc.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_aiwc PROPERTIES
	LABELS "scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_aiwc.sh;${IDTECH3_DIR_ENGINE_CORE}/aiwc_metrics.c")

add_test(NAME test_ttp
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_ttp.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_ttp PROPERTIES
	LABELS "scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_ttp.sh;${IDTECH3_DIR_ENGINE_CORE}/ttp_model.c")

add_test(NAME test_bubblesh
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_bubblesh.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_bubblesh PROPERTIES
	LABELS "scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_bubblesh.sh;${IDTECH3_DIR_ENGINE_CORE}/bubblesh_model.c")

add_test(NAME test_vuda
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_vuda.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_vuda PROPERTIES
	LABELS "scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_vuda.sh;${CMAKE_SOURCE_DIR}/extensions/research/vuda/vuda_model.c")

add_test(NAME test_vksplat
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_vksplat.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_vksplat PROPERTIES
	LABELS "scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_vksplat.sh;${CMAKE_SOURCE_DIR}/extensions/research/vksplat/vksplat_model.c")

add_test(NAME test_curast
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_curast.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_curast PROPERTIES
	LABELS "scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_curast.sh;${CMAKE_SOURCE_DIR}/extensions/research/curast/curast_model.c")

add_test(NAME test_radiusfps
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_radiusfps.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_radiusfps PROPERTIES
	LABELS "scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_radiusfps.sh;${CMAKE_SOURCE_DIR}/extensions/research/radiusfps/radiusfps_cpu.c")

add_test(NAME test_x3dpra
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_x3dpra.sh
	${CMAKE_BINARY_DIR})
set_tests_properties(test_x3dpra PROPERTIES
	LABELS "smoke;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_x3dpra.sh;${CMAKE_SOURCE_DIR}/extensions/research/x3dpra/x3dpra_physics.c")

add_test(NAME test_dax
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_dax.sh
	${CMAKE_BINARY_DIR})
set_tests_properties(test_dax PROPERTIES
	LABELS "smoke;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_dax.sh;${CMAKE_SOURCE_DIR}/extensions/research/dax/dax_benchmark.c")

add_test(NAME test_dk_qsd
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_dk_qsd.sh
	${CMAKE_BINARY_DIR})
set_tests_properties(test_dk_qsd PROPERTIES
	LABELS "smoke;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_dk_qsd.sh;${CMAKE_SOURCE_DIR}/extensions/research/dk_qsd/dk_qsd_dense.c")

add_test(NAME test_dlm
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_dlm.sh
	${CMAKE_BINARY_DIR})
set_tests_properties(test_dlm PROPERTIES
	LABELS "smoke;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_dlm.sh;${CMAKE_SOURCE_DIR}/extensions/research/dlm/dlm_matrix.c")

	add_test(NAME test_sfca
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_sfca.sh
	${CMAKE_BINARY_DIR})
set_tests_properties(test_sfca PROPERTIES
	LABELS "smoke;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_sfca.sh;${CMAKE_SOURCE_DIR}/extensions/research/sfca/sfca_core.c")

add_test(NAME test_howdark
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_howdark.sh
	${CMAKE_BINARY_DIR})
set_tests_properties(test_howdark PROPERTIES
	LABELS "smoke;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_howdark.sh;${CMAKE_SOURCE_DIR}/extensions/research/howdark/howdark_model.c")

add_test(NAME test_nebrdf
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_nebrdf.sh
	${CMAKE_BINARY_DIR})
set_tests_properties(test_nebrdf PROPERTIES
	LABELS "smoke;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_nebrdf.sh;${CMAKE_SOURCE_DIR}/extensions/research/nebrdf/nebrdf_model.c")

add_test(NAME test_rtfem
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_rtfem.sh
	${CMAKE_BINARY_DIR})
set_tests_properties(test_rtfem PROPERTIES
	LABELS "smoke;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_rtfem.sh;${CMAKE_SOURCE_DIR}/extensions/research/rtfem/rtfem_model.c")

add_test(NAME test_vgs
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_vgs.sh
	${CMAKE_BINARY_DIR})
set_tests_properties(test_vgs PROPERTIES
	LABELS "smoke;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_vgs.sh;${CMAKE_SOURCE_DIR}/extensions/research/vgs/vgs_model.c")

add_test(NAME test_cem
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_cem.sh
	${CMAKE_BINARY_DIR})
set_tests_properties(test_cem PROPERTIES
	LABELS "smoke;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_cem.sh;${CMAKE_SOURCE_DIR}/extensions/research/cem/cem_model.c")

add_test(NAME test_gccfer
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_gccfer.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_gccfer PROPERTIES
	LABELS "scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_gccfer.sh;${CMAKE_SOURCE_DIR}/extensions/research/gccfer/gccfer_dataset.c")

add_test(NAME test_python
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_python.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_python PROPERTIES
	LABELS "scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_python.sh;${IDTECH3_DIR_ENGINE_CORE}/python_debug.c")

add_test(NAME test_app_crdt
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_app_crdt.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_app_crdt PROPERTIES
	LABELS "scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_app_crdt.sh;${CMAKE_SOURCE_DIR}/tests/scripts/idtech3_minimal_content_smoke.sh;${IDTECH3_DIR_ENGINE_CORE}/app_crdt.c")

add_test(NAME test_idtech3_emulator
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_idtech3_emulator.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_idtech3_emulator PROPERTIES
	LABELS "scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_idtech3_emulator.sh;${CMAKE_SOURCE_DIR}/cmake/IdTech3Emulator.cmake;${CMAKE_SOURCE_DIR}/docs/IDTECH3_EMULATOR.md")

add_executable(unit_emulator_contract tests/unit/test_emulator_contract.c)
add_test(NAME unit_emulator_contract COMMAND unit_emulator_contract)
set_tests_properties(unit_emulator_contract PROPERTIES LABELS "unit;validation")

add_test(NAME test_mimir
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_mimir.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_mimir PROPERTIES
	LABELS "scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_mimir.sh;${CMAKE_SOURCE_DIR}/extensions/research/mimir/mimir_model.c")

add_test(NAME test_iris
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_iris.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_iris PROPERTIES
	LABELS "scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_iris.sh;${CMAKE_SOURCE_DIR}/extensions/research/iris/iris_model.c")

add_test(NAME test_physics
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_physics.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_physics PROPERTIES
	LABELS "scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_physics.sh;${CMAKE_SOURCE_DIR}/tests/scripts/idtech3_minimal_content_smoke.sh;${CMAKE_SOURCE_DIR}/modules/physics/phys_middleware.c;${CMAKE_SOURCE_DIR}/docs/renderer_validation/devdata/rtest_base/maps/rtest_parity.bsp;${CMAKE_SOURCE_DIR}/docs/renderer_validation/devdata/rtest_base/vm/qagame.qvm")

add_test(NAME test_districts
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_districts.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_districts PROPERTIES
	LABELS "scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_districts.sh;${CMAKE_SOURCE_DIR}/modules/world/world_district.cpp;${CMAKE_SOURCE_DIR}/tests/data/usd/world_playfield.usda")

add_test(NAME test_openworld
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_openworld.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_openworld PROPERTIES
	LABELS "scripts;validation;openworld;sector_stream"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_openworld.sh;${CMAKE_SOURCE_DIR}/modules/world/world_open.cpp;${CMAKE_SOURCE_DIR}/tests/data/openworld/sprites/sector_0_0.ents")

add_test(NAME test_world_config
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_world_config.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_world_config PROPERTIES
	LABELS "scripts;validation;openworld;sector_stream"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_world_config.sh;${CMAKE_SOURCE_DIR}/modules/world/world_config.cpp;${CMAKE_SOURCE_DIR}/docs/WORLD_CONFIG.md")

add_test(NAME test_proc
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_proc.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_proc PROPERTIES
	LABELS "scripts;validation;openworld;sector_stream"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_proc.sh;${CMAKE_SOURCE_DIR}/modules/world/world_proc.cpp;${IDTECH3_DIR_RUNTIME_CLIENT}/world/cl_proc.cpp")

add_test(NAME test_cm_stream_merge
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_cm_stream_merge.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_cm_stream_merge PROPERTIES
	LABELS "scripts;validation;openworld;sector_stream"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_cm_stream_merge.sh;${CMAKE_SOURCE_DIR}/scripts/tools/gen_sector_bsp.py;${IDTECH3_DIR_ENGINE_CORE}/cm_stream_merge.c")

add_test(NAME test_nav_bake
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_nav_bake.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_nav_bake PROPERTIES
	LABELS "scripts;validation;openworld;sector_stream"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_nav_bake.sh;${CMAKE_SOURCE_DIR}/modules/navigation/nav_recast.cpp;${CMAKE_SOURCE_DIR}/tests/data/openworld/maps/sector_0_0.bsp;${CMAKE_SOURCE_DIR}/tests/data/openworld/nav/sector_0_0.nav;${CMAKE_SOURCE_DIR}/scripts/bake_openworld_nav.sh")

add_test(NAME test_openworld_sync
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_openworld_sync.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_openworld_sync PROPERTIES
	LABELS "scripts;validation;openworld;sector_stream"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_openworld_sync.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/tr_bsp_stream.c;${CMAKE_SOURCE_DIR}/runtime/server/sv_openworld.c")

add_test(NAME test_bsp_stream_vbo
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_bsp_stream_vbo.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_bsp_stream_vbo PROPERTIES
	LABELS "scripts;validation;openworld;sector_stream;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_bsp_stream_vbo.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/tr_bsp_stream.c;${CMAKE_SOURCE_DIR}/renderers/vulkan/tr_bsp.c;${CMAKE_SOURCE_DIR}/renderers/vulkan/vk_vbo.c")

add_test(NAME test_temporal_motion_policy
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_temporal_motion_policy.sh)
set_tests_properties(test_temporal_motion_policy PROPERTIES
	LABELS "scripts;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_temporal_motion_policy.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/vk_view_state.c;${CMAKE_SOURCE_DIR}/renderers/vulkan/vk_postfx_params.c")

add_test(NAME test_deferred_lighting
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_deferred_lighting.sh)
set_tests_properties(test_deferred_lighting PROPERTIES
	LABELS "scripts;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_deferred_lighting.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/vk_deferred_gbuffer.c;${CMAKE_SOURCE_DIR}/renderers/vulkan/shaders/glsl/deferred_lighting.comp")

add_test(NAME test_modern_vulkan_default
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_modern_vulkan_default.sh)
set_tests_properties(test_modern_vulkan_default PROPERTIES
	LABELS "scripts;renderer;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_modern_vulkan_default.sh;${CMAKE_SOURCE_DIR}/config/modern_vulkan.cfg;${CMAKE_SOURCE_DIR}/docs/RENDERERS.md")

add_test(NAME test_modern_renderer_profile_runtime
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_modern_renderer_profile_runtime.sh source)
set_tests_properties(test_modern_renderer_profile_runtime PROPERTIES
	LABELS "scripts;renderer;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_modern_renderer_profile_runtime.sh;${CMAKE_SOURCE_DIR}/config/modern_vulkan.cfg;${CMAKE_SOURCE_DIR}/docs/renderer_validation/README.md")

add_test(NAME test_idtech3_tv_streaming
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_idtech3_tv_streaming.sh source)
set_tests_properties(test_idtech3_tv_streaming PROPERTIES
	LABELS "scripts;validation;streaming"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_idtech3_tv_streaming.sh;${CMAKE_SOURCE_DIR}/tests/scripts/idtech3_client_runtime_smoke.sh;${CMAKE_SOURCE_DIR}/runtime/client/platform/cl_streaming.c;${CMAKE_SOURCE_DIR}/docs/IDTECH3_TV.md")

add_test(NAME renderer_tier_a_resolver
	COMMAND ${CMAKE_SOURCE_DIR}/scripts/resolve_renderer_tiers.sh --tier A --summary-file ${CMAKE_BINARY_DIR}/renderer_tier_a_summary.json)
set_tests_properties(renderer_tier_a_resolver PROPERTIES
	LABELS "scripts;renderer;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/scripts/resolve_renderer_tiers.sh;${CMAKE_SOURCE_DIR}/scripts/renderer_regression_check.sh;${CMAKE_SOURCE_DIR}/scripts/gpu_golden_capture.sh")

add_test(NAME test_conditional_stubs
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_conditional_stubs.sh)
set_tests_properties(test_conditional_stubs PROPERTIES
	LABELS "scripts;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_conditional_stubs.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/vk_experimental_renderer_stubs.c;${CMAKE_SOURCE_DIR}/renderers/common/tr_vector_font_stub.c")

add_test(NAME test_q3_stock_smoke
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_q3_stock_smoke.sh ${CMAKE_SOURCE_DIR}/release)
set_tests_properties(test_q3_stock_smoke PROPERTIES
	LABELS "scripts;qvm;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_q3_stock_smoke.sh;${CMAKE_SOURCE_DIR}/runtime/client/core/cl_cgame.c")

add_test(NAME test_platform_renderer_stubs
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_platform_renderer_stubs.sh)
set_tests_properties(test_platform_renderer_stubs PROPERTIES
	LABELS "scripts;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_platform_renderer_stubs.sh;${CMAKE_SOURCE_DIR}/renderers/common/tr_platform_renderer_stub.c;${CMAKE_SOURCE_DIR}/renderers/common/renderer_backend.h")

add_test(NAME test_vulkan_rtx
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_vulkan_rtx.sh)
set_tests_properties(test_vulkan_rtx PROPERTIES
	LABELS "scripts;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_vulkan_rtx.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/extensions/rtx/vk_rtx.c;${CMAKE_SOURCE_DIR}/renderers/vulkan/extensions/rtx/vk_rtx_world.c")

add_test(NAME test_vulkan_extensions_layout
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_vulkan_extensions_layout.sh)
set_tests_properties(test_vulkan_extensions_layout PROPERTIES
	LABELS "scripts;renderer;manifest"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_vulkan_extensions_layout.sh;${CMAKE_SOURCE_DIR}/cmake/renderers/VulkanExtensionSources.cmake")

add_test(NAME test_client_modular
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_client_modular.sh)
set_tests_properties(test_client_modular PROPERTIES
	LABELS "scripts;client"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_client_modular.sh;${IDTECH3_DIR_RUNTIME_CLIENT}/core/cl_main.c;${IDTECH3_DIR_RUNTIME_CLIENT}/core/cl_lifecycle.c;${IDTECH3_DIR_RUNTIME_CLIENT}/core/cl_frame.c;${IDTECH3_DIR_RUNTIME_CLIENT}/core/cl_cvars.c")

add_test(NAME test_client_domains
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_client_domains.sh)
set_tests_properties(test_client_domains PROPERTIES
	LABELS "scripts;client;manifest"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_client_domains.sh;${CMAKE_SOURCE_DIR}/cmake/client/ClientSources.cmake")

add_test(NAME test_game_ai_middleware
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_game_ai_middleware.sh)
set_tests_properties(test_game_ai_middleware PROPERTIES
	LABELS "scripts;client;manifest"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_game_ai_middleware.sh;${CMAKE_SOURCE_DIR}/cmake/modules/ClientGameAiSources.cmake")

add_test(NAME test_cpp20_sources
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_cpp20_sources.sh)
set_tests_properties(test_cpp20_sources PROPERTIES
	LABELS "scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_cpp20_sources.sh;${CMAKE_SOURCE_DIR}/CMakeLists.txt;${CMAKE_SOURCE_DIR}/modules/world/world_open.cpp;${IDTECH3_DIR_ENGINE_CORE}/cluster_graph.cpp")

add_test(NAME test_engine_module_manifest
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_engine_module_manifest.sh)
set_tests_properties(test_engine_module_manifest PROPERTIES
	LABELS "scripts;validation;manifest"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_engine_module_manifest.sh;${CMAKE_SOURCE_DIR}/docs/ENGINE_MODULE_MANIFEST.md")

add_test(NAME test_repository_layout_2026
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_repository_layout_2026.sh)
set_tests_properties(test_repository_layout_2026 PROPERTIES
	LABELS "scripts;validation;manifest"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_repository_layout_2026.sh;${CMAKE_SOURCE_DIR}/cmake/IdTech3Layout.cmake;${CMAKE_SOURCE_DIR}/scripts/layout_forwarding_symlinks.sh;${CMAKE_SOURCE_DIR}/engine/core/common.c")

add_test(NAME test_msvc_layout_bridge
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_msvc_layout_bridge.sh)
set_tests_properties(test_msvc_layout_bridge PROPERTIES
	LABELS "scripts;validation;manifest"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_msvc_layout_bridge.sh;${CMAKE_SOURCE_DIR}/engine/platform/win32/msvc2017/quake3e.vcxproj")

add_test(NAME test_msvc_manifest_drift
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_msvc_manifest_drift.sh)
set_tests_properties(test_msvc_manifest_drift PROPERTIES
	LABELS "scripts;validation;manifest"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_msvc_manifest_drift.sh;${CMAKE_SOURCE_DIR}/scripts/msvc/check_vcxproj_drift.py;${CMAKE_SOURCE_DIR}/cmake/export/MsvcSourceManifest.cmake")

add_test(NAME test_msvc_vcxproj_paths_resolve
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_msvc_vcxproj_paths_resolve.sh)
set_tests_properties(test_msvc_vcxproj_paths_resolve PROPERTIES
	LABELS "scripts;validation;manifest"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_msvc_vcxproj_paths_resolve.sh;${CMAKE_SOURCE_DIR}/engine/platform/win32/msvc2017/quake3e.vcxproj;${CMAKE_SOURCE_DIR}/engine/platform/win32/msvc2017/vulkan.vcxproj")

add_test(NAME test_msvc_solution
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_msvc_solution.sh)
set_tests_properties(test_msvc_solution PROPERTIES
	LABELS "scripts;validation;manifest"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_msvc_solution.sh;${CMAKE_SOURCE_DIR}/engine/platform/win32/msvc2017/quake3e.sln")

add_test(NAME test_src_shim_audit
	COMMAND ${CMAKE_SOURCE_DIR}/scripts/audit_src_shim_references.sh)
set_tests_properties(test_src_shim_audit PROPERTIES
	LABELS "scripts;validation;manifest"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/scripts/audit_src_shim_references.sh")

add_test(NAME test_legacy_intact
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_legacy_intact.sh)
set_tests_properties(test_legacy_intact PROPERTIES
	LABELS "scripts;validation;manifest"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_legacy_intact.sh;${CMAKE_SOURCE_DIR}/docs/core/LEGACY_AND_MODERN.md")

add_test(NAME test_trunk_policy
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_trunk_policy.sh)
set_tests_properties(test_trunk_policy PROPERTIES
	LABELS "scripts;validation;trunk"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_trunk_policy.sh;${CMAKE_SOURCE_DIR}/docs/BRANCHES.md;${CMAKE_SOURCE_DIR}/.github/dependabot.yml")

add_test(NAME test_dynamic_light_scale
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_dynamic_light_scale.sh)
set_tests_properties(test_dynamic_light_scale PROPERTIES
	LABELS "scripts;validation;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_dynamic_light_scale.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/tr_light.c")

add_test(NAME test_classic_lighting
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_classic_lighting.sh)
set_tests_properties(test_classic_lighting PROPERTIES
	LABELS "scripts;validation;renderer;compat"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_classic_lighting.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/tr_init.c")

add_test(NAME test_auto_graphics_profile
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_auto_graphics_profile.sh)
set_tests_properties(test_auto_graphics_profile PROPERTIES
	LABELS "scripts;validation;client;compat"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_auto_graphics_profile.sh;${CMAKE_SOURCE_DIR}/runtime/client/core/cl_cgame.c")

add_test(NAME test_stock_baseq3
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_stock_baseq3.sh)
set_tests_properties(test_stock_baseq3 PROPERTIES
	LABELS "scripts;validation;client;compat"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_stock_baseq3.sh;${CMAKE_SOURCE_DIR}/runtime/client/client.h;${CMAKE_SOURCE_DIR}/config/classic_baseq3.cfg")

add_test(NAME test_trap_getvalue_aliases
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_trap_getvalue_aliases.sh)
set_tests_properties(test_trap_getvalue_aliases PROPERTIES
	LABELS "scripts;validation;client;server;compat"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_trap_getvalue_aliases.sh;${CMAKE_SOURCE_DIR}/runtime/client/core/cl_cgame.c;${CMAKE_SOURCE_DIR}/runtime/server/sv_game.c")

add_test(NAME test_voip_lip_flap
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_voip_lip_flap.sh)
set_tests_properties(test_voip_lip_flap PROPERTIES
	LABELS "scripts;validation;client;voip"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_voip_lip_flap.sh;${CMAKE_SOURCE_DIR}/runtime/client/platform/cl_voip.c;${CMAKE_SOURCE_DIR}/docs/VOIP.md")

add_test(NAME test_facs_facial
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_facs_facial.sh)
set_tests_properties(test_facs_facial PROPERTIES
	LABELS "scripts;validation;client;facs"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_facs_facial.sh;${CMAKE_SOURCE_DIR}/runtime/game/g_facial.c;${CMAKE_SOURCE_DIR}/docs/FACS.md")

add_test(NAME test_freeusd_wiring
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_freeusd_wiring.sh)
set_tests_properties(test_freeusd_wiring PROPERTIES
	LABELS "scripts;validation;freeusd"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_freeusd_wiring.sh;${CMAKE_SOURCE_DIR}/cmake/FreeUSD.cmake;${CMAKE_SOURCE_DIR}/docs/FREEUSD.md")

add_test(NAME test_material_blend
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_material_blend.sh)
set_tests_properties(test_material_blend PROPERTIES
	LABELS "scripts;validation;renderer;pbr"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_material_blend.sh;${CMAKE_SOURCE_DIR}/docs/MATERIAL_BLEND.md;${CMAKE_SOURCE_DIR}/renderers/vulkan/shaders/glsl/gen_frag.tmpl")

add_test(NAME test_material_paint
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_material_paint.sh)
set_tests_properties(test_material_paint PROPERTIES
	LABELS "scripts;validation;renderer;pbr"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_material_paint.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/tr_material_paint.c;${CMAKE_SOURCE_DIR}/docs/MATERIAL_BLEND.md")

add_test(NAME test_studio_chrome
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_studio_chrome.sh)
set_tests_properties(test_studio_chrome PROPERTIES
	LABELS "scripts;validation;renderer;imgui"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_studio_chrome.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/inspector/vk_imgui_chrome_panels.cpp;${CMAKE_SOURCE_DIR}/docs/IN_ENGINE_STUDIO_TOOLS.md")

add_test(NAME test_cbt_terrain
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_cbt_terrain.sh)
set_tests_properties(test_cbt_terrain PROPERTIES
	LABELS "scripts;validation;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_cbt_terrain.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/vk_terrain.c;${CMAKE_SOURCE_DIR}/docs/CBT_TERRAIN.md")

add_test(NAME test_raster_ultra_1_14
	COMMAND ${CMAKE_SOURCE_DIR}/scripts/raster_ultra_1_14_check.sh)
set_tests_properties(test_raster_ultra_1_14 PROPERTIES
	LABELS "scripts;validation;renderer;raster_ultra"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/scripts/raster_ultra_1_14_check.sh;${CMAKE_SOURCE_DIR}/docs/RASTER_ULTRA_1.14.md;${CMAKE_SOURCE_DIR}/renderers/vulkan/vk_biome.c")

add_test(NAME test_raster_ultra_2_0
	COMMAND ${CMAKE_SOURCE_DIR}/scripts/raster_ultra_2_0_check.sh)
set_tests_properties(test_raster_ultra_2_0 PROPERTIES
	LABELS "scripts;validation;renderer;raster_ultra"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/scripts/raster_ultra_2_0_check.sh;${CMAKE_SOURCE_DIR}/docs/RASTER_ULTRA_2.0.md;${CMAKE_SOURCE_DIR}/config/modern_raster_ultra_2.cfg")

add_test(NAME test_raster_ultra_2_1
	COMMAND ${CMAKE_SOURCE_DIR}/scripts/raster_ultra_2_1_check.sh)
set_tests_properties(test_raster_ultra_2_1 PROPERTIES
	LABELS "scripts;validation;renderer;raster_ultra"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/scripts/raster_ultra_2_1_check.sh;${CMAKE_SOURCE_DIR}/docs/RASTER_ULTRA_2.1.md;${CMAKE_SOURCE_DIR}/config/modern_raster_cinematic.cfg")

add_test(NAME test_cinematic_engine_1_0
	COMMAND ${CMAKE_SOURCE_DIR}/scripts/cinematic_engine_1_0_check.sh)
set_tests_properties(test_cinematic_engine_1_0 PROPERTIES
	LABELS "scripts;validation;renderer;cinematic"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/scripts/cinematic_engine_1_0_check.sh;${CMAKE_SOURCE_DIR}/docs/CINEMATIC_ENGINE_PLATFORM_1.0.md;${CMAKE_SOURCE_DIR}/config/modern_cinematic_raster.cfg")

add_test(NAME test_oit_corruption
	COMMAND ${CMAKE_SOURCE_DIR}/scripts/oit_corruption_check.sh)
set_tests_properties(test_oit_corruption PROPERTIES
	LABELS "scripts;validation;renderer;oit"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/scripts/oit_corruption_check.sh;${CMAKE_SOURCE_DIR}/config/repro_oit_corruption.cfg;${CMAKE_SOURCE_DIR}/docs/MOMENT_OIT_STOCHASTIC_ALPHA.md")

add_test(NAME test_temporal_ghost
	COMMAND ${CMAKE_SOURCE_DIR}/scripts/temporal_ghost_check.sh)
set_tests_properties(test_temporal_ghost PROPERTIES
	LABELS "scripts;validation;renderer;temporal"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/scripts/temporal_ghost_check.sh;${CMAKE_SOURCE_DIR}/docs/RENDERER_TEMPORAL_GHOSTING.md")

add_test(NAME test_surf_temporal_startup
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_surf_temporal_startup.sh)
set_tests_properties(test_surf_temporal_startup PROPERTIES
	LABELS "scripts;validation;renderer;temporal;surf"
	TIMEOUT 60
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_surf_temporal_startup.sh;${CMAKE_SOURCE_DIR}/config/surf.cfg;${CMAKE_SOURCE_DIR}/config/surf_autoexec.cfg")

add_test(NAME test_surf_viewmodel_projection
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_surf_viewmodel_projection.sh)
set_tests_properties(test_surf_viewmodel_projection PROPERTIES
	LABELS "scripts;validation;renderer;temporal;surf"
	TIMEOUT 60
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_surf_viewmodel_projection.sh;${CMAKE_SOURCE_DIR}/config/surf.cfg")

add_test(NAME test_temporal_weapon_validation
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_temporal_weapon_validation.sh)
set_tests_properties(test_temporal_weapon_validation PROPERTIES
	LABELS "scripts;validation;renderer;temporal;surf"
	TIMEOUT 60
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_temporal_weapon_validation.sh;${CMAKE_SOURCE_DIR}/tests/data/temporal_weapon_validation.json")

add_test(NAME test_weapon_bloom_policy
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_weapon_bloom_policy.sh)
set_tests_properties(test_weapon_bloom_policy PROPERTIES
	LABELS "scripts;validation;renderer;temporal;surf"
	TIMEOUT 90
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_weapon_bloom_policy.sh;${CMAKE_SOURCE_DIR}/config/surf.cfg;${CMAKE_SOURCE_DIR}/renderers/vulkan/vk_frame_submit.c")

add_test(NAME test_temporal_lifetime_faults
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_temporal_lifetime_faults.sh)
set_tests_properties(test_temporal_lifetime_faults PROPERTIES
	LABELS "scripts;validation;renderer;temporal;surf"
	TIMEOUT 120
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_temporal_lifetime_faults.sh;${CMAKE_SOURCE_DIR}/tests/data/temporal_weapon_validation.json;${CMAKE_SOURCE_DIR}/renderers/vulkan/vk_temporal.c")

add_test(NAME test_temporal_reset_policy
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_temporal_reset_policy.sh)
set_tests_properties(test_temporal_reset_policy PROPERTIES
	LABELS "scripts;validation;renderer;temporal"
	TIMEOUT 60
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_temporal_reset_policy.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/vk_temporal.h")

add_test(NAME test_high_throughput_1_0
	COMMAND ${CMAKE_SOURCE_DIR}/scripts/high_throughput_1_0_check.sh)
set_tests_properties(test_high_throughput_1_0 PROPERTIES
	LABELS "scripts;validation;renderer;high_throughput"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/scripts/high_throughput_1_0_check.sh;${CMAKE_SOURCE_DIR}/docs/HIGH_THROUGHPUT_RASTER_1.0.md;${CMAKE_SOURCE_DIR}/config/modern_high_throughput.cfg")

add_test(NAME test_high_throughput_1_1
	COMMAND ${CMAKE_SOURCE_DIR}/scripts/high_throughput_1_1_check.sh)
set_tests_properties(test_high_throughput_1_1 PROPERTIES
	LABELS "scripts;validation;renderer;high_throughput"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/scripts/high_throughput_1_1_check.sh;${CMAKE_SOURCE_DIR}/docs/HIGH_THROUGHPUT_RASTER_1.1.md;${CMAKE_SOURCE_DIR}/config/modern_high_throughput_animation.cfg")

add_test(NAME test_md3_paint
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_md3_paint.sh)
set_tests_properties(test_md3_paint PROPERTIES
	LABELS "scripts;validation;renderer;pbr"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_md3_paint.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/tr_material_paint.c;${CMAKE_SOURCE_DIR}/docs/MATERIAL_BLEND.md")

add_test(NAME test_steam_wiring
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_steam_wiring.sh)
set_tests_properties(test_steam_wiring PROPERTIES
	LABELS "scripts;validation;steam"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_steam_wiring.sh;${CMAKE_SOURCE_DIR}/runtime/client/platform/cl_steam.c;${CMAKE_SOURCE_DIR}/docs/STEAM.md")

add_test(NAME test_p2p_networking
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_p2p_networking.sh)
set_tests_properties(test_p2p_networking PROPERTIES
	LABELS "scripts;validation;networking"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_p2p_networking.sh;${CMAKE_SOURCE_DIR}/runtime/client/core/cl_cmds.c;${CMAKE_SOURCE_DIR}/engine/core/net_p2p.c;${CMAKE_SOURCE_DIR}/docs/P2P_NETWORKING.md")

add_test(NAME test_p2p_nat_sim
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_p2p_nat_sim.sh)
set_tests_properties(test_p2p_nat_sim PROPERTIES
	LABELS "scripts;p2p;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_p2p_nat_sim.sh;${CMAKE_SOURCE_DIR}/engine/core/net_p2p_stun_codec.c;${CMAKE_SOURCE_DIR}/tests/unit/test_p2p_stun.c")

add_test(NAME test_p2p_nat_live
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_p2p_nat_live.sh host)
set_tests_properties(test_p2p_nat_live PROPERTIES
	LABELS "p2p;live"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_p2p_nat_live.sh;${CMAKE_SOURCE_DIR}/docs/P2P_NAT_TESTING.md")

add_test(NAME test_p2p_reconnect
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_p2p_reconnect.sh)
set_tests_properties(test_p2p_reconnect PROPERTIES
	LABELS "p2p;scripts"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_p2p_reconnect.sh;${CMAKE_SOURCE_DIR}/tests/scripts/idtech3_minimal_content_smoke.sh;${CMAKE_SOURCE_DIR}/runtime/client/core/cl_p2p_session.c;${CMAKE_SOURCE_DIR}/runtime/server/sv_client.c")

add_test(NAME test_p2p_migrate
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_p2p_migrate.sh)
set_tests_properties(test_p2p_migrate PROPERTIES
	LABELS "p2p;scripts"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_p2p_migrate.sh;${CMAKE_SOURCE_DIR}/runtime/client/core/cl_p2p_session.c;${CMAKE_SOURCE_DIR}/docs/P2P_NETWORKING.md")

add_test(NAME test_p2p_ice_guard
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_p2p_ice_guard.sh)
set_tests_properties(test_p2p_ice_guard PROPERTIES
	LABELS "p2p;scripts"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_p2p_ice_guard.sh;${CMAKE_SOURCE_DIR}/engine/core/net_p2p_ice.c;${CMAKE_SOURCE_DIR}/runtime/server/sv_ccmds.c")

add_test(NAME test_oscar_bridge
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_oscar_bridge.sh all)
set_tests_properties(test_oscar_bridge PROPERTIES
	LABELS "scripts;networking;oscar"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_oscar_bridge.sh;${CMAKE_SOURCE_DIR}/engine/core/net_oscar.c;${CMAKE_SOURCE_DIR}/docs/OSCAR_INTEGRATION.md")

add_test(NAME test_console_font
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_console_font.sh)
set_tests_properties(test_console_font PROPERTIES
	LABELS "scripts;validation;client;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_console_font.sh;${CMAKE_SOURCE_DIR}/runtime/client/shell/cl_scrn.c")

add_test(NAME test_build_profiles
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_build_profiles.sh)
set_tests_properties(test_build_profiles PROPERTIES
	LABELS "scripts;validation;manifest"
  REQUIRED_FILES "${CMAKE_SOURCE_DIR}/cmake/IdTech3Extension.cmake;${CMAKE_SOURCE_DIR}/cmake/IdTech3Profile.cmake;${CMAKE_SOURCE_DIR}/cmake/IdTech3Layout.cmake")

add_test(NAME test_warning_free_ci
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_warning_free_ci.sh)
set_tests_properties(test_warning_free_ci PROPERTIES
	LABELS "scripts;validation;ci"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_warning_free_ci.sh;${CMAKE_SOURCE_DIR}/scripts/check_first_party_warnings.sh;${CMAKE_SOURCE_DIR}/.github/workflows/build.yml")

add_test(NAME test_tech_debt_audit
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_tech_debt_audit.sh)
set_tests_properties(test_tech_debt_audit PROPERTIES
	LABELS "scripts;validation;debt"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_tech_debt_audit.sh;${CMAKE_SOURCE_DIR}/docs/core/SHIM_REMOVAL_CHECKLIST.md;${CMAKE_SOURCE_DIR}/renderers/vulkan/shaders/spirv/shader_data.c")

add_test(NAME test_no_aux_core
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_no_aux_core.sh)
set_tests_properties(test_no_aux_core PROPERTIES
	LABELS "scripts;validation;manifest"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_no_aux_core.sh;${CMAKE_SOURCE_DIR}/cmake/EngineQcommonSources.cmake;${CMAKE_SOURCE_DIR}/cmake/server/ServerSources.cmake;${CMAKE_SOURCE_DIR}/cmake/renderers/VulkanCoreSources.cmake;${CMAKE_SOURCE_DIR}/cmake/modules/BotlibSources.cmake;${CMAKE_SOURCE_DIR}/cmake/modules/AudioMp3Sources.cmake")

add_test(NAME test_vector_font_mode2
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_vector_font_mode2.sh ${CMAKE_SOURCE_DIR})
set_tests_properties(test_vector_font_mode2 PROPERTIES
	LABELS "scripts;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_vector_font_mode2.sh;${CMAKE_SOURCE_DIR}/renderers/vulkan/vk_vector_font.c;${CMAKE_SOURCE_DIR}/renderers/common/tr_vector_font_glyphlet.c")

add_test(NAME test_openworld_runtime
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_openworld_runtime.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_openworld_runtime PROPERTIES
	LABELS "scripts;validation;openworld;sector_stream"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_openworld_runtime.sh;${CMAKE_SOURCE_DIR}/scripts/tools/gen_hub_bsp.py;${CMAKE_SOURCE_DIR}/scripts/tools/gen_sector_bsp.py;${IDTECH3_DIR_ENGINE_CORE}/com_openworld_smoke.c")

add_test(NAME test_sector_stream_fidelity
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_sector_stream_fidelity.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_sector_stream_fidelity PROPERTIES
	LABELS "scripts;validation;openworld;sector_stream"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_sector_stream_fidelity.sh;${CMAKE_SOURCE_DIR}/tests/data/openworld/openworld_smoke_fidelity.cfg;${IDTECH3_DIR_ENGINE_CORE}/com_openworld_smoke.c;${CMAKE_SOURCE_DIR}/modules/navigation/nav_recast.cpp")

add_test(NAME sector_stream_matrix
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_sector_stream_matrix.sh
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
set_tests_properties(sector_stream_matrix PROPERTIES
	LABELS "validation;openworld"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_sector_stream_matrix.sh;${CMAKE_SOURCE_DIR}/CMakeLists.txt"
	RESOURCE_LOCK "idtech3_headless_validation")

add_custom_target(test-sector-stream
	COMMAND ${CMAKE_COMMAND} -E env
		RELEASE_DIR=${CMAKE_SOURCE_DIR}/release
		BUILD_DIR=${CMAKE_BINARY_DIR}
		${CMAKE_SOURCE_DIR}/tests/scripts/test_sector_stream_matrix.sh
	WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
	COMMENT "Run sector stream path matrix (ctest wiring + stream tests)"
	USES_TERMINAL)

add_test(NAME test_openworld_residency
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_openworld_residency.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_openworld_residency PROPERTIES
	LABELS "scripts;validation;openworld;sector_stream"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_openworld_residency.sh;${CMAKE_SOURCE_DIR}/modules/world/world_residency.cpp;${CMAKE_SOURCE_DIR}/modules/world/world_residency.h")

add_test(NAME test_graph_compute
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_graph_compute.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_graph_compute PROPERTIES
	LABELS "scripts;validation;openworld;sector_stream"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_graph_compute.sh;${CMAKE_SOURCE_DIR}/modules/world/sector_graph.cpp;${IDTECH3_DIR_ENGINE_CORE}/cluster_graph.cpp;${CMAKE_SOURCE_DIR}/renderers/vulkan/vk_graph_bfs.c")

add_test(NAME test_fog_biology
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_fog_biology.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_fog_biology PROPERTIES
	LABELS "scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_fog_biology.sh;${CMAKE_SOURCE_DIR}/modules/world/fog_biology.cpp;${CMAKE_SOURCE_DIR}/modules/world/fog_biology.h")

add_test(NAME test_arc_blanc
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_arc_blanc.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_arc_blanc PROPERTIES
	LABELS "scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_arc_blanc.sh;${CMAKE_SOURCE_DIR}/modules/world/arc_blanc/arc_blanc.c;${CMAKE_SOURCE_DIR}/docs/ARC_BLANC.md")

add_test(NAME test_init_optional_submodules
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_init_optional_submodules.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_init_optional_submodules PROPERTIES
	LABELS "scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_init_optional_submodules.sh;${CMAKE_SOURCE_DIR}/scripts/init_optional_submodules.sh")

add_test(NAME test_csharp_scripting
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_csharp_scripting.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_csharp_scripting PROPERTIES
	LABELS "scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_csharp_scripting.sh")

add_test(NAME test_compile_engine_lto
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_compile_engine_lto.sh ${CMAKE_SOURCE_DIR}/scripts/compile_engine.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_compile_engine_lto PROPERTIES
	LABELS "unit;scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_compile_engine_lto.sh")

add_test(NAME test_vk_vegetation_dispatch_order
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_vk_vegetation_dispatch_order.sh
		${CMAKE_SOURCE_DIR}/renderers/vulkan/tr_shade.c
		${CMAKE_SOURCE_DIR}/renderers/vulkan/vk_frame_submit.c
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_vk_vegetation_dispatch_order PROPERTIES
	LABELS "unit;scripts;validation")

add_test(NAME test_vulkan_mesh_shader_opt_in
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_vulkan_mesh_shader_opt_in.sh ${CMAKE_SOURCE_DIR}
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_vulkan_mesh_shader_opt_in PROPERTIES
	LABELS "unit;scripts;validation")

add_test(NAME test_vulkan_runtime_regressions
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_vulkan_runtime_regressions.sh ${CMAKE_SOURCE_DIR}
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_vulkan_runtime_regressions PROPERTIES
	LABELS "unit;scripts;validation;renderer")

add_test(NAME test_botlib_chat_message_bounds
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_botlib_chat_message_bounds.sh
		${CMAKE_SOURCE_DIR}/modules/botlib/be_ai_chat.c
		${CMAKE_SOURCE_DIR}/engine/platform/win32/botlib/be_ai_chat.c
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_botlib_chat_message_bounds PROPERTIES
	LABELS "unit;scripts;validation")

add_test(NAME test_vulkan_renderer_guards
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_vulkan_renderer_guards.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_vulkan_renderer_guards PROPERTIES
	LABELS "unit;scripts;validation")

add_test(NAME test_vulkan_regression_source_guards
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_vulkan_regression_source_guards.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_vulkan_regression_source_guards PROPERTIES
	LABELS "unit;scripts;validation;renderer")

add_test(NAME test_botlib_bounded_strings
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_botlib_bounded_strings.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_botlib_bounded_strings PROPERTIES
	LABELS "unit;scripts;validation")

add_test(NAME test_pk3_native_library_cache
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_pk3_native_library_cache.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_pk3_native_library_cache PROPERTIES
	LABELS "unit;scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_pk3_native_library_cache.sh")
add_test(NAME test_libtorrent_content
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_libtorrent_content.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_libtorrent_content PROPERTIES
	LABELS "unit;scripts;validation;content"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_libtorrent_content.sh")
# --- Unit tests ---
option(BUILD_UNIT_TESTS "Build unit tests" ON)
if(BUILD_UNIT_TESTS)
	# Phase 5e: former src/* shims provided navigation/, server/, qcommon/ under one include root.
	# Prefer modules/ + runtime/ (+ repo-root qcommon bridge) so #include "navigation/..." still works.
	set(IDTECH3_UNIT_INCLUDE_DIRS
		${CMAKE_SOURCE_DIR}
		${CMAKE_SOURCE_DIR}/modules
		${CMAKE_SOURCE_DIR}/runtime
		${CMAKE_SOURCE_DIR}/extensions/research
		${IDTECH3_DIR_ENGINE_CORE}
	)
	add_executable(unit_macros tests/unit/test_macros.c)
	target_include_directories(unit_macros PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	add_test(NAME unit_macros COMMAND unit_macros)
	set_tests_properties(unit_macros PROPERTIES LABELS "unit;validation")

	add_executable(unit_qvm_abi tests/unit/test_qvm_abi.c)
	target_include_directories(unit_qvm_abi PRIVATE
		${IDTECH3_UNIT_INCLUDE_DIRS}
		${CMAKE_SOURCE_DIR}/modules/game
		${CMAKE_SOURCE_DIR}/renderers/common)
	add_test(NAME unit_qvm_abi COMMAND unit_qvm_abi)
	set_tests_properties(unit_qvm_abi PROPERTIES LABELS "unit;validation;qvm")

	add_executable(unit_qmath tests/unit/test_q_math.c engine/core/q_math.c)
	target_include_directories(unit_qmath PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_qmath PRIVATE m)
	endif()
	add_test(NAME unit_qmath COMMAND unit_qmath)
	set_tests_properties(unit_qmath PROPERTIES LABELS "unit;validation")

	add_executable(unit_surfaceflags tests/unit/test_surfaceflags.c)
	target_include_directories(unit_surfaceflags PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	add_test(NAME unit_surfaceflags COMMAND unit_surfaceflags)
	set_tests_properties(unit_surfaceflags PROPERTIES LABELS "unit;validation")

	add_executable(unit_qhelpers
		tests/unit/test_q_helpers.c
		tests/stub_qcommon_min.c
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_include_directories(unit_qhelpers PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_qhelpers PRIVATE m)
	endif()
	add_test(NAME unit_qhelpers COMMAND unit_qhelpers)
	set_tests_properties(unit_qhelpers PROPERTIES LABELS "unit;validation")

	add_executable(unit_crc
		tests/unit/test_crc.c
		tests/stub_qcommon_min.c
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_include_directories(unit_crc PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_crc PRIVATE m)
	endif()
	add_test(NAME unit_crc COMMAND unit_crc)
	set_tests_properties(unit_crc PROPERTIES LABELS "unit;validation")

	add_executable(unit_pathutil
		tests/unit/test_pathutil.c
		tests/stub_qcommon_min.c
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_include_directories(unit_pathutil PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_pathutil PRIVATE m)
	endif()
	add_test(NAME unit_pathutil COMMAND unit_pathutil)
	set_tests_properties(unit_pathutil PROPERTIES LABELS "unit;validation")

	add_executable(unit_torrent_manifest
		tests/unit/test_torrent_manifest.cpp
		engine/content/torrent_manifest.cpp)
	target_include_directories(unit_torrent_manifest PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	add_test(NAME unit_torrent_manifest COMMAND unit_torrent_manifest)
	set_tests_properties(unit_torrent_manifest PROPERTIES LABELS "unit;validation;content")

	add_executable(unit_msg
		tests/unit/test_msg.c
		tests/stub_qcommon_min.c
		tests/stub_msg_cvar.c
		engine/core/msg.c
		engine/core/huffman_static.cpp)
	target_include_directories(unit_msg PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	target_compile_definitions(unit_msg PRIVATE DEDICATED)
	if(NOT MSVC)
		target_link_libraries(unit_msg PRIVATE m)
	endif()
	add_test(NAME unit_msg COMMAND unit_msg)
	set_tests_properties(unit_msg PROPERTIES LABELS "unit;validation")

	add_executable(unit_huffman_adaptive
		tests/unit/test_huffman_adaptive.c
		engine/core/huffman.c)
	target_include_directories(unit_huffman_adaptive PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	add_test(NAME unit_huffman_adaptive COMMAND unit_huffman_adaptive)
	set_tests_properties(unit_huffman_adaptive PROPERTIES LABELS "unit;validation;network")

	add_executable(unit_info
		tests/unit/test_info.c
		tests/stub_qcommon_min.c
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_include_directories(unit_info PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_info PRIVATE m)
	endif()
	add_test(NAME unit_info COMMAND unit_info)
	set_tests_properties(unit_info PROPERTIES LABELS "unit;validation")

	add_executable(unit_cm_bounds
		tests/unit/test_cm_bounds.c
		engine/core/cm_bounds.c
		engine/core/q_math.c)
	target_include_directories(unit_cm_bounds PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_cm_bounds PRIVATE m)
	endif()
	add_test(NAME unit_cm_bounds COMMAND unit_cm_bounds)
	set_tests_properties(unit_cm_bounds PROPERTIES LABELS "unit;validation")

	add_executable(unit_cl_compat_math
		tests/unit/test_cl_compat_math.c
		runtime/client/core/cl_compat_math.c)
	target_include_directories(unit_cl_compat_math PRIVATE
		${IDTECH3_UNIT_INCLUDE_DIRS}
		${CMAKE_SOURCE_DIR}/runtime/client/core)
	if(NOT MSVC)
		target_link_libraries(unit_cl_compat_math PRIVATE m)
	endif()
	add_test(NAME unit_cl_compat_math COMMAND unit_cl_compat_math)
	set_tests_properties(unit_cl_compat_math PROPERTIES LABELS "unit;validation;client")

	add_executable(unit_p2p_stun
		tests/unit/test_p2p_stun.c
		tests/stub_qcommon_min.c
		engine/core/net_p2p_stun_codec.c
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_include_directories(unit_p2p_stun PRIVATE
		${CMAKE_SOURCE_DIR}
		${CMAKE_SOURCE_DIR}/engine/core
		${CMAKE_SOURCE_DIR}/extensions/research)
	target_compile_definitions(unit_p2p_stun PRIVATE NET_P2P_STUN_CODEC_STANDALONE=1)
	if(NOT MSVC)
		target_link_libraries(unit_p2p_stun PRIVATE m)
	endif()
	add_test(NAME unit_p2p_stun COMMAND unit_p2p_stun)
	set_tests_properties(unit_p2p_stun PROPERTIES LABELS "unit;p2p;validation")

	add_executable(unit_oscar_protocol
		tests/unit/test_oscar_protocol.c
		tests/stub_qcommon_min.c
		engine/core/net_oscar_protocol.c
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_include_directories(unit_oscar_protocol PRIVATE
		${CMAKE_SOURCE_DIR}
		${CMAKE_SOURCE_DIR}/engine/core)
	if(NOT MSVC)
		target_link_libraries(unit_oscar_protocol PRIVATE m)
	endif()
	add_test(NAME unit_oscar_protocol COMMAND unit_oscar_protocol)
	set_tests_properties(unit_oscar_protocol PROPERTIES LABELS "unit;networking;oscar;validation")

	add_executable(unit_oscar_raw
		tests/unit/test_oscar_raw.c
		tests/stub_qcommon_min.c
		engine/core/net_oscar_raw.c
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_include_directories(unit_oscar_raw PRIVATE
		${CMAKE_SOURCE_DIR}
		${CMAKE_SOURCE_DIR}/engine/core)
	if(NOT MSVC)
		target_link_libraries(unit_oscar_raw PRIVATE m)
	endif()
	add_test(NAME unit_oscar_raw COMMAND unit_oscar_raw)
	set_tests_properties(unit_oscar_raw PROPERTIES LABELS "unit;networking;oscar;validation")

	add_executable(unit_oscar_roster
		tests/unit/test_oscar_roster.c
		tests/stub_qcommon_min.c
		engine/core/net_oscar_roster.c
		engine/core/net_oscar_raw.c
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_include_directories(unit_oscar_roster PRIVATE
		${CMAKE_SOURCE_DIR}
		${CMAKE_SOURCE_DIR}/engine/core)
	if(NOT MSVC)
		target_link_libraries(unit_oscar_roster PRIVATE m)
	endif()
	add_test(NAME unit_oscar_roster COMMAND unit_oscar_roster)
	set_tests_properties(unit_oscar_roster PROPERTIES LABELS "unit;networking;oscar;validation")

	if(USE_SDL AND TARGET SDL3::SDL3)
		add_executable(unit_sdl3_features tests/unit/test_sdl3_features.c)
		target_link_libraries(unit_sdl3_features PRIVATE SDL3::SDL3)
		add_test(NAME unit_sdl3_features COMMAND unit_sdl3_features)
		set_tests_properties(unit_sdl3_features PROPERTIES LABELS "unit;validation;client;sdl")
	endif()

	add_executable(unit_parse
		tests/unit/test_parse.c
		tests/stub_qcommon_min.c
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_include_directories(unit_parse PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_parse PRIVATE m)
	endif()
	add_test(NAME unit_parse COMMAND unit_parse)
	set_tests_properties(unit_parse PROPERTIES LABELS "unit;validation")

	add_executable(unit_engine_sprite_map
		tests/unit/test_engine_sprite_map.c
		tests/stub_qcommon_min.c
		engine/core/q_shared.c
		engine/core/q_math.c
		engine/core/engine_sprite_map.c)
	target_include_directories(unit_engine_sprite_map PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_engine_sprite_map PRIVATE m)
	endif()
	add_test(NAME unit_engine_sprite_map COMMAND unit_engine_sprite_map)
	set_tests_properties(unit_engine_sprite_map PROPERTIES LABELS "unit;validation")

	add_executable(unit_vox_parse
		tests/unit/test_vox_parse.c
		tests/stub_qcommon_min.c
		renderers/common/tr_vox_parse.c
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_include_directories(unit_vox_parse PRIVATE
		${IDTECH3_UNIT_INCLUDE_DIRS}
		${CMAKE_SOURCE_DIR}/renderers/common)
	if(NOT MSVC)
		target_link_libraries(unit_vox_parse PRIVATE m)
	endif()
	add_test(NAME unit_vox_parse COMMAND unit_vox_parse)
	set_tests_properties(unit_vox_parse PROPERTIES LABELS "unit;validation")

	add_executable(unit_babble_parse
		tests/unit/test_babble_parse.c
		tests/stub_qcommon_min.c
		modules/dialogue/babble_parse.c
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_include_directories(unit_babble_parse PRIVATE
		${IDTECH3_UNIT_INCLUDE_DIRS}
		${CMAKE_SOURCE_DIR})
	if(NOT MSVC)
		target_link_libraries(unit_babble_parse PRIVATE m)
	endif()
	add_test(NAME unit_babble_parse COMMAND unit_babble_parse)
	set_tests_properties(unit_babble_parse PROPERTIES LABELS "unit;validation;ubertools")

	add_executable(unit_tiki_parse
		tests/unit/test_tiki_parse.c
		tests/stub_qcommon_min.c
		renderers/common/tr_model_tiki.c
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_include_directories(unit_tiki_parse PRIVATE
		${IDTECH3_UNIT_INCLUDE_DIRS}
		${CMAKE_SOURCE_DIR}/renderers/common)
	if(NOT MSVC)
		target_link_libraries(unit_tiki_parse PRIVATE m)
	endif()
	add_test(NAME unit_tiki_parse COMMAND unit_tiki_parse)
	set_tests_properties(unit_tiki_parse PROPERTIES LABELS "unit;validation;ubertools")

	add_executable(unit_oit_alpha_normalize tests/unit/test_oit_alpha_normalize.c)
	if(NOT MSVC)
		target_link_libraries(unit_oit_alpha_normalize PRIVATE m)
	endif()
	add_test(NAME unit_oit_alpha_normalize COMMAND unit_oit_alpha_normalize)
	set_tests_properties(unit_oit_alpha_normalize PROPERTIES LABELS "unit;validation;oit")

	add_executable(unit_depth_view tests/unit/test_depth_view.c)
	if(NOT MSVC)
		target_link_libraries(unit_depth_view PRIVATE m)
	endif()
	add_test(NAME unit_depth_view COMMAND unit_depth_view)
	set_tests_properties(unit_depth_view PROPERTIES LABELS "unit;validation;depth")

	add_executable(unit_bsp30_triangulate
		tests/unit/test_bsp30_triangulate.c
		renderers/common/tr_bsp30_triangulate.c)
	target_include_directories(unit_bsp30_triangulate PRIVATE
		${CMAKE_SOURCE_DIR}/renderers/common)
	if(NOT MSVC)
		target_link_libraries(unit_bsp30_triangulate PRIVATE m)
	endif()
	add_test(NAME unit_bsp30_triangulate COMMAND unit_bsp30_triangulate)
	set_tests_properties(unit_bsp30_triangulate PROPERTIES LABELS "unit;validation;bsp30")

	add_executable(unit_gpu_frustum
		tests/unit/test_gpu_frustum.c)
	target_include_directories(unit_gpu_frustum PRIVATE
		${CMAKE_SOURCE_DIR}/renderers/vulkan)
	if(NOT MSVC)
		target_link_libraries(unit_gpu_frustum PRIVATE m)
	endif()
	add_test(NAME unit_gpu_frustum COMMAND unit_gpu_frustum)
	set_tests_properties(unit_gpu_frustum PROPERTIES LABELS "unit;validation;gpu_visibility")


	add_executable(unit_cluster_math
		tests/unit/test_cluster_math.c
		renderers/vulkan/vk_cluster_math.cpp)
	target_include_directories(unit_cluster_math PRIVATE
		${CMAKE_SOURCE_DIR}/renderers/vulkan)
	if(NOT MSVC)
		target_link_libraries(unit_cluster_math PRIVATE m)
	endif()
	add_test(NAME unit_cluster_math COMMAND unit_cluster_math)
	set_tests_properties(unit_cluster_math PROPERTIES LABELS "unit;validation;cluster")

	add_executable(unit_engine_decal_map
		tests/unit/test_engine_decal_map.c
		tests/stub_qcommon_min.c
		engine/core/q_shared.c
		engine/core/q_math.c
		engine/core/engine_decal_map.c)
	target_include_directories(unit_engine_decal_map PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(UNIX)
		target_link_libraries(unit_engine_decal_map PRIVATE m)
	endif()
	add_test(NAME unit_engine_decal_map COMMAND unit_engine_decal_map)
	set_tests_properties(unit_engine_decal_map PROPERTIES LABELS "unit;validation")

	add_executable(unit_aiwc
		tests/unit/test_aiwc_metrics.c
		engine/core/aiwc_metrics.c
		engine/core/aiwc_matmul.c)
	target_include_directories(unit_aiwc PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_aiwc PRIVATE m)
	endif()
	add_test(NAME unit_aiwc COMMAND unit_aiwc)
	set_tests_properties(unit_aiwc PROPERTIES LABELS "unit;validation")

	add_executable(unit_ttp
		tests/unit/test_ttp_model.c
		engine/core/ttp_model.c
		engine/core/ttp_sim.c)
	target_include_directories(unit_ttp PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_ttp PRIVATE m)
	endif()
	add_test(NAME unit_ttp COMMAND unit_ttp)
	set_tests_properties(unit_ttp PROPERTIES LABELS "unit;validation")

	add_executable(unit_bubblesh
		tests/unit/test_bubblesh_model.c
		engine/core/bubblesh_model.c)
	target_include_directories(unit_bubblesh PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_bubblesh PRIVATE m)
	endif()
	add_test(NAME unit_bubblesh COMMAND unit_bubblesh)
	set_tests_properties(unit_bubblesh PROPERTIES LABELS "unit;validation")

	add_executable(unit_vuda_model
		tests/unit/test_vuda_model.c
		extensions/research/vuda/vuda_model.c)
	target_include_directories(unit_vuda_model PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_vuda_model PRIVATE m)
	endif()
	add_test(NAME unit_vuda_model COMMAND unit_vuda_model)
	set_tests_properties(unit_vuda_model PROPERTIES LABELS "unit;validation")

	add_executable(unit_vksplat_model
		tests/unit/test_vksplat_model.c
		extensions/research/vksplat/vksplat_model.c)
	target_include_directories(unit_vksplat_model PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_vksplat_model PRIVATE m)
	endif()
	add_test(NAME unit_vksplat_model COMMAND unit_vksplat_model)
	set_tests_properties(unit_vksplat_model PROPERTIES LABELS "unit;validation")

	add_executable(unit_curast_model
		tests/unit/test_curast_model.c
		extensions/research/curast/curast_model.c)
	target_include_directories(unit_curast_model PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_curast_model PRIVATE m)
	endif()
	add_test(NAME unit_curast_model COMMAND unit_curast_model)
	set_tests_properties(unit_curast_model PROPERTIES LABELS "unit;validation")

	add_executable(unit_infernux_model
		tests/unit/test_infernux_model.c
		extensions/research/infernux/infernux_model.c)
	target_include_directories(unit_infernux_model PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_infernux_model PRIVATE m)
	endif()
	add_test(NAME unit_infernux_model COMMAND unit_infernux_model)
	set_tests_properties(unit_infernux_model PROPERTIES LABELS "unit;validation")

	add_executable(unit_app_crdt
		tests/unit/test_app_crdt.c
		tests/unit/app_crdt_linkstub.c
		engine/core/app_crdt.c
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_include_directories(unit_app_crdt PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	target_compile_definitions(unit_app_crdt PRIVATE APP_CRDT_UNIT_TEST)
	if(NOT MSVC)
		target_link_libraries(unit_app_crdt PRIVATE m)
	endif()
	add_test(NAME unit_app_crdt COMMAND unit_app_crdt)
	set_tests_properties(unit_app_crdt PROPERTIES LABELS "unit;validation")

	add_executable(unit_mimir_model
		tests/unit/test_mimir_model.c
		extensions/research/mimir/mimir_model.c)
	target_include_directories(unit_mimir_model PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_mimir_model PRIVATE m)
	endif()
	add_test(NAME unit_mimir_model COMMAND unit_mimir_model)
	set_tests_properties(unit_mimir_model PROPERTIES LABELS "unit;validation")

	add_executable(unit_iris_model
		tests/unit/test_iris_model.c
		extensions/research/iris/iris_model.c)
	target_include_directories(unit_iris_model PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_iris_model PRIVATE m)
	endif()
	add_test(NAME unit_iris_model COMMAND unit_iris_model)
	set_tests_properties(unit_iris_model PROPERTIES LABELS "unit;validation")

	add_executable(unit_radiusfps
		tests/unit/test_radiusfps.c
		extensions/research/radiusfps/radiusfps_cpu.c)
	target_include_directories(unit_radiusfps PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_radiusfps PRIVATE m)
	endif()
	if(USE_RADIUSFPS_CUDA)
		target_sources(unit_radiusfps PRIVATE extensions/research/radiusfps/radiusfps_cuda.cu)
		set_property(TARGET unit_radiusfps PROPERTY CUDA_SEPARABLE_COMPILATION ON)
		target_compile_definitions(unit_radiusfps PRIVATE RADIUSFPS_HAVE_CUDA=1)
		target_link_libraries(unit_radiusfps PRIVATE CUDA::cudart)
	endif()
	add_test(NAME unit_radiusfps COMMAND unit_radiusfps)
	set_tests_properties(unit_radiusfps PROPERTIES LABELS "unit;validation")

	add_executable(unit_gccfer
		tests/unit/test_gccfer.c
		extensions/research/gccfer/gccfer_dataset.c
		extensions/research/gccfer/gccfer_cafer.c
		extensions/research/gccfer/gccfer_model.c)
	target_include_directories(unit_gccfer PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_gccfer PRIVATE m)
	endif()
	add_test(NAME unit_gccfer COMMAND unit_gccfer)
	set_tests_properties(unit_gccfer PROPERTIES LABELS "unit;validation")

	add_executable(unit_dax
		tests/unit/test_dax.c
		extensions/research/dax/dax_benchmark.c
		extensions/research/dax/dax_model.c
		extensions/research/dax/dax_eval.c)
	target_include_directories(unit_dax PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(UNIX AND NOT APPLE)
		target_link_libraries(unit_dax PRIVATE m)
	endif()
	add_test(NAME unit_dax COMMAND unit_dax)
	set_tests_properties(unit_dax PROPERTIES LABELS "unit;validation")

	add_executable(unit_dk_qsd
		tests/unit/test_dk_qsd.c
		tests/stub_dk_qsd_deps.c
		extensions/research/dk_qsd/dk_qsd.c
		extensions/research/dk_qsd/dk_qsd_kernels.c
		extensions/research/dk_qsd/dk_qsd_dense.c
		extensions/research/dk_qsd/dk_qsd_mps.c
		extensions/research/dk_qsd/dk_qsd_observables.c)
	target_include_directories(unit_dk_qsd PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(UNIX AND NOT APPLE)
		target_link_libraries(unit_dk_qsd PRIVATE m)
	endif()
	add_test(NAME unit_dk_qsd COMMAND unit_dk_qsd)
	set_tests_properties(unit_dk_qsd PROPERTIES LABELS "unit;validation")

	add_executable(unit_dlm
		tests/unit/test_dlm.c
		tests/stub_dlm_deps.c
		extensions/research/dlm/dlm_matrix.c
		extensions/research/dlm/dlm_eval.c)
	target_include_directories(unit_dlm PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(UNIX AND NOT APPLE)
		target_link_libraries(unit_dlm PRIVATE m)
	endif()
	add_test(NAME unit_dlm COMMAND unit_dlm)
	set_tests_properties(unit_dlm PROPERTIES LABELS "unit;validation")

	add_executable(unit_sfca
		tests/unit/test_sfca.c
		tests/stub_sfca_deps.c
		extensions/research/sfca/sfca_core.c
		extensions/research/sfca/sfca_run.c
		extensions/research/sfca/sfca_scan.c)
	target_include_directories(unit_sfca PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(UNIX AND NOT APPLE)
		target_link_libraries(unit_sfca PRIVATE m)
	endif()
	add_test(NAME unit_sfca COMMAND unit_sfca)
	set_tests_properties(unit_sfca PROPERTIES LABELS "unit;validation")

	add_executable(unit_howdark
		tests/unit/test_howdark.c
		extensions/research/howdark/howdark_model.c
		extensions/research/howdark/howdark_eval.c)
	target_include_directories(unit_howdark PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(UNIX AND NOT APPLE)
		target_link_libraries(unit_howdark PRIVATE m)
	endif()
	add_test(NAME unit_howdark COMMAND unit_howdark)
	set_tests_properties(unit_howdark PROPERTIES LABELS "unit;validation")

	add_executable(unit_nebrdf
		tests/unit/test_nebrdf.c
		extensions/research/nebrdf/nebrdf_model.c
		extensions/research/nebrdf/nebrdf_eval.c)
	target_include_directories(unit_nebrdf PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(UNIX AND NOT APPLE)
		target_link_libraries(unit_nebrdf PRIVATE m)
	endif()
	add_test(NAME unit_nebrdf COMMAND unit_nebrdf)
	set_tests_properties(unit_nebrdf PROPERTIES LABELS "unit;validation")

	add_executable(unit_rtfem
		tests/unit/test_rtfem.c
		extensions/research/rtfem/rtfem_model.c
		extensions/research/rtfem/rtfem_eval.c)
	target_include_directories(unit_rtfem PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(UNIX AND NOT APPLE)
		target_link_libraries(unit_rtfem PRIVATE m)
	endif()
	add_test(NAME unit_rtfem COMMAND unit_rtfem)
	set_tests_properties(unit_rtfem PROPERTIES LABELS "unit;validation")

	add_executable(unit_vgs
		tests/unit/test_vgs.c
		extensions/research/vgs/vgs_model.c
		extensions/research/vgs/vgs_eval.c)
	target_include_directories(unit_vgs PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(UNIX AND NOT APPLE)
		target_link_libraries(unit_vgs PRIVATE m)
	endif()
	add_test(NAME unit_vgs COMMAND unit_vgs)
	set_tests_properties(unit_vgs PROPERTIES LABELS "unit;validation")

	add_executable(unit_cem
		tests/unit/test_cem.c
		extensions/research/cem/cem_model.c
		extensions/research/cem/cem_eval.c)
	target_include_directories(unit_cem PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(UNIX AND NOT APPLE)
		target_link_libraries(unit_cem PRIVATE m)
	endif()
	add_test(NAME unit_cem COMMAND unit_cem)
	set_tests_properties(unit_cem PROPERTIES LABELS "unit;validation")

	add_executable(unit_x3dpra
		tests/unit/test_x3dpra.c
		extensions/research/x3dpra/x3dpra_physics.c
		extensions/research/x3dpra/x3dpra_scene.c
		extensions/research/x3dpra/x3dpra_opt.c
		extensions/research/x3dpra/x3dpra_forward.c)
	target_include_directories(unit_x3dpra PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(UNIX AND NOT APPLE)
		target_link_libraries(unit_x3dpra PRIVATE m)
	endif()
	add_test(NAME unit_x3dpra COMMAND unit_x3dpra)
	set_tests_properties(unit_x3dpra PROPERTIES LABELS "unit;validation")

	add_executable(unit_iris_io
		tests/unit/test_iris_io.c
		extensions/research/iris/iris_io.c
		tests/stub_qcommon_min.c
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_include_directories(unit_iris_io PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_iris_io PRIVATE m)
	endif()
	add_test(NAME unit_iris_io COMMAND unit_iris_io)
	set_tests_properties(unit_iris_io PROPERTIES LABELS "unit;validation")

	add_executable(unit_endian
		tests/unit/test_endian.c
		tests/stub_qcommon_min.c
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_include_directories(unit_endian PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_endian PRIVATE m)
	endif()
	add_test(NAME unit_endian COMMAND unit_endian)
	set_tests_properties(unit_endian PROPERTIES LABELS "unit;validation")

	add_executable(unit_vm_native_module
		tests/unit/test_vm_native_module.c
		engine/core/vm_native_module.c)
	target_include_directories(unit_vm_native_module PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	add_test(NAME unit_vm_native_module COMMAND unit_vm_native_module)
	set_tests_properties(unit_vm_native_module PROPERTIES LABELS "unit;validation")

	add_executable(unit_nanovdb_decode
		tests/unit/test_nanovdb_decode.c
		tests/stub_qcommon_min.c
		tests/stub_jobs_deps.c
		renderers/vulkan/vk_nanovdb_decode.c
		engine/core/jobs.c
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_include_directories(unit_nanovdb_decode PRIVATE
		${CMAKE_SOURCE_DIR}
		${IDTECH3_DIR_ENGINE_CORE}
		${IDTECH3_DIR_RENDERERS}/vulkan)
	target_compile_definitions(unit_nanovdb_decode PRIVATE
		TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data")
	if(NOT MSVC)
		target_link_libraries(unit_nanovdb_decode PRIVATE m)
	endif()
	if(TARGET Threads::Threads)
		target_link_libraries(unit_nanovdb_decode PRIVATE Threads::Threads)
	endif()
	add_test(NAME unit_nanovdb_decode COMMAND unit_nanovdb_decode)
	set_tests_properties(unit_nanovdb_decode PROPERTIES LABELS "unit;validation")

	if(USE_RECAST_NAV AND TARGET recast_nav)
		add_executable(unit_openworld_nav
			tests/unit/test_openworld_nav.c
			tests/stub_qcommon_min.c
			tests/stub_openworld_nav.c
			engine/core/q_shared.c
			engine/core/q_math.c)
		target_include_directories(unit_openworld_nav PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
		target_link_libraries(unit_openworld_nav PRIVATE recast_nav)
		if(NOT MSVC)
			target_link_libraries(unit_openworld_nav PRIVATE m)
		endif()
		add_test(NAME unit_openworld_nav
			COMMAND unit_openworld_nav
				${CMAKE_SOURCE_DIR}/tests/data/openworld/maps/sector_0_0.bsp
				${CMAKE_BINARY_DIR}/sector_0_0_test.nav)
		set_tests_properties(unit_openworld_nav PROPERTIES LABELS "unit;validation;openworld;sector_stream")
	endif()

	add_executable(unit_world_residency
		tests/unit/test_world_residency.c
		tests/stub_qcommon_min.c
		tests/stub_world_residency_deps.c
		modules/world/world_residency.cpp
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_compile_definitions(unit_world_residency PRIVATE WORLD_RESIDENCY_UNIT_TEST)
	target_include_directories(unit_world_residency PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_world_residency PRIVATE m)
	endif()
	add_test(NAME unit_world_residency COMMAND unit_world_residency)
	set_tests_properties(unit_world_residency PROPERTIES LABELS "unit;validation;openworld;sector_stream")

	add_executable(unit_sector_graph
		tests/unit/test_sector_graph.c
		tests/stub_qcommon_min.c
		tests/stub_sector_graph_deps.c
		modules/world/sector_graph.cpp
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_compile_definitions(unit_sector_graph PRIVATE SECTOR_GRAPH_UNIT_TEST)
	target_include_directories(unit_sector_graph PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_sector_graph PRIVATE m)
	endif()
	add_test(NAME unit_sector_graph COMMAND unit_sector_graph)
	set_tests_properties(unit_sector_graph PROPERTIES LABELS "unit;validation;openworld;sector_stream")

	add_executable(unit_cluster_graph
		tests/unit/test_cluster_graph.c
		tests/stub_qcommon_min.c
		tests/stub_cluster_graph_deps.c
		engine/core/cluster_graph.cpp
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_compile_definitions(unit_cluster_graph PRIVATE CLUSTER_GRAPH_UNIT_TEST)
	target_include_directories(unit_cluster_graph PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_cluster_graph PRIVATE m)
	endif()
	add_test(NAME unit_cluster_graph COMMAND unit_cluster_graph)
	set_tests_properties(unit_cluster_graph PROPERTIES LABELS "unit;validation;openworld;sector_stream")

	add_executable(unit_sv_auth
		tests/unit/test_sv_auth.c
		tests/stub_qcommon_min.c
		tests/stub_sv_auth_deps.c
		runtime/server/sv_auth.c
		engine/core/md5.cpp
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_include_directories(unit_sv_auth PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_sv_auth PRIVATE m)
	endif()
	add_test(NAME unit_sv_auth COMMAND unit_sv_auth)
	set_tests_properties(unit_sv_auth PROPERTIES LABELS "unit;validation")

	add_executable(unit_fog_biology
		tests/unit/test_fog_biology.c
		tests/stub_qcommon_min.c
		tests/stub_fog_biology_deps.c
		modules/world/fog_biology.cpp
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_compile_definitions(unit_fog_biology PRIVATE FOG_BIOLOGY_UNIT_TEST)
	target_include_directories(unit_fog_biology PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_fog_biology PRIVATE m)
	endif()
	add_test(NAME unit_fog_biology COMMAND unit_fog_biology)
	set_tests_properties(unit_fog_biology PROPERTIES LABELS "unit;validation")

	if(USE_ARC_BLANC)
		add_executable(unit_arc_blanc
			tests/unit/test_arc_blanc.c
			tests/stub_qcommon_min.c
			tests/stub_arc_blanc_deps.c
			modules/world/arc_blanc/arc_blanc.c
			modules/world/arc_blanc/arc_blanc_fft.c
			modules/world/arc_blanc/arc_blanc_spectrum.c
			modules/world/arc_blanc/arc_blanc_ocean.c
			modules/world/arc_blanc/arc_blanc_velocity.c
			modules/world/arc_blanc/arc_blanc_coupling.c
			engine/core/q_shared.c
			engine/core/q_math.c)
		target_compile_definitions(unit_arc_blanc PRIVATE ARC_BLANC_UNIT_TEST)
		target_include_directories(unit_arc_blanc PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
		if(NOT MSVC)
			target_link_libraries(unit_arc_blanc PRIVATE m)
		endif()
		add_test(NAME unit_arc_blanc COMMAND unit_arc_blanc)
		set_tests_properties(unit_arc_blanc PROPERTIES LABELS "unit;validation")
	endif()

	add_executable(unit_genetic_gan
		tests/unit/test_genetic_gan.c
		tests/stub_qcommon_min.c
		tests/stub_genetic_gan_deps.c
		modules/world/genetic_gan.cpp
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_compile_definitions(unit_genetic_gan PRIVATE GENETIC_GAN_UNIT_TEST)
	target_include_directories(unit_genetic_gan PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_genetic_gan PRIVATE m)
	endif()
	add_test(NAME unit_genetic_gan COMMAND unit_genetic_gan)
	set_tests_properties(unit_genetic_gan PROPERTIES LABELS "unit;validation")

	add_executable(unit_jobs
		tests/unit/test_jobs.c
		tests/stub_qcommon_min.c
		tests/stub_jobs_deps.c
		engine/core/jobs.c
		engine/core/q_shared.c
		engine/core/q_math.c)
	target_include_directories(unit_jobs PRIVATE ${IDTECH3_UNIT_INCLUDE_DIRS})
	if(NOT MSVC)
		target_link_libraries(unit_jobs PRIVATE m)
	endif()
	if(TARGET Threads::Threads)
		target_link_libraries(unit_jobs PRIVATE Threads::Threads)
	endif()
	add_test(NAME unit_jobs COMMAND unit_jobs)
	set_tests_properties(unit_jobs PROPERTIES LABELS "unit;validation")

	add_test(NAME test_genetic_gan
		COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_genetic_gan.sh
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
	set_tests_properties(test_genetic_gan PROPERTIES
		LABELS "validation"
		REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_genetic_gan.sh;${CMAKE_SOURCE_DIR}/modules/world/genetic_gan.cpp;${CMAKE_SOURCE_DIR}/modules/world/genetic_gan.h")
endif()

# Demo mod pack (config-only .pk3); optional - see samples/demo_game/README.md
if(BUILD_SAMPLES_DEMO_GAME AND NOT ANDROID)
	include("${CMAKE_SOURCE_DIR}/examples/demo_game/CMakeLists.txt")
endif()

include(${CMAKE_SOURCE_DIR}/cmake/export/MsvcSourceManifest.cmake)
idtech3_export_msvc_manifest()
