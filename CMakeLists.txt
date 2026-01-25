CMAKE_MINIMUM_REQUIRED(VERSION 3.24)

PROJECT(idtech3)

SET(CNAME "idtech3")
SET(DNAME "idtech3_server")

SET(RENDERER_PREFIX ${CNAME})
SET(RENDERER_DEFAULT vulkan) # valid options: opengl, vulkan, opengl2

OPTION(USE_SDL "" ON)
OPTION(USE_CURL "" ON)
OPTION(USE_LOCAL_HEADERS "" ON)
OPTION(USE_VULKAN "" ON)
OPTION(USE_MP3 "Enable MP3 codec support" ON)
OPTION(USE_SYSTEM_JPEG "" OFF)
OPTION(USE_RENDERER_DLOPEN "" ON)
OPTION(SKIP_IDPAK_CHECK "Skip checking for original Quake III pak files (pak0.pk3)" OFF)

SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules)

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
  file(GLOB ASM_SRCS "src/asm/*.S" "src/asm/*.s")
  list(APPEND CLIENT_SRCS ${ASM_SRCS})
  # Ensure CMake knows how to compile assembly sources
  enable_language(ASM)
  if(ASM_SRCS)
    set_source_files_properties(${ASM_SRCS} PROPERTIES LANGUAGE ASM)
    # run assembler sources through the C preprocessor and use Intel syntax
    # Note: -masm=intel is GCC-specific; Clang uses Intel syntax by default for .s files
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
      set_source_files_properties(${ASM_SRCS} PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp -masm=intel")
    else()
      set_source_files_properties(${ASM_SRCS} PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp")
    endif()
  endif()
  # Define USE_ASM to tell the code that assembly optimizations are available
  ADD_COMPILE_DEFINITIONS(USE_ASM)
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
	TARGET_LINK_LIBRARIES(client ${JPEG_LIBRARIES})
ELSE()
	AUX_SOURCE_DIRECTORY(src/libjpeg JPEG_SRCS)
	#ADD_LIBRARY(jpeg ${JPEG_SRCS})
	#TARGET_LINK_LIBRARIES(client jpeg)
	ADD_LIBRARY(client OBJECT ${CLIENT_SRCS} ${JPEG_SRCS})
	# Ensure client can find qcommon headers needed by some client-side modules (eg. mp3)
	target_include_directories(client PRIVATE ${CMAKE_SOURCE_DIR}/src/qcommon)
ENDIF()

# renderers
AUX_SOURCE_DIRECTORY(src/renderercommon RENDERER_COMMON_SRCS)
AUX_SOURCE_DIRECTORY(src/renderer RENDERER_GL_SRCS)
AUX_SOURCE_DIRECTORY(src/renderervk RENDERER_VK_SRCS)

IF(USE_RENDERER_DLOPEN)
	SET(AUX_SRCS
		src/qcommon/q_shared.c
		src/qcommon/puff.c
		src/qcommon/q_math.c
	)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_RENDERER_DLOPEN RENDERER_PREFIX="${RENDERER_PREFIX}" RENDERER_DEFAULT="${RENDERER_DEFAULT}")

	ADD_LIBRARY(${RENDERER_PREFIX}_opengl${RENDEXT} SHARED ${RENDERER_GL_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
	TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE USE_RENDERER_DLOPEN)

	ADD_LIBRARY(${RENDERER_PREFIX}_vulkan${RENDEXT} SHARED ${RENDERER_VK_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
	TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_RENDERER_DLOPEN)
	# Ensure shared renderer libraries link to the math and dl libs so symbol versions
	# (eg. powf@GLIBC_2.2.5) are resolved against the dynamic libm rather than a
	# static libm which would break building shared objects.
	TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_opengl${RENDEXT} m ${CMAKE_DL_LIBS})
	TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_vulkan${RENDEXT} m ${CMAKE_DL_LIBS})
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
	ELSE()
		ADD_LIBRARY(${RENDERER_PREFIX}_opengl OBJECT ${RENDERER_GL_SRCS} ${RENDERER_COMMON_SRCS})
	ENDIF()
ENDIF()

# common function, window system
IF(UNIX)
	SET (Q3_SRCS
		src/unix/unix_main.c
		src/unix/unix_shared.c
		src/unix/linux_signals.c)

	IF(USE_SDL)
		AUX_SOURCE_DIRECTORY(src/sdl Q3_UI_SRCS)
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

ADD_LIBRARY(q3ui OBJECT ${Q3_UI_SRCS})

IF(NOT MSVC)
	IF(USE_SDL)
		find_package(SDL2 REQUIRED)
		# Ensure compatibility: if the FindSDL2 module didn't create SDL2::Core/SDL2::Main/SDL2::SDL2
		# targets (varies between systems / config vs module mode), synthesize lightweight IMPORTED targets
		if(NOT TARGET SDL2::Core)
			if(DEFINED SDL2_LIBRARY AND SDL2_LIBRARY)
				add_library(SDL2::Core UNKNOWN IMPORTED)
				set_property(TARGET SDL2::Core PROPERTY IMPORTED_LOCATION "${SDL2_LIBRARY}")
				if(DEFINED SDL2_INCLUDE_DIR AND SDL2_INCLUDE_DIR)
					set_property(TARGET SDL2::Core PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}")
				endif()
			elseif(DEFINED SDL2_LIBRARIES AND SDL2_LIBRARIES)
				add_library(SDL2::Core UNKNOWN IMPORTED)
				set_property(TARGET SDL2::Core PROPERTY INTERFACE_LINK_LIBRARIES "${SDL2_LIBRARIES}")
				if(DEFINED SDL2_INCLUDE_DIR AND SDL2_INCLUDE_DIR)
					set_property(TARGET SDL2::Core PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}")
				endif()
			endif()
		endif()
		# Provide aliases expected by some FindSDL2 implementations
		if(NOT TARGET SDL2::SDL2 AND TARGET SDL2::Core)
			add_library(SDL2::SDL2 ALIAS SDL2::Core)
		endif()
		if(NOT TARGET SDL2::Main AND TARGET SDL2::Core)
			add_library(SDL2::Main INTERFACE IMPORTED)
			set_property(TARGET SDL2::Main PROPERTY INTERFACE_LINK_LIBRARIES SDL2::Core)
		endif()
		TARGET_LINK_LIBRARIES(q3ui SDL2::Main)
	ELSEIF(UNIX)
		find_package (X11 REQUIRED)
		TARGET_LINK_LIBRARIES(q3ui ${X11_LIBRARIES})
	ENDIF()
ENDIF()

IF(USE_VULKAN)
	TARGET_COMPILE_DEFINITIONS(q3ui PRIVATE USE_VULKAN_API)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_VULKAN_API)
ENDIF()

IF(USE_MP3)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_MP3)
ENDIF()

# client executable

ADD_EXECUTABLE(${CNAME}${BINEXT} ${EXE_TYPE} ${Q3_SRCS})
TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} qcommon botlib client q3ui)
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