CMAKE_MINIMUM_REQUIRED(VERSION 3.24)

PROJECT(idtech3 LANGUAGES C CXX)

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

if(C_STANDARD_STRICT)
  if(MSVC)
    add_compile_options(/W4)
  else()
    set(C_STRICT_WARNINGS
      -Wall
      -Wextra
      -Wpedantic
      -Wshadow
      -Wstrict-prototypes
      -Wmissing-prototypes
      -Wold-style-definition
      -Wformat=2
      -Wundef
      -Wcast-qual
      -Wpointer-arith
      -Wvla
    )
    add_compile_options(${C_STRICT_WARNINGS})
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

if(USE_DUKTAPE)
  set(DUKTAPE_VENDORED_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/external/duktape/duktape.c")
  set(DUKTAPE_VENDORED_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/src/external/duktape/duktape.h")

  if((NOT USE_SYSTEM_DUKTAPE) AND EXISTS "${DUKTAPE_VENDORED_SOURCE}" AND EXISTS "${DUKTAPE_VENDORED_HEADER}")
    add_library(js_runtime_internal STATIC ${DUKTAPE_VENDORED_SOURCE})
    target_include_directories(js_runtime_internal PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/external/duktape")
    set_property(TARGET js_runtime_internal PROPERTY POSITION_INDEPENDENT_CODE ON)
    if(NOT MSVC)
      target_compile_options(js_runtime_internal PRIVATE -fno-strict-aliasing)
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
OPTION(USE_OPENAL "Enable OpenAL audio backend" ON)
OPTION(USE_CURL "" ON)
OPTION(USE_LOCAL_HEADERS "" ON)
OPTION(USE_VULKAN "" ON)
OPTION(USE_MP3 "Enable MP3 codec support" ON)
#
OPTION(USE_ZLIB "Enable zlib support (vendored)" ON)
OPTION(USE_LIBPNG "Enable libpng support (vendored)" ON)
OPTION(USE_GLM "Enable glm headers (vendored)" ON)
OPTION(USE_LIBRSVG "Enable SVG image loading via librsvg/cairo backend" ON)
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
OPTION(SKIP_IDPAK_CHECK "Skip checking for original Quake III pak files (pak0.pk3)" ON)
OPTION(BUILD_FREETYPE "Enable FreeType-powered font rendering / font generation" ON)
OPTION(USE_DTLS "Enable DTLS encryption for game network traffic" OFF)
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

set(HAVE_LIBRSVG FALSE)
if(USE_LIBRSVG)
  find_package(PkgConfig QUIET)
  if(PKG_CONFIG_FOUND)
    pkg_check_modules(LIBRSVG QUIET IMPORTED_TARGET librsvg-2.0)
    pkg_check_modules(CAIRO QUIET IMPORTED_TARGET cairo)
    if(LIBRSVG_FOUND AND CAIRO_FOUND)
      set(HAVE_LIBRSVG TRUE)
      message(STATUS "SVG loader: enabled (librsvg + cairo)")
    else()
      message(STATUS "SVG loader: disabled (install librsvg2-dev and libcairo2-dev)")
    endif()
  else()
    message(STATUS "SVG loader: disabled (pkg-config not available)")
  endif()
endif()

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
      target_compile_definitions(zlibstatic PRIVATE _LFS64_LARGEFILE=1 _LARGEFILE64_SOURCE=1 Z_LARGE64=1)
      set_property(TARGET zlibstatic PROPERTY POSITION_INDEPENDENT_CODE ON)
    endif()
    if(TARGET zlib)
      target_compile_definitions(zlib PRIVATE ZLIB_CONST)
      target_compile_definitions(zlib PRIVATE _LFS64_LARGEFILE=1 _LARGEFILE64_SOURCE=1 Z_LARGE64=1)
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
  find_package(Freetype REQUIRED)
  add_library(idtech3_freetype INTERFACE)
  target_compile_definitions(idtech3_freetype INTERFACE BUILD_FREETYPE)
  target_include_directories(idtech3_freetype INTERFACE ${FREETYPE_INCLUDE_DIRS})
  target_link_libraries(idtech3_freetype INTERFACE ${FREETYPE_LIBRARIES})
endif()

AUX_SOURCE_DIRECTORY(src/qcommon QCOMMON_SRCS)
# exclude platform-dependent QVM JIT compilers; add the active backend below
list(FILTER QCOMMON_SRCS EXCLUDE REGEX ".*vm_(aarch64|armv7l|powerpc|x86)\\.c")
# exclude optional network modules; add when options enabled
list(FILTER QCOMMON_SRCS EXCLUDE REGEX ".*net_dtls\\.c")
list(FILTER QCOMMON_SRCS EXCLUDE REGEX ".*net_sdr\\.c")

# USE_DTLS: DTLS encryption (requires OpenSSL)
set(USE_DTLS_FOUND FALSE)
if(USE_DTLS)
  find_package(OpenSSL QUIET)
  if(OpenSSL_FOUND)
    list(APPEND QCOMMON_SRCS src/qcommon/net_dtls.c)
    set(USE_DTLS_FOUND TRUE)
    message(STATUS "DTLS: enabled (OpenSSL)")
  else()
    message(WARNING "USE_DTLS requested but OpenSSL not found; DTLS disabled")
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

# add platform-dependent QVM bytecode compilers
IF(CMAKE_SYSTEM_PROCESSOR MATCHES AMD64|x86|i*86)
	IF(CMAKE_SYSTEM_PROCESSOR MATCHES AMD64)
		IF(WIN32)
			SET(BINEXT .x64)
		ELSE()
			SET(BINEXT .x86_64)
		ENDIF()
		SET(RENDEXT _x86_64)
	ELSE()
		#SET(BINEXT .x86)
		SET(RENDEXT _x86)
	ENDIF()
	list(APPEND QCOMMON_SRCS src/qcommon/vm_x86.c)
ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES aarch64|arm64)
	SET(BINEXT .aarch64)
	SET(RENDEXT _aarch64)
	list(APPEND QCOMMON_SRCS src/qcommon/vm_aarch64.c)
ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES arm*)
	SET(BINEXT .arm)
	SET(RENDEXT _arm)
	list(APPEND QCOMMON_SRCS src/qcommon/vm_armv7l.c)
	ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64le|ppc64")
		IF(CMAKE_SYSTEM_PROCESSOR MATCHES "ppc64le")
			SET(BINEXT .ppc64le)
			SET(RENDEXT _ppc64le)
			list(APPEND QCOMMON_SRCS src/qcommon/vm_powerpc.c)
		ELSE()
			SET(BINEXT .ppc64)
			SET(RENDEXT _ppc64)
			list(APPEND QCOMMON_SRCS src/qcommon/vm_powerpc.c)
		ENDIF()
ELSE()
	TARGET_COMPILE_DEFINITIONS(qcommon PRIVATE -DNO_VM_COMPILED)
	TARGET_COMPILE_DEFINITIONS(qcommon_ded PRIVATE -DNO_VM_COMPILED)
ENDIF()

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
if(USE_DUKTAPE AND DUKTAPE_FOUND)
  target_compile_definitions(qcommon PRIVATE USE_DUKTAPE)
  target_include_directories(qcommon PRIVATE ${DUKTAPE_INCLUDE_DIR})
endif()
if(USE_DTLS_FOUND)
  target_compile_definitions(qcommon PRIVATE USE_DTLS=1)
  target_include_directories(qcommon PRIVATE ${OPENSSL_INCLUDE_DIR})
  target_link_libraries(qcommon PRIVATE OpenSSL::SSL OpenSSL::Crypto)
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
if(USE_DUKTAPE AND DUKTAPE_FOUND)
  target_compile_definitions(qcommon_ded PRIVATE USE_DUKTAPE)
  target_include_directories(qcommon_ded PRIVATE ${DUKTAPE_INCLUDE_DIR})
endif()
if(USE_DTLS_FOUND)
  target_compile_definitions(qcommon_ded PRIVATE USE_DTLS=1)
  target_include_directories(qcommon_ded PRIVATE ${OPENSSL_INCLUDE_DIR})
  target_link_libraries(qcommon_ded PRIVATE OpenSSL::SSL OpenSSL::Crypto)
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

IF(NO_VM_COMPILED)
	TARGET_COMPILE_DEFINITIONS(qcommon PRIVATE NO_VM_COMPILED)
	TARGET_COMPILE_DEFINITIONS(qcommon_ded PRIVATE NO_VM_COMPILED)
ENDIF()

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
IF(NOT USE_CURL)
	list(REMOVE_ITEM CLIENT_SRCS src/client/cl_curl.c)
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
    pkg_check_modules(FFMPEG_PKG QUIET libavcodec libavformat libavutil libswscale)
    if(FFMPEG_PKG_FOUND)
      set(FFMPEG_FOUND TRUE)
      message(STATUS "FFmpeg video codec: enabled (${FFMPEG_PKG_LINK_LIBRARIES})")
    endif()
  endif()
  if(NOT FFMPEG_FOUND)
    message(WARNING "FFmpeg requested but pkg-config could not find libavcodec/libavformat/libavutil/libswscale; FFmpeg codec disabled.")
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
# --- End modern video codec support ---

#ADD_LIBRARY(client OBJECT ${CLIENT_SRCS})

# jpeg
IF(USE_SYSTEM_JPEG)
	ADD_LIBRARY(client OBJECT ${CLIENT_SRCS})
	find_package(JPEG REQUIRED)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE -DUSE_SYSTEM_JPEG)
	if(USE_FLUX)
		TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_FLUX)
		target_include_directories(client PRIVATE ${EXTERNAL_ROOT}/src/cflux2)
	endif()
	if(USE_OPUS)
		target_compile_definitions(client PRIVATE INCLUDE_INTTYPES_H=1 INCLUDE_SYS_TYPES_H=1)
	endif()
	if(USE_GLM)
		target_include_directories(client PRIVATE ${EXTERNAL_GLM_INCLUDE})
	endif()
	TARGET_LINK_LIBRARIES(client ${JPEG_LIBRARIES})
ELSE()
	AUX_SOURCE_DIRECTORY(${EXTERNAL_JPEG_SRC_DIR} JPEG_SRCS)
	#ADD_LIBRARY(jpeg ${JPEG_SRCS})
	#TARGET_LINK_LIBRARIES(client jpeg)
	ADD_LIBRARY(client OBJECT ${CLIENT_SRCS} ${JPEG_SRCS})
	if(USE_FLUX)
		TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_FLUX)
		target_include_directories(client PRIVATE ${EXTERNAL_ROOT}/src/cflux2)
	endif()
	# Ensure client can find qcommon headers needed by some client-side modules (eg. mp3)
	target_include_directories(client PRIVATE ${CMAKE_SOURCE_DIR}/src/qcommon ${EXTERNAL_JPEG_SRC_DIR})
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
ENDIF()

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
        COMPILE_OPTIONS "-Wno-shadow"
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
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_RENDERER_DLOPEN RENDERER_PREFIX="${RENDERER_PREFIX}" RENDERER_DEFAULT=${RENDERER_DEFAULT})

	ADD_LIBRARY(${RENDERER_PREFIX}_opengl${RENDEXT} SHARED ${RENDERER_GL_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
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
	if(HAVE_LIBRSVG)
		target_compile_definitions(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE USE_LIBRSVG)
		target_compile_definitions(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_LIBRSVG)
		target_link_libraries(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE PkgConfig::LIBRSVG PkgConfig::CAIRO)
		target_link_libraries(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE PkgConfig::LIBRSVG PkgConfig::CAIRO)
	endif()
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
		if(HAVE_LIBRSVG)
			target_compile_definitions(${RENDERER_PREFIX}_vulkan PRIVATE USE_LIBRSVG)
			target_link_libraries(${RENDERER_PREFIX}_vulkan PRIVATE PkgConfig::LIBRSVG PkgConfig::CAIRO)
		endif()
	ELSE()
		ADD_LIBRARY(${RENDERER_PREFIX}_opengl OBJECT ${RENDERER_GL_SRCS} ${RENDERER_COMMON_SRCS})
		TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_opengl PRIVATE RENDERER_OPENGL)
		IF(BUILD_FREETYPE)
			target_link_libraries(${RENDERER_PREFIX}_opengl PRIVATE idtech3_freetype)
		ENDIF()
		if(HAVE_LIBRSVG)
			target_compile_definitions(${RENDERER_PREFIX}_opengl PRIVATE USE_LIBRSVG)
			target_link_libraries(${RENDERER_PREFIX}_opengl PRIVATE PkgConfig::LIBRSVG PkgConfig::CAIRO)
		endif()
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
else()
  if(USE_VULKAN)
    target_include_directories(${RENDERER_PREFIX}_vulkan PRIVATE ${EXTERNAL_INCLUDE}/cgltf)
  endif()
endif()

# --- ImGui Inspector (optional, requires C++) ---
option(USE_IMGUI "Enable ImGui inspector overlay" ON)
if(USE_IMGUI)
  enable_language(CXX)
  set(IMGUI_SRCS
    src/renderers/vulkan/inspector/vk_imgui.cpp
  )
  set(IMGUI_INCLUDE_DIRS
    ${EXTERNAL_SRC}/cimgui/imgui
    ${EXTERNAL_SRC}/cimgui/imgui/backends
    ${EXTERNAL_SRC}/cimgui
    ${CMAKE_SOURCE_DIR}/src/renderers/common
  )
  if(USE_RENDERER_DLOPEN)
    target_sources(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${IMGUI_SRCS})
    target_include_directories(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${IMGUI_INCLUDE_DIRS})
    target_compile_definitions(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_IMGUI)
  else()
    if(USE_VULKAN)
      target_sources(${RENDERER_PREFIX}_vulkan PRIVATE ${IMGUI_SRCS})
      target_include_directories(${RENDERER_PREFIX}_vulkan PRIVATE ${IMGUI_INCLUDE_DIRS})
      target_compile_definitions(${RENDERER_PREFIX}_vulkan PRIVATE USE_IMGUI)
    endif()
  endif()
  message(STATUS "ImGui Inspector: enabled")
endif()

# common function, window system
IF(ANDROID)
	SET(Q3_SRCS
		src/platform/unix/unix_shared.c
		src/platform/android/android_stubs.c
		src/platform/android/android_main.c)
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
	ENDIF(UNIX)

# Audio backends/effects are built with the client module only.

IF(Q3_UI_SRCS)
ADD_LIBRARY(q3ui OBJECT ${Q3_UI_SRCS})
ENDIF()

if(USE_LOCAL_HEADERS)
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
		  message(FATAL_ERROR "SDL2 could not be found. Please install or set SDL2_DIR/SDL2_PATH.")
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
if(USE_BULLET_PHYSICS)
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
ADD_EXECUTABLE(${DNAME}${BINEXT} ${EXE_TYPE} ${Q3_SRCS})
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
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/release/idtech3_server")

add_test(NAME check_artifacts
	COMMAND ${CMAKE_SOURCE_DIR}/tests/scripts/check_artifacts.sh ${CMAKE_SOURCE_DIR}/release
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
set_tests_properties(check_artifacts PROPERTIES
	LABELS "validation;artifacts"
	REQUIRED_FILES "${CMAKE_SOURCE_DIR}/release/idtech3_server")
