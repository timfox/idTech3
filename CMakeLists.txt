CMAKE_MINIMUM_REQUIRED(VERSION 3.20)

PROJECT(idtech3 C CXX ASM)

# Set C standard (C23 for modern features, fallback to C17/C11 if not available)
# C23 requires CMake 3.20+ and modern compilers (GCC 14+, Clang 18+)
# C17 is widely supported and provides modern features
IF(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	# Try C23 first (CMake 3.20+ supports it), compiler will use highest available
	# If C23 not supported by compiler, it will fall back to C17 automatically
	SET(CMAKE_C_STANDARD 23)
	SET(CMAKE_C_STANDARD_REQUIRED OFF)
ELSEIF(CMAKE_C_COMPILER_ID MATCHES "MSVC")
	# MSVC uses different versioning, C11 is safer
	SET(CMAKE_C_STANDARD 11)
	SET(CMAKE_C_STANDARD_REQUIRED ON)
ELSE()
	# Default to C17 for other compilers
	SET(CMAKE_C_STANDARD 17)
	SET(CMAKE_C_STANDARD_REQUIRED OFF)
ENDIF()

# Set C++ standard for ECS code (EnTT requires C++17)
IF(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
	SET(CMAKE_CXX_STANDARD 17)
	SET(CMAKE_CXX_STANDARD_REQUIRED ON)
ELSEIF(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
	SET(CMAKE_CXX_STANDARD 17)
	SET(CMAKE_CXX_STANDARD_REQUIRED ON)
ELSE()
	SET(CMAKE_CXX_STANDARD 17)
	SET(CMAKE_CXX_STANDARD_REQUIRED OFF)
ENDIF()

# Enable modern compiler warnings for better code quality
IF(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	# Format string checking (catches printf format errors) - applies to both C and C++
	ADD_COMPILE_OPTIONS(-Wformat=2)
	# Warn about shadowed variables - applies to both C and C++
	ADD_COMPILE_OPTIONS(-Wshadow)
	# Warn about unused parameters - applies to both C and C++
	ADD_COMPILE_OPTIONS(-Wunused-parameter)
	# Warn about unused variables - applies to both C and C++
	ADD_COMPILE_OPTIONS(-Wunused-variable)
	
	# C-specific flags (only apply to C files, not C++)
	# These use generator expressions to conditionally apply based on language
	ADD_COMPILE_OPTIONS(
		$<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>
		$<$<COMPILE_LANGUAGE:C>:-Wmissing-prototypes>
	)
	
	# Note: -Wold-style-cast is C++ only, not applicable for C code
ENDIF()

# Sanitizer support (GCC/Clang only, requires debug build)
IF(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	IF(ENABLE_ASAN)
		ADD_COMPILE_OPTIONS(-fsanitize=address)
		ADD_LINK_OPTIONS(-fsanitize=address)
		SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")
		SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
		MESSAGE(STATUS "AddressSanitizer (ASan) enabled")
	ENDIF()
	
	IF(ENABLE_UBSAN)
		ADD_COMPILE_OPTIONS(-fsanitize=undefined)
		ADD_LINK_OPTIONS(-fsanitize=undefined)
		SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=undefined")
		SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=undefined")
		MESSAGE(STATUS "UndefinedBehaviorSanitizer (UBSan) enabled")
	ENDIF()
	
	IF(ENABLE_MSAN)
		ADD_COMPILE_OPTIONS(-fsanitize=memory)
		ADD_LINK_OPTIONS(-fsanitize=memory)
		SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=memory")
		SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=memory")
		MESSAGE(STATUS "MemorySanitizer (MSan) enabled")
	ENDIF()
	
	IF(ENABLE_TSAN)
		ADD_COMPILE_OPTIONS(-fsanitize=thread)
		ADD_LINK_OPTIONS(-fsanitize=thread)
		SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
		SET(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=thread")
		MESSAGE(STATUS "ThreadSanitizer (TSan) enabled")
	ENDIF()
	
	# Valgrind support (add suppression file support)
	IF(ENABLE_VALGRIND)
		ADD_COMPILE_OPTIONS(-g -O0)
		ADD_COMPILE_DEFINITIONS(HAVE_VALGRIND)
		MESSAGE(STATUS "Valgrind support enabled (debug symbols, no optimization)")
	ENDIF()
	
	# Check for Valgrind headers
	IF(ENABLE_VALGRIND)
		CHECK_INCLUDE_FILE(valgrind/valgrind.h HAVE_VALGRIND_H)
		IF(HAVE_VALGRIND_H)
			ADD_COMPILE_DEFINITIONS(HAVE_VALGRIND)
			MESSAGE(STATUS "Valgrind headers found")
		ELSE()
			MESSAGE(WARNING "Valgrind headers not found, Valgrind integration disabled")
		ENDIF()
	ENDIF()
ENDIF()

# Windows-specific: Dr. Memory support
IF(WIN32 AND ENABLE_DRMEMORY)
	ADD_COMPILE_OPTIONS(/Zi)
	CHECK_INCLUDE_FILE(drmemory.h HAVE_DRMEMORY_H)
	IF(HAVE_DRMEMORY_H)
		ADD_COMPILE_DEFINITIONS(HAVE_DRMEMORY)
		MESSAGE(STATUS "Dr. Memory support enabled (debug symbols)")
	ELSE()
		MESSAGE(WARNING "Dr. Memory headers not found")
	ENDIF()
ENDIF()

# Memory safety and profiling options
OPTION(ENABLE_ASAN "Enable AddressSanitizer (ASan) for memory error detection" OFF)
OPTION(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer (UBSan) for undefined behavior detection" OFF)
OPTION(ENABLE_MSAN "Enable MemorySanitizer (MSan) for uninitialized memory detection" OFF)
OPTION(ENABLE_TSAN "Enable ThreadSanitizer (TSan) for data race detection" OFF)
OPTION(ENABLE_MEMORY_TRACKING "Enable memory usage tracking and statistics" ON)

# Static analysis options
OPTION(ENABLE_STATIC_ANALYSIS "Enable static analysis tools (clang-tidy, cppcheck)" OFF)

IF(ENABLE_STATIC_ANALYSIS)
	# Try to find clang-tidy
	FIND_PROGRAM(CLANG_TIDY_EXE NAMES clang-tidy)
	IF(CLANG_TIDY_EXE)
		SET(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE}" "-checks=-*,readability-*,performance-*,portability-*,bugprone-*")
		MESSAGE(STATUS "Static analysis: clang-tidy enabled")
	ELSE()
		MESSAGE(WARNING "Static analysis: clang-tidy not found")
	ENDIF()
	
	# Try to find cppcheck
	FIND_PROGRAM(CPPCHECK_EXE NAMES cppcheck)
	IF(CPPCHECK_EXE)
		SET(CMAKE_C_CPPCHECK "${CPPCHECK_EXE}" "--enable=all" "--suppress=missingIncludeSystem")
		MESSAGE(STATUS "Static analysis: cppcheck enabled")
	ELSE()
		MESSAGE(WARNING "Static analysis: cppcheck not found")
	ENDIF()
ENDIF()

SET(CNAME "idtech3")
SET(DNAME "idtech3.server")

SET(RENDERER_PREFIX ${CNAME})
SET(RENDERER_DEFAULT vulkan) # valid options: opengl, vulkan, opengl2, d3d12

OPTION(USE_SDL "" ON)
OPTION(USE_CURL "" ON)
OPTION(USE_LOCAL_HEADERS "" ON)
OPTION(USE_VULKAN "" ON)
OPTION(USE_VMA "Enable Vulkan Memory Allocator (VMA) for better memory management" ON)
OPTION(USE_D3D12 "Enable DirectX 12 renderer (Windows only)" ON)
OPTION(USE_SYSTEM_JPEG "" OFF)
OPTION(USE_RENDERER_DLOPEN "" ON)
OPTION(USE_OPENEXR "Enable OpenEXR (.exr) image support" ON)
OPTION(USE_ASSIMP "Enable Assimp 3D asset import library" ON)
OPTION(USE_CJSON "" ON)
OPTION(USE_ZSTD "" ON)
OPTION(USE_ENET "" ON)
OPTION(USE_SQLITE "" ON)
OPTION(USE_FREETYPE "" ON)
OPTION(USE_OPENSSL "" ON)
OPTION(USE_LUA "" ON)
OPTION(USE_ENTT "Enable EnTT ECS system" ON)
OPTION(USE_CIMGUI "Enable ImGui" ON)
OPTION(USE_WEBSOCKETS "Enable WebSocket support" ON)
OPTION(USE_OPENAL "Enable OpenAL enhanced audio system" ON)
#OPTION(USE_OPENGL "" OFF)
#OPTION(USE_OPENGL2 "" OFF)
#OPTION(USE_USD "" ON)
#OPTION(USE_GLTF "" ON)

IF(ANDROID)
	MESSAGE(STATUS "Configuring Android build for ABI: ${CMAKE_ANDROID_ARCH_ABI}")
	SET(BUILD_SERVER OFF CACHE BOOL "" FORCE)
	SET(BUILD_TESTS OFF CACHE BOOL "" FORCE)
	SET(USE_RENDERER_DLOPEN OFF CACHE BOOL "" FORCE)
	SET(CMAKE_POSITION_INDEPENDENT_CODE ON)
ENDIF()

SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules)

AUX_SOURCE_DIRECTORY(src/qcommon QCOMMON_SRCS)
# exclude platform-dependent QVM bytecode compilers
list(FILTER QCOMMON_SRCS EXCLUDE REGEX ".*vm_[alx].*.c")
# Add structured logging system
list(APPEND QCOMMON_SRCS src/qcommon/q_log.c)
# Add wrapper files conditionally
IF(USE_CJSON)
	list(APPEND QCOMMON_SRCS src/qcommon/json_cjson.c)
ENDIF()
IF(USE_ZSTD)
	list(APPEND QCOMMON_SRCS src/qcommon/zstd_compress.c)
ENDIF()
IF(USE_ENET)
	list(APPEND QCOMMON_SRCS src/qcommon/net_enet.c)
ENDIF()

IF(USE_SQLITE)
	list(APPEND QCOMMON_SRCS src/qcommon/sqlite_wrapper.c)
ENDIF()

IF(USE_OPENSSL)
	list(APPEND QCOMMON_SRCS src/qcommon/openssl_wrapper.c)
ENDIF()

IF(USE_LUA)
	list(APPEND QCOMMON_SRCS src/qcommon/lua_wrapper.c)
	list(APPEND QCOMMON_SRCS src/qcommon/lua_game.c)
	list(APPEND QCOMMON_SRCS src/qcommon/lua_renderer.c)
	list(APPEND QCOMMON_SRCS src/qcommon/lua_sound.c)
ENDIF()

# Internationalization (i18n) support
list(APPEND QCOMMON_SRCS src/qcommon/i18n.c)

IF(USE_FREETYPE)
	list(APPEND QCOMMON_SRCS src/qcommon/freetype_wrapper.c)
ENDIF()

# EnTT ECS system
IF(USE_ENTT)
	# Add EnTT include directory
	INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/libs/entt/src)
	# Add compile definition
	ADD_COMPILE_DEFINITIONS(USE_ENTT)
	# Add C++ source files for ECS (will be created)
	IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/qcommon/ecs.cpp)
		list(APPEND QCOMMON_SRCS src/qcommon/ecs.cpp)
	ENDIF()
	IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/qcommon/ecs_systems.cpp)
		list(APPEND QCOMMON_SRCS src/qcommon/ecs_systems.cpp)
	ENDIF()
	IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/server/sv_ecs.cpp)
		list(APPEND SERVER_SRCS src/server/sv_ecs.cpp)
	ENDIF()
	IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/server/sv_ecs_systems.cpp)
		list(APPEND SERVER_SRCS src/server/sv_ecs_systems.cpp)
	ENDIF()
ENDIF()

# add platform-dependent QVM bytecode compilers
# Force 64-bit builds by default
IF(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
	SET(BINEXT .aarch64)
	SET(RENDEXT _aarch64)
	list(APPEND QCOMMON_SRCS src/qcommon/vm_aarch64.c)
ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x86|i*86")
	# Default to 64-bit for x86 architectures
	IF(WIN32)
		SET(BINEXT .x64)
	ELSE()
		SET(BINEXT .x86_64)
	ENDIF()
	SET(RENDEXT _x86_64)
	list(APPEND QCOMMON_SRCS src/qcommon/vm_x86.c)
ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
	SET(BINEXT .arm)
	SET(RENDEXT _arm)
	list(APPEND QCOMMON_SRCS src/qcommon/vm_armv7l.c)
ELSE()
	TARGET_COMPILE_DEFINITIONS(qcommon PRIVATE -DNO_VM_COMPILED)
	TARGET_COMPILE_DEFINITIONS(qcommon_ded PRIVATE -DNO_VM_COMPILED)
ENDIF()

IF(ANDROID)
	SET(BINEXT "")
	IF(DEFINED CMAKE_ANDROID_ARCH_ABI)
		SET(RENDEXT "_${CMAKE_ANDROID_ARCH_ABI}")
	ELSE()
		SET(RENDEXT "_android")
	ENDIF()
ENDIF()

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

# client + cURL
AUX_SOURCE_DIRECTORY(src/client CLIENT_SRCS)

# Add OpenAL enhanced audio system
IF(USE_OPENAL)
	list(APPEND CLIENT_SRCS src/client/snd_openal.c)
ENDIF()

IF(USE_OPENEXR)
	# Try CMake's config package first (OpenEXR 3.x and newer)
	find_package(OpenEXR QUIET)

	if(TARGET OpenEXR::OpenEXR)
		set(OpenEXR_FOUND TRUE)
		set(OPENEXR_LIBRARIES OpenEXR::OpenEXR)
	endif()

	# Fallback for FindOpenEXR or older package layouts
	if(NOT OpenEXR_FOUND)
		# Some FindOpenEXR modules define these variables
		if(DEFINED OpenEXR_LIBRARIES)
			set(OPENEXR_LIBRARIES ${OpenEXR_LIBRARIES})
		endif()
		if(DEFINED OpenEXR_INCLUDE_DIRS)
			set(OPENEXR_INCLUDE_DIRS ${OpenEXR_INCLUDE_DIRS})
		endif()
		if(OPENEXR_LIBRARIES)
			set(OpenEXR_FOUND TRUE)
		endif()
	endif()

	# Manual search as a last resort
	if(NOT OpenEXR_FOUND)
		FIND_PATH(OPENEXR_INCLUDE_DIR
			NAMES ImfRgbaFile.h OpenEXR/ImfRgbaFile.h
			PATHS
				/usr/include
				/usr/local/include
				/opt/local/include
				/usr/include/OpenEXR
				/usr/local/include/OpenEXR
		)
		FIND_LIBRARY(OPENEXR_LIBRARY
			NAMES OpenEXR IlmImf
			PATHS
				/usr/lib
				/usr/lib/x86_64-linux-gnu
				/usr/local/lib
				/opt/local/lib
		)
		if(OPENEXR_INCLUDE_DIR AND OPENEXR_LIBRARY)
			set(OpenEXR_FOUND TRUE)
			set(OPENEXR_LIBRARIES ${OPENEXR_LIBRARY})
			set(OPENEXR_INCLUDE_DIRS ${OPENEXR_INCLUDE_DIR})
		endif()
	endif()

	if(OpenEXR_FOUND AND OPENEXR_LIBRARIES)
		ADD_COMPILE_DEFINITIONS(USE_OPENEXR)
		message(STATUS "OpenEXR found: ${OPENEXR_LIBRARIES}")
	else()
		message(WARNING "OpenEXR not found, disabling OpenEXR support")
		message(STATUS "To enable OpenEXR support, install: sudo apt-get install libopenexr-dev")
		set(USE_OPENEXR OFF CACHE BOOL "" FORCE)
	endif()
ENDIF()

IF(USE_ASSIMP)
	# Prefer modern CMake config package
	find_package(assimp QUIET)

	if(TARGET assimp::assimp)
		set(ASSIMP_FOUND TRUE)
		set(ASSIMP_LIBRARIES assimp::assimp)
	endif()

	# Fallback to older FindAssimp.cmake style modules
	if(NOT ASSIMP_FOUND)
		find_package(Assimp QUIET)
		if(Assimp_FOUND)
			set(ASSIMP_FOUND TRUE)
			if(DEFINED ASSIMP_LIBRARIES)
				set(ASSIMP_LIBRARIES ${ASSIMP_LIBRARIES})
			endif()
			if(DEFINED ASSIMP_INCLUDE_DIRS)
				set(ASSIMP_INCLUDE_DIRS ${ASSIMP_INCLUDE_DIRS})
			endif()
		endif()
	endif()

	# Manual search as a last resort
	if(NOT ASSIMP_FOUND)
		FIND_PATH(ASSIMP_INCLUDE_DIR
			NAMES assimp/Importer.hpp
			PATHS
				/usr/include
				/usr/local/include
				/opt/local/include
		)
		FIND_LIBRARY(ASSIMP_LIBRARY
			NAMES assimp
			PATHS
				/usr/lib
				/usr/lib/x86_64-linux-gnu
				/usr/local/lib
				/opt/local/lib
		)
		if(ASSIMP_INCLUDE_DIR AND ASSIMP_LIBRARY)
			set(ASSIMP_FOUND TRUE)
			set(ASSIMP_LIBRARIES ${ASSIMP_LIBRARY})
			set(ASSIMP_INCLUDE_DIRS ${ASSIMP_INCLUDE_DIR})
		endif()
	endif()

	if(ASSIMP_FOUND AND ASSIMP_LIBRARIES)
		ADD_COMPILE_DEFINITIONS(USE_ASSIMP)
		message(STATUS "Assimp found: ${ASSIMP_LIBRARIES}")
	else()
		message(WARNING "Assimp not found, disabling Assimp support")
		message(STATUS "To enable Assimp support, install: sudo apt-get install libassimp-dev")
		set(USE_ASSIMP OFF CACHE BOOL "" FORCE)
	endif()
ENDIF()

# Add debug overlay
IF(USE_CIMGUI)
	list(APPEND CLIENT_SRCS src/client/cl_imgui_debug.c)
ENDIF()
IF(NOT USE_CURL)
	list(REMOVE_ITEM CLIENT_SRCS src/client/cl_curl.c)
	list(REMOVE_ITEM CLIENT_SRCS src/client/cl_net_enhanced.c)
ELSE()
	# Add enhanced networking support
	list(APPEND CLIENT_SRCS src/client/cl_net_enhanced.c)
ENDIF()

# Add assembly files for x86_64
IF(CMAKE_SYSTEM_PROCESSOR MATCHES AMD64|x86_64)
	list(APPEND CLIENT_SRCS src/asm/snd_mix_x86_64.s)
	set_source_files_properties(src/asm/snd_mix_x86_64.s PROPERTIES
		COMPILE_FLAGS "-x assembler-with-cpp -DELF"
		LANGUAGE ASM)
ENDIF()

IF(USE_CIMGUI)
	set(CIMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/cimgui)
	set(CIMGUI_IMGUI_DIR ${CIMGUI_DIR}/imgui)
	if(EXISTS ${CIMGUI_DIR}/cimgui.cpp AND EXISTS ${CIMGUI_IMGUI_DIR}/imgui.cpp)
		set(CIMGUI_CORE_SRCS
			${CIMGUI_DIR}/cimgui.cpp
			${CIMGUI_IMGUI_DIR}/imgui.cpp
			${CIMGUI_IMGUI_DIR}/imgui_demo.cpp
			${CIMGUI_IMGUI_DIR}/imgui_draw.cpp
			${CIMGUI_IMGUI_DIR}/imgui_tables.cpp
			${CIMGUI_IMGUI_DIR}/imgui_widgets.cpp
			${CIMGUI_IMGUI_DIR}/misc/cpp/imgui_stdlib.cpp
		)
		add_library(imgui_shared SHARED ${CIMGUI_CORE_SRCS})
		target_include_directories(imgui_shared PUBLIC
			${CIMGUI_DIR}
			${CIMGUI_IMGUI_DIR}
			${CIMGUI_IMGUI_DIR}/misc/cpp
		)
		target_compile_definitions(imgui_shared PUBLIC IMGUI_USER_CONFIG="cimconfig.h")
		target_compile_features(imgui_shared PUBLIC cxx_std_17)
		set_target_properties(imgui_shared PROPERTIES OUTPUT_NAME "imgui_shared")
		list(APPEND CLIENT_SRCS
			${CIMGUI_DIR}/cimgui_impl.cpp
			${CIMGUI_IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
		)
	else()
		message(WARNING "cimgui sources not found, disabling USE_CIMGUI")
		set(USE_CIMGUI OFF CACHE BOOL "" FORCE)
	endif()
ENDIF()

#ADD_LIBRARY(client OBJECT ${CLIENT_SRCS})

# jpeg
IF(USE_SYSTEM_JPEG)
	ADD_LIBRARY(client OBJECT ${CLIENT_SRCS})
	find_package(JPEG REQUIRED)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE -DUSE_SYSTEM_JPEG)
	TARGET_LINK_LIBRARIES(client ${JPEG_LIBRARIES})
ELSE()
	AUX_SOURCE_DIRECTORY(libs/jpeg JPEG_SRCS)
	#ADD_LIBRARY(jpeg ${JPEG_SRCS})
	#TARGET_LINK_LIBRARIES(client jpeg)
	ADD_LIBRARY(client OBJECT ${CLIENT_SRCS} ${JPEG_SRCS})
	# Add JPEG include directory for internal includes
	TARGET_INCLUDE_DIRECTORIES(client PRIVATE libs/jpeg)
	# Suppress warnings for third-party JPEG library code
	IF(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
		FOREACH(JPEG_SRC ${JPEG_SRCS})
			set_source_files_properties(${JPEG_SRC} PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter")
		ENDFOREACH()
	ENDIF()
ENDIF()

# Add OGG/Vorbis include directories for client (used by snd_codec_ogg.c)
TARGET_INCLUDE_DIRECTORIES(client PRIVATE libs/ogg/include libs/vorbis/include)

IF(USE_CIMGUI AND DEFINED CIMGUI_DIR)
	set(CIMGUI_USER_CONFIG "\"cimconfig.h\"")
	TARGET_INCLUDE_DIRECTORIES(client PRIVATE
		${CIMGUI_DIR}
		${CIMGUI_IMGUI_DIR}
		${CIMGUI_IMGUI_DIR}/backends
		${CIMGUI_IMGUI_DIR}/misc/cpp
	)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE
		USE_CIMGUI
		IMGUI_USER_CONFIG=${CIMGUI_USER_CONFIG}
		CIMGUI_USE_SDL2
	)
ENDIF()

# cJSON library
IF(USE_CJSON)
	SET(CJSON_SRCS libs/cjson/cJSON.c)
	list(APPEND QCOMMON_SRCS ${CJSON_SRCS})
	TARGET_INCLUDE_DIRECTORIES(qcommon PRIVATE libs/cjson)
	TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE libs/cjson)
ENDIF()

# zstd library
IF(USE_ZSTD)
	SET(ZSTD_SRCS
		libs/zstd/lib/common/debug.c
		libs/zstd/lib/common/entropy_common.c
		libs/zstd/lib/common/error_private.c
		libs/zstd/lib/common/fse_decompress.c
		libs/zstd/lib/common/pool.c
		libs/zstd/lib/common/threading.c
		libs/zstd/lib/common/xxhash.c
		libs/zstd/lib/common/zstd_common.c
		libs/zstd/lib/compress/fse_compress.c
		libs/zstd/lib/compress/hist.c
		libs/zstd/lib/compress/huf_compress.c
		libs/zstd/lib/compress/zstd_compress.c
		libs/zstd/lib/compress/zstd_compress_literals.c
		libs/zstd/lib/compress/zstd_compress_sequences.c
		libs/zstd/lib/compress/zstd_compress_superblock.c
		libs/zstd/lib/compress/zstd_double_fast.c
		libs/zstd/lib/compress/zstd_fast.c
		libs/zstd/lib/compress/zstd_lazy.c
		libs/zstd/lib/compress/zstd_ldm.c
		libs/zstd/lib/compress/zstd_opt.c
		libs/zstd/lib/compress/zstdmt_compress.c
		libs/zstd/lib/decompress/huf_decompress.c
		libs/zstd/lib/decompress/zstd_ddict.c
		libs/zstd/lib/decompress/zstd_decompress.c
		libs/zstd/lib/decompress/zstd_decompress_block.c
	)
	list(APPEND QCOMMON_SRCS ${ZSTD_SRCS})
	TARGET_INCLUDE_DIRECTORIES(qcommon PRIVATE libs/zstd/lib libs/zstd/lib/common)
	TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE libs/zstd/lib libs/zstd/lib/common)
ENDIF()

# ENet library
IF(USE_ENET)
	SET(ENET_SRCS
		libs/enet/callbacks.c
		libs/enet/compress.c
		libs/enet/host.c
		libs/enet/list.c
		libs/enet/packet.c
		libs/enet/peer.c
		libs/enet/protocol.c
	)
	IF(WIN32)
		list(APPEND ENET_SRCS libs/enet/win32.c)
	ELSE()
		list(APPEND ENET_SRCS libs/enet/unix.c)
	ENDIF()
	list(APPEND QCOMMON_SRCS ${ENET_SRCS})
	TARGET_INCLUDE_DIRECTORIES(qcommon PRIVATE libs/enet/include)
	TARGET_INCLUDE_DIRECTORIES(qcommon_ded PRIVATE libs/enet/include)
ENDIF()

# renderers
AUX_SOURCE_DIRECTORY(src/renderercommon RENDERER_COMMON_SRCS)
AUX_SOURCE_DIRECTORY(src/renderer RENDERER_GL_SRCS)
AUX_SOURCE_DIRECTORY(src/renderervk RENDERER_VK_SRCS)

IF(USE_CIMGUI AND DEFINED CIMGUI_DIR)
	list(APPEND RENDERER_VK_SRCS
		${CIMGUI_DIR}/cimgui_impl.cpp
		${CIMGUI_IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
	)
ENDIF()

list(APPEND RENDERER_VK_SRCS src/renderervk/vk_imgui.c)

# DirectX 12 renderer (Windows only)
IF(WIN32 AND USE_D3D12)
	AUX_SOURCE_DIRECTORY(src/rendererd3d12 RENDERER_D3D12_SRCS)
	IF(USE_CIMGUI AND DEFINED CIMGUI_DIR)
		list(APPEND RENDERER_D3D12_SRCS
			${CIMGUI_DIR}/cimgui_impl.cpp
			${CIMGUI_IMGUI_DIR}/backends/imgui_impl_dx12.cpp
		)
	ENDIF()
ENDIF()

# VMA (Vulkan Memory Allocator)
IF(USE_VMA AND USE_VULKAN)
	# Check if VMA header exists
	IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/vma/vk_mem_alloc.h)
		TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_VMA)
		TARGET_INCLUDE_DIRECTORIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE libs/vma)
		TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan PRIVATE USE_VMA)
		TARGET_INCLUDE_DIRECTORIES(${RENDERER_PREFIX}_vulkan PRIVATE libs/vma)
		MESSAGE(STATUS "VMA: Vulkan Memory Allocator enabled")
	ELSE()
		MESSAGE(WARNING "VMA: Header not found at libs/vma/vk_mem_alloc.h. Download it from https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator")
		SET(USE_VMA OFF CACHE BOOL "" FORCE)
	ENDIF()
ENDIF()

IF(USE_RENDERER_DLOPEN)
	SET(AUX_SRCS
		src/qcommon/q_shared.c
		src/qcommon/puff.c
		src/qcommon/q_math.c
	)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_RENDERER_DLOPEN RENDERER_PREFIX="${RENDERER_PREFIX}" RENDERER_DEFAULT=${RENDERER_DEFAULT})

	ADD_LIBRARY(${RENDERER_PREFIX}_opengl${RENDEXT} SHARED ${RENDERER_GL_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
	SET_TARGET_PROPERTIES(${RENDERER_PREFIX}_opengl${RENDEXT} PROPERTIES OUTPUT_NAME "${RENDERER_PREFIX}_opengl${RENDEXT}" PREFIX "")
	TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE USE_RENDERER_DLOPEN)
	TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_opengl${RENDEXT} m)

	ADD_LIBRARY(${RENDERER_PREFIX}_vulkan${RENDEXT} SHARED ${RENDERER_VK_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
	SET_TARGET_PROPERTIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PROPERTIES OUTPUT_NAME "${RENDERER_PREFIX}_vulkan${RENDEXT}" PREFIX "")
	TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_RENDERER_DLOPEN USE_VULKAN)
	TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE m)
	
	IF(WIN32 AND USE_D3D12)
		ADD_LIBRARY(${RENDERER_PREFIX}_d3d12${RENDEXT} SHARED ${RENDERER_D3D12_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
		SET_TARGET_PROPERTIES(${RENDERER_PREFIX}_d3d12${RENDEXT} PROPERTIES OUTPUT_NAME "${RENDERER_PREFIX}_d3d12${RENDEXT}" PREFIX "")
		TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_d3d12${RENDEXT} PRIVATE USE_RENDERER_DLOPEN USE_D3D12)
		TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_d3d12${RENDEXT} PRIVATE d3d12 dxgi d3dcompiler)
	ENDIF()
ELSE()
	IF(USE_VULKAN)
		ADD_LIBRARY(${RENDERER_PREFIX}_vulkan OBJECT ${RENDERER_VK_SRCS} ${RENDERER_COMMON_SRCS})
	ELSEIF(WIN32 AND USE_D3D12)
		ADD_LIBRARY(${RENDERER_PREFIX}_d3d12 OBJECT ${RENDERER_D3D12_SRCS} ${RENDERER_COMMON_SRCS})
	ELSE()
		ADD_LIBRARY(${RENDERER_PREFIX}_opengl OBJECT ${RENDERER_GL_SRCS} ${RENDERER_COMMON_SRCS})
	ENDIF()
ENDIF()

IF(USE_CIMGUI AND TARGET imgui_shared)
	if(TARGET ${RENDERER_PREFIX}_vulkan${RENDEXT})
		target_include_directories(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE
			${CIMGUI_DIR}
			${CIMGUI_IMGUI_DIR}
			${CIMGUI_IMGUI_DIR}/backends
			${CIMGUI_IMGUI_DIR}/misc/cpp
			${CMAKE_CURRENT_SOURCE_DIR}/src/renderercommon
			${CMAKE_CURRENT_SOURCE_DIR}/src/renderercommon/vulkan
		)
		target_compile_definitions(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE
			USE_CIMGUI
			IMGUI_USER_CONFIG="cimconfig.h"
			CIMGUI_USE_VULKAN
		)
		target_link_libraries(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE imgui_shared)
	elseif(TARGET ${RENDERER_PREFIX}_vulkan)
		target_include_directories(${RENDERER_PREFIX}_vulkan PRIVATE
			${CIMGUI_DIR}
			${CIMGUI_IMGUI_DIR}
			${CIMGUI_IMGUI_DIR}/backends
			${CIMGUI_IMGUI_DIR}/misc/cpp
			${CMAKE_CURRENT_SOURCE_DIR}/src/renderercommon
			${CMAKE_CURRENT_SOURCE_DIR}/src/renderercommon/vulkan
		)
		target_compile_definitions(${RENDERER_PREFIX}_vulkan PRIVATE
			USE_CIMGUI
			IMGUI_USER_CONFIG="cimconfig.h"
			CIMGUI_USE_VULKAN
		)
	endif()
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
	AUX_SOURCE_DIRECTORY(src/win32 Q3_SRCS)
	AUX_SOURCE_DIRECTORY(src/win32 Q3_UI_SRCS)
ENDIF()

IF(USE_SDL)
	find_package(SDL2 REQUIRED)
	TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${SDL2_INCLUDE_DIR})
	TARGET_INCLUDE_DIRECTORIES(qcommon PUBLIC ${SDL2_INCLUDE_DIR})
	TARGET_INCLUDE_DIRECTORIES(qcommon_ded PUBLIC ${SDL2_INCLUDE_DIR})
ENDIF()

IF(USE_CURL)
	# Try CMake's find_package first
	find_package(CURL QUIET)
	
	# If not found, try pkg-config
	IF(NOT CURL_FOUND)
		find_package(PkgConfig QUIET)
		IF(PKG_CONFIG_FOUND)
			pkg_check_modules(CURL libcurl QUIET)
			IF(CURL_FOUND)
				SET(CURL_INCLUDE_DIR ${CURL_INCLUDE_DIRS})
				SET(CURL_LIBRARIES ${CURL_LIBRARIES})
			ENDIF()
		ENDIF()
	ENDIF()
	
	# If still not found, try manual search
	IF(NOT CURL_FOUND)
		FIND_PATH(CURL_INCLUDE_DIR curl/curl.h
			PATHS
			/usr/include
			/usr/local/include
			/opt/local/include
			/usr/include/x86_64-linux-gnu
		)
		FIND_LIBRARY(CURL_LIBRARIES
			NAMES curl libcurl libcurl.so.4 libcurl.so
			PATHS
			/usr/lib
			/usr/lib/x86_64-linux-gnu
			/usr/local/lib
			/opt/local/lib
		)
		IF(CURL_INCLUDE_DIR AND CURL_LIBRARIES)
			SET(CURL_FOUND TRUE)
		ENDIF()
	ENDIF()
	
	IF(CURL_FOUND)
		TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${CURL_INCLUDE_DIR})
		TARGET_INCLUDE_DIRECTORIES(qcommon PUBLIC ${CURL_INCLUDE_DIR})
		TARGET_INCLUDE_DIRECTORIES(qcommon_ded PUBLIC ${CURL_INCLUDE_DIR})
		MESSAGE(STATUS "CURL found: ${CURL_LIBRARIES}")
	ELSE()
		MESSAGE(WARNING "CURL not found, disabling CURL support")
		MESSAGE(STATUS "To enable CURL support, install: sudo apt-get install libcurl4-openssl-dev")
		SET(USE_CURL OFF CACHE BOOL "" FORCE)
		list(REMOVE_ITEM CLIENT_SRCS src/client/cl_curl.c)
	ENDIF()
ENDIF()

IF(USE_SQLITE)
	# Try CMake's find_package first
	find_package(SQLite3 QUIET)
	
	# If not found, try manual search
	IF(NOT SQLite3_FOUND)
		FIND_PATH(SQLite3_INCLUDE_DIR sqlite3.h
			PATHS
			/usr/include
			/usr/local/include
			/opt/local/include
			/usr/include/x86_64-linux-gnu
		)
		FIND_LIBRARY(SQLite3_LIBRARIES
			NAMES sqlite3 libsqlite3
			PATHS
			/usr/lib
			/usr/lib/x86_64-linux-gnu
			/usr/local/lib
			/opt/local/lib
		)
		IF(SQLite3_INCLUDE_DIR AND SQLite3_LIBRARIES)
			SET(SQLite3_FOUND TRUE)
			SET(SQLite3_INCLUDE_DIRS ${SQLite3_INCLUDE_DIR})
		ENDIF()
	ENDIF()
	
	IF(SQLite3_FOUND)
		TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${SQLite3_INCLUDE_DIRS})
		TARGET_INCLUDE_DIRECTORIES(qcommon PUBLIC ${SQLite3_INCLUDE_DIRS})
		TARGET_INCLUDE_DIRECTORIES(qcommon_ded PUBLIC ${SQLite3_INCLUDE_DIRS})
		TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_SQLITE)
		TARGET_COMPILE_DEFINITIONS(qcommon PRIVATE USE_SQLITE)
		TARGET_COMPILE_DEFINITIONS(qcommon_ded PRIVATE USE_SQLITE)
		MESSAGE(STATUS "SQLite found: ${SQLite3_LIBRARIES}")
	ELSE()
		MESSAGE(WARNING "SQLite not found, disabling SQLite support")
		MESSAGE(STATUS "To enable SQLite support, install: sudo apt-get install libsqlite3-dev")
		SET(USE_SQLITE OFF CACHE BOOL "" FORCE)
	ENDIF()
ENDIF()

IF(USE_FREETYPE)
	# Try CMake's find_package first
	find_package(Freetype QUIET)
	
	# If not found, try manual search
	IF(NOT Freetype_FOUND)
		FIND_PATH(FREETYPE_INCLUDE_DIRS
			NAMES freetype2/freetype/freetype.h freetype/freetype.h
			PATHS
			/usr/include
			/usr/local/include
			/opt/local/include
			/usr/include/x86_64-linux-gnu
		)
		FIND_LIBRARY(FREETYPE_LIBRARIES
			NAMES freetype libfreetype
			PATHS
			/usr/lib
			/usr/lib/x86_64-linux-gnu
			/usr/local/lib
			/opt/local/lib
		)
		IF(FREETYPE_INCLUDE_DIRS AND FREETYPE_LIBRARIES)
			SET(Freetype_FOUND TRUE)
		ENDIF()
	ENDIF()
	
	IF(Freetype_FOUND)
		TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${FREETYPE_INCLUDE_DIRS})
		TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_FREETYPE)
		MESSAGE(STATUS "Freetype found: ${FREETYPE_LIBRARIES}")
	ELSE()
		MESSAGE(WARNING "Freetype not found, disabling Freetype support")
		MESSAGE(STATUS "To enable Freetype support, install: sudo apt-get install libfreetype6-dev")
		SET(USE_FREETYPE OFF CACHE BOOL "" FORCE)
	ENDIF()
ENDIF()

IF(USE_OPENSSL)
	# Try CMake's find_package first
	find_package(OpenSSL QUIET)
	
	# If not found, try manual search
	IF(NOT OpenSSL_FOUND)
		FIND_PATH(OPENSSL_INCLUDE_DIR
			NAMES openssl/ssl.h openssl/crypto.h
			PATHS
			/usr/include
			/usr/local/include
			/opt/local/include
			/usr/include/x86_64-linux-gnu
		)
		FIND_LIBRARY(OPENSSL_SSL_LIBRARY
			NAMES ssl libssl
			PATHS
			/usr/lib
			/usr/lib/x86_64-linux-gnu
			/usr/local/lib
			/opt/local/lib
		)
		FIND_LIBRARY(OPENSSL_CRYPTO_LIBRARY
			NAMES crypto libcrypto
			PATHS
			/usr/lib
			/usr/lib/x86_64-linux-gnu
			/usr/local/lib
			/opt/local/lib
		)
		IF(OPENSSL_INCLUDE_DIR AND OPENSSL_SSL_LIBRARY AND OPENSSL_CRYPTO_LIBRARY)
			SET(OpenSSL_FOUND TRUE)
			SET(OPENSSL_LIBRARIES ${OPENSSL_SSL_LIBRARY} ${OPENSSL_CRYPTO_LIBRARY})
			SET(OPENSSL_INCLUDE_DIRS ${OPENSSL_INCLUDE_DIR})
		ENDIF()
	ENDIF()
	
	IF(OpenSSL_FOUND)
		TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${OPENSSL_INCLUDE_DIRS})
		TARGET_INCLUDE_DIRECTORIES(qcommon PUBLIC ${OPENSSL_INCLUDE_DIRS})
		TARGET_INCLUDE_DIRECTORIES(qcommon_ded PUBLIC ${OPENSSL_INCLUDE_DIRS})
		TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_OPENSSL)
		TARGET_COMPILE_DEFINITIONS(qcommon PRIVATE USE_OPENSSL)
		TARGET_COMPILE_DEFINITIONS(qcommon_ded PRIVATE USE_OPENSSL)
		MESSAGE(STATUS "OpenSSL found: ${OPENSSL_LIBRARIES}")
	ELSE()
		MESSAGE(WARNING "OpenSSL not found, disabling OpenSSL support")
		MESSAGE(STATUS "To enable OpenSSL support, install: sudo apt-get install libssl-dev")
		SET(USE_OPENSSL OFF CACHE BOOL "" FORCE)
	ENDIF()
ENDIF()

IF(USE_WEBSOCKETS)
	# Try CMake's find_package first
	find_package(LibWebSockets QUIET)
	
	# If not found, try pkg-config
	IF(NOT LibWebSockets_FOUND)
		find_package(PkgConfig QUIET)
		IF(PKG_CONFIG_FOUND)
			pkg_check_modules(LIBWEBSOCKETS libwebsockets QUIET)
			IF(LIBWEBSOCKETS_FOUND)
				SET(LIBWEBSOCKETS_INCLUDE_DIR ${LIBWEBSOCKETS_INCLUDE_DIRS})
				SET(LIBWEBSOCKETS_LIBRARIES ${LIBWEBSOCKETS_LIBRARIES})
				SET(LibWebSockets_FOUND TRUE)
			ENDIF()
		ENDIF()
	ENDIF()
	
	# If still not found, try manual search
	IF(NOT LibWebSockets_FOUND)
		FIND_PATH(LIBWEBSOCKETS_INCLUDE_DIR
			NAMES libwebsockets.h
			PATHS
			/usr/include
			/usr/local/include
			/opt/local/include
			/usr/include/x86_64-linux-gnu
		)
		FIND_LIBRARY(LIBWEBSOCKETS_LIBRARY
			NAMES websockets libwebsockets
			PATHS
			/usr/lib
			/usr/lib/x86_64-linux-gnu
			/usr/local/lib
			/opt/local/lib
		)
		IF(LIBWEBSOCKETS_INCLUDE_DIR AND LIBWEBSOCKETS_LIBRARY)
			SET(LibWebSockets_FOUND TRUE)
			SET(LIBWEBSOCKETS_LIBRARIES ${LIBWEBSOCKETS_LIBRARY})
			SET(LIBWEBSOCKETS_INCLUDE_DIRS ${LIBWEBSOCKETS_INCLUDE_DIR})
		ENDIF()
	ENDIF()
	
	IF(LibWebSockets_FOUND)
		TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${LIBWEBSOCKETS_INCLUDE_DIRS})
		TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_WEBSOCKETS)
		MESSAGE(STATUS "libwebsockets found: ${LIBWEBSOCKETS_LIBRARIES}")
	ELSE()
		MESSAGE(WARNING "libwebsockets not found, disabling WebSocket support")
		MESSAGE(STATUS "To enable WebSocket support, install: sudo apt-get install libwebsockets-dev")
		SET(USE_WEBSOCKETS OFF CACHE BOOL "" FORCE)
	ENDIF()
ENDIF()

IF(USE_LUA)
	# Try CMake's find_package first (supports Lua 5.1-5.4)
	find_package(Lua QUIET)
	
	# If still not found, try manual search for common Lua versions
	IF(NOT Lua_FOUND)
		FOREACH(LUA_VERSION 5.4 5.3 5.2 5.1)
			# Try versioned subdirectory first (e.g., /usr/include/lua5.4/)
			FIND_PATH(LUA_INCLUDE_DIR
				NAMES lua.h
				PATHS
				/usr/include/lua${LUA_VERSION}
				/usr/local/include/lua${LUA_VERSION}
				/opt/local/include/lua${LUA_VERSION}
				/usr/include/x86_64-linux-gnu/lua${LUA_VERSION}
				# Then try standard locations
				/usr/include
				/usr/local/include
				/opt/local/include
				/usr/include/x86_64-linux-gnu
			)
			FIND_LIBRARY(LUA_LIBRARY
				NAMES lua${LUA_VERSION} lua
				PATHS
				/usr/lib/x86_64-linux-gnu
				/usr/lib
				/usr/local/lib
				/opt/local/lib
			)
			IF(LUA_INCLUDE_DIR AND LUA_LIBRARY)
				SET(Lua_FOUND TRUE)
				SET(LUA_LIBRARIES ${LUA_LIBRARY})
				# If header is in subdirectory (e.g., lua5.4/), use parent directory
				# so #include <lua.h> works (it will find lua5.4/lua.h)
				GET_FILENAME_COMPONENT(LUA_INCLUDE_PARENT ${LUA_INCLUDE_DIR} DIRECTORY)
				IF(LUA_INCLUDE_DIR MATCHES "lua[0-9]")
					# Header is in versioned subdirectory, use parent
					SET(LUA_INCLUDE_DIRS ${LUA_INCLUDE_PARENT})
				ELSE()
					# Header is directly in include directory
					SET(LUA_INCLUDE_DIRS ${LUA_INCLUDE_DIR})
				ENDIF()
				BREAK()
			ENDIF()
		ENDFOREACH()
	ENDIF()
	
	IF(Lua_FOUND)
		# Ensure LUA_INCLUDE_DIRS is set - CMake's find_package might not set it correctly
		IF(NOT LUA_INCLUDE_DIRS OR LUA_INCLUDE_DIRS STREQUAL "")
			# Try to find it manually
			FIND_PATH(LUA_INCLUDE_DIR_CHECK lua.h 
				PATHS 
				/usr/include/lua5.4
				/usr/include
				/usr/local/include/lua5.4
				/usr/local/include
			)
			IF(LUA_INCLUDE_DIR_CHECK)
				SET(LUA_INCLUDE_DIRS ${LUA_INCLUDE_DIR_CHECK})
			ENDIF()
		ENDIF()
		
		IF(LUA_INCLUDE_DIRS AND NOT LUA_INCLUDE_DIRS STREQUAL "")
			# Handle versioned Lua include directories (e.g., /usr/include/lua5.4/)
			# If lua.h is in a versioned subdirectory, we need to include both
			# the parent directory and the subdirectory so #include <lua.h> works
			STRING(REGEX MATCH "lua[0-9.]+" LUA_VERSION_MATCH "${LUA_INCLUDE_DIRS}")
			IF(LUA_VERSION_MATCH)
				# Header is in versioned subdirectory like lua5.4/
				# Extract parent directory by removing the versioned subdirectory
				STRING(REGEX REPLACE "/lua[0-9.]+$" "" LUA_INCLUDE_PARENT "${LUA_INCLUDE_DIRS}")
				# Add both parent and subdirectory to include path
				TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${LUA_INCLUDE_PARENT} ${LUA_INCLUDE_DIRS})
				TARGET_INCLUDE_DIRECTORIES(qcommon PUBLIC ${LUA_INCLUDE_PARENT} ${LUA_INCLUDE_DIRS})
				TARGET_INCLUDE_DIRECTORIES(qcommon_ded PUBLIC ${LUA_INCLUDE_PARENT} ${LUA_INCLUDE_DIRS})
				MESSAGE(STATUS "Lua found (versioned): ${LUA_LIBRARIES}, includes: ${LUA_INCLUDE_PARENT} and ${LUA_INCLUDE_DIRS}")
			ELSE()
				# Header is directly in include directory
				TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${LUA_INCLUDE_DIRS})
				TARGET_INCLUDE_DIRECTORIES(qcommon PUBLIC ${LUA_INCLUDE_DIRS})
				TARGET_INCLUDE_DIRECTORIES(qcommon_ded PUBLIC ${LUA_INCLUDE_DIRS})
				MESSAGE(STATUS "Lua found: ${LUA_LIBRARIES}, includes: ${LUA_INCLUDE_DIRS}")
			ENDIF()
		ELSE()
			MESSAGE(WARNING "Lua found but could not determine include directory, disabling Lua support")
			SET(USE_LUA OFF CACHE BOOL "" FORCE)
			list(REMOVE_ITEM QCOMMON_SRCS src/qcommon/lua_wrapper.c)
			SET(Lua_FOUND FALSE)
		ENDIF()
		
		IF(Lua_FOUND)
			TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_LUA)
			TARGET_COMPILE_DEFINITIONS(qcommon PRIVATE USE_LUA)
			TARGET_COMPILE_DEFINITIONS(qcommon_ded PRIVATE USE_LUA)
		ENDIF()
	ELSE()
		MESSAGE(WARNING "Lua not found, disabling Lua support")
		MESSAGE(STATUS "To enable Lua support, install: sudo apt-get install liblua5.4-dev")
		MESSAGE(STATUS "Reconfiguring CMake with USE_LUA=OFF")
		SET(USE_LUA OFF CACHE BOOL "" FORCE)
		# Remove lua_wrapper.c from sources since Lua is not available
		list(REMOVE_ITEM QCOMMON_SRCS src/qcommon/lua_wrapper.c)
	ENDIF()
ENDIF()

IF(USE_OPENAL)
	# Try CMake's find_package first
	find_package(OpenAL QUIET)
	
	# If still not found, try manual search
	IF(NOT OpenAL_FOUND)
		FIND_PATH(OPENAL_INCLUDE_DIR
			NAMES AL/al.h
			PATHS
			/usr/include
			/usr/local/include
			/opt/local/include
			/usr/include/x86_64-linux-gnu
		)
		FIND_LIBRARY(OPENAL_LIBRARY
			NAMES openal OpenAL
			PATHS
			/usr/lib
			/usr/lib/x86_64-linux-gnu
			/usr/local/lib
			/opt/local/lib
		)
		IF(OPENAL_INCLUDE_DIR AND OPENAL_LIBRARY)
			SET(OpenAL_FOUND TRUE)
			SET(OPENAL_LIBRARIES ${OPENAL_LIBRARY})
			SET(OPENAL_INCLUDE_DIRS ${OPENAL_INCLUDE_DIR})
		ENDIF()
	ELSE()
		# CMake's find_package found OpenAL, use its variables
		IF(TARGET OpenAL::OpenAL)
			# Modern CMake target-based approach
			SET(OPENAL_LIBRARIES OpenAL::OpenAL)
		ELSEIF(OpenAL_LIBRARIES)
			# Use provided library variable
			SET(OPENAL_LIBRARIES ${OpenAL_LIBRARIES})
		ELSEIF(OpenAL_LIBRARY)
			# Fallback to single library variable
			SET(OPENAL_LIBRARIES ${OpenAL_LIBRARY})
		ELSE()
			# Try to find the library manually
			FIND_LIBRARY(OPENAL_LIBRARY
				NAMES openal OpenAL
				PATHS
				/usr/lib
				/usr/lib/x86_64-linux-gnu
				/usr/local/lib
				/opt/local/lib
			)
			IF(OPENAL_LIBRARY)
				SET(OPENAL_LIBRARIES ${OPENAL_LIBRARY})
			ENDIF()
		ENDIF()
		IF(OpenAL_INCLUDE_DIRS)
			SET(OPENAL_INCLUDE_DIRS ${OpenAL_INCLUDE_DIRS})
		ELSEIF(OpenAL_INCLUDE_DIR)
			SET(OPENAL_INCLUDE_DIRS ${OpenAL_INCLUDE_DIR})
		ENDIF()
	ENDIF()
	
	IF(OpenAL_FOUND AND OPENAL_LIBRARIES)
		TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${OPENAL_INCLUDE_DIRS})
		TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_OPENAL)
		MESSAGE(STATUS "OpenAL found: ${OPENAL_LIBRARIES}")
	ELSE()
		MESSAGE(WARNING "OpenAL not found, disabling enhanced audio system")
		MESSAGE(STATUS "To enable OpenAL support, install: sudo apt-get install libopenal-dev")
		SET(USE_OPENAL OFF CACHE BOOL "" FORCE)
		list(REMOVE_ITEM CLIENT_SRCS src/client/snd_openal.c)
	ENDIF()
ENDIF()

IF(USE_VULKAN)
	# Check if local Vulkan headers exist (renderer has bundled headers)
	IF(EXISTS "${CMAKE_SOURCE_DIR}/src/renderercommon/vulkan/vulkan.h")
		# Renderer uses local Vulkan headers, so we just need the library
		# Try multiple search paths and library name variations
		FIND_LIBRARY(VULKAN_LIBRARY 
			NAMES vulkan libvulkan.so.1 libvulkan.so
			PATHS 
				/usr/lib/x86_64-linux-gnu
				/usr/lib64
				/usr/lib
				/usr/local/lib
				/lib/x86_64-linux-gnu
			NO_DEFAULT_PATH
		)
		# If not found with NO_DEFAULT_PATH, try default search
		IF(NOT VULKAN_LIBRARY)
			FIND_LIBRARY(VULKAN_LIBRARY NAMES vulkan libvulkan.so.1 libvulkan.so)
		ENDIF()
		IF(VULKAN_LIBRARY)
			MESSAGE(STATUS "Found Vulkan library: ${VULKAN_LIBRARY} (using local headers)")
			TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_VULKAN)
			TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_VULKAN)
			TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${VULKAN_LIBRARY})
		ELSE()
			MESSAGE(WARNING "Vulkan library not found, disabling Vulkan support")
			SET(USE_VULKAN OFF CACHE BOOL "" FORCE)
		ENDIF()
	ELSE()
		# Try CMake's find_package first
		find_package(Vulkan)
		IF(Vulkan_FOUND)
			TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_VULKAN)
			TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_VULKAN)
			TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${Vulkan_INCLUDE_DIRS})
			TARGET_INCLUDE_DIRECTORIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PUBLIC ${Vulkan_INCLUDE_DIRS})
			TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${Vulkan_LIBRARIES})
		ELSE()
			# Try to find Vulkan manually if CMake's find_package fails
			FIND_PATH(VULKAN_INCLUDE_DIR NAMES vulkan/vulkan.h PATHS /usr/include /usr/local/include)
			FIND_LIBRARY(VULKAN_LIBRARY NAMES vulkan PATHS /usr/lib /usr/lib64 /usr/local/lib /usr/lib/x86_64-linux-gnu)
			IF(VULKAN_INCLUDE_DIR AND VULKAN_LIBRARY)
				MESSAGE(STATUS "Found Vulkan manually: ${VULKAN_INCLUDE_DIR}, ${VULKAN_LIBRARY}")
				TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_VULKAN)
				TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_VULKAN)
				TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${VULKAN_INCLUDE_DIR})
				TARGET_INCLUDE_DIRECTORIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PUBLIC ${VULKAN_INCLUDE_DIR})
				TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${VULKAN_LIBRARY})
			ELSE()
				MESSAGE(WARNING "Vulkan not found, disabling Vulkan support")
				SET(USE_VULKAN OFF CACHE BOOL "" FORCE)
			ENDIF()
		ENDIF()
	ENDIF()
ENDIF()

IF(USE_OPENEXR AND OpenEXR_FOUND AND OPENEXR_LIBRARIES)
	# Attach OpenEXR include paths and libraries to the appropriate targets
	if(USE_RENDERER_DLOPEN)
		if(TARGET ${RENDERER_PREFIX}_opengl${RENDEXT})
			if(OPENEXR_INCLUDE_DIRS)
				target_include_directories(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE ${OPENEXR_INCLUDE_DIRS})
			endif()
			# Use plain signature here to match the existing linkage for this target
			target_link_libraries(${RENDERER_PREFIX}_opengl${RENDEXT} ${OPENEXR_LIBRARIES})
		endif()
		if(TARGET ${RENDERER_PREFIX}_vulkan${RENDEXT})
			if(OPENEXR_INCLUDE_DIRS)
				target_include_directories(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${OPENEXR_INCLUDE_DIRS})
			endif()
			target_link_libraries(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${OPENEXR_LIBRARIES})
		endif()
		if(WIN32 AND USE_D3D12 AND TARGET ${RENDERER_PREFIX}_d3d12${RENDEXT})
			if(OPENEXR_INCLUDE_DIRS)
				target_include_directories(${RENDERER_PREFIX}_d3d12${RENDEXT} PRIVATE ${OPENEXR_INCLUDE_DIRS})
			endif()
			target_link_libraries(${RENDERER_PREFIX}_d3d12${RENDEXT} PRIVATE ${OPENEXR_LIBRARIES})
		endif()
	else()
		# Non-DLOPEN renderers are linked directly into the main executable(s)
		if(OPENEXR_INCLUDE_DIRS)
			target_include_directories(${CNAME}${BINEXT} PRIVATE ${OPENEXR_INCLUDE_DIRS})
			if(BUILD_SERVER AND NOT ANDROID)
				target_include_directories(${DNAME}${BINEXT} PRIVATE ${OPENEXR_INCLUDE_DIRS})
			endif()
		endif()
		target_link_libraries(${CNAME}${BINEXT} ${OPENEXR_LIBRARIES})
		if(BUILD_SERVER AND NOT ANDROID)
			target_link_libraries(${DNAME}${BINEXT} ${OPENEXR_LIBRARIES})
		endif()
	endif()
ENDIF()

IF(ANDROID)
	ADD_LIBRARY(${CNAME}${BINEXT} SHARED ${Q3_SRCS})
ELSE()
	ADD_EXECUTABLE(${CNAME}${BINEXT} ${EXE_TYPE} ${Q3_SRCS})
ENDIF()
TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} qcommon botlib client q3ui)
IF(USE_CIMGUI)
	set_property(TARGET ${CNAME}${BINEXT} PROPERTY LINKER_LANGUAGE CXX)
ENDIF()
IF(UNIX AND NOT ANDROID)
	TARGET_LINK_OPTIONS(${CNAME}${BINEXT} PRIVATE -rdynamic) # Export symbols for dynamically loaded libraries (Unix only)
ENDIF()

IF(USE_SDL)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${SDL2_LIBRARY})
ENDIF()

IF(USE_CURL)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${CURL_LIBRARIES})
ENDIF()

IF(USE_SQLITE)
	IF(SQLite3_FOUND)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${SQLite3_LIBRARIES})
	ENDIF()
ENDIF()

IF(USE_FREETYPE)
	IF(Freetype_FOUND)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${FREETYPE_LIBRARIES})
	ENDIF()
ENDIF()

IF(USE_OPENSSL)
	IF(OpenSSL_FOUND)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${OPENSSL_LIBRARIES})
	ENDIF()
ENDIF()

IF(USE_LUA)
	IF(Lua_FOUND)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${LUA_LIBRARIES})
	ENDIF()
ENDIF()

IF(USE_WEBSOCKETS)
	IF(LibWebSockets_FOUND)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${LIBWEBSOCKETS_LIBRARIES})
	ENDIF()
ENDIF()

IF(USE_OPENAL)
	IF(OpenAL_FOUND AND OPENAL_LIBRARIES)
		IF(TARGET OpenAL::OpenAL)
			# Use modern CMake target
			TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} OpenAL::OpenAL)
		ELSE()
			# Use library variable
			TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${OPENAL_LIBRARIES})
		ENDIF()
	ENDIF()
ENDIF()

IF(USE_ASSIMP)
	IF(ASSIMP_FOUND AND ASSIMP_LIBRARIES)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${ASSIMP_LIBRARIES})
	ENDIF()
ENDIF()

IF(ANDROID)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} android log OpenSLES EGL GLESv1_CM GLESv2)
ENDIF()

IF(BUILD_SERVER AND NOT ANDROID)
	ADD_EXECUTABLE(${DNAME}${BINEXT} ${EXE_TYPE} ${Q3_SRCS})
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} qcommon_ded botlib)

	IF(USE_SDL)
		TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} ${SDL2_LIBRARY})
	ENDIF()

	IF(USE_CURL)
		TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} ${CURL_LIBRARIES})
	ENDIF()

	IF(USE_SQLITE)
		IF(SQLite3_FOUND)
			TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} ${SQLite3_LIBRARIES})
		ENDIF()
	ENDIF()

	IF(USE_OPENSSL)
		IF(OpenSSL_FOUND)
			TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} ${OPENSSL_LIBRARIES})
		ENDIF()
	ENDIF()

	IF(USE_LUA)
		IF(Lua_FOUND)
			TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} ${LUA_LIBRARIES})
		ENDIF()
	ENDIF()

	IF(USE_ASSIMP)
		IF(ASSIMP_FOUND AND ASSIMP_LIBRARIES)
			TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} ${ASSIMP_LIBRARIES})
		ENDIF()
	ENDIF()
ENDIF()

IF(USE_CIMGUI AND TARGET imgui_shared)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} imgui_shared)
	IF(BUILD_SERVER AND NOT ANDROID)
		TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} imgui_shared)
	ENDIF()
	IF(TARGET ${RENDERER_PREFIX}_vulkan${RENDEXT})
		set_target_properties(${RENDERER_PREFIX}_vulkan${RENDEXT} PROPERTIES
			BUILD_RPATH "\$ORIGIN"
			INSTALL_RPATH "\$ORIGIN")
	ENDIF()
	set_target_properties(${CNAME}${BINEXT} PROPERTIES
		BUILD_RPATH "\$ORIGIN"
		INSTALL_RPATH "\$ORIGIN")
	IF(BUILD_SERVER AND NOT ANDROID)
		set_target_properties(${DNAME}${BINEXT} PROPERTIES
			BUILD_RPATH "\$ORIGIN"
			INSTALL_RPATH "\$ORIGIN")
	ENDIF()
ENDIF()

# Platform-specific libraries
IF(WIN32)
	# Windows-specific libraries
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} winmm comctl32 ws2_32)
	IF(BUILD_SERVER AND NOT ANDROID)
		TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} winmm comctl32 ws2_32)
	ENDIF()
ELSE()
	# Unix-specific libraries (math library and dynamic loading)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} m ${CMAKE_DL_LIBS})
	IF(BUILD_SERVER AND NOT ANDROID)
		TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} m ${CMAKE_DL_LIBS})
	ENDIF()
ENDIF()

# q3ui - includes both platform UI sources (Q3_UI_SRCS) and game UI sources (Q3UI_SRCS)
# Q3_UI_SRCS contains platform-specific UI code (SDL/X11/Windows), Q3UI_SRCS contains game UI code
AUX_SOURCE_DIRECTORY(src/ui Q3UI_SRCS)
IF(Q3UI_SRCS)
	# Combine platform UI sources with game UI sources
	ADD_LIBRARY(q3ui OBJECT ${Q3_UI_SRCS} ${Q3UI_SRCS})
ELSE()
	# If no game UI sources, use platform UI sources only
	IF(Q3_UI_SRCS)
		ADD_LIBRARY(q3ui OBJECT ${Q3_UI_SRCS})
	ELSE()
		# Create dummy source file if no UI sources found at all
		FILE(WRITE ${CMAKE_BINARY_DIR}/q3ui_dummy.c "// Dummy file for q3ui when no sources are present\n")
		ADD_LIBRARY(q3ui OBJECT ${CMAKE_BINARY_DIR}/q3ui_dummy.c)
		MESSAGE(STATUS "No UI source files found, using dummy q3ui library")
	ENDIF()
ENDIF()

# Add SDL include directories to q3ui if SDL is enabled
IF(USE_SDL)
	TARGET_INCLUDE_DIRECTORIES(q3ui PUBLIC ${SDL2_INCLUDE_DIR})
ENDIF()

# Add USE_VULKAN definition to q3ui if Vulkan is enabled (needed for sdl_glimp.c)
IF(USE_VULKAN)
	TARGET_COMPILE_DEFINITIONS(q3ui PRIVATE USE_VULKAN)
ENDIF()

# Unit tests
OPTION(BUILD_TESTS "Build unit tests" OFF)
IF(BUILD_TESTS)
	ENABLE_TESTING()
	ADD_EXECUTABLE(test_qcommon tests/test_qcommon.c src/qcommon/q_shared.c)
	TARGET_INCLUDE_DIRECTORIES(test_qcommon PRIVATE src/qcommon)
	TARGET_INCLUDE_DIRECTORIES(test_qcommon PRIVATE src)
	ADD_TEST(NAME qcommon_tests COMMAND test_qcommon)
	MESSAGE(STATUS "Unit tests enabled")
ENDIF()
