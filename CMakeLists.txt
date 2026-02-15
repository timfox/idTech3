CMAKE_MINIMUM_REQUIRED(VERSION 3.24)

PROJECT(idtech3 LANGUAGES C)

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
          -Wno-stringop-overflow
          -Wno-sign-compare
        )
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
          -Wno-maybe-uninitialized
        )
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

SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules)

set(EXTERNAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/external")
set(EXTERNAL_SRC "${EXTERNAL_ROOT}/src")
set(EXTERNAL_INCLUDE "${EXTERNAL_ROOT}/include")
set(EXTERNAL_SDL_INCLUDE_DIR "${EXTERNAL_SRC}/sdl/include/SDL2")
set(EXTERNAL_CURL_WINDOWS_INCLUDE_DIR "${EXTERNAL_SRC}/curl/windows/include")
set(EXTERNAL_JPEG_SRC_DIR "${EXTERNAL_SRC}/jpeg")
set(EXTERNAL_ZLIB_SRC "${EXTERNAL_SRC}/zlib")
set(EXTERNAL_LIBPNG_SRC "${EXTERNAL_SRC}/libpng")
set(EXTERNAL_GLM_INCLUDE "${EXTERNAL_SRC}/glm")

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
list(REMOVE_ITEM SERVER_SRCS src/server/sv_rankings.c)

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
  src/audio/backends/snd_backend_openal.c
  src/audio/backends/snd_backend_null.c
  src/audio/effects/snd_efx.c
)
if(USE_SDL)
  list(APPEND CLIENT_SRCS src/audio/backends/snd_backend_sdl.c)
endif()
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
	src/renderers/rendercommon/tr_font_stub.c  # placeholder, will be replaced below
	src/renderers/rendercommon/tr_image_bmp.c
	src/renderers/rendercommon/tr_image_jpg.c
	src/renderers/rendercommon/tr_image_pcx.c
	src/renderers/rendercommon/tr_image_png.c
	src/renderers/rendercommon/tr_image_tga.c
	src/renderers/rendercommon/tr_noise.c
)
IF(BUILD_FREETYPE)
	list(REMOVE_ITEM RENDERER_COMMON_SRCS src/renderers/rendercommon/tr_font_stub.c)
	list(APPEND RENDERER_COMMON_SRCS src/renderers/rendercommon/tr_font.c)
ELSE()
	# Keep tr_font_stub.c
ENDIF()
AUX_SOURCE_DIRECTORY(src/renderers/openglrenderer RENDERER_GL_SRCS)
AUX_SOURCE_DIRECTORY(src/renderers/vulkanrenderer RENDERER_VK_SRCS)
list(APPEND RENDERER_VK_SRCS
    src/renderers/vulkanrenderer/utils/mikktspace/mikktspace.c
)
# Suppress shadow warnings for MikkTSpace third-party code
if(MSVC)
    set_source_files_properties(src/renderers/vulkanrenderer/utils/mikktspace/mikktspace.c PROPERTIES
        COMPILE_OPTIONS "/wd4456;/wd4457;/wd4458;/wd4459"
    )
else()
    set_source_files_properties(src/renderers/vulkanrenderer/utils/mikktspace/mikktspace.c PROPERTIES
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
ELSE()
	IF(USE_VULKAN)
		ADD_LIBRARY(${RENDERER_PREFIX}_vulkan OBJECT ${RENDERER_VK_SRCS} ${RENDERER_COMMON_SRCS})
		TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan PRIVATE RENDERER_VULKAN)
		IF(BUILD_FREETYPE)
			target_link_libraries(${RENDERER_PREFIX}_vulkan PRIVATE idtech3_freetype)
		ENDIF()
	ELSE()
		ADD_LIBRARY(${RENDERER_PREFIX}_opengl OBJECT ${RENDERER_GL_SRCS} ${RENDERER_COMMON_SRCS})
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

# common function, window system
IF(UNIX)
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
	IF(USE_VULKAN)
		list(APPEND Q3_UI_SRCS src/platform/win32/win_qvk.c)
	ENDIF()
ENDIF(UNIX)

# Audio backends/effects are built with the client module only.

ADD_LIBRARY(q3ui OBJECT ${Q3_UI_SRCS})

if(USE_LOCAL_HEADERS)
	target_include_directories(q3ui PRIVATE ${EXTERNAL_SDL_INCLUDE_DIR})
	target_include_directories(client PRIVATE ${EXTERNAL_SDL_INCLUDE_DIR})
endif()
IF(USE_SDL)
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
		TARGET_LINK_LIBRARIES(q3ui SDL2::Main)
		TARGET_LINK_LIBRARIES(client PRIVATE SDL2::Core)
		if(SDL2_INCLUDE_DIRS)
			target_include_directories(client PRIVATE ${SDL2_INCLUDE_DIRS})
		elseif(SDL2_INCLUDE_DIR)
			target_include_directories(client PRIVATE ${SDL2_INCLUDE_DIR})
		endif()
	ELSEIF(UNIX)
		find_package (X11 REQUIRED)
		TARGET_LINK_LIBRARIES(q3ui ${X11_LIBRARIES})
	ENDIF()
ENDIF()

target_include_directories(q3ui PRIVATE ${CMAKE_SOURCE_DIR}/src/audio)

IF(USE_OPENAL)
	if(NOT OPENAL_FOUND)
	  message(FATAL_ERROR "OpenAL could not be found. Please install or set OPENAL paths.")
	endif()
	TARGET_COMPILE_DEFINITIONS(q3ui PRIVATE USE_OPENAL)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_OPENAL)
	TARGET_COMPILE_DEFINITIONS(q3ui PRIVATE USE_OPENAL_EFX)
	target_include_directories(q3ui PRIVATE ${OPENAL_INCLUDE_DIR})
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
	TARGET_COMPILE_DEFINITIONS(q3ui PRIVATE USE_VULKAN_API)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_VULKAN_API)
ENDIF()

IF(USE_MP3)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_MP3)
ENDIF()

# Ensure GNU-style --export-dynamic is not present in global linker flags on macOS
if(APPLE)
  if(DEFINED CMAKE_EXE_LINKER_FLAGS)
    string(REPLACE "-Wl,--export-dynamic" "" CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS}")
  endif()
  if(DEFINED CMAKE_SHARED_LINKER_FLAGS)
    string(REPLACE "-Wl,--export-dynamic" "" CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
  endif()
endif()

# client executable

ADD_EXECUTABLE(${CNAME}${BINEXT} ${EXE_TYPE} ${Q3_SRCS})
TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE qcommon botlib client q3ui)
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

# dedicated server executable

ADD_EXECUTABLE(${DNAME}${BINEXT} ${EXE_TYPE} ${Q3_SRCS})
TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE qcommon_ded botlib)
#TARGET_LINK_OPTIONS(${DNAME} PRIVATE -fno-pie) # v3.13+

if(UNIX AND NOT MSVC AND NOT APPLE)
  target_link_options(${DNAME}${BINEXT} PRIVATE "-Wl,--export-dynamic")
endif()

IF(WIN32)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE winmm comctl32 ws2_32)
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE winmm comctl32 ws2_32)
ELSE()
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE m ${CMAKE_DL_LIBS})
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE m ${CMAKE_DL_LIBS})
ENDIF()
