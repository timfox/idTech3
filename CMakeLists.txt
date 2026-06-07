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

option(C_STANDARD_STRICT "Enable the stricter warning set for engine builds" ON)
option(CI_BUILD "Enable CI-specific behaviors such as warnings-as-errors" OFF)
option(USE_FLUX "Enable FLUX.2 image generation support" ON)
option(ENABLE_FORTIFY_SOURCE "Enable _FORTIFY_SOURCE=2 buffer overflow protection (GCC/Clang)" ON)
option(ENABLE_ASAN "Enable AddressSanitizer for memory error detection (Debug builds)" OFF)
option(ENABLE_LTO "Enable IPO/LTO for Release and RelWithDebInfo (GCC/Clang; longer links, smaller/faster binaries)" OFF)
option(BUILD_EXAMPLE_DEMO_GAME "Build examples/demo_game (idtech3_demo.pk3 + helper)" OFF)

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
    # For MSVC, treat warnings as errors but allow unused functions
    add_compile_options(/W4 /wd4100 /wd4505)
  else()
    # For GCC/Clang, disable problematic warnings for CI
    add_compile_options(-Wno-unused-function -Wno-shadow -Wno-cast-qual -Wno-vla)
  endif()
endif()

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

# --- Begin FindSDL2.cmake
# -------------------------------------------------------------
# Adapted FindSDL2.cmake module for CMake
#
# (Full CMake FindSDL2.cmake functionality inlined here for build portability)
# This logic will try to locate an SDL2 installation, define imported targets,
# and provide variables similar to the canonical FindSDL2.cmake
#
# Minimal compatibility implementation follows:
# -------------------------------------------------------------
#
# Variables defined:
#   SDL2_FOUND
#   SDL2_INCLUDE_DIR
#   SDL2_INCLUDE_DIRS
#   SDL2_LIBRARY
#   SDL2_LIBRARIES
# Targets defined (if found):
#   SDL2::Core
#   SDL2::SDL2
#   SDL2::Main
#
# Usage (as in this CMakeLists.txt):
#   find_package(SDL2 REQUIRED)
#   target_link_libraries(foo SDL2::Main)
#
# Only for local inline usage -- not a full drop-in for all SDL2 find modules.
#
# -------------------------------------------------------------
set(_SDL2_HINT_PATHS
  $ENV{SDL2_DIR}
  $ENV{SDL2_PATH}
  ${CMAKE_PREFIX_PATH}
  ${CMAKE_INSTALL_PREFIX}
  /usr/local
  /usr
  /opt/local
  /opt
  /sw
)

find_path(SDL2_INCLUDE_DIR
  NAMES SDL.h SDL2/SDL.h
  PATH_SUFFIXES SDL2 include
  PATHS ${_SDL2_HINT_PATHS}
)

# Try SDL2::SDL2 (split main and core lib), or fallback to sdl2 for older systems
set(_SDL2_LIB_NAMES SDL2 SDL2-2.0 SDL2d)
find_library(SDL2_LIBRARY
  NAMES ${_SDL2_LIB_NAMES}
  PATH_SUFFIXES lib64 lib
  PATHS ${_SDL2_HINT_PATHS}
)

# Common main helper (may not be present)
find_library(SDL2_MAIN_LIBRARY
  NAMES SDL2main
  PATH_SUFFIXES lib64 lib
  PATHS ${_SDL2_HINT_PATHS}
)

if(SDL2_INCLUDE_DIR AND SDL2_LIBRARY)
  set(SDL2_FOUND TRUE)
  set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
  if(SDL2_MAIN_LIBRARY)
    set(SDL2_LIBRARIES ${SDL2_LIBRARY} ${SDL2_MAIN_LIBRARY})
  else()
    set(SDL2_LIBRARIES ${SDL2_LIBRARY})
  endif()

  if(NOT TARGET SDL2::Core)
    add_library(SDL2::Core UNKNOWN IMPORTED)
    set_target_properties(SDL2::Core PROPERTIES
      IMPORTED_LOCATION "${SDL2_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}")
  endif()
  if(NOT TARGET SDL2::SDL2)
    add_library(SDL2::SDL2 ALIAS SDL2::Core)
  endif()
  if(SDL2_MAIN_LIBRARY AND NOT TARGET SDL2::Main)
    add_library(SDL2::Main UNKNOWN IMPORTED)
    set_target_properties(SDL2::Main PROPERTIES
      IMPORTED_LOCATION "${SDL2_MAIN_LIBRARY}"
      INTERFACE_LINK_LIBRARIES SDL2::Core
    )
  elseif(NOT TARGET SDL2::Main)
    add_library(SDL2::Main INTERFACE IMPORTED)
    set_property(TARGET SDL2::Main PROPERTY INTERFACE_LINK_LIBRARIES SDL2::Core)
  endif()
else()
  set(SDL2_FOUND FALSE)
endif()
# --- End FindSDL2.cmake

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

set(LUA_FOUND FALSE)
set(LUA_LINK_LIBRARIES "")
set(LUA_VERSION_NUM 0)
set(LUA_VERSION_STRING "")
set(LUA_EXTRA_LIBRARIES "")
set(DUKTAPE_FOUND FALSE)
set(DUKTAPE_LINK_LIBRARIES "")
set(DUKTAPE_EXTRA_LIBRARIES "")

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
  set(DUKTAPE_VENDORED_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/external/duktape/duktape.c")
  set(DUKTAPE_VENDORED_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/src/external/duktape/duktape.h")

  if((NOT USE_SYSTEM_DUKTAPE) AND EXISTS "${DUKTAPE_VENDORED_SOURCE}" AND EXISTS "${DUKTAPE_VENDORED_HEADER}")
    add_library(js_runtime_internal STATIC ${DUKTAPE_VENDORED_SOURCE})
    target_include_directories(js_runtime_internal PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/external/duktape")
    set_property(TARGET js_runtime_internal PROPERTY POSITION_INDEPENDENT_CODE ON)
    if(NOT MSVC)
      target_compile_options(js_runtime_internal PRIVATE -fno-strict-aliasing -Wno-format-nonliteral -Wno-unused-but-set-variable)
    endif()

    set(DUKTAPE_FOUND TRUE)
    set(DUKTAPE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/external/duktape")
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
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/ogg/src/bitwise.c")
    if(NOT TARGET ogg)
      add_library(ogg STATIC
        src/external/src/ogg/src/bitwise.c
        src/external/src/ogg/src/framing.c
      )
      target_include_directories(ogg PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/ogg/include)
      target_compile_definitions(ogg PRIVATE INCLUDE_INTTYPES_H=1 INCLUDE_SYS_TYPES_H=1)
      if(NOT MSVC)
        target_compile_options(ogg PRIVATE -Wno-undef)
      endif()
    endif()
    set(OGG_LIBRARY ogg)
    set(OGG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/ogg/include)
  endif()
endif()

if(USE_OPUS)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/opus/CMakeLists.txt")
    set(OPUS_BUILD_SHARED_LIBRARY OFF CACHE BOOL "" FORCE)
    set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(OPUS_BUILD_FRAMEWORK OFF CACHE BOOL "" FORCE)
    set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
    add_subdirectory(src/external/src/opus ${CMAKE_BINARY_DIR}/external/opus)
    set(SKIP_INSTALL_ALL OFF CACHE BOOL "" FORCE)
    set(OPUS_LIBRARY opus)
    set(OPUS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/opus/include)
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
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/opusfile/src/opusfile.c")
    if(NOT TARGET opusfile)
      add_library(opusfile STATIC
        src/external/src/opusfile/src/info.c
        src/external/src/opusfile/src/internal.c
        src/external/src/opusfile/src/opusfile.c
        src/external/src/opusfile/src/stream.c
      )
      target_compile_definitions(opusfile PRIVATE OP_DISABLE_HTTP INCLUDE_INTTYPES_H=1 INCLUDE_SYS_TYPES_H=1)
      target_include_directories(opusfile PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/opusfile/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/opusfile
        ${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/ogg/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/opus/include
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
    set(OPUSFILE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/opusfile/include)
  endif()
endif()

if(USE_FLAC)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/flac/CMakeLists.txt")
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
    add_subdirectory(src/external/src/flac ${CMAKE_BINARY_DIR}/external/flac)
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
    set(FLAC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/flac/include)
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
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/nestegg/src/nestegg.c")
    if(NOT TARGET nestegg)
      add_library(nestegg STATIC
        src/external/src/nestegg/src/nestegg.c
        src/external/src/nestegg/halloc/src/halloc.c
      )
      target_include_directories(nestegg PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/nestegg
        ${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/nestegg/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/nestegg/halloc
      )
      if(NOT MSVC)
        target_compile_options(nestegg PRIVATE -Wno-unused-function)
      endif()
    endif()
    set(NESTEGG_LIBRARY nestegg)
    set(NESTEGG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/nestegg/include)
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
    set(OPUS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/opus/include)
  endif()
  if(NOT OPUSFILE_INCLUDE_DIR)
    set(OPUSFILE_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/opusfile/include)
  endif()
  if(NOT OGG_INCLUDE_DIR)
    set(OGG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/ogg/include)
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
    set(FLAC_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/flac/include)
  endif()
  set(FLAC_FOUND TRUE)
endif()

if(USE_WEBM AND TARGET nestegg AND USE_OPUS AND OPUS_LIBRARY AND OPUS_INCLUDE_DIR)
  if(NOT NESTEGG_LIBRARY)
    set(NESTEGG_LIBRARY nestegg)
  endif()
  if(NOT NESTEGG_INCLUDE_DIR)
    set(NESTEGG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/external/src/nestegg/include)
  endif()
  set(WEBM_FOUND TRUE)
endif()

SET(CNAME "idtech3")
SET(DNAME "idtech3_server")

SET(RENDERER_PREFIX ${CNAME})
SET(RENDERER_DEFAULT vulkan) # valid options: opengl, vulkan
add_compile_definitions(RENDERER_PREFIX="${RENDERER_PREFIX}" RENDERER_DEFAULT="${RENDERER_DEFAULT}")

OPTION(USE_SDL "" ON)
# When no system SDL2 is detected, download and build SDL2 from the official release tarball
# (headers under src/external/src/sdl are not sufficient to link the client).
option(USE_BUNDLED_SDL2 "Build SDL2 from source if system SDL2 is not found (requires network on first configure)" ON)
OPTION(USE_OPENAL "Enable OpenAL audio backend" ON)
OPTION(USE_CURL "" ON)
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
OPTION(USE_VULKAN "" ON)
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

# Set when FetchContent-built SDL2 is used (avoid mixing vendored snapshot headers with built lib).
set(IDTECH3_USING_FETCHED_SDL2 OFF)
if(USE_SDL AND NOT SDL2_FOUND AND USE_BUNDLED_SDL2 AND NOT ANDROID AND NOT MSVC)
  set(_IDTECH3_SDL2_VER "2.30.11")
  set(_IDTECH3_SDL2_SHA256 "8b8d4aef2038533da814965220f88f77d60dfa0f32685f80ead65e501337da7f")
  include(FetchContent)
  FetchContent_Declare(
    idtech3_sdl2
    URL "https://github.com/libsdl-org/SDL/releases/download/release-${_IDTECH3_SDL2_VER}/SDL2-${_IDTECH3_SDL2_VER}.tar.gz"
    URL_HASH SHA256=${_IDTECH3_SDL2_SHA256}
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
  set(SDL_TESTS OFF CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  if(CMAKE_POSITION_INDEPENDENT_CODE)
    set(SDL_STATIC_PIC ON CACHE BOOL "" FORCE)
  endif()
  message(STATUS "SDL2: system library not found; building SDL ${_IDTECH3_SDL2_VER} via FetchContent (one-time download)")
  FetchContent_MakeAvailable(idtech3_sdl2)
  if(NOT TARGET SDL2::SDL2)
    message(FATAL_ERROR "SDL2 FetchContent: expected target SDL2::SDL2 after building bundled SDL2.")
  endif()
  if(NOT TARGET SDL2::Core)
    add_library(SDL2::Core INTERFACE IMPORTED)
    set_property(TARGET SDL2::Core PROPERTY INTERFACE_LINK_LIBRARIES SDL2::SDL2)
  endif()
  if(NOT TARGET SDL2::Main)
    add_library(SDL2::Main INTERFACE IMPORTED)
    if(TARGET SDL2::SDL2main)
      set_property(TARGET SDL2::Main PROPERTY INTERFACE_LINK_LIBRARIES SDL2::SDL2main SDL2::SDL2)
    else()
      set_property(TARGET SDL2::Main PROPERTY INTERFACE_LINK_LIBRARIES SDL2::SDL2)
    endif()
  endif()
  FetchContent_GetProperties(idtech3_sdl2 SOURCE_DIR _idtech3_sdl2_src)
  if(_idtech3_sdl2_src)
    set(SDL2_INCLUDE_DIR "${_idtech3_sdl2_src}/include")
    set(SDL2_INCLUDE_DIRS "${SDL2_INCLUDE_DIR}")
  endif()
  set(SDL2_FOUND TRUE)
  set(IDTECH3_USING_FETCHED_SDL2 ON)
  message(STATUS "SDL2: bundled build enabled (includes + libs from ${_idtech3_sdl2_src})")
endif()

OPTION(SKIP_IDPAK_CHECK "Skip checking for original Quake III pak files (pak0.pk3)" ON)
OPTION(BUILD_FREETYPE "Enable FreeType-powered font rendering / font generation" ON)
if(BUILD_FREETYPE)
	include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/DownloadInterFonts.cmake)
endif()
OPTION(USE_DTLS "Enable DTLS encryption for game network traffic (OpenSSL)" ON)
OPTION(USE_STEAM "Enable Steam API (achievements, overlay, rich presence, Deck detection)" OFF)
OPTION(USE_STEAM_NETWORKING "Enable Steam Datagram Relay (SDR) transport" OFF)
OPTION(USE_VULKAN_RTX "Enable Vulkan ray tracing (VK_KHR_ray_tracing_*); requires RT-capable GPU" OFF)
OPTION(USE_METAL_RENDERER "Build native Metal renderer (Apple only)" OFF)
OPTION(USE_DXR_RENDERER "Build DirectX 12 + DXR renderer (Windows only)" OFF)

if(USE_METAL_RENDERER)
  if(NOT APPLE)
    message(WARNING "USE_METAL_RENDERER is Apple-only; disabling.")
    set(USE_METAL_RENDERER OFF CACHE BOOL "" FORCE)
  else()
    message(WARNING "Metal renderer not yet implemented (docs/RENDERERS_FUTURE.md); option reserved for future use.")
  endif()
endif()
if(USE_DXR_RENDERER)
  if(NOT WIN32)
    message(WARNING "USE_DXR_RENDERER is Windows-only; disabling.")
    set(USE_DXR_RENDERER OFF CACHE BOOL "" FORCE)
  else()
    message(WARNING "DXR renderer not yet implemented (docs/RENDERERS_FUTURE.md); option reserved for future use.")
  endif()
endif()

set(EXTERNAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/external")
set(EXTERNAL_SRC "${EXTERNAL_ROOT}/src")
set(EXTERNAL_INCLUDE "${EXTERNAL_ROOT}/include")
set(EXTERNAL_SDL_INCLUDE_DIR "${EXTERNAL_SRC}/sdl/include/SDL2")
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
	add_subdirectory(src/external/src/cflux2)
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

AUX_SOURCE_DIRECTORY(src/qcommon QCOMMON_SRCS)
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
    list(APPEND QCOMMON_SRCS src/qcommon/net_dtls.c)
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
        list(APPEND QCOMMON_SRCS src/qcommon/net_dtls.c)
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
    list(APPEND QCOMMON_SRCS src/qcommon/net_sdr.c)
  endif()
  find_path(STEAMWORKS_INCLUDE_DIR steam/steam_api.h
    HINTS $ENV{STEAMWORKS_SDK} $ENV{STEAMWORKS_SDK}/public
    PATH_SUFFIXES include)
  if(STEAMWORKS_INCLUDE_DIR)
    find_library(STEAMWORKS_LIBRARY steam_api
      HINTS $ENV{STEAMWORKS_SDK} $ENV{STEAMWORKS_SDK}/redistributable_bin
      PATH_SUFFIXES linux64 linux32 win64)
    if(STEAMWORKS_LIBRARY)
      set(STEAMWORKS_FOUND TRUE)
      message(STATUS "Steam SDR: Steamworks SDK found (${STEAMWORKS_INCLUDE_DIR})")
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
	list(APPEND QCOMMON_SRCS src/qcommon/vm_x86.c)
	set(NO_VM_COMPILED OFF)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES aarch64|arm64)
	set(BINEXT .aarch64)
	set(RENDEXT _aarch64)
	list(APPEND QCOMMON_SRCS src/qcommon/vm_aarch64.c)
	set(NO_VM_COMPILED OFF)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES arm*)
	set(BINEXT .arm)
	set(RENDEXT _arm)
	list(APPEND QCOMMON_SRCS src/qcommon/vm_armv7l.c)
	set(NO_VM_COMPILED OFF)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64le|ppc64")
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64le")
		set(BINEXT .ppc64le)
		set(RENDEXT _ppc64le)
	else()
		set(BINEXT .ppc64)
		set(RENDEXT _ppc64)
	endif()
	list(APPEND QCOMMON_SRCS src/qcommon/vm_powerpc.c)
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
AUX_SOURCE_DIRECTORY(src/botlib BOTLIB_SRCS)
ADD_LIBRARY(botlib OBJECT ${BOTLIB_SRCS})
TARGET_COMPILE_DEFINITIONS(botlib PRIVATE BOTLIB)
if(USE_GLM)
  target_include_directories(botlib PRIVATE ${EXTERNAL_GLM_INCLUDE})
endif()

AUX_SOURCE_DIRECTORY(src/server SERVER_SRCS)

# universal binary
ADD_LIBRARY(qcommon OBJECT ${QCOMMON_SRCS} ${SERVER_SRCS})
if(USE_GLM)
  target_include_directories(qcommon PRIVATE ${EXTERNAL_GLM_INCLUDE})
endif()
target_include_directories(qcommon PRIVATE
  ${CMAKE_SOURCE_DIR}/src/platform
  ${CMAKE_SOURCE_DIR}/src/platform/win32
  ${CMAKE_SOURCE_DIR}/src/platform/platform/win32
  ${CMAKE_SOURCE_DIR}/src/client
  ${CMAKE_SOURCE_DIR}/src/audio
)
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

# dedicated server
ADD_LIBRARY(qcommon_ded OBJECT ${QCOMMON_SRCS} ${SERVER_SRCS})
TARGET_COMPILE_DEFINITIONS(qcommon_ded PUBLIC DEDICATED)
if(USE_GLM)
  target_include_directories(qcommon_ded PRIVATE ${EXTERNAL_GLM_INCLUDE})
endif()
target_include_directories(qcommon_ded PRIVATE
  ${CMAKE_SOURCE_DIR}/src/platform
  ${CMAKE_SOURCE_DIR}/src/platform/win32
  ${CMAKE_SOURCE_DIR}/src/platform/platform/win32
  ${CMAKE_SOURCE_DIR}/src/client
  ${CMAKE_SOURCE_DIR}/src/audio
)
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
  target_compile_definitions(qcommon_ded PRIVATE USE_STEAM_NETWORKING=1)
endif()
if(USE_STEAM_NETWORKING AND STEAMWORKS_FOUND)
  target_compile_definitions(client PRIVATE USE_STEAM_NETWORKING=1 USE_STEAM=1)
  target_include_directories(client PRIVATE ${STEAMWORKS_INCLUDE_DIR})
  message(STATUS "Steam: full API + SDR enabled")
elseif(USE_STEAM AND STEAMWORKS_FOUND)
  target_compile_definitions(client PRIVATE USE_STEAM=1)
  target_include_directories(client PRIVATE ${STEAMWORKS_INCLUDE_DIR})
  message(STATUS "Steam: API enabled (achievements, overlay, rich presence, Deck)")
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

# client + cURL
# client sources
AUX_SOURCE_DIRECTORY(src/client CLIENT_SRCS)
# remove audio implementation from client; moved to src/audio
list(REMOVE_ITEM CLIENT_SRCS
  src/client/snd_main.c
  src/client/snd_dma.c
  src/client/snd_mem.c
  src/client/snd_mix.c
  src/client/snd_mix_fallback.c
  src/client/snd_wavelet.c
  src/client/snd_adpcm.c
  src/client/snd_codec.c
  src/client/snd_codec_mp3.c
  src/client/snd_codec_ogg.c
  src/client/snd_codec_wav.c
)
list(APPEND CLIENT_SRCS
  src/audio/snd_main.c
  src/audio/mix/snd_dma.c
  src/audio/mix/snd_mem.c
  src/audio/mix/snd_mix.c
  src/audio/mix/snd_mix_fallback.c
  src/audio/mix/snd_wavelet.c
  src/audio/mix/snd_adpcm.c
  src/audio/codecs/snd_codec.c
  src/audio/codecs/snd_codec_mp3.c
  src/audio/codecs/snd_codec_ogg.c
  src/audio/codecs/snd_codec_wav.c
  src/audio/codecs/snd_codec_opus.c
  src/audio/codecs/snd_codec_flac.c
  src/audio/codecs/snd_codec_webm.c
  src/audio/backends/snd_backend_null.c
)
if(USE_OPENAL)
  list(APPEND CLIENT_SRCS src/audio/backends/snd_backend_openal.c)
  list(APPEND CLIENT_SRCS src/audio/effects/snd_efx.c)
endif()
if(USE_SDL)
  list(APPEND CLIENT_SRCS src/audio/backends/snd_backend_sdl.c)
endif()
list(APPEND CLIENT_SRCS
  src/audio/snd_music_adaptive.c
  src/game/g_director.c
  src/game/g_response.c
  src/game/g_choreography.c
  src/game/g_dismember.c
  src/game/g_facial.c
  src/game/g_horde.c
  src/game/g_goap.c
  src/game/g_aiml.c
  src/game/g_eda.c
  src/game/g_engine_systems.c
  src/game/g_bt.c
  src/game/g_entity_bridge.c
  src/game/g_lua_bindings.c
  src/client/cl_map_background.c
  src/client/cl_window_title.c
)
# include architecture-specific assembly implementations (snd_mix etc)
# Only include x86/x86_64 assembly files on x86/x86_64 architectures on Unix-like systems
# Assembly is disabled on Windows due to preprocessor macro issues with MSYS2/MinGW
IF(UNIX AND CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86|i.86|x86_64|X86_64")
  # Only enable handwritten x86/x86_64 assembly when building a 64-bit target.
  # 32-bit builds will use the portable C fallbacks to avoid assembler/arch mismatches.
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    file(GLOB ASM_SRCS "src/asm/*.S" "src/asm/*.s")
    list(APPEND CLIENT_SRCS ${ASM_SRCS})
    # Ensure CMake knows how to compile assembly sources
    enable_language(ASM)
    if(ASM_SRCS)
      set_source_files_properties(${ASM_SRCS} PROPERTIES LANGUAGE ASM)
      # run assembler sources through the C preprocessor and use Intel syntax
      # Note: -masm=intel is GCC-specific; Clang uses Intel syntax by default for .s files
      set(ASM_FLAGS "-x assembler-with-cpp")
      if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
        set(ASM_FLAGS "${ASM_FLAGS} -masm=intel")
      endif()
      set_source_files_properties(${ASM_SRCS} PROPERTIES COMPILE_FLAGS "${ASM_FLAGS}")
    endif()
    # Define USE_ASM to tell the code that assembly optimizations are available
    ADD_COMPILE_DEFINITIONS(USE_ASM)
  else()
    # 32-bit: do not compile asm sources; rely on C implementations.
    set(ASM_SRCS "")
  endif()
ENDIF()
IF(NOT USE_CURL OR NOT CURL_FOUND)
	list(REMOVE_ITEM CLIENT_SRCS src/client/cl_curl.c)
ENDIF()
IF(USE_CURL AND NOT CURL_FOUND)
	message(WARNING "USE_CURL is ON but libcurl was not found; cl_curl disabled.")
ENDIF()
IF(NOT USE_MP3)
	list(REMOVE_ITEM CLIENT_SRCS src/client/snd_codec_mp3.c)
	list(REMOVE_ITEM CLIENT_SRCS src/audio/codecs/snd_codec_mp3.c)
ENDIF()
IF(USE_MP3)
	AUX_SOURCE_DIRECTORY(src/audio/mp3 MP3_SRCS)
	list(APPEND CLIENT_SRCS ${MP3_SRCS})
ENDIF()

# Opus/FLAC sources already listed above; remove if not found.
if(NOT USE_OPUS OR NOT OPUS_FOUND)
  list(REMOVE_ITEM CLIENT_SRCS src/audio/codecs/snd_codec_opus.c)
endif()
if(NOT USE_FLAC OR NOT FLAC_FOUND)
  list(REMOVE_ITEM CLIENT_SRCS src/audio/codecs/snd_codec_flac.c)
endif()
if(NOT USE_WEBM OR NOT WEBM_FOUND)
  list(REMOVE_ITEM CLIENT_SRCS src/audio/codecs/snd_codec_webm.c)
endif()

# --- Modern video codec support ---
option(USE_FFMPEG "Enable FFmpeg video codec support (requires system libavcodec)" ON)
option(USE_DAV1D "Enable dav1d AV1 video codec support (requires system libdav1d)" ON)
option(USE_VPX "Enable libvpx VP8/VP9 video codec support (requires system libvpx)" ON)
option(USE_THEORA "Enable Theora video codec support (requires system libtheora)" ON)

set(FFMPEG_FOUND FALSE)
set(DAV1D_FOUND FALSE)
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
    message(WARNING "FFmpeg requested but pkg-config could not find libavcodec/libavformat/libavutil/libswscale/libswresample; FFmpeg codec disabled.")
    list(REMOVE_ITEM CLIENT_SRCS src/client/cl_cin_ffmpeg.c)
  endif()
else()
  list(REMOVE_ITEM CLIENT_SRCS src/client/cl_cin_ffmpeg.c)
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
    message(WARNING "dav1d requested but not found; AV1 codec disabled.")
    list(REMOVE_ITEM CLIENT_SRCS src/client/cl_cin_dav1d.c)
  endif()
else()
  list(REMOVE_ITEM CLIENT_SRCS src/client/cl_cin_dav1d.c)
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
    message(WARNING "libvpx requested but not found; VP8/VP9 codec disabled.")
    list(REMOVE_ITEM CLIENT_SRCS src/client/cl_cin_vpx.c)
  endif()
else()
  list(REMOVE_ITEM CLIENT_SRCS src/client/cl_cin_vpx.c)
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
    message(WARNING "Theora requested but libtheoradec not found; Theora codec disabled.")
    list(REMOVE_ITEM CLIENT_SRCS src/client/cl_cin_theora.c)
  endif()
else()
  list(REMOVE_ITEM CLIENT_SRCS src/client/cl_cin_theora.c)
endif()

# pkg-config resolves *host* FFmpeg/dav1d/vpx/Theora; never use those include paths for NDK targets.
if(ANDROID)
  set(FFMPEG_FOUND FALSE)
  set(DAV1D_FOUND FALSE)
  set(VPX_FOUND FALSE)
  set(THEORA_FOUND FALSE)
  list(REMOVE_ITEM CLIENT_SRCS src/client/cl_cin_ffmpeg.c)
  list(REMOVE_ITEM CLIENT_SRCS src/client/cl_cin_dav1d.c)
  list(REMOVE_ITEM CLIENT_SRCS src/client/cl_cin_vpx.c)
  list(REMOVE_ITEM CLIENT_SRCS src/client/cl_cin_theora.c)
  message(STATUS "Android: host pkg-config video codecs disabled (FFmpeg/dav1d/vpx/Theora); use vendor/NDK builds to re-enable")
endif()
# --- End modern video codec support ---

#ADD_LIBRARY(client OBJECT ${CLIENT_SRCS})

# jpeg
IF(USE_SYSTEM_JPEG)
	ADD_LIBRARY(client OBJECT ${CLIENT_SRCS})
	find_package(JPEG REQUIRED)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE -DUSE_SYSTEM_JPEG)
	target_include_directories(client PRIVATE ${CMAKE_SOURCE_DIR}/src/game)
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
	target_include_directories(client PRIVATE ${CMAKE_SOURCE_DIR}/src/qcommon ${EXTERNAL_JPEG_SRC_DIR})
	# ECS (EnTT) - alternative to gentity
	target_include_directories(client PRIVATE ${CMAKE_SOURCE_DIR}/src/game)
	# Audio module headers (moved from client/)
	target_include_directories(client PRIVATE
		${CMAKE_SOURCE_DIR}/src/audio
		${CMAKE_SOURCE_DIR}/src/audio/codecs
		${CMAKE_SOURCE_DIR}/src/client
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

if(USE_FLUX)
	target_compile_definitions(client PRIVATE USE_FLUX)
	target_include_directories(client PRIVATE ${EXTERNAL_ROOT}/src/cflux2)
endif()

# renderers
set(RENDERER_COMMON_SRCS
	src/renderers/common/tr_font_stub.c  # placeholder, will be replaced below
	src/renderers/common/tr_image_bmp.c
	src/renderers/common/tr_image_jpg.c
	src/renderers/common/tr_image_pcx.c
	src/renderers/common/tr_image_png.c
	src/renderers/common/tr_image_tga.c
	src/renderers/common/tr_image_hdr.c
	src/renderers/common/tr_image_qoi.c
	src/renderers/common/tr_image_dds.c
	src/renderers/common/tr_image_svg.c
	src/renderers/common/tr_model_mesh_import.c
	src/renderers/vulkan/tr_model_gltf.c
	src/renderers/vulkan/tr_model_obj.c
	src/renderers/vulkan/tr_model_md5.c
	src/renderers/common/tr_image_ktx2.c
	src/renderers/common/tr_noise.c
)
IF(BUILD_FREETYPE)
	list(REMOVE_ITEM RENDERER_COMMON_SRCS src/renderers/common/tr_font_stub.c)
	list(APPEND RENDERER_COMMON_SRCS src/renderers/common/tr_font.c)
ELSE()
	# Keep tr_font_stub.c
ENDIF()
AUX_SOURCE_DIRECTORY(src/renderers/opengl RENDERER_GL_SRCS)
AUX_SOURCE_DIRECTORY(src/renderers/vulkan RENDERER_VK_SRCS)
# Exclude generated shader blobs to avoid duplicate symbols with committed spirv/ copies
list(FILTER RENDERER_VK_SRCS EXCLUDE REGEX ".*/shaders/spirv/generated/shader_data\\.c$")
list(FILTER RENDERER_VK_SRCS EXCLUDE REGEX ".*/shaders/spirv/generated/shader_binding\\.c$")
list(APPEND RENDERER_VK_SRCS
    src/renderers/vulkan/utils/mikktspace/mikktspace.c
)
set(VK_SHADER_DIR "${CMAKE_SOURCE_DIR}/src/renderers/vulkan/shaders")
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
    set_source_files_properties(src/renderers/vulkan/utils/mikktspace/mikktspace.c PROPERTIES
        COMPILE_OPTIONS "/wd4456;/wd4457;/wd4458;/wd4459"
    )
else()
    set_source_files_properties(src/renderers/vulkan/utils/mikktspace/mikktspace.c PROPERTIES
        COMPILE_OPTIONS "-Wno-shadow;-Wno-unused-but-set-variable"
    )
endif()

SET(AUX_SRCS)
if(NOT USE_RENDERER_DLOPEN)
	list(APPEND AUX_SRCS
		src/qcommon/q_shared.c
		src/qcommon/q_math.c)
	if(NOT USE_LIBPNG)
		list(APPEND AUX_SRCS src/qcommon/puff.c)
	endif()
endif()
IF(USE_RENDERER_DLOPEN)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_RENDERER_DLOPEN RENDERER_PREFIX="${RENDERER_PREFIX}")

	ADD_LIBRARY(${RENDERER_PREFIX}_opengl${RENDEXT} SHARED ${RENDERER_GL_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
	target_sources(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE src/renderers/opengl/tr_gltf_rb_opengl.c)
	TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE USE_RENDERER_DLOPEN RENDERER_OPENGL)
	IF(BUILD_FREETYPE)
		target_link_libraries(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE idtech3_freetype)
	ENDIF()

	ADD_LIBRARY(${RENDERER_PREFIX}_vulkan${RENDEXT} SHARED ${RENDERER_VK_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
	TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_RENDERER_DLOPEN RENDERER_VULKAN)
	if(USE_VULKAN_RTX)
	  target_compile_definitions(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_VULKAN_RTX=1)
	  message(STATUS "Vulkan RTX: ray tracing extensions will be requested when available")
	endif()
	add_dependencies(${RENDERER_PREFIX}_vulkan${RENDEXT} vk_shaders_autogen)
	IF(BUILD_FREETYPE)
		target_link_libraries(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE idtech3_freetype)
	ENDIF()
	if(WIN32)
		target_link_libraries(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE qcommon m ${CMAKE_DL_LIBS})
		target_link_libraries(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE qcommon m ${CMAKE_DL_LIBS})
	else()
		target_link_libraries(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE m ${CMAKE_DL_LIBS})
		target_link_libraries(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE m ${CMAKE_DL_LIBS})
	endif()
	IF(APPLE)
		target_link_options(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE "-undefined" "dynamic_lookup")
		target_link_options(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE "-undefined" "dynamic_lookup")
	ENDIF()
	if(UNIX AND NOT APPLE)
		target_link_options(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE "-Wl,--allow-shlib-undefined")
		target_link_options(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE "-Wl,--allow-shlib-undefined")
	endif()
	# Normalize target filenames for dlopen-friendly plugin names:
	# set OUTPUT_NAME and remove default 'lib' PREFIX so filenames become:
	#   idtech3_opengl_x86_64.so  (instead of libidtech3_opengl_x86_64.so)
	# Use simple names by default, architecture-specific names only when explicitly needed
	set(RENDERER_OUT_OPengl "idtech3_opengl")
	set(RENDERER_OUT_Vulkan "idtech3_vulkan")
	set_target_properties(${RENDERER_PREFIX}_opengl${RENDEXT} PROPERTIES OUTPUT_NAME ${RENDERER_OUT_OPengl} PREFIX "")
	set_target_properties(${RENDERER_PREFIX}_vulkan${RENDEXT} PROPERTIES OUTPUT_NAME ${RENDERER_OUT_Vulkan} PREFIX "")
	# MinGW/PE: without this, renderer DLLs can link but export no symbols (GetRefAPI missing) - breaks dlopen on Windows.
	if(WIN32)
		set_target_properties(${RENDERER_PREFIX}_opengl${RENDEXT} ${RENDERER_PREFIX}_vulkan${RENDEXT}
			PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
	endif()
ELSE()
	IF(USE_VULKAN)
		ADD_LIBRARY(${RENDERER_PREFIX}_vulkan OBJECT ${RENDERER_VK_SRCS} ${RENDERER_COMMON_SRCS})
		TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan PRIVATE RENDERER_VULKAN)
		if(USE_VULKAN_RTX)
		  target_compile_definitions(${RENDERER_PREFIX}_vulkan PRIVATE USE_VULKAN_RTX=1)
		endif()
		add_dependencies(${RENDERER_PREFIX}_vulkan vk_shaders_autogen)
		IF(BUILD_FREETYPE)
			target_link_libraries(${RENDERER_PREFIX}_vulkan PRIVATE idtech3_freetype)
		ENDIF()
	ELSE()
		ADD_LIBRARY(${RENDERER_PREFIX}_opengl OBJECT ${RENDERER_GL_SRCS} ${RENDERER_COMMON_SRCS})
		target_sources(${RENDERER_PREFIX}_opengl PRIVATE src/renderers/opengl/tr_gltf_rb_opengl.c)
		TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_opengl PRIVATE RENDERER_OPENGL)
		IF(BUILD_FREETYPE)
			target_link_libraries(${RENDERER_PREFIX}_opengl PRIVATE idtech3_freetype)
		ENDIF()
	ENDIF()
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
      target_link_libraries(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE ${LIBPNG_TARGET})
      target_link_libraries(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${LIBPNG_TARGET})
      target_compile_definitions(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE USE_LIBPNG USE_ZLIB)
      target_compile_definitions(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_LIBPNG USE_ZLIB)
    else()
      if(USE_VULKAN)
        target_link_libraries(${RENDERER_PREFIX}_vulkan PRIVATE ${LIBPNG_TARGET})
        target_compile_definitions(${RENDERER_PREFIX}_vulkan PRIVATE USE_LIBPNG USE_ZLIB)
      else()
        target_link_libraries(${RENDERER_PREFIX}_opengl PRIVATE ${LIBPNG_TARGET})
        target_compile_definitions(${RENDERER_PREFIX}_opengl PRIVATE USE_LIBPNG USE_ZLIB)
      endif()
    endif()
  endif()
endif()

if(USE_GLM)
  if(USE_RENDERER_DLOPEN)
    target_include_directories(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE ${EXTERNAL_GLM_INCLUDE})
    target_include_directories(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${EXTERNAL_GLM_INCLUDE})
  else()
    if(USE_VULKAN)
      target_include_directories(${RENDERER_PREFIX}_vulkan PRIVATE ${EXTERNAL_GLM_INCLUDE})
    else()
      target_include_directories(${RENDERER_PREFIX}_opengl PRIVATE ${EXTERNAL_GLM_INCLUDE})
    endif()
  endif()
endif()

# EXR image loading via tinyexr -- built as separate static lib to avoid
# duplicate TINYEXR_IMPLEMENTATION symbols when both renderers are loaded
enable_language(CXX)
add_library(exr_loader STATIC src/renderers/common/tr_image_exr.cpp)
target_include_directories(exr_loader PRIVATE ${EXTERNAL_INCLUDE}/tinyexr ${CMAKE_SOURCE_DIR}/src)
target_include_directories(exr_loader PRIVATE ${EXTERNAL_ZLIB_SRC})
set_property(TARGET exr_loader PROPERTY POSITION_INDEPENDENT_CODE ON)
if(NOT MSVC)
  target_compile_options(exr_loader PRIVATE -Wno-old-style-cast -Wno-sign-compare)
endif()
if(TARGET ZLIB::ZLIB)
  target_link_libraries(exr_loader PRIVATE ZLIB::ZLIB)
endif()
if(USE_RENDERER_DLOPEN)
  target_link_libraries(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE exr_loader)
  target_link_libraries(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE exr_loader)
else()
  if(USE_VULKAN)
    target_link_libraries(${RENDERER_PREFIX}_vulkan PRIVATE exr_loader)
  else()
    target_link_libraries(${RENDERER_PREFIX}_opengl PRIVATE exr_loader)
  endif()
endif()

# cgltf include for glTF 2.0 model loading
if(USE_RENDERER_DLOPEN)
  target_include_directories(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${EXTERNAL_INCLUDE}/cgltf)
  target_include_directories(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE ${EXTERNAL_INCLUDE}/cgltf)
else()
  if(USE_VULKAN)
    target_include_directories(${RENDERER_PREFIX}_vulkan PRIVATE ${EXTERNAL_INCLUDE}/cgltf)
  endif()
  if(NOT USE_VULKAN)
    target_include_directories(${RENDERER_PREFIX}_opengl PRIVATE ${EXTERNAL_INCLUDE}/cgltf)
  endif()
endif()

# --- ImGui Inspector (optional, requires C++) ---
option(USE_IMGUI "Enable ImGui inspector overlay" ON)
# Disable on macOS: imgui C++ headers get pulled into C compilation units, causing "unknown type name 'constexpr'" errors.
if(USE_IMGUI AND NOT APPLE)
  enable_language(CXX)
  set(IMGUI_SRCS
    src/renderers/vulkan/inspector/vk_imgui.cpp
    src/renderers/vulkan/inspector/vk_imgui_vulkan.cpp
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
    ${CMAKE_SOURCE_DIR}/src/renderers/common
  )
  if(USE_RENDERER_DLOPEN)
    target_sources(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${IMGUI_SRCS})
    target_include_directories(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${IMGUI_INCLUDE_DIRS})
    target_compile_definitions(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_IMGUI USE_VULKAN)
    if(USE_SDL AND TARGET SDL2::Core)
      target_link_libraries(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE SDL2::Core)
    endif()
  else()
    if(USE_VULKAN)
      target_sources(${RENDERER_PREFIX}_vulkan PRIVATE ${IMGUI_SRCS})
      target_include_directories(${RENDERER_PREFIX}_vulkan PRIVATE ${IMGUI_INCLUDE_DIRS})
      target_compile_definitions(${RENDERER_PREFIX}_vulkan PRIVATE USE_IMGUI USE_VULKAN)
      if(USE_SDL AND TARGET SDL2::Core)
        target_link_libraries(${RENDERER_PREFIX}_vulkan PRIVATE SDL2::Core)
      endif()
    endif()
  endif()
  message(STATUS "ImGui Inspector: enabled")
endif()

# common function, window system
IF(ANDROID)
	SET(Q3_SRCS
		src/platform/unix/unix_shared.c
		src/platform/android/android_stubs.c
		src/platform/android/android_main.c
		src/platform/android/android_asset_bootstrap.c
		src/platform/android/android_logcat.c
		src/platform/android/android_touch_overlay.c)
	SET(Q3_UI_SRCS "")
ELSEIF(UNIX)
	SET (Q3_SRCS
		src/platform/unix/unix_main.c
		src/platform/unix/unix_shared.c
		src/platform/unix/linux_signals.c)

	IF(USE_SDL)
		list(APPEND Q3_UI_SRCS
			src/platform/sdl/sdl_gamma.c
			src/platform/sdl/sdl_glimp.c
			src/platform/sdl/sdl_input.c)
	ELSE()
		SET(Q3_UI_SRCS
			src/platform/unix/linux_glimp.c
			src/platform/unix/linux_qgl.c
			src/platform/unix/linux_snd.c
			src/platform/unix/x11_dga.c
			src/platform/unix/x11_randr.c
			src/platform/unix/x11_vidmode.c)
		IF(USE_VULKAN)
			list(APPEND Q3_UI_SRCS src/platform/unix/linux_qvk.c)
		ENDIF()
	ENDIF()

	IF(APPLE)
		SET(EXE_TYPE MACOSX_BUNDLE)
		list(APPEND Q3_SRCS src/platform/unix/quake3_flat.icns)
		set_source_files_properties(src/platform/unix/quake3_flat.icns PROPERTIES
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
		src/platform/win32/win_main.c
		src/platform/win32/win_shared.c
		src/platform/win32/win_syscon.c
		src/platform/win32/win_resource.rc)
	
	# Set include directory for resource compiler to find icon and manifest files
	if(MINGW)
		set_source_files_properties(src/platform/win32/win_resource.rc PROPERTIES
			COMPILE_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/src/platform/win32")
	endif()

	IF(USE_SDL)
		SET(Q3_UI_SRCS
			src/platform/sdl/sdl_gamma.c
			src/platform/sdl/sdl_glimp.c
			src/platform/sdl/sdl_input.c
			src/platform/win32/win_minimize.c)
	ELSE()
		SET(Q3_UI_SRCS
			src/platform/win32/win_gamma.c
			src/platform/win32/win_glimp.c
			src/platform/win32/win_input.c
			src/platform/win32/win_minimize.c
			src/platform/win32/win_qgl.c
			src/platform/win32/win_wndproc.c
			src/platform/win32/win_snd.c)
	ENDIF()
		IF(USE_VULKAN AND NOT USE_SDL)
			list(APPEND Q3_UI_SRCS src/platform/win32/win_qvk.c)
		ENDIF()
ENDIF()

# Audio backends/effects are built with the client module only.

IF(Q3_UI_SRCS)
ADD_LIBRARY(q3ui OBJECT ${Q3_UI_SRCS})
ENDIF()

if(USE_LOCAL_HEADERS AND NOT IDTECH3_USING_FETCHED_SDL2)
	if(TARGET q3ui)
		target_include_directories(q3ui PRIVATE ${EXTERNAL_SDL_INCLUDE_DIR})
	endif()
	target_include_directories(client PRIVATE ${EXTERNAL_SDL_INCLUDE_DIR})
endif()
IF(USE_SDL AND TARGET q3ui)
	if(SDL2_INCLUDE_DIRS)
		target_include_directories(q3ui PRIVATE ${SDL2_INCLUDE_DIRS})
	elseif(SDL2_INCLUDE_DIR)
		target_include_directories(q3ui PRIVATE ${SDL2_INCLUDE_DIR})
	endif()
ENDIF()
if(USE_LOCAL_HEADERS AND WIN32)
	target_include_directories(client PRIVATE ${EXTERNAL_CURL_WINDOWS_INCLUDE_DIR})
endif()

IF(NOT MSVC)
	IF(USE_SDL)
		if(NOT SDL2_FOUND)
		  message(FATAL_ERROR "SDL2 could not be found. Install SDL2 dev packages, set SDL2_DIR/SDL2_PATH, or enable USE_BUNDLED_SDL2=ON (default) for an automatic source build.")
		endif()
		if(TARGET q3ui)
			TARGET_LINK_LIBRARIES(q3ui SDL2::Main)
		endif()
		TARGET_LINK_LIBRARIES(client PRIVATE SDL2::Core)
		if(SDL2_INCLUDE_DIRS)
			target_include_directories(client PRIVATE ${SDL2_INCLUDE_DIRS})
		elseif(SDL2_INCLUDE_DIR)
			target_include_directories(client PRIVATE ${SDL2_INCLUDE_DIR})
		endif()
	ELSEIF(UNIX AND NOT ANDROID)
		find_package (X11 REQUIRED)
		if(TARGET q3ui)
			TARGET_LINK_LIBRARIES(q3ui ${X11_LIBRARIES})
		endif()
	ENDIF()
ENDIF()

if(TARGET q3ui)
	target_include_directories(q3ui PRIVATE ${CMAKE_SOURCE_DIR}/src/audio)
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

IF(USE_VULKAN)
	if(TARGET q3ui)
		TARGET_COMPILE_DEFINITIONS(q3ui PRIVATE USE_VULKAN_API)
	endif()
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_VULKAN_API)
ENDIF()

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
    src/external/src/recast/Recast/Source/*.cpp
    src/external/src/recast/Detour/Source/*.cpp
    src/external/src/recast/DetourCrowd/Source/*.cpp
    src/external/src/recast/DetourTileCache/Source/*.cpp
  )
  ADD_LIBRARY(recast_nav STATIC ${RECAST_SRCS} src/navigation/nav_recast.cpp src/navigation/nav_bsp_extract.c)
  target_include_directories(recast_nav PRIVATE
    ${CMAKE_SOURCE_DIR}/src/external/src/recast/Recast/Include
    ${CMAKE_SOURCE_DIR}/src/external/src/recast/Detour/Include
    ${CMAKE_SOURCE_DIR}/src/external/src/recast/DetourCrowd/Include
    ${CMAKE_SOURCE_DIR}/src/external/src/recast/DetourTileCache/Include
    ${CMAKE_SOURCE_DIR}/src/navigation
  )
  set_property(TARGET recast_nav PROPERTY POSITION_INDEPENDENT_CODE ON)
  if(NOT MSVC)
    target_compile_options(recast_nav PRIVATE -Wno-unused-parameter -Wno-sign-compare -Wno-cast-qual -Wno-shadow)
  endif()
  message(STATUS "Recast Navigation: enabled")
endif()

# --- ECS (EnTT) module - alternative to gentity ---
enable_language(CXX)
ADD_LIBRARY(ecs_module STATIC src/game/ecs.cpp)
set_target_properties(ecs_module PROPERTIES
  CXX_STANDARD 17
  CXX_STANDARD_REQUIRED ON
)
target_include_directories(ecs_module PRIVATE
  ${EXTERNAL_INCLUDE}/entt/single_include
  ${CMAKE_SOURCE_DIR}/src/game
  ${CMAKE_SOURCE_DIR}/src/qcommon
)
if(NOT MSVC)
  target_compile_options(ecs_module PRIVATE -Wno-unused-parameter)
endif()
message(STATUS "ECS (EnTT): enabled")

# --- Bullet Physics module ---
option(USE_BULLET_PHYSICS "Enable Bullet Physics integration" ON)
set(PHYSICS_SRCS
  src/physics/phys_bullet.c
  src/physics/phys_procedural_anim.c
  src/physics/phys_dmm.c
  src/physics/phys_ik.c
  src/physics/phys_dmm_materials.c
  src/physics/phys_cloth.c
)

set(BULLET_FOUND FALSE)
if(USE_BULLET_PHYSICS AND NOT ANDROID)
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(BULLET_PKG QUIET bullet)
    if(BULLET_PKG_FOUND)
      set(BULLET_FOUND TRUE)
      list(APPEND PHYSICS_SRCS src/physics/phys_bullet_impl.cpp)
      message(STATUS "Bullet Physics: enabled with C++ backend")
    endif()
  endif()
  if(NOT BULLET_FOUND)
    message(STATUS "Bullet Physics: C wrapper only (install libbullet-dev for full backend)")
  endif()
elseif(USE_BULLET_PHYSICS AND ANDROID)
  message(STATUS "Bullet Physics: C wrapper only on Android (no host pkg-config Bullet)")
endif()

ADD_LIBRARY(phys_module OBJECT ${PHYSICS_SRCS})
target_include_directories(phys_module PRIVATE ${CMAKE_SOURCE_DIR}/src/physics)
if(NOT MSVC)
  target_compile_options(phys_module PRIVATE -Wno-unused-parameter)
endif()
if(BULLET_FOUND)
  target_compile_definitions(phys_module PRIVATE USE_BULLET_PHYSICS_IMPL)
  target_include_directories(phys_module PRIVATE ${BULLET_PKG_INCLUDE_DIRS})
  target_link_libraries(phys_module ${BULLET_PKG_LINK_LIBRARIES})
  enable_language(CXX)
endif()

# client executable / shared library

IF(ANDROID)
	ADD_LIBRARY(${CNAME}${BINEXT} SHARED ${Q3_SRCS})
ELSE()
	ADD_EXECUTABLE(${CNAME}${BINEXT} ${EXE_TYPE} ${Q3_SRCS})
ENDIF()
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
if(TARGET Threads::Threads)
  TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE Threads::Threads)
endif()
if(USE_RECAST_NAV AND TARGET recast_nav)
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
	IF(USE_VULKAN)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${RENDERER_PREFIX}_vulkan)
	ELSE()
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${RENDERER_PREFIX}_opengl)
	ENDIF()
ENDIF()

# dedicated server executable (not built on Android)

IF(NOT ANDROID)
# Dedicated server must be a console subsystem on Windows so CI smoke tests can read stdout.
IF(WIN32)
ADD_EXECUTABLE(${DNAME}${BINEXT} ${Q3_SRCS})
ELSE()
ADD_EXECUTABLE(${DNAME}${BINEXT} ${EXE_TYPE} ${Q3_SRCS})
ENDIF()
TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE qcommon_ded botlib)
if(TARGET Threads::Threads)
  TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE Threads::Threads)
endif()
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
if((USE_STEAM OR USE_STEAM_NETWORKING) AND STEAMWORKS_FOUND)
  TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${STEAMWORKS_LIBRARY})
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

# Deterministic Vulkan shader codegen/check tooling.
# Usage:
#   cmake --build <build-dir> --target vk_shader_regen
#   cmake --build <build-dir> --target vk_shader_check
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
	add_custom_target(vk_shader_regen
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/vk_shader_tool.py regen
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		COMMENT "Regenerating Vulkan shader_data.c and shader_binding.c deterministically")

	add_custom_target(vk_shader_check
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/scripts/vk_shader_tool.py check --ref HEAD
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

add_test(NAME test_compile_engine_lto
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_compile_engine_lto.sh ${CMAKE_SOURCE_DIR}/scripts/compile_engine.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_compile_engine_lto PROPERTIES
	LABELS "unit;scripts;validation"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_compile_engine_lto.sh")

add_test(NAME test_vk_vegetation_dispatch_order
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_vk_vegetation_dispatch_order.sh
		${CMAKE_SOURCE_DIR}/src/renderers/vulkan/tr_shade.c
		${CMAKE_SOURCE_DIR}/src/renderers/vulkan/vk_frame_submit.c
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
		${CMAKE_SOURCE_DIR}/src/platform/botlib/be_ai_chat.c
		${CMAKE_SOURCE_DIR}/src/platform/win32/botlib/be_ai_chat.c
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

add_test(NAME test_gltf_opengl_regressions
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_gltf_opengl_regressions.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_gltf_opengl_regressions PROPERTIES
	LABELS "unit;scripts;validation;renderer"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/tests/scripts/test_gltf_opengl_regressions.sh")

add_test(NAME test_botlib_bounded_strings
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/test_botlib_bounded_strings.sh
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(test_botlib_bounded_strings PROPERTIES
	LABELS "unit;scripts;validation")
# --- Unit tests ---
option(BUILD_UNIT_TESTS "Build unit tests" ON)
if(BUILD_UNIT_TESTS)
	add_executable(unit_macros tests/unit/test_macros.c)
	target_include_directories(unit_macros PRIVATE ${CMAKE_SOURCE_DIR}/src)
	add_test(NAME unit_macros COMMAND unit_macros)
	set_tests_properties(unit_macros PROPERTIES LABELS "unit;validation")

	add_executable(unit_qmath tests/unit/test_q_math.c src/qcommon/q_math.c)
	target_include_directories(unit_qmath PRIVATE ${CMAKE_SOURCE_DIR}/src)
	if(NOT MSVC)
		target_link_libraries(unit_qmath PRIVATE m)
	endif()
	add_test(NAME unit_qmath COMMAND unit_qmath)
	set_tests_properties(unit_qmath PROPERTIES LABELS "unit;validation")

	add_executable(unit_surfaceflags tests/unit/test_surfaceflags.c)
	target_include_directories(unit_surfaceflags PRIVATE ${CMAKE_SOURCE_DIR}/src)
	add_test(NAME unit_surfaceflags COMMAND unit_surfaceflags)
	set_tests_properties(unit_surfaceflags PROPERTIES LABELS "unit;validation")

	add_executable(unit_qhelpers
		tests/unit/test_q_helpers.c
		tests/stub_qcommon_min.c
		src/qcommon/q_shared.c
		src/qcommon/q_math.c)
	target_include_directories(unit_qhelpers PRIVATE ${CMAKE_SOURCE_DIR}/src)
	if(NOT MSVC)
		target_link_libraries(unit_qhelpers PRIVATE m)
	endif()
	add_test(NAME unit_qhelpers COMMAND unit_qhelpers)
	set_tests_properties(unit_qhelpers PROPERTIES LABELS "unit;validation")

	add_executable(unit_crc
		tests/unit/test_crc.c
		tests/stub_qcommon_min.c
		src/qcommon/q_shared.c
		src/qcommon/q_math.c)
	target_include_directories(unit_crc PRIVATE ${CMAKE_SOURCE_DIR}/src)
	if(NOT MSVC)
		target_link_libraries(unit_crc PRIVATE m)
	endif()
	add_test(NAME unit_crc COMMAND unit_crc)
	set_tests_properties(unit_crc PROPERTIES LABELS "unit;validation")

	add_executable(unit_pathutil
		tests/unit/test_pathutil.c
		tests/stub_qcommon_min.c
		src/qcommon/q_shared.c
		src/qcommon/q_math.c)
	target_include_directories(unit_pathutil PRIVATE ${CMAKE_SOURCE_DIR}/src)
	if(NOT MSVC)
		target_link_libraries(unit_pathutil PRIVATE m)
	endif()
	add_test(NAME unit_pathutil COMMAND unit_pathutil)
	set_tests_properties(unit_pathutil PROPERTIES LABELS "unit;validation")

	add_executable(unit_msg
		tests/unit/test_msg.c
		tests/stub_qcommon_min.c
		tests/stub_msg_cvar.c
		src/qcommon/msg.c
		src/qcommon/huffman_static.c)
	target_include_directories(unit_msg PRIVATE ${CMAKE_SOURCE_DIR}/src)
	target_compile_definitions(unit_msg PRIVATE DEDICATED)
	if(NOT MSVC)
		target_link_libraries(unit_msg PRIVATE m)
	endif()
	add_test(NAME unit_msg COMMAND unit_msg)
	set_tests_properties(unit_msg PROPERTIES LABELS "unit;validation")

	add_executable(unit_info
		tests/unit/test_info.c
		tests/stub_qcommon_min.c
		src/qcommon/q_shared.c
		src/qcommon/q_math.c)
	target_include_directories(unit_info PRIVATE ${CMAKE_SOURCE_DIR}/src)
	if(NOT MSVC)
		target_link_libraries(unit_info PRIVATE m)
	endif()
	add_test(NAME unit_info COMMAND unit_info)
	set_tests_properties(unit_info PROPERTIES LABELS "unit;validation")

	add_executable(unit_cm_bounds
		tests/unit/test_cm_bounds.c
		src/qcommon/cm_bounds.c
		src/qcommon/q_math.c)
	target_include_directories(unit_cm_bounds PRIVATE ${CMAKE_SOURCE_DIR}/src)
	if(NOT MSVC)
		target_link_libraries(unit_cm_bounds PRIVATE m)
	endif()
	add_test(NAME unit_cm_bounds COMMAND unit_cm_bounds)
	set_tests_properties(unit_cm_bounds PROPERTIES LABELS "unit;validation")

	add_executable(unit_parse
		tests/unit/test_parse.c
		tests/stub_qcommon_min.c
		src/qcommon/q_shared.c
		src/qcommon/q_math.c)
	target_include_directories(unit_parse PRIVATE ${CMAKE_SOURCE_DIR}/src)
	if(NOT MSVC)
		target_link_libraries(unit_parse PRIVATE m)
	endif()
	add_test(NAME unit_parse COMMAND unit_parse)
	set_tests_properties(unit_parse PROPERTIES LABELS "unit;validation")

	add_executable(unit_endian
		tests/unit/test_endian.c
		tests/stub_qcommon_min.c
		src/qcommon/q_shared.c
		src/qcommon/q_math.c)
	target_include_directories(unit_endian PRIVATE ${CMAKE_SOURCE_DIR}/src)
	if(NOT MSVC)
		target_link_libraries(unit_endian PRIVATE m)
	endif()
	add_test(NAME unit_endian COMMAND unit_endian)
	set_tests_properties(unit_endian PROPERTIES LABELS "unit;validation")

	add_executable(unit_vm_native_module
		tests/unit/test_vm_native_module.c
		tests/stub_qcommon_min.c
		src/qcommon/vm_native_module.c
		src/qcommon/q_shared.c
		src/qcommon/q_math.c)
	target_include_directories(unit_vm_native_module PRIVATE ${CMAKE_SOURCE_DIR}/src)
	if(NOT MSVC)
		target_link_libraries(unit_vm_native_module PRIVATE m)
	endif()
	add_test(NAME unit_vm_native_module COMMAND unit_vm_native_module)
	set_tests_properties(unit_vm_native_module PROPERTIES LABELS "unit;validation")
endif()

# Example mod pack (config-only .pk3); optional - see examples/demo_game/README.md
if(BUILD_EXAMPLE_DEMO_GAME AND NOT ANDROID)
	include("${CMAKE_SOURCE_DIR}/examples/demo_game/CMakeLists.txt")
endif()
