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

# Enable modern compiler warnings for better code quality
IF(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	# Format string checking (catches printf format errors)
	ADD_COMPILE_OPTIONS(-Wformat=2)
	# Require function prototypes
	ADD_COMPILE_OPTIONS(-Wstrict-prototypes)
	# Warn about missing prototypes
	ADD_COMPILE_OPTIONS(-Wmissing-prototypes)
	# Warn about shadowed variables
	ADD_COMPILE_OPTIONS(-Wshadow)
	# Note: -Wold-style-cast is C++ only, not applicable for C code
ENDIF()

SET(CNAME "idtech3")
SET(DNAME "idtech3.ded")

SET(RENDERER_PREFIX ${CNAME})
SET(RENDERER_DEFAULT vulkan) # valid options: opengl, vulkan, opengl2

OPTION(USE_SDL "" ON)
OPTION(USE_CURL "" ON)
OPTION(USE_LOCAL_HEADERS "" ON)
OPTION(USE_VULKAN "" ON)
OPTION(USE_SYSTEM_JPEG "" OFF)
OPTION(USE_RENDERER_DLOPEN "" ON)
OPTION(USE_CJSON "" ON)
OPTION(USE_ZSTD "" ON)
OPTION(USE_ENET "" ON)

SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules)

AUX_SOURCE_DIRECTORY(code/qcommon QCOMMON_SRCS)
# exclude platform-dependent QVM bytecode compilers
list(FILTER QCOMMON_SRCS EXCLUDE REGEX ".*vm_[alx].*.c")
# Add wrapper files conditionally
IF(USE_CJSON)
	list(APPEND QCOMMON_SRCS code/qcommon/json_cjson.c)
ENDIF()
IF(USE_ZSTD)
	list(APPEND QCOMMON_SRCS code/qcommon/zstd_compress.c)
ENDIF()
IF(USE_ENET)
	list(APPEND QCOMMON_SRCS code/qcommon/net_enet.c)
ENDIF()
# add platform-dependent QVM bytecode compilers
# Force 64-bit builds by default
IF(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
	SET(BINEXT .aarch64)
	SET(RENDEXT _aarch64)
	list(APPEND QCOMMON_SRCS code/qcommon/vm_aarch64.c)
ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x86|i*86")
	# Default to 64-bit for x86 architectures
	IF(WIN32)
		SET(BINEXT .x64)
	ELSE()
		SET(BINEXT .x86_64)
	ENDIF()
	SET(RENDEXT _x86_64)
	list(APPEND QCOMMON_SRCS code/qcommon/vm_x86.c)
ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
	SET(BINEXT .arm)
	SET(RENDEXT _arm)
	list(APPEND QCOMMON_SRCS code/qcommon/vm_armv7l.c)
ELSE()
	TARGET_COMPILE_DEFINITIONS(qcommon PRIVATE -DNO_VM_COMPILED)
	TARGET_COMPILE_DEFINITIONS(qcommon_ded PRIVATE -DNO_VM_COMPILED)
ENDIF()

# botlib
AUX_SOURCE_DIRECTORY(code/botlib BOTLIB_SRCS)
ADD_LIBRARY(botlib OBJECT ${BOTLIB_SRCS})
TARGET_COMPILE_DEFINITIONS(botlib PRIVATE BOTLIB)

AUX_SOURCE_DIRECTORY(code/server SERVER_SRCS)
list(REMOVE_ITEM SERVER_SRCS code/server/sv_rankings.c)

# universal binary
ADD_LIBRARY(qcommon OBJECT ${QCOMMON_SRCS} ${SERVER_SRCS})

# dedicated server
ADD_LIBRARY(qcommon_ded OBJECT ${QCOMMON_SRCS} ${SERVER_SRCS})
TARGET_COMPILE_DEFINITIONS(qcommon_ded PUBLIC DEDICATED)

# client + cURL
AUX_SOURCE_DIRECTORY(code/client CLIENT_SRCS)
IF(NOT USE_CURL)
	list(REMOVE_ITEM CLIENT_SRCS code/client/cl_curl.c)
ENDIF()

# Add assembly files for x86_64
IF(CMAKE_SYSTEM_PROCESSOR MATCHES AMD64|x86_64)
	list(APPEND CLIENT_SRCS code/asm/snd_mix_x86_64.s)
	set_source_files_properties(code/asm/snd_mix_x86_64.s PROPERTIES
		COMPILE_FLAGS "-x assembler-with-cpp -DELF"
		LANGUAGE ASM)
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
ENDIF()

# Add OGG/Vorbis include directories for client (used by snd_codec_ogg.c)
TARGET_INCLUDE_DIRECTORIES(client PRIVATE libs/ogg/include libs/vorbis/include)

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
AUX_SOURCE_DIRECTORY(code/renderercommon RENDERER_COMMON_SRCS)
AUX_SOURCE_DIRECTORY(code/renderer RENDERER_GL_SRCS)
AUX_SOURCE_DIRECTORY(code/renderervk RENDERER_VK_SRCS)

IF(USE_RENDERER_DLOPEN)
	SET(AUX_SRCS
		code/qcommon/q_shared.c
		code/qcommon/puff.c
		code/qcommon/q_math.c
	)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_RENDERER_DLOPEN RENDERER_PREFIX="${RENDERER_PREFIX}" RENDERER_DEFAULT=${RENDERER_DEFAULT})

	ADD_LIBRARY(${RENDERER_PREFIX}_opengl${RENDEXT} SHARED ${RENDERER_GL_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
	TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE USE_RENDERER_DLOPEN)
	TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_opengl${RENDEXT} m)

	ADD_LIBRARY(${RENDERER_PREFIX}_vulkan${RENDEXT} SHARED ${RENDERER_VK_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
	TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_RENDERER_DLOPEN)
	TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_vulkan${RENDEXT} m)
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
		code/unix/unix_main.c
		code/unix/unix_shared.c
		code/unix/linux_signals.c)

	IF(USE_SDL)
		AUX_SOURCE_DIRECTORY(code/sdl Q3_UI_SRCS)
	ELSE()
		SET(Q3_UI_SRCS
			code/unix/linux_glimp.c
			code/unix/linux_qgl.c
			code/unix/linux_snd.c
			code/unix/x11_dga.c
			code/unix/x11_randr.c
			code/unix/x11_vidmode.c)
		IF(USE_VULKAN)
			list(APPEND Q3_UI_SRCS code/unix/linux_qvk.c)
		ENDIF()
	ENDIF()

	IF(APPLE)
		SET(EXE_TYPE MACOSX_BUNDLE)
		list(APPEND Q3_SRCS code/unix/quake3_flat.icns)
		set_source_files_properties(code/unix/quake3_flat.icns PROPERTIES
			MACOSX_PACKAGE_LOCATION Resources)
		SET(MACOSX_BUNDLE_ICON_FILE quake3_flat.icns)
	ENDIF()
ELSEIF(WIN32)
	SET(EXE_TYPE WIN32)
	AUX_SOURCE_DIRECTORY(code/win32 Q3_SRCS)
	AUX_SOURCE_DIRECTORY(code/win32 Q3_UI_SRCS)
ENDIF()

IF(USE_SDL)
	find_package(SDL2 REQUIRED)
	TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${SDL2_INCLUDE_DIR})
	TARGET_INCLUDE_DIRECTORIES(qcommon PUBLIC ${SDL2_INCLUDE_DIR})
	TARGET_INCLUDE_DIRECTORIES(qcommon_ded PUBLIC ${SDL2_INCLUDE_DIR})
ENDIF()

IF(USE_CURL)
	find_package(CURL)
	IF(CURL_FOUND)
		TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${CURL_INCLUDE_DIR})
		TARGET_INCLUDE_DIRECTORIES(qcommon PUBLIC ${CURL_INCLUDE_DIR})
		TARGET_INCLUDE_DIRECTORIES(qcommon_ded PUBLIC ${CURL_INCLUDE_DIR})
	ELSE()
		MESSAGE(WARNING "CURL not found, disabling CURL support")
		SET(USE_CURL OFF CACHE BOOL "" FORCE)
		list(REMOVE_ITEM CLIENT_SRCS code/client/cl_curl.c)
	ENDIF()
ENDIF()

IF(USE_VULKAN)
	find_package(Vulkan)
	IF(Vulkan_FOUND)
		TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${Vulkan_INCLUDE_DIRS})
		TARGET_INCLUDE_DIRECTORIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PUBLIC ${Vulkan_INCLUDE_DIRS})
		TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_vulkan${RENDEXT} ${Vulkan_LIBRARIES})
	ELSE()
		MESSAGE(WARNING "Vulkan not found, disabling Vulkan support")
		SET(USE_VULKAN OFF CACHE BOOL "" FORCE)
	ENDIF()
ENDIF()

ADD_EXECUTABLE(${CNAME}${BINEXT} ${EXE_TYPE} ${Q3_SRCS})
TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} qcommon botlib client q3ui)
IF(UNIX)
	TARGET_LINK_OPTIONS(${CNAME}${BINEXT} PRIVATE -rdynamic) # Export symbols for dynamically loaded libraries (Unix only)
ENDIF()

IF(USE_SDL)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${SDL2_LIBRARY})
ENDIF()

IF(USE_CURL)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} ${CURL_LIBRARIES})
ENDIF()

ADD_EXECUTABLE(${DNAME}${BINEXT} ${EXE_TYPE} ${Q3_SRCS})
TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} qcommon_ded botlib)

IF(USE_SDL)
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} ${SDL2_LIBRARY})
ENDIF()

IF(USE_CURL)
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} ${CURL_LIBRARIES})
ENDIF()

# Platform-specific libraries
IF(WIN32)
	# Windows-specific libraries
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} winmm comctl32 ws2_32)
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} winmm comctl32 ws2_32)
ELSE()
	# Unix-specific libraries (math library and dynamic loading)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} m ${CMAKE_DL_LIBS})
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} m ${CMAKE_DL_LIBS})
ENDIF()

# q3ui - includes both platform UI sources (Q3_UI_SRCS) and game UI sources (Q3UI_SRCS)
# Q3_UI_SRCS contains platform-specific UI code (SDL/X11/Windows), Q3UI_SRCS contains game UI code
AUX_SOURCE_DIRECTORY(code/ui Q3UI_SRCS)
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
