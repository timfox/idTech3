CMAKE_MINIMUM_REQUIRED(VERSION 3.24)

PROJECT(idtech3 LANGUAGES C)

set(CMAKE_C_STANDARD 23)
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
    add_compile_options(/WX)
  else()
    add_compile_options(-Werror)
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

SET(CNAME "idtech3")
SET(DNAME "idtech3_server")

SET(RENDERER_PREFIX ${CNAME})
SET(RENDERER_DEFAULT vulkan) # valid options: opengl, vulkan

OPTION(USE_SDL "" ON)
OPTION(USE_OPENAL "Enable OpenAL audio backend" ON)
OPTION(USE_CURL "" ON)
OPTION(USE_LOCAL_HEADERS "" ON)
OPTION(USE_VULKAN "" ON)
OPTION(USE_MP3 "Enable MP3 codec support" ON)
OPTION(USE_SYSTEM_JPEG "" OFF)
OPTION(USE_RENDERER_DLOPEN "" ON)
OPTION(SKIP_IDPAK_CHECK "Skip checking for original Quake III pak files (pak0.pk3)" ON)
OPTION(BUILD_FREETYPE "Enable FreeType-powered font rendering / font generation" ON)

SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules)

set(EXTERNAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/external")
set(EXTERNAL_SRC "${EXTERNAL_ROOT}/src")
set(EXTERNAL_INCLUDE "${EXTERNAL_ROOT}/include")
set(EXTERNAL_SDL_INCLUDE_DIR "${EXTERNAL_SRC}/sdl/include/SDL2")
set(EXTERNAL_CURL_WINDOWS_INCLUDE_DIR "${EXTERNAL_SRC}/curl/windows/include")
set(EXTERNAL_JPEG_SRC_DIR "${EXTERNAL_SRC}/jpeg")

# Add cflux2 external library
if(USE_FLUX AND (NOT WIN32 OR CYGWIN))
	add_subdirectory(src/external/src/cflux2)
elseif(USE_FLUX)
	message(STATUS "Skipping cflux2 build on Windows (sys/mman.h missing)")
endif()

if(BUILD_FREETYPE)
  find_package(Freetype REQUIRED)
  add_library(idtech3_freetype INTERFACE)
  target_compile_definitions(idtech3_freetype INTERFACE BUILD_FREETYPE)
  target_include_directories(idtech3_freetype INTERFACE ${FREETYPE_INCLUDE_DIRS})
  target_link_libraries(idtech3_freetype INTERFACE ${FREETYPE_LIBRARIES})
endif()

AUX_SOURCE_DIRECTORY(src/qcommon QCOMMON_SRCS)
# exclude platform-dependent QVM bytecode compilers
list(FILTER QCOMMON_SRCS EXCLUDE REGEX ".*vm_[alx].*.c")
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

AUX_SOURCE_DIRECTORY(src/server SERVER_SRCS)
list(REMOVE_ITEM SERVER_SRCS src/server/sv_rankings.c)

# universal binary
ADD_LIBRARY(qcommon OBJECT ${QCOMMON_SRCS} ${SERVER_SRCS})

# dedicated server
ADD_LIBRARY(qcommon_ded OBJECT ${QCOMMON_SRCS} ${SERVER_SRCS})
TARGET_COMPILE_DEFINITIONS(qcommon_ded PUBLIC DEDICATED)

# Apply SKIP_IDPAK_CHECK to qcommon targets that compile files.c
if(SKIP_IDPAK_CHECK)
  TARGET_COMPILE_DEFINITIONS(qcommon PRIVATE SKIP_IDPAK_CHECK=1)
  TARGET_COMPILE_DEFINITIONS(qcommon_ded PRIVATE SKIP_IDPAK_CHECK=1)
endif()

# client + cURL
# client sources
AUX_SOURCE_DIRECTORY(src/client CLIENT_SRCS)
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
ENDIF()
IF(USE_MP3)
	AUX_SOURCE_DIRECTORY(src/client/mp3 MP3_SRCS)
	list(APPEND CLIENT_SRCS ${MP3_SRCS})
ENDIF()

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

IF(USE_RENDERER_DLOPEN)
	SET(AUX_SRCS
		src/qcommon/q_shared.c
		src/qcommon/puff.c
		src/qcommon/q_math.c
	)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_RENDERER_DLOPEN RENDERER_PREFIX="${RENDERER_PREFIX}" RENDERER_DEFAULT="${RENDERER_DEFAULT}")

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
	# Ensure shared renderer libraries link to the math and dl libs so symbol versions
	# (eg. powf@GLIBC_2.2.5) are resolved against the dynamic libm rather than a
	# static libm which would break building shared objects.
	target_link_libraries(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE m ${CMAKE_DL_LIBS})
	target_link_libraries(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE m ${CMAKE_DL_LIBS})
	# Normalize target filenames for dlopen-friendly plugin names:
	# set OUTPUT_NAME and remove default 'lib' PREFIX so filenames become:
	#   idtech3_opengl_x86_64.so  (instead of libidtech3_opengl_x86_64.so)
	if(TARGET_ARCH_NAME)
	  set(RENDERER_OUT_OPengl "idtech3_opengl_${TARGET_ARCH_NAME}")
	  set(RENDERER_OUT_Vulkan "idtech3_vulkan_${TARGET_ARCH_NAME}")
	else()
	  set(RENDERER_OUT_OPengl "idtech3_opengl${RENDEXT}")
	  set(RENDERER_OUT_Vulkan "idtech3_vulkan${RENDEXT}")
	endif()
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

# common function, window system
IF(UNIX)
	SET (Q3_SRCS
		src/unix/unix_main.c
		src/unix/unix_shared.c
		src/unix/linux_signals.c)

	IF(USE_SDL)
		AUX_SOURCE_DIRECTORY(src/audio/backends/sdl Q3_UI_SRCS)
		list(APPEND Q3_UI_SRCS
			src/platform/sdl/sdl_gamma.c
			src/platform/sdl/sdl_glimp.c
			src/platform/sdl/sdl_input.c)
	ELSE()
		SET(Q3_UI_SRCS
			src/unix/linux_glimp.c
			src/unix/linux_qgl.c
			src/unix/linux_snd.c
			src/unix/x11_dga.c
			src/unix/x11_randr.c
			src/unix/x11_vidmode.c)
		IF(USE_VULKAN)
			list(APPEND Q3_UI_SRCS src/unix/linux_qvk.c)
		ENDIF()
	ENDIF()

	IF(APPLE)
		SET(EXE_TYPE MACOSX_BUNDLE)
		list(APPEND Q3_SRCS src/unix/quake3_flat.icns)
		set_source_files_properties(src/unix/quake3_flat.icns PROPERTIES
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
		src/win32/win_main.c
		src/win32/win_shared.c
		src/win32/win_syscon.c
		src/win32/win_resource.rc)
	
	# Set include directory for resource compiler to find icon and manifest files
	if(MINGW)
		set_source_files_properties(src/win32/win_resource.rc PROPERTIES
			COMPILE_FLAGS "-I${CMAKE_CURRENT_SOURCE_DIR}/src/win32")
	endif()

	SET(Q3_UI_SRCS
		src/win32/win_gamma.c
		src/win32/win_glimp.c
		src/win32/win_input.c
		src/win32/win_minimize.c
		src/win32/win_qgl.c
		src/win32/win_snd.c
		src/win32/win_wndproc.c)
	IF(USE_VULKAN)
		list(APPEND Q3_UI_SRCS src/win32/win_qvk.c)
	ENDIF()
ENDIF(UNIX)

IF(USE_OPENAL)
	list(APPEND Q3_UI_SRCS
		src/audio/backends/openal/openal_snd.c
		src/audio/acoustics/s_acoustics.c
	)
ENDIF()

ADD_LIBRARY(q3ui OBJECT ${Q3_UI_SRCS})

if(USE_LOCAL_HEADERS)
	target_include_directories(q3ui PRIVATE ${EXTERNAL_SDL_INCLUDE_DIR})
endif()
if(USE_LOCAL_HEADERS AND WIN32)
	target_include_directories(client PRIVATE ${EXTERNAL_CURL_WINDOWS_INCLUDE_DIR})
endif()

IF(NOT MSVC)
	IF(USE_SDL)
		if(NOT SDL2_FOUND)
		  message(FATAL_ERROR "SDL2 could not be found. Please install or set SDL2_DIR/SDL2_PATH.")
		endif()
		TARGET_LINK_LIBRARIES(q3ui SDL2::Main)
	ELSEIF(UNIX)
		find_package (X11 REQUIRED)
		TARGET_LINK_LIBRARIES(q3ui ${X11_LIBRARIES})
	ENDIF()
ENDIF()

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
TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} qcommon botlib client q3ui)
if(USE_FLUX)
	TARGET_COMPILE_DEFINITIONS(${CNAME}${BINEXT} PRIVATE USE_FLUX)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} cflux2)
endif()
# Link OpenAL only when enabled
IF(USE_OPENAL)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} OpenAL::OpenAL)
ENDIF()
#TARGET_LINK_OPTIONS(${CNAME} PRIVATE -fno-pie) # v3.13+

if(UNIX AND NOT MSVC AND NOT APPLE)
  # Ensure main executable exports symbols so dlopen'd renderer plugins can resolve engine symbols.
  # --export-dynamic is a GNU ld option and should not be used on macOS (Darwin) where the linker rejects it.
  target_link_options(${CNAME}${BINEXT} PRIVATE "-Wl,--export-dynamic")
endif()

IF(NOT USE_RENDERER_DLOPEN)
	IF(USE_VULKAN)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${RENDERER_PREFIX}_vulkan)
	ELSE()
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${RENDERER_PREFIX}_opengl)
	ENDIF()
ENDIF()

# dedicated server executable

ADD_EXECUTABLE(${DNAME}${BINEXT} ${EXE_TYPE} ${Q3_SRCS})
TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} qcommon_ded botlib)
#TARGET_LINK_OPTIONS(${DNAME} PRIVATE -fno-pie) # v3.13+

if(UNIX AND NOT MSVC AND NOT APPLE)
  target_link_options(${DNAME}${BINEXT} PRIVATE "-Wl,--export-dynamic")
endif()

IF(WIN32)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} winmm comctl32 ws2_32)
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} winmm comctl32 ws2_32)
ELSE()
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} m ${CMAKE_DL_LIBS})
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} m ${CMAKE_DL_LIBS})
ENDIF()