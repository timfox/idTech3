# ==============================================================================
# id Tech 3 Build System - CMake Configuration
# ==============================================================================

cmake_minimum_required(VERSION 3.20)
project(idtech3 C CXX ASM)

# Find dependencies
find_package(OpenGL QUIET)
find_package(Lua QUIET)
if(Lua_FOUND)
    # Create Lua::lua imported target for consistent linking across all targets
    if(NOT TARGET Lua::lua)
        add_library(Lua::lua INTERFACE IMPORTED)
        set_target_properties(Lua::lua PROPERTIES
            INTERFACE_INCLUDE_DIRECTORIES "${LUA_INCLUDE_DIRS}"
            INTERFACE_LINK_LIBRARIES "${LUA_LIBRARIES}"
        )
        message(STATUS "Created Lua::lua imported target with includes: ${LUA_INCLUDE_DIRS} libs: ${LUA_LIBRARIES}")
    endif()
else()
    message(WARNING "Lua not found - Lua integration will be disabled")
    set(USE_LUA OFF CACHE BOOL "Lua not found" FORCE)
endif()

# Prefer colored compiler diagnostics when supported
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
	set(CMAKE_COLOR_DIAGNOSTICS ON)
endif()
set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
add_compile_options($<$<OR:$<C_COMPILER_ID:GNU>,$<C_COMPILER_ID:Clang>>:-fdiagnostics-color=always>)

# Simple helper to print colored headings during configure
# Use ANSI escapes; double-backslash to survive CMake parsing
set(COL_RESET "\\033[0m")
set(COL_TITLE "\\033[1;36m") # bright cyan
set(COL_STEP  "\\033[1;35m") # bright magenta
set(COL_OK    "\\033[1;32m") # bright green
set(COL_WARN  "\\033[1;33m") # bright yellow
set(COL_ERROR "\\033[1;31m") # bright red

function(color_msg COLOR TEXT)
	if (CMAKE_COLOR_MAKEFILE)
		message("${COLOR}${TEXT}${COL_RESET}")
	else()
		message("${TEXT}")
	endif()
endfunction()

color_msg("${COL_TITLE}" "=== Configuring id Tech 3 build ===")
color_msg("${COL_STEP}"  "-- Compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")

# ------------------------------------------------------------------------------
# Language Standard Configuration
# ------------------------------------------------------------------------------
# Set C17 as default for broad compatibility, C23 for GCC/Clang
IF(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    SET(CMAKE_C_STANDARD 23)
    SET(CMAKE_C_STANDARD_REQUIRED ON)
    MESSAGE(STATUS "Using C23 standard")
ELSE()
    SET(CMAKE_C_STANDARD 17)
    SET(CMAKE_C_STANDARD_REQUIRED OFF)  # Allow fallback for MSVC
    MESSAGE(STATUS "Using C17 standard (MSVC or other compiler)")
ENDIF()
SET(CMAKE_C_EXTENSIONS OFF)

# Set C++17 as default for broad compatibility, C++23 for GCC/Clang
IF(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    SET(CMAKE_CXX_STANDARD 23)
    SET(CMAKE_CXX_STANDARD_REQUIRED ON)
    MESSAGE(STATUS "Using C++23 standard")
ELSE()
    SET(CMAKE_CXX_STANDARD 17)
    SET(CMAKE_CXX_STANDARD_REQUIRED OFF)  # Allow fallback for MSVC
    MESSAGE(STATUS "Using C++17 standard (MSVC or other compiler)")
ENDIF()
SET(CMAKE_CXX_EXTENSIONS OFF)

# Enable modern compiler warnings for better code quality
IF(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	ADD_COMPILE_OPTIONS(
		-Wall
		-Wextra
		-Wpedantic
		-Wformat=2
		-Wshadow
		-Wunused-parameter
		-Wunused-variable
		$<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>
		$<$<COMPILE_LANGUAGE:C>:-Wmissing-prototypes>
	)
ELSEIF(MSVC)
	ADD_COMPILE_OPTIONS(/W4)
ENDIF()

# Sanitizer & coverage support (GCC/Clang only, requires debug build)
IF(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
	IF(ENABLE_ASAN)
		MESSAGE(STATUS "AddressSanitizer (ASan) enabled (configured per-target)")
	ENDIF()
	
	IF(ENABLE_UBSAN)
		MESSAGE(STATUS "UndefinedBehaviorSanitizer (UBSan) enabled (configured per-target)")
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

	IF(ENABLE_COVERAGE)
		IF(ENABLE_ASAN OR ENABLE_UBSAN OR ENABLE_MSAN OR ENABLE_TSAN)
			MESSAGE(WARNING "Coverage with sanitizers can produce unusable reports; consider disabling sanitizers.")
		ENDIF()
		ADD_COMPILE_OPTIONS(-g -O0 --coverage)
		ADD_LINK_OPTIONS(--coverage)
		SET(COVERAGE_ENABLED TRUE)
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
OPTION(ENABLE_ASAN "Enable AddressSanitizer (ASan) for memory error detection" ON)
OPTION(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer (UBSan) for undefined behavior detection" ON)
OPTION(ENABLE_MSAN "Enable MemorySanitizer (MSan) for uninitialized memory detection" OFF)
OPTION(ENABLE_TSAN "Enable ThreadSanitizer (TSan) for data race detection" OFF)
OPTION(ENABLE_VALGRIND "Enable Valgrind integration (debug symbols, no optimization)" OFF)
OPTION(ENABLE_COVERAGE "Enable code coverage instrumentation (gcc/clang, Debug)" OFF)
OPTION(ENABLE_MEMORY_TRACKING "Enable memory usage tracking and statistics" ON)
OPTION(USE_TRACY "Enable Tracy Profiler integration for performance analysis" OFF)

# Windows-specific memory analysis
OPTION(ENABLE_DRMEMORY "Enable Dr. Memory integration (Windows only)" OFF)

# Static analysis options
OPTION(ENABLE_STATIC_ANALYSIS "Enable static analysis tools (clang-tidy, cppcheck)" OFF)
OPTION(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)
OPTION(ENABLE_CPPCHECK "Enable cppcheck static analysis" OFF)

IF(ENABLE_STATIC_ANALYSIS OR ENABLE_CLANG_TIDY OR ENABLE_CPPCHECK)
	# Try to find clang-tidy
	FIND_PROGRAM(CLANG_TIDY_EXE NAMES clang-tidy)
	IF((ENABLE_STATIC_ANALYSIS OR ENABLE_CLANG_TIDY) AND CLANG_TIDY_EXE)
		# Use .clang-tidy configuration file if it exists
		IF(EXISTS "${CMAKE_SOURCE_DIR}/.clang-tidy")
			SET(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE}")
			SET(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
			MESSAGE(STATUS "Static analysis: clang-tidy enabled (using .clang-tidy config)")
		ELSE()
			# Fallback to basic checks
			SET(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE}" "-checks=-*,readability-*,performance-*,portability-*,bugprone-*")
			SET(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}" "-checks=-*,readability-*,performance-*,portability-*,bugprone-*")
			MESSAGE(STATUS "Static analysis: clang-tidy enabled (basic checks)")
		ENDIF()
	ELSEIF((ENABLE_STATIC_ANALYSIS OR ENABLE_CLANG_TIDY) AND NOT CLANG_TIDY_EXE)
		MESSAGE(WARNING "Static analysis: clang-tidy not found")
		MESSAGE(STATUS "Install clang-tidy: sudo apt install clang-tidy")
	ENDIF()

	# Try to find cppcheck
	FIND_PROGRAM(CPPCHECK_EXE NAMES cppcheck)
	IF((ENABLE_STATIC_ANALYSIS OR ENABLE_CPPCHECK) AND CPPCHECK_EXE)
		# Use cppcheck.cfg configuration file if it exists
		IF(EXISTS "${CMAKE_SOURCE_DIR}/cppcheck.cfg")
			SET(CMAKE_C_CPPCHECK "${CPPCHECK_EXE}" "--config-exclude=${CMAKE_SOURCE_DIR}/cppcheck.cfg")
			SET(CMAKE_CXX_CPPCHECK "${CPPCHECK_EXE}" "--config-exclude=${CMAKE_SOURCE_DIR}/cppcheck.cfg")
			MESSAGE(STATUS "Static analysis: cppcheck enabled (using cppcheck.cfg config)")
		ELSE()
			# Fallback to basic configuration
			SET(CMAKE_C_CPPCHECK "${CPPCHECK_EXE}" "--enable=all" "--suppress=missingIncludeSystem" "--inline-suppr")
			SET(CMAKE_CXX_CPPCHECK "${CPPCHECK_EXE}" "--enable=all" "--suppress=missingIncludeSystem" "--inline-suppr")
			MESSAGE(STATUS "Static analysis: cppcheck enabled (basic config)")
		ENDIF()
	ELSEIF((ENABLE_STATIC_ANALYSIS OR ENABLE_CPPCHECK) AND NOT CPPCHECK_EXE)
		MESSAGE(WARNING "Static analysis: cppcheck not found")
		MESSAGE(STATUS "Install cppcheck: sudo apt install cppcheck")
	ENDIF()
ENDIF()

# Coverage reporting target (gcovr/lcov) if coverage instrumentation is enabled
FIND_PROGRAM(GCOVR_EXE NAMES gcovr)
FIND_PROGRAM(LCOV_EXE NAMES lcov)
FIND_PROGRAM(GENHTML_EXE NAMES genhtml)

IF(ENABLE_COVERAGE)
	# Run tests first to generate coverage data
	IF(BUILD_TESTS)
		FIND_PROGRAM(CTEST_EXE NAMES ctest)
		IF(CTEST_EXE)
			ADD_CUSTOM_TARGET(coverage_run_tests
				COMMAND ${CTEST_EXE} --output-on-failure
				WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
				COMMENT "Running tests to generate coverage data"
			)
		ELSE()
			ADD_CUSTOM_TARGET(coverage_run_tests
				COMMAND ${CMAKE_COMMAND} -E echo "Warning: ctest not found, skipping test execution"
				COMMENT "Skipping test execution (ctest not found)"
			)
		ENDIF()
	ELSE()
		# If tests aren't built, create a dummy target
		ADD_CUSTOM_TARGET(coverage_run_tests
			COMMAND ${CMAKE_COMMAND} -E echo "Warning: BUILD_TESTS is OFF, no tests to run"
			COMMENT "Skipping test execution (BUILD_TESTS=OFF)"
		)
	ENDIF()

	# gcovr-based coverage report (preferred)
	IF(GCOVR_EXE)
		ADD_CUSTOM_TARGET(coverage
			DEPENDS coverage_run_tests
			COMMAND ${GCOVR_EXE}
				--root ${CMAKE_SOURCE_DIR}
				--filter ${CMAKE_SOURCE_DIR}/src
				--filter ${CMAKE_SOURCE_DIR}/tests
				--exclude ${CMAKE_SOURCE_DIR}/libs
				--exclude ${CMAKE_BINARY_DIR}
				--exclude ".*/tests/.*"
				--print-summary
				--html --html-details -o coverage.html
				--xml -o coverage.xml
				--txt -o coverage.txt
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
			COMMENT "Generating coverage report with gcovr"
		)
		MESSAGE(STATUS "Coverage: gcovr found - 'coverage' target available")
	ELSE()
		MESSAGE(WARNING "gcovr not found; install with: sudo apt install gcovr")
	ENDIF()

	# lcov-based coverage report (alternative)
	IF(LCOV_EXE AND GENHTML_EXE)
		ADD_CUSTOM_TARGET(coverage_lcov
			DEPENDS coverage_run_tests
			COMMAND ${LCOV_EXE} --capture --directory . --output-file coverage.info --ignore-errors mismatch
			COMMAND ${LCOV_EXE} --remove coverage.info '*/libs/*' '*/tests/*' '*/build/*' --output-file coverage.info
			COMMAND ${GENHTML_EXE} coverage.info --output-directory coverage_lcov --legend --show-details
			WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
			COMMENT "Generating coverage report with lcov"
		)
		MESSAGE(STATUS "Coverage: lcov/genhtml found - 'coverage_lcov' target available")
	ELSEIF(ENABLE_COVERAGE AND NOT LCOV_EXE)
		MESSAGE(STATUS "Coverage: lcov not found (optional) - install with: sudo apt install lcov")
	ENDIF()
ENDIF()

# ------------------------------------------------------------------------------
# GtkRadiant (editor + tools) toggle
# ------------------------------------------------------------------------------
OPTION(BUILD_RADIANT "Build GtkRadiant editor, tools, and plugins (requires GTK2/GTKGLext)" OFF)
IF(ANDROID AND BUILD_RADIANT)
	MESSAGE(WARNING "GtkRadiant is not supported on Android; disabling BUILD_RADIANT")
	SET(BUILD_RADIANT OFF CACHE BOOL "" FORCE)
ENDIF()

# ==== SDL2 Integration ====
# SDL2 is optional; some CI jobs (notably MSVC) don't have it installed.
# Default to ON where discovery is typically available, but keep Windows default OFF.
if(WIN32)
	set(USE_SDL_DEFAULT OFF)
else()
	set(USE_SDL_DEFAULT ON)
endif()
OPTION(USE_SDL "Enable SDL2 integration" ${USE_SDL_DEFAULT})

if(USE_SDL)
	# Use CMake's built-in FindSDL2 or pkg-config fallback
	find_package(PkgConfig)
	if(PKG_CONFIG_FOUND)
		pkg_check_modules(SDL2 IMPORTED_TARGET sdl2)
	endif()

	find_package(SDL2 QUIET)
	if(NOT SDL2_FOUND)
		# If built-in not found, try imported target from pkg-config
		if(TARGET PkgConfig::SDL2)
			add_library(SDL2::SDL2 ALIAS PkgConfig::SDL2)
			set(SDL2_FOUND TRUE)
		endif()
	endif()

	if(SDL2_FOUND)
		message(STATUS "SDL2 library found: using SDL2::SDL2")
		set(SDL2_MAIN_LIBRARY SDL2::SDL2)
		set(SDL2_INCLUDE_DIRS "")
		if(DEFINED SDL2_INCLUDE_DIR)
			list(APPEND SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR})
		endif()
		if(DEFINED SDL2_INCLUDE_DIRS_)
			list(APPEND SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS_})
		endif()
		set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS} ${SDL2_INCLUDE_DIRS})
	else()
		# Don't hard-fail configure; builds that don't need SDL should still work.
		message(WARNING "SDL2 could not be found; disabling USE_SDL (set SDL2_DIR to enable).")
		set(USE_SDL OFF CACHE BOOL "" FORCE)
	endif()
endif()

# Application naming configuration
# Check for command line mod name override
if(DEFINED MOD_NAME AND NOT "${MOD_NAME}" STREQUAL "")
    # Normalize mod name early for use in target naming
    string(TOLOWER "${MOD_NAME}" MOD_NAME_LOWER)
    string(REGEX REPLACE "[^a-z0-9]" "" MOD_NAME_CLEAN "${MOD_NAME_LOWER}")

    set(ENGINE_NAME "${MOD_NAME}" CACHE STRING "Engine display name")
    set(CNAME "${MOD_NAME_CLEAN}" CACHE STRING "Main executable/library name")
    set(DNAME "${MOD_NAME_CLEAN}.server" CACHE STRING "Dedicated server executable name")
    # Generate package name from mod name
    set(APP_PACKAGE_NAME "com.${MOD_NAME_CLEAN}.game" CACHE STRING "Android package name")
    set(APP_DISPLAY_NAME "${MOD_NAME}" CACHE STRING "Application display name")
else()
    set(ENGINE_NAME "idTech3" CACHE STRING "Engine display name")
    set(CNAME "idtech3" CACHE STRING "Main executable/library name")
    set(DNAME "idtech3.server" CACHE STRING "Dedicated server executable name")
    set(APP_PACKAGE_NAME "com.idtech3.engine" CACHE STRING "Android package name")
    set(APP_DISPLAY_NAME "${ENGINE_NAME}" CACHE STRING "Application display name")
endif()

# Allow direct override of display name
set(APP_DISPLAY_NAME "${APP_DISPLAY_NAME}" CACHE STRING "Application display name (overrides ENGINE_NAME)")
set(APP_PACKAGE_NAME "${APP_PACKAGE_NAME}" CACHE STRING "Android package name")

SET(RENDERER_PREFIX ${CNAME})
set(RENDERER_DEFAULT "opengl" CACHE STRING "Default renderer (opengl, vulkan, opengl2, d3d12, metal)")

# Some configurations (notably monolithic Vulkan without dlopen) are not fully supported yet.
# Prefer the dlopen renderer path to keep builds reproducible (and to provide `make install`).
if(NOT USE_RENDERER_DLOPEN AND RENDERER_DEFAULT STREQUAL "vulkan")
	message(WARNING "USE_RENDERER_DLOPEN=OFF with RENDERER_DEFAULT=vulkan is not supported yet; forcing USE_RENDERER_DLOPEN=ON.")
	set(USE_RENDERER_DLOPEN ON CACHE BOOL "" FORCE)
endif()

# Installation layout
include(GNUInstallDirs)

OPTION(USE_CURL "" ON)
OPTION(USE_LOCAL_HEADERS "" ON)
OPTION(USE_VULKAN "" ON)
OPTION(USE_VMA "Enable Vulkan Memory Allocator (VMA) for better memory management" ON)
OPTION(USE_D3D12 "Enable DirectX 12 renderer (Windows only)" ON)
OPTION(USE_METAL "Enable Metal renderer (macOS/iOS only)" ON)
OPTION(USE_ANDROID "Enable Android platform support" ON)
OPTION(USE_SYSTEM_JPEG "" OFF)
OPTION(USE_RENDERER_DLOPEN "" ON)
OPTION(USE_OPENEXR "Enable OpenEXR (.exr) image support" ON)
OPTION(USE_NANOSVG "Enable SVG (Scalable Vector Graphics) image support" ON)
OPTION(USE_ASSIMP "Enable Assimp 3D asset import library" ON)
OPTION(USE_CJSON "" ON)
OPTION(USE_ZSTD "" ON)
OPTION(USE_ENET "" ON)
OPTION(USE_SQLITE "" ON)
OPTION(USE_FREETYPE "" ON)
OPTION(USE_BROTLI "" ON)
OPTION(USE_OPENSSL "" ON)
OPTION(USE_AIML "Enable AIML 2.1 parser/interpreter support" ON)
OPTION(USE_ENTT "Enable EnTT ECS system" ON)
OPTION(USE_STEAMWORKS "Enable Steamworks SDK integration for Steam Deck features" OFF)
OPTION(USE_TRACY "Enable Tracy profiler instrumentation (header-only shim)" OFF)
OPTION(USE_CIMGUI "Enable ImGui" ON)
OPTION(USE_WEBSOCKETS "Enable WebSocket support" ON)
OPTION(USE_OPENAL "Enable OpenAL enhanced audio system" ON)
OPTION(USE_BULLET "Enable Bullet Physics library for rigid body simulation" ON)
OPTION(USE_JOBSYSTEM "Enable multi-threaded job system for parallel execution" ON)
OPTION(USE_THEORA "Enable Theora (Ogg Theora) video codec support" ON)
OPTION(USE_VPX "Enable VP8/VP9 (WebM) video codec support" ON)
OPTION(USE_DAV1D "Enable AV1 video codec support (dav1d decoder)" ON)
OPTION(USE_STB_TRUETYPE "Enable stb_truetype font baking (SDF/TTF runtime atlases)" ON)
# Engine targets
OPTION(BUILD_SERVER "Build dedicated server executable" ON)
#OPTION(USE_OPENGL "" OFF)
#OPTION(USE_OPENGL2 "" OFF)
#OPTION(USE_USD "" ON)
#OPTION(USE_GLTF "" ON)

IF(ANDROID)
	MESSAGE(STATUS "Configuring Android build for ABI: ${CMAKE_ANDROID_ARCH_ABI}")
	SET(BUILD_SERVER OFF CACHE BOOL "" FORCE)
	SET(BUILD_TESTS OFF CACHE BOOL "" FORCE)
	SET(USE_RENDERER_DLOPEN OFF CACHE BOOL "" FORCE)
ENDIF()

IF(USE_STB_TRUETYPE)
	add_compile_definitions(USE_STB_TRUETYPE)
ENDIF()

IF(USE_VULKAN)
	add_compile_definitions(USE_VULKAN)
ENDIF()

# Reproducible builds: pass SOURCE_DATE_EPOCH as compile definition if set
IF(DEFINED ENV{SOURCE_DATE_EPOCH})
	add_compile_definitions(SOURCE_DATE_EPOCH="$ENV{SOURCE_DATE_EPOCH}")
ENDIF()

SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules)

SET(QCOMMON_SRCS
    src/common/cm_load.c
    src/common/cm_patch.c
    src/common/cm_polylib.c
    src/common/cm_test.c
    src/common/cm_trace.c
    src/common/cmd.c
    src/common/common.c
    src/common/crash_handler.c
    src/common/cvar.c
    src/common/q_lockfree.c
    src/common/ecs.cpp
    src/common/ecs_systems.cpp
    src/common/entity_facade.cpp
    src/common/event_system.c
    src/common/files.c
    src/common/files_v2.c
    src/common/files_v2_impl.c
    src/common/freetype_wrapper.c
    src/common/history.c
    src/common/huffman.c
    src/common/huffman_static.c
    src/common/i18n.c
    src/common/job_queue.c
    src/common/job_system.c
    src/common/json_cjson.c
    src/common/keys.c
    src/common/lua_coroutine.c
    src/common/lua_debug.c
    src/common/lua_encounter.c
    src/common/lua_entity.cpp
    src/common/lua_events.c
    src/common/lua_game.cpp
    src/common/lua_renderer.c
    src/common/lua_sequence.c
    src/common/lua_sound.c
    src/common/lua_wrapper.c
    src/common/md4.c
    src/common/md5.c
    src/common/memory_stats.c
    src/common/msg.c
    src/common/msg_secure.c
    src/common/net_chan.c
    src/common/net_enet.c
    src/common/net_ip.c
    src/common/oop_services.cpp
    src/common/openssl_wrapper.c
    src/common/performance_counters.c
    src/common/puff.c
    src/common/q_assert.c
    src/common/q_log.c
    src/common/q_math.c
    src/common/q_memory_safety.c
    src/common/q_memtrack.c
    src/common/q_shared.c
    src/common/q_scalability.c
    src/common/q_asset_loaders.c
    src/common/q_telemetry.c
    src/common/q_watchdog.c
    src/common/sqlite_wrapper.c
    src/common/syscall_registry.c
    src/common/thread_platform.c
    src/common/unzip.c
    src/common/vm.c
    src/common/vm_aarch64.c
    src/common/vm_armv7l.c
    src/common/vm_hot_reload.c
    src/common/vm_interpreted.c
    src/common/vm_x86.c
    src/common/zstd_compress.c
)
# exclude platform-dependent QVM bytecode compilers
list(FILTER QCOMMON_SRCS EXCLUDE REGEX ".*vm_[alx].*.c")
# Add structured logging system
list(APPEND QCOMMON_SRCS src/common/q_log.c)
# Add performance counters
list(APPEND QCOMMON_SRCS src/common/performance_counters.c)
# Add memory safety utilities
list(APPEND QCOMMON_SRCS src/common/q_memory_safety.c)
# Add stability framework
list(APPEND QCOMMON_SRCS src/common/q_stability.c)
# Add error recovery system
list(APPEND QCOMMON_SRCS src/common/q_error_recovery.c)
# Add input validation system
list(APPEND QCOMMON_SRCS src/common/q_input_validation.c)
# Add font effects system
# Font effects are renderer-specific, not common
# list(APPEND QCOMMON_SRCS src/renderers/renderercommon/tr_font_effects.c)
# Add secure message functions
list(APPEND QCOMMON_SRCS src/common/msg_secure.c)
# Add secure message functions
list(APPEND QCOMMON_SRCS src/common/msg_secure.c)
# Add memory statistics tracking
list(APPEND QCOMMON_SRCS src/common/memory_stats.c)
# Add VM hot reload system
list(APPEND QCOMMON_SRCS src/common/vm_hot_reload.c)
# Add event system
list(APPEND QCOMMON_SRCS src/common/event_system.c)
# Add crash handler and assert system
list(APPEND QCOMMON_SRCS src/common/crash_handler.c)
list(APPEND QCOMMON_SRCS src/common/q_assert.c)
# Add watchdog system
list(APPEND QCOMMON_SRCS src/common/q_watchdog.c)
list(APPEND QCOMMON_SRCS src/common/files_v2.c)
list(APPEND QCOMMON_SRCS src/common/files_v2_impl.c)
# Add wrapper files conditionally
IF(USE_CJSON)
	list(APPEND QCOMMON_SRCS src/common/json_cjson.c)
ENDIF()
IF(USE_ZSTD)
	list(APPEND QCOMMON_SRCS src/common/zstd_compress.c)
ENDIF()
IF(USE_ENET)
	list(APPEND QCOMMON_SRCS src/common/net_enet.c)
ENDIF()

IF(USE_SQLITE)
	list(APPEND QCOMMON_SRCS src/common/sqlite_wrapper.c)
ENDIF()

IF(USE_OPENSSL)
	list(APPEND QCOMMON_SRCS src/common/openssl_wrapper.c)
ENDIF()


IF(USE_AIML)
	list(APPEND QCOMMON_SRCS
		src/common/aiml/aiml_parser.cpp
		src/common/aiml/aiml_interpreter.cpp
		src/common/aiml/aiml_c_api.cpp
	)
	ADD_COMPILE_DEFINITIONS(USE_AIML)
ENDIF()

# Internationalization (i18n) support
list(APPEND QCOMMON_SRCS src/common/i18n.c)

IF(USE_FREETYPE)
	list(APPEND QCOMMON_SRCS src/common/freetype_wrapper.c)
ENDIF()

# EnTT ECS system
IF(USE_ENTT)
	# Add EnTT include directory
	INCLUDE_DIRECTORIES(${CMAKE_CURRENT_SOURCE_DIR}/libs/entt/src)
	# Add compile definition
	ADD_COMPILE_DEFINITIONS(USE_ENTT)
	# Add C++ source files for ECS (will be created)
IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/common/ecs.cpp)
		list(APPEND QCOMMON_SRCS src/common/ecs.cpp)
ENDIF()
IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/common/ecs_systems.cpp)
		list(APPEND QCOMMON_SRCS src/common/ecs_systems.cpp)
ENDIF()
	IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/server/sv_ecs.cpp)
		list(APPEND SERVER_SRCS src/server/sv_ecs.cpp)
	ENDIF()
	IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/server/sv_ecs_systems.cpp)
		list(APPEND SERVER_SRCS src/server/sv_ecs_systems.cpp)
	ENDIF()
ENDIF()

IF(USE_BULLET)
	# Try CMake's config package first (common when Bullet is installed from source or via vcpkg)
	# Clear BULLET_LIBRARIES first to prevent BulletConfig.cmake from setting wrong libraries
	unset(BULLET_LIBRARIES)
	unset(BULLET_LIBRARIES CACHE)
	find_package(Bullet CONFIG QUIET)
	
	if(TARGET Bullet::BulletDynamics)
		set(BULLET_FOUND TRUE)
		# Use CMake targets - these handle dependencies correctly
		# Override any BULLET_LIBRARIES that BulletConfig.cmake might have set
		set(BULLET_LIBRARIES
			Bullet::BulletDynamics
			Bullet::BulletCollision
			Bullet::LinearMath
		)
		# Get include directories from the CMake target
		# Note: Bullet's CMake config may return incorrect relative paths that get resolved
		# incorrectly, so we always validate and use the correct absolute paths
		if(EXISTS "/usr/include/bullet/btBulletDynamicsCommon.h")
			set(BULLET_INCLUDE_DIRS /usr/include/bullet)
		elseif(EXISTS "/usr/local/include/bullet/btBulletDynamicsCommon.h")
			set(BULLET_INCLUDE_DIRS /usr/local/include/bullet)
		else()
			# Fallback: common system paths where Bullet headers are located
			set(BULLET_INCLUDE_DIRS /usr/include/bullet /usr/local/include/bullet)
		endif()
		# Normalize relative paths (BulletConfig.cmake may set relative paths like "include/bullet")
		# Convert relative paths to absolute paths and fix non-existent absolute paths
		if(BULLET_INCLUDE_DIRS)
			set(BULLET_INCLUDE_DIRS_NORMALIZED "")
			foreach(INC_DIR ${BULLET_INCLUDE_DIRS})
				if(IS_ABSOLUTE ${INC_DIR})
					# Check if the absolute path actually exists
					if(EXISTS ${INC_DIR})
						list(APPEND BULLET_INCLUDE_DIRS_NORMALIZED ${INC_DIR})
					else()
						# Path doesn't exist, try to find it under /usr or /usr/local
						# Extract the last component (e.g., "bullet" from "/home/tim/Desktop/idtech3/include/bullet")
						get_filename_component(INC_DIR_BASENAME ${INC_DIR} NAME)
						if(EXISTS "/usr/include/${INC_DIR_BASENAME}")
							list(APPEND BULLET_INCLUDE_DIRS_NORMALIZED "/usr/include/${INC_DIR_BASENAME}")
						elseif(EXISTS "/usr/local/include/${INC_DIR_BASENAME}")
							list(APPEND BULLET_INCLUDE_DIRS_NORMALIZED "/usr/local/include/${INC_DIR_BASENAME}")
						else()
							# Fallback to common system paths
							list(APPEND BULLET_INCLUDE_DIRS_NORMALIZED /usr/include/bullet /usr/local/include/bullet)
						endif()
					endif()
				else()
					# Relative path - check if it exists under /usr or /usr/local
					if(EXISTS "/usr/${INC_DIR}")
						list(APPEND BULLET_INCLUDE_DIRS_NORMALIZED "/usr/${INC_DIR}")
					elseif(EXISTS "/usr/local/${INC_DIR}")
						list(APPEND BULLET_INCLUDE_DIRS_NORMALIZED "/usr/local/${INC_DIR}")
					else()
						# Fallback to common system paths
						list(APPEND BULLET_INCLUDE_DIRS_NORMALIZED /usr/include/bullet /usr/local/include/bullet)
					endif()
				endif()
			endforeach()
			# Remove duplicates
			list(REMOVE_DUPLICATES BULLET_INCLUDE_DIRS_NORMALIZED)
			set(BULLET_INCLUDE_DIRS ${BULLET_INCLUDE_DIRS_NORMALIZED})
		endif()
	endif()
	
	# Fallback to module-style FindBullet.cmake or older package layouts
	if(NOT BULLET_FOUND)
		find_package(Bullet QUIET)
		if(Bullet_FOUND OR BULLET_FOUND)
			# BulletConfig.cmake may set BULLET_LIBRARIES to include libraries that don't exist
			# (e.g., BulletInverseDynamics, BulletSoftBody). Filter them out and only use
			# the libraries that actually exist.
			if(DEFINED BULLET_LIBRARIES)
				# Filter out non-existent libraries
				set(BULLET_LIBRARIES_FILTERED "")
				foreach(LIB ${BULLET_LIBRARIES})
					# Check if it's a CMake target (starts with Bullet::)
					if(LIB MATCHES "^Bullet::")
						list(APPEND BULLET_LIBRARIES_FILTERED ${LIB})
					else()
						# Check if library file exists
						find_library(LIB_FILE ${LIB} PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib)
						if(LIB_FILE)
							list(APPEND BULLET_LIBRARIES_FILTERED ${LIB_FILE})
						endif()
					endif()
				endforeach()
				# If filtering removed everything, use only the core libraries
				if(NOT BULLET_LIBRARIES_FILTERED)
					set(BULLET_LIBRARIES_FILTERED BulletDynamics BulletCollision LinearMath)
				endif()
				set(BULLET_LIBRARIES ${BULLET_LIBRARIES_FILTERED})
			elseif(DEFINED BULLET_DYNAMICS_LIBRARY AND DEFINED BULLET_COLLISION_LIBRARY AND DEFINED BULLET_MATH_LIBRARY)
				set(BULLET_LIBRARIES
					${BULLET_DYNAMICS_LIBRARY}
					${BULLET_COLLISION_LIBRARY}
					${BULLET_MATH_LIBRARY}
				)
			endif()
			if(DEFINED BULLET_INCLUDE_DIRS)
				set(BULLET_INCLUDE_DIRS ${BULLET_INCLUDE_DIRS})
				# Normalize relative paths (BulletConfig.cmake may set relative paths like "include/bullet")
				if(BULLET_INCLUDE_DIRS)
					set(BULLET_INCLUDE_DIRS_NORMALIZED "")
					foreach(INC_DIR ${BULLET_INCLUDE_DIRS})
						if(IS_ABSOLUTE ${INC_DIR})
							list(APPEND BULLET_INCLUDE_DIRS_NORMALIZED ${INC_DIR})
						else()
							# Check if it's a relative path that should be under /usr or /usr/local
							if(EXISTS "/usr/${INC_DIR}")
								list(APPEND BULLET_INCLUDE_DIRS_NORMALIZED "/usr/${INC_DIR}")
							elseif(EXISTS "/usr/local/${INC_DIR}")
								list(APPEND BULLET_INCLUDE_DIRS_NORMALIZED "/usr/local/${INC_DIR}")
							else()
								# Fallback to common system paths
								list(APPEND BULLET_INCLUDE_DIRS_NORMALIZED /usr/include/bullet /usr/local/include/bullet)
							endif()
						endif()
					endforeach()
					set(BULLET_INCLUDE_DIRS ${BULLET_INCLUDE_DIRS_NORMALIZED})
				endif()
			elseif(DEFINED BULLET_INCLUDE_DIR)
				set(BULLET_INCLUDE_DIRS ${BULLET_INCLUDE_DIR})
				# Normalize relative paths
				if(BULLET_INCLUDE_DIRS)
					set(BULLET_INCLUDE_DIRS_NORMALIZED "")
					foreach(INC_DIR ${BULLET_INCLUDE_DIRS})
						if(IS_ABSOLUTE ${INC_DIR})
							list(APPEND BULLET_INCLUDE_DIRS_NORMALIZED ${INC_DIR})
						else()
							# Check if it's a relative path that should be under /usr or /usr/local
							if(EXISTS "/usr/${INC_DIR}")
								list(APPEND BULLET_INCLUDE_DIRS_NORMALIZED "/usr/${INC_DIR}")
							elseif(EXISTS "/usr/local/${INC_DIR}")
								list(APPEND BULLET_INCLUDE_DIRS_NORMALIZED "/usr/local/${INC_DIR}")
							else()
								# Fallback to common system paths
								list(APPEND BULLET_INCLUDE_DIRS_NORMALIZED /usr/include/bullet /usr/local/include/bullet)
							endif()
						endif()
					endforeach()
					set(BULLET_INCLUDE_DIRS ${BULLET_INCLUDE_DIRS_NORMALIZED})
				endif()
			endif()
			if(Bullet_FOUND)
				set(BULLET_FOUND TRUE)
			endif()
		endif()
	endif()
	
	# Manual search as a last resort (typical distro layout)
	if(NOT BULLET_FOUND)
		FIND_PATH(BULLET_INCLUDE_DIR
			NAMES btBulletDynamicsCommon.h
			PATHS
				/usr/include
				/usr/local/include
				/opt/local/include
				/usr/include/bullet
				/usr/local/include/bullet
		)
		FIND_LIBRARY(BULLET_DYNAMICS_LIBRARY
			NAMES BulletDynamics
			PATHS
				/usr/lib
				/usr/lib/x86_64-linux-gnu
				/usr/local/lib
				/opt/local/lib
		)
		FIND_LIBRARY(BULLET_COLLISION_LIBRARY
			NAMES BulletCollision
			PATHS
				/usr/lib
				/usr/lib/x86_64-linux-gnu
				/usr/local/lib
				/opt/local/lib
		)
		FIND_LIBRARY(BULLET_MATH_LIBRARY
			NAMES LinearMath
			PATHS
				/usr/lib
				/usr/lib/x86_64-linux-gnu
				/usr/local/lib
				/opt/local/lib
		)
		if(BULLET_INCLUDE_DIR AND BULLET_DYNAMICS_LIBRARY AND BULLET_COLLISION_LIBRARY AND BULLET_MATH_LIBRARY)
			set(BULLET_FOUND TRUE)
			set(BULLET_LIBRARIES
				${BULLET_DYNAMICS_LIBRARY}
				${BULLET_COLLISION_LIBRARY}
				${BULLET_MATH_LIBRARY}
			)
			set(BULLET_INCLUDE_DIRS ${BULLET_INCLUDE_DIR})
		endif()
	endif()
	
	if(BULLET_FOUND AND BULLET_LIBRARIES)
		# Filter out any non-existent libraries that BulletConfig.cmake might have added
		set(BULLET_LIBRARIES_FINAL "")
		foreach(LIB ${BULLET_LIBRARIES})
			# Keep CMake targets
			if(LIB MATCHES "^Bullet::")
				list(APPEND BULLET_LIBRARIES_FINAL ${LIB})
			# Keep only core library names (filter out BulletInverseDynamics, BulletSoftBody, Bullet3Common)
			elseif(LIB MATCHES "^(BulletDynamics|BulletCollision|LinearMath)$")
				list(APPEND BULLET_LIBRARIES_FINAL ${LIB})
			endif()
		endforeach()
		# If we have CMake targets, use those; otherwise use filtered list
		if(BULLET_LIBRARIES_FINAL)
			set(BULLET_LIBRARIES ${BULLET_LIBRARIES_FINAL})
		endif()
		MESSAGE(STATUS "Bullet found: ${BULLET_LIBRARIES}")
	else()
		MESSAGE(WARNING "Bullet not found, disabling Bullet support")
		SET(USE_BULLET OFF CACHE BOOL "" FORCE)
	endif()
ENDIF()


# add platform-dependent QVM bytecode compilers
# Force 64-bit builds by default
IF(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
	SET(BINEXT .aarch64)
	SET(RENDEXT _aarch64)
	list(APPEND QCOMMON_SRCS src/common/vm_aarch64.c)
ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x86|i*86")
	# Default to 64-bit for x86 architectures
	IF(WIN32)
		SET(BINEXT .x64)
	ELSE()
		SET(BINEXT .x86_64)
	ENDIF()
	SET(RENDEXT _x86_64)
	list(APPEND QCOMMON_SRCS src/common/vm_x86.c)
ELSEIF(CMAKE_SYSTEM_PROCESSOR MATCHES "arm*")
	SET(BINEXT .arm)
	SET(RENDEXT _arm)
	list(APPEND QCOMMON_SRCS src/common/vm_armv7l.c)
ELSE()
	TARGET_COMPILE_DEFINITIONS(common PRIVATE -DNO_VM_COMPILED)
	TARGET_COMPILE_DEFINITIONS(common_ded PRIVATE -DNO_VM_COMPILED)
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
set_property(TARGET botlib PROPERTY POSITION_INDEPENDENT_CODE ON)

AUX_SOURCE_DIRECTORY(src/server SERVER_SRCS)
list(REMOVE_ITEM SERVER_SRCS src/server/sv_rankings.c)

# universal binary - common library should only contain truly common code
ADD_LIBRARY(common OBJECT ${QCOMMON_SRCS})
	TARGET_INCLUDE_DIRECTORIES(common PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/common)
	set_property(TARGET common PROPERTY POSITION_INDEPENDENT_CODE ON)
IF(USE_JOBSYSTEM)
	TARGET_COMPILE_DEFINITIONS(common PRIVATE USE_JOBSYSTEM)
ENDIF()
IF(USE_BULLET)
	IF(BULLET_FOUND AND BULLET_LIBRARIES)
		# Set include directories - always use absolute paths to avoid CMake resolving relative paths incorrectly
		# Bullet's CMake config may set relative paths that get resolved relative to CMAKE_CURRENT_SOURCE_DIR
		if(EXISTS "/usr/include/bullet/btBulletDynamicsCommon.h")
			TARGET_INCLUDE_DIRECTORIES(common PUBLIC /usr/include/bullet)
		elseif(EXISTS "/usr/local/include/bullet/btBulletDynamicsCommon.h")
			TARGET_INCLUDE_DIRECTORIES(common PUBLIC /usr/local/include/bullet)
		else()
			# Fallback: common system paths
			TARGET_INCLUDE_DIRECTORIES(common PUBLIC /usr/include/bullet /usr/local/include/bullet)
		endif()
		TARGET_COMPILE_DEFINITIONS(common PRIVATE USE_BULLET)
	ENDIF()
ENDIF()

# dedicated server
ADD_LIBRARY(common_ded OBJECT ${QCOMMON_SRCS})
TARGET_COMPILE_DEFINITIONS(common_ded PRIVATE DEDICATED)
set_property(TARGET common_ded PROPERTY POSITION_INDEPENDENT_CODE ON)
IF(USE_JOBSYSTEM)
	TARGET_COMPILE_DEFINITIONS(common_ded PRIVATE USE_JOBSYSTEM)
ENDIF()

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

IF(USE_NANOSVG)
	# nanosvg is a single-header library, check if headers exist
	IF(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/nanosvg/nanosvg.h" AND 
	   EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/nanosvg/nanosvgrast.h")
		ADD_COMPILE_DEFINITIONS(USE_NANOSVG)
		TARGET_INCLUDE_DIRECTORIES(common PRIVATE libs/nanosvg)
		TARGET_INCLUDE_DIRECTORIES(common_ded PRIVATE libs/nanosvg)
		message(STATUS "nanosvg found: using bundled headers")
	else()
		message(WARNING "nanosvg headers not found, disabling SVG support")
		message(STATUS "To enable SVG support, download nanosvg from https://github.com/memononen/nanosvg")
		message(STATUS "Place nanosvg.h and nanosvgrast.h in libs/nanosvg/")
		set(USE_NANOSVG OFF CACHE BOOL "" FORCE)
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

# Add Steam Deck integration
IF(USE_STEAMWORKS)
	list(APPEND CLIENT_SRCS src/client/cl_steamdeck.c)
	ADD_COMPILE_DEFINITIONS(USE_STEAMWORKS)
	
	# Find Steamworks SDK
	# Steamworks SDK is typically located in:
	# - Steam SDK installation directory
	# - Third-party/steamworks directory
	# - System installation (uncommon)
	FIND_PATH(STEAMWORKS_INCLUDE_DIR
		NAMES steam/steam_api.h
		PATHS
			${CMAKE_CURRENT_SOURCE_DIR}/third-party/steamworks/sdk/public/steam
			${CMAKE_CURRENT_SOURCE_DIR}/libs/steamworks/sdk/public/steam
			/usr/include/steam
			/usr/local/include/steam
		PATH_SUFFIXES
			steam
	)
	
	# Find Steamworks library
	FIND_LIBRARY(STEAMWORKS_LIBRARY
		NAMES steam_api
		PATHS
			${CMAKE_CURRENT_SOURCE_DIR}/third-party/steamworks/sdk/redistributable_bin
			${CMAKE_CURRENT_SOURCE_DIR}/libs/steamworks/sdk/redistributable_bin
			/usr/lib
			/usr/lib/x86_64-linux-gnu
			/usr/local/lib
		PATH_SUFFIXES
			linux64
			linux32
	)
	
	if(STEAMWORKS_INCLUDE_DIR AND STEAMWORKS_LIBRARY)
		set(STEAMWORKS_FOUND TRUE)
		TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${STEAMWORKS_INCLUDE_DIR}/..)
		TARGET_LINK_LIBRARIES(client PRIVATE ${STEAMWORKS_LIBRARY})
		message(STATUS "Steamworks SDK found: ${STEAMWORKS_LIBRARY}")
	else()
		message(WARNING "Steamworks SDK not found, Steam Deck features will be disabled")
		message(STATUS "To enable Steamworks SDK, download from https://partner.steamgames.com/doc/sdk")
		message(STATUS "Place it in third-party/steamworks/ or libs/steamworks/")
		set(USE_STEAMWORKS OFF CACHE BOOL "" FORCE)
	endif()
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
		# Suppress clang pragma warnings for cimgui (GCC doesn't recognize clang pragmas)
		target_compile_options(imgui_shared PRIVATE
			$<$<C_COMPILER_ID:GNU>:-Wno-unknown-pragmas>
			$<$<CXX_COMPILER_ID:GNU>:-Wno-unknown-pragmas>
		)
		list(APPEND CLIENT_SRCS ${CIMGUI_DIR}/cimgui_impl.cpp)
		# Only include SDL backend when SDL is enabled (CI may build with USE_SDL=0).
		if(USE_SDL)
			list(APPEND CLIENT_SRCS ${CIMGUI_IMGUI_DIR}/backends/imgui_impl_sdl2.cpp)
		endif()
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
	IF(USE_JOBSYSTEM)
		TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_JOBSYSTEM)
	ENDIF()
ELSE()
	AUX_SOURCE_DIRECTORY(libs/jpeg JPEG_SRCS)
	#ADD_LIBRARY(jpeg ${JPEG_SRCS})
	#TARGET_LINK_LIBRARIES(client jpeg)
	ADD_LIBRARY(client OBJECT ${CLIENT_SRCS} ${JPEG_SRCS})
	# Add JPEG include directory for internal includes
	TARGET_INCLUDE_DIRECTORIES(client PRIVATE libs/jpeg)
	IF(USE_JOBSYSTEM)
		TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_JOBSYSTEM)
	ENDIF()
	# Suppress warnings for third-party JPEG library code
	IF(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
		FOREACH(JPEG_SRC ${JPEG_SRCS})
			set_source_files_properties(${JPEG_SRC} PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter")
		ENDFOREACH()
	ENDIF()
ENDIF()

# Add OGG/Vorbis include directories for client (used by snd_codec_ogg.c)
TARGET_INCLUDE_DIRECTORIES(client PRIVATE src/common src libs/ogg/include libs/vorbis/include)

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
	)
	if(USE_SDL)
		TARGET_COMPILE_DEFINITIONS(client PRIVATE CIMGUI_USE_SDL2 USE_SDL)
	endif()
	# Unified Vulkan flag (no separate USE_VULKAN_API needed)
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_OPENGL_API)
ENDIF()

# cJSON library
IF(USE_CJSON)
	SET(CJSON_SRCS libs/cjson/cJSON.c)
	list(APPEND QCOMMON_SRCS ${CJSON_SRCS})
	TARGET_INCLUDE_DIRECTORIES(common PRIVATE libs/cjson)
	TARGET_INCLUDE_DIRECTORIES(common_ded PRIVATE libs/cjson)
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
	TARGET_INCLUDE_DIRECTORIES(common PRIVATE libs/zstd/lib libs/zstd/lib/common)
	TARGET_INCLUDE_DIRECTORIES(common_ded PRIVATE libs/zstd/lib libs/zstd/lib/common)
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
	TARGET_INCLUDE_DIRECTORIES(common PRIVATE libs/enet/include)
	TARGET_INCLUDE_DIRECTORIES(common_ded PRIVATE libs/enet/include)
ENDIF()

# renderers
# NOTE: renderer common sources are currently provided by each renderer and shared
# Renderer-common code shared by all renderers (OpenGL/Vulkan/etc).
# Note: `src/renderercommon/` contains compatibility shims; the real shared
# renderer code lives under `src/renderers/renderercommon/`.
AUX_SOURCE_DIRECTORY(src/renderers/renderercommon RENDERER_COMMON_SRCS)
# Remove OpenEXR support if not available
if(NOT USE_OPENEXR OR NOT OpenEXR_FOUND)
	list(REMOVE_ITEM RENDERER_COMMON_SRCS src/renderers/renderercommon/tr_image_exr.cpp)
endif()
# Add FreeType wrapper to renderer common sources when FreeType is enabled
IF(USE_FREETYPE)
	list(APPEND RENDERER_COMMON_SRCS src/common/freetype_wrapper.c)
ENDIF()
# Add stb_truetype font support for all renderers
IF(USE_STB_TRUETYPE)
	list(APPEND RENDERER_COMMON_SRCS src/renderers/opengl/tr_font_stb.c)
	list(APPEND RENDERER_COMMON_SRCS src/renderers/renderercommon/tr_font_effects.c)
ENDIF()
# Always include tr_services.c for safe shader loading
list(APPEND RENDERER_COMMON_SRCS src/renderers/renderercommon/tr_services.c)
# Note: FreeType-dependent files are included when USE_FREETYPE is defined
# They have proper guards to compile to empty implementations when FreeType is not available
AUX_SOURCE_DIRECTORY(src/renderers/opengl RENDERER_GL_SRCS)
list(APPEND RENDERER_GL_SRCS src/renderers/opengl/tr_main.c)
SET(RENDERER_VK_SRCS
	src/renderers/vulkan/tr_animation.c
	src/renderers/vulkan/tr_backend.c
	src/renderers/vulkan/tr_bsp.c
	src/renderers/vulkan/tr_cmds.c
	src/renderers/vulkan/tr_curve.c
	src/renderers/vulkan/tr_image.c
	src/renderers/vulkan/tr_init.c
	src/renderers/vulkan/tr_light.c
	src/renderers/vulkan/tr_lightclusters.c
	src/renderers/vulkan/tr_main.c
	src/renderers/vulkan/tr_marks.c
	src/renderers/vulkan/tr_math_optimized.c
	src/renderers/vulkan/tr_mesh.c
	src/renderers/vulkan/tr_model.c
	src/renderers/vulkan/tr_model_iqm.c
	# NOTE: tr_rtx.c is a standalone q2rtx-style experimental renderer stub and
	# defines symbols that conflict with the main Vulkan renderer.
	# Keep it out of the build unless/ until it is namespaced and integrated.
	# src/renderers/vulkan/tr_rtx.c
	src/renderers/vulkan/tr_scene.c
	src/renderers/vulkan/tr_shade.c
	src/renderers/vulkan/tr_shade_calc.c
	src/renderers/vulkan/tr_shader.c
	src/renderers/vulkan/tr_shadows.c
	src/renderers/vulkan/tr_sky.c
	src/renderers/vulkan/tr_surface.c
	src/renderers/vulkan/tr_world.c
	src/renderers/vulkan/vk.c
	src/renderers/vulkan/vk_stubs.c
	src/renderers/vulkan/vk_fsr.c
	src/renderers/vulkan/tr_font_vk.c
	src/renderers/vulkan/vk_memory.c
	src/renderers/vulkan/vk_vbo.c
	src/renderers/vulkan/vk_flares.c
	src/renderers/vulkan/vk_utils.c
	src/renderers/vulkan/vk_commands.c
	src/renderers/vulkan/vk_pipeline.c
	src/renderers/vulkan/vk_material_parser.c
	src/renderers/vulkan/vk_material_system.c
	src/renderers/vulkan/vk_advanced_materials.c
	src/renderers/vulkan/vk_atmosphere.c
	src/renderers/vulkan/vk_buffers.c
	src/renderers/vulkan/vk_cell_streaming.c
	src/renderers/vulkan/vk_compute.c
	src/renderers/vulkan/vk_dlss.c
	src/renderers/vulkan/vk_framebuffer.c
	src/renderers/vulkan/vk_gibs.c
	src/renderers/vulkan/vk_gpu_culling.c
	src/renderers/vulkan/vk_gpu_memory.c
	src/renderers/vulkan/vk_layered_materials.c
	src/renderers/vulkan/vk_mesh_shaders.c
	src/renderers/vulkan/vk_particles.c
	src/renderers/vulkan/vk_pbo.c
	src/renderers/vulkan/vk_world_effects.c
	src/renderers/vulkan/vk_portal_lights.c
	src/renderers/vulkan/vk_post_process.cpp
	src/renderers/vulkan/vk_proc_dressing.c
	src/renderers/vulkan/vk_raytracing.c
	src/renderers/vulkan/vk_renderpass.c
	src/renderers/vulkan/vk_scene_graph.c
	src/renderers/vulkan/vk_sync.c
	src/renderers/vulkan/vk_virtual_texture.c
	src/renderers/vulkan/vk_imgui.c
	src/renderers/vulkan/vk_bindless.c
	src/renderers/vulkan/vk_shader_cache.c
	src/renderers/vulkan/vk_async_compile.c
	src/renderers/vulkan/vk_dynamic_rendering.c
	src/renderers/vulkan/vk_frame.cpp
	src/renderers/vulkan/vk_images.cpp
	src/renderers/vulkan/vk_draw.cpp
	src/renderers/vulkan/vk_descriptors.cpp
	src/renderers/vulkan/vk_postprocess.cpp
	src/renderers/vulkan/vk_config.cpp
	src/renderers/vulkan/tr_model_assimp.cpp
)

# Vulkan experimental modules (work-in-progress). Keep ON by default for prioritized
# Vulkan RTX implementation development.
option(IDTECH3_VK_EXPERIMENTAL "Build experimental Vulkan renderer modules" ON)
	if(NOT IDTECH3_VK_EXPERIMENTAL)
	list(REMOVE_ITEM RENDERER_VK_SRCS
		src/renderers/vulkan/gltf_loader.c
		src/renderers/vulkan/tr_font_vk.c
		src/renderers/vulkan/tr_image.c
		src/renderers/vulkan/vk_compute_raytracing.c
		src/renderers/vulkan/vk_dynamic_rendering.c
		src/renderers/vulkan/vk_graphics_settings.c
		src/renderers/vulkan/vk_ibl.c
		src/renderers/vulkan/vk_multithreaded_rendering.c
		src/renderers/vulkan/vk_physics.c
		src/renderers/vulkan/vk_sem.c
		src/renderers/vulkan/vk_shadows.c
	)
endif()
# Add VMA C++ implementation file if VMA is enabled
IF(USE_VMA AND USE_VULKAN)
	list(APPEND RENDERER_VK_SRCS src/renderers/vulkan/vk_vma.cpp)
ENDIF()

IF(USE_CIMGUI AND DEFINED CIMGUI_DIR)
	list(APPEND RENDERER_VK_SRCS
		${CIMGUI_DIR}/cimgui_impl.cpp
		${CIMGUI_IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
	)
ENDIF()

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

# Metal renderer (macOS/iOS only)
IF(APPLE AND USE_METAL AND NOT ANDROID)
	SET(RENDERER_METAL_SRCS
		src/renderermetal/metal.mm
		src/renderermetal/tr_init.c
		src/renderermetal/tr_main.c
		src/renderermetal/tr_image.c
		src/renderermetal/tr_surface.c
	)
	IF(USE_CIMGUI AND DEFINED CIMGUI_DIR)
		list(APPEND RENDERER_METAL_SRCS
			${CIMGUI_DIR}/cimgui_impl.cpp
			${CIMGUI_IMGUI_DIR}/backends/imgui_impl_metal.mm
		)
	ENDIF()
	
	# Compile Metal shaders
	IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/renderermetal/shaders/default.metal)
		IF(IOS)
			SET(METAL_SDK iphoneos)
		ELSE()
			SET(METAL_SDK macosx)
		ENDIF()
		ADD_CUSTOM_COMMAND(
			OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/default.metallib
			COMMAND xcrun -sdk ${METAL_SDK} metal -c ${CMAKE_CURRENT_SOURCE_DIR}/src/renderermetal/shaders/default.metal -o ${CMAKE_CURRENT_BINARY_DIR}/default.air
			COMMAND xcrun -sdk ${METAL_SDK} metallib ${CMAKE_CURRENT_BINARY_DIR}/default.air -o ${CMAKE_CURRENT_BINARY_DIR}/default.metallib
			DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/renderermetal/shaders/default.metal
			COMMENT "Compiling Metal shaders"
		)
		ADD_CUSTOM_TARGET(metal_shaders DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/default.metallib)
	ENDIF()
ENDIF()

IF(USE_RENDERER_DLOPEN)
	SET(AUX_SRCS
		src/common/q_shared.c
		src/common/puff.c
		src/common/q_math.c
	)
	IF(USE_VULKAN)
		TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_VULKAN)
	ENDIF()
	TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_RENDERER_DLOPEN RENDERER_PREFIX="${RENDERER_PREFIX}" RENDERER_DEFAULT=${RENDERER_DEFAULT})

	ADD_LIBRARY(${RENDERER_PREFIX}_opengl${RENDEXT} SHARED ${RENDERER_GL_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
	SET_TARGET_PROPERTIES(${RENDERER_PREFIX}_opengl${RENDEXT} PROPERTIES 
        OUTPUT_NAME "${RENDERER_PREFIX}_opengl${RENDEXT}" 
        PREFIX ""
        POSITION_INDEPENDENT_CODE ON
    )
	TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE USE_RENDERER_DLOPEN USE_OPENGL_API)
    TARGET_COMPILE_OPTIONS(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE -fvisibility=hidden)
	IF(USE_FREETYPE)
		TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE BUILD_FREETYPE)
	ENDIF()
	# `src/renderers/renderercommon/*` uses legacy relative includes like "../qcommon/...".
	# Adding `src/renderers` to the include path keeps those includes resolving to `src/qcommon/...`.
	target_include_directories(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/renderers)
	IF(USE_FREETYPE)
		target_include_directories(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE /usr/include/freetype2)
	ENDIF()
	TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_opengl${RENDEXT} m /usr/lib/x86_64-linux-gnu/libGL.so.1)
	IF(USE_FREETYPE)
		TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_opengl${RENDEXT} /usr/lib/x86_64-linux-gnu/libfreetype.so)
	ENDIF()
	TARGET_LINK_OPTIONS(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE -Wl,--export-dynamic -Wl,--unresolved-symbols=ignore-all)

	if(USE_VULKAN)
		ADD_LIBRARY(${RENDERER_PREFIX}_vulkan${RENDEXT} SHARED ${RENDERER_VK_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
		SET_TARGET_PROPERTIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PROPERTIES 
            OUTPUT_NAME "${RENDERER_PREFIX}_vulkan${RENDEXT}" 
            PREFIX ""
            POSITION_INDEPENDENT_CODE ON
        )
		TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_RENDERER_DLOPEN USE_VULKAN USE_FREETYPE)
        TARGET_COMPILE_OPTIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE -fvisibility=hidden)
		IF(USE_FREETYPE)
			TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE BUILD_FREETYPE)
		ENDIF()
		# `src/renderers/renderercommon/*` uses legacy relative includes like "../qcommon/...".
		# Adding `src/renderers` to the include path keeps those includes resolving to `src/qcommon/...`.
	target_include_directories(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/renderers ${CMAKE_CURRENT_SOURCE_DIR}/src/common ${CMAKE_CURRENT_SOURCE_DIR}/src/renderers/renderercommon)
	IF(USE_FREETYPE)
		target_include_directories(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE /usr/include/freetype2)
	ENDIF()
		IF(IDTECH3_VK_EXPERIMENTAL)
			TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE IDTECH3_VK_EXPERIMENTAL)
		ENDIF()
		TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE m SDL2::SDL2)
		IF(USE_FREETYPE)
			TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE /usr/lib/x86_64-linux-gnu/libfreetype.so)
		ENDIF()
		TARGET_LINK_OPTIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE -Wl,--export-dynamic)
		# Suppress some warnings for Vulkan renderer to avoid warning-as-error issues
        TARGET_COMPILE_OPTIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE
            $<$<AND:$<COMPILE_LANGUAGE:C>,$<C_COMPILER_ID:GNU,Clang>>:-Wno-missing-prototypes>
            $<$<C_COMPILER_ID:GNU,Clang>:-Wno-unused-variable>
            $<$<C_COMPILER_ID:GNU,Clang>:-Wno-unused-function>
            $<$<C_COMPILER_ID:GNU,Clang>:-Wno-unused-parameter>
            $<$<C_COMPILER_ID:GNU,Clang>:-Wno-shadow>
            $<$<C_COMPILER_ID:GNU,Clang>:-Wno-int-to-pointer-cast>
            $<$<C_COMPILER_ID:GNU,Clang>:-Wno-pointer-arith>
            $<$<C_COMPILER_ID:GNU,Clang>:-Wno-sign-compare>
            $<$<C_COMPILER_ID:GNU,Clang>:-Wno-pedantic>
            $<$<C_COMPILER_ID:GNU,Clang>:-Wno-format>
            $<$<C_COMPILER_ID:GNU,Clang>:-Wno-switch>
		)

		# VMA (Vulkan Memory Allocator) - configure after target is created
		IF(USE_VMA AND USE_VULKAN)
			# Check if VMA header exists
			IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/vma/vk_mem_alloc.h)
				TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_VMA)
				TARGET_INCLUDE_DIRECTORIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE libs/vma)
				# Suppress warnings from third-party VMA library code
				IF(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
					SET_SOURCE_FILES_PROPERTIES(src/renderers/vulkan/vk_vma.cpp PROPERTIES
						COMPILE_FLAGS "-Wno-unused-parameter -Wno-unused-variable"
					)
				ENDIF()
				MESSAGE(STATUS "VMA: Vulkan Memory Allocator enabled")
			ELSE()
				MESSAGE(WARNING "VMA: Header not found at libs/vma/vk_mem_alloc.h. Download it from https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator")
				SET(USE_VMA OFF CACHE BOOL "" FORCE)
			ENDIF()
		ENDIF()
	else()
		MESSAGE(STATUS "Vulkan renderer disabled (USE_VULKAN=OFF)")
	endif()
	
	IF(WIN32 AND USE_D3D12)
		ADD_LIBRARY(${RENDERER_PREFIX}_d3d12${RENDEXT} SHARED ${RENDERER_D3D12_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
		SET_TARGET_PROPERTIES(${RENDERER_PREFIX}_d3d12${RENDEXT} PROPERTIES OUTPUT_NAME "${RENDERER_PREFIX}_d3d12${RENDEXT}" PREFIX "")
		TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_d3d12${RENDEXT} PRIVATE USE_RENDERER_DLOPEN USE_D3D12)
		TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_d3d12${RENDEXT} PRIVATE d3d12 dxgi d3dcompiler)
	ENDIF()
	
	IF(APPLE AND USE_METAL AND NOT ANDROID)
		ADD_LIBRARY(${RENDERER_PREFIX}_metal${RENDEXT} SHARED ${RENDERER_METAL_SRCS} ${RENDERER_COMMON_SRCS} ${AUX_SRCS})
		SET_TARGET_PROPERTIES(${RENDERER_PREFIX}_metal${RENDEXT} PROPERTIES OUTPUT_NAME "${RENDERER_PREFIX}_metal${RENDEXT}" PREFIX "")
		TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_metal${RENDEXT} PRIVATE USE_RENDERER_DLOPEN USE_METAL)
		TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_metal${RENDEXT} PRIVATE "-framework Metal" "-framework MetalKit" "-framework QuartzCore" "-framework Foundation")
		IF(IOS)
			TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_metal${RENDEXT} PRIVATE "-framework UIKit" "-framework GameController")
		ELSE()
			TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_metal${RENDEXT} PRIVATE "-framework AppKit")
		ENDIF()
		SET_SOURCE_FILES_PROPERTIES(${RENDERER_METAL_SRCS} PROPERTIES COMPILE_FLAGS "-x objective-c++ -fobjc-arc")
	ENDIF()
	ELSE()
		IF(USE_VULKAN)
			ADD_LIBRARY(${RENDERER_PREFIX}_vulkan OBJECT ${RENDERER_VK_SRCS} ${RENDERER_COMMON_SRCS})
			set_property(TARGET ${RENDERER_PREFIX}_vulkan PROPERTY POSITION_INDEPENDENT_CODE ON)
			TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan PRIVATE USE_VULKAN USE_FREETYPE)
			TARGET_INCLUDE_DIRECTORIES(${RENDERER_PREFIX}_vulkan PRIVATE /usr/include/freetype2)
		
		# VMA (Vulkan Memory Allocator) - configure after target is created
		IF(USE_VMA AND USE_VULKAN)
			# Check if VMA header exists
			IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libs/vma/vk_mem_alloc.h)
				TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan PRIVATE USE_VMA)
				TARGET_INCLUDE_DIRECTORIES(${RENDERER_PREFIX}_vulkan PRIVATE libs/vma)
				MESSAGE(STATUS "VMA: Vulkan Memory Allocator enabled")
			ELSE()
				MESSAGE(WARNING "VMA: Header not found at libs/vma/vk_mem_alloc.h. Download it from https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator")
				SET(USE_VMA OFF CACHE BOOL "" FORCE)
			ENDIF()
		ENDIF()
	ELSEIF(WIN32 AND USE_D3D12)
		ADD_LIBRARY(${RENDERER_PREFIX}_d3d12 OBJECT ${RENDERER_D3D12_SRCS} ${RENDERER_COMMON_SRCS})
	ELSEIF(APPLE AND USE_METAL AND NOT ANDROID)
		ADD_LIBRARY(${RENDERER_PREFIX}_metal OBJECT ${RENDERER_METAL_SRCS} ${RENDERER_COMMON_SRCS})
		SET_SOURCE_FILES_PROPERTIES(${RENDERER_METAL_SRCS} PROPERTIES COMPILE_FLAGS "-x objective-c++ -fobjc-arc")
	ELSE()
		ADD_LIBRARY(${RENDERER_PREFIX}_opengl OBJECT ${RENDERER_GL_SRCS} ${RENDERER_COMMON_SRCS})
		set_property(TARGET ${RENDERER_PREFIX}_opengl PROPERTY POSITION_INDEPENDENT_CODE ON)
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
IF(ANDROID)
	# Android-specific sources (C++23)
	SET (Q3_SRCS
		src/unix/android_main.cpp
		src/unix/unix_shared.c
		src/unix/linux_signals.c)
ELSEIF(UNIX)
	AUX_SOURCE_DIRECTORY(src/server SERVER_SRCS)
	list(REMOVE_ITEM SERVER_SRCS src/server/sv_rankings.c)
	SET (Q3_SRCS
		src/unix/unix_main.c
		src/unix/unix_shared.c
		src/unix/linux_signals.c
		${SERVER_SRCS})
		# Dedicated server executable uses the same platform + server sources,
		# but links against the DEDICATED-flavored common object library.
		set(Q3_DED_SRCS ${Q3_SRCS})

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

		# These UNIX/X11 backends are legacy code and tend to trigger a lot of
		# noisy warnings under modern compilers. Keep CI usable by suppressing
		# the most common ones for these files specifically.
		IF(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
			set_source_files_properties(
				src/unix/linux_glimp.c
				src/unix/linux_qgl.c
				src/unix/linux_snd.c
				src/unix/linux_qvk.c
				src/unix/x11_dga.c
				src/unix/x11_randr.c
				src/unix/x11_vidmode.c
				PROPERTIES COMPILE_FLAGS "-Wno-missing-prototypes -Wno-shadow -Wno-sign-compare -Wno-pedantic -Wno-unused-parameter"
			)
		ENDIF()
	ENDIF()

	IF(APPLE)
		# Detect iOS vs macOS
		IF(CMAKE_SYSTEM_NAME MATCHES "iOS")
			SET(IOS TRUE)
			SET(EXE_TYPE MACOSX_BUNDLE)
			# iOS-specific sources
			list(APPEND Q3_SRCS src/unix/ios_appdelegate.mm)
			list(APPEND Q3_SRCS src/unix/ios_integration.mm)
			list(APPEND Q3_SRCS src/unix/macos_platform.mm)
			list(APPEND Q3_SRCS src/unix/macos_integration.c)
			SET_SOURCE_FILES_PROPERTIES(src/unix/ios_appdelegate.mm PROPERTIES COMPILE_FLAGS "-x objective-c++ -fobjc-arc")
			SET_SOURCE_FILES_PROPERTIES(src/unix/ios_integration.mm PROPERTIES COMPILE_FLAGS "-x objective-c++ -fobjc-arc")
			SET_SOURCE_FILES_PROPERTIES(src/unix/macos_platform.mm PROPERTIES COMPILE_FLAGS "-x objective-c++ -fobjc-arc")
			# Target may not exist yet (created later); link frameworks after creation.
			IF(TARGET ${CNAME}${BINEXT})
				TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE "-framework UIKit" "-framework Foundation" "-framework Metal" "-framework QuartzCore" "-framework GameController")
			ENDIF()
		ELSE()
			SET(IOS FALSE)
			SET(EXE_TYPE MACOSX_BUNDLE)
			# macOS-specific sources
			list(APPEND Q3_SRCS src/unix/macos_main.mm)
			list(APPEND Q3_SRCS src/unix/macos_platform.mm)
			list(APPEND Q3_SRCS src/unix/macos_integration.c)
			list(APPEND Q3_SRCS src/unix/quake3_flat.icns)
			set_source_files_properties(src/unix/quake3_flat.icns PROPERTIES
				MACOSX_PACKAGE_LOCATION Resources)
			SET(MACOSX_BUNDLE_ICON_FILE quake3_flat.icns)
			SET_SOURCE_FILES_PROPERTIES(src/unix/macos_main.mm PROPERTIES COMPILE_FLAGS "-x objective-c++ -fobjc-arc")
			SET_SOURCE_FILES_PROPERTIES(src/unix/macos_platform.mm PROPERTIES COMPILE_FLAGS "-x objective-c++ -fobjc-arc")
			# Target may not exist yet (created later); link frameworks after creation.
			IF(TARGET ${CNAME}${BINEXT})
				TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE "-framework AppKit" "-framework Foundation" "-framework Metal" "-framework QuartzCore")
			ENDIF()
		ENDIF()
	ENDIF()
ELSEIF(WIN32)
	SET(EXE_TYPE WIN32)
	AUX_SOURCE_DIRECTORY(src/win32 Q3_SRCS)
	AUX_SOURCE_DIRECTORY(src/win32 Q3_UI_SRCS)
ENDIF()

# If a platform didn't set dedicated sources explicitly, default to Q3_SRCS.
if(NOT DEFINED Q3_DED_SRCS)
	set(Q3_DED_SRCS ${Q3_SRCS})
endif()

IF(USE_SDL)
	find_package(SDL2 REQUIRED)

	# Normalize include directories for both modern (SDL2_INCLUDE_DIRS) and legacy (SDL2_INCLUDE_DIR) FindSDL2 modules.
	set(SDL2_INCLUDE_DIRS_SAFE "")
	if(DEFINED SDL2_INCLUDE_DIRS)
		list(APPEND SDL2_INCLUDE_DIRS_SAFE ${SDL2_INCLUDE_DIRS})
	endif()
	if(DEFINED SDL2_INCLUDE_DIR)
		list(APPEND SDL2_INCLUDE_DIRS_SAFE ${SDL2_INCLUDE_DIR})
	endif()
	list(REMOVE_DUPLICATES SDL2_INCLUDE_DIRS_SAFE)

	# Prefer the imported target; fall back to raw library list when only that is available.
	if(TARGET SDL2::SDL2)
		set(SDL2_LINK_LIBS SDL2::SDL2)
	elseif(DEFINED SDL2_LIBRARIES)
		set(SDL2_LINK_LIBS ${SDL2_LIBRARIES})
	else()
		message(FATAL_ERROR "SDL2 found but no linkable target; expected SDL2::SDL2 or SDL2_LIBRARIES")
	endif()

	TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${SDL2_INCLUDE_DIRS_SAFE})
	TARGET_INCLUDE_DIRECTORIES(common PUBLIC ${SDL2_INCLUDE_DIRS_SAFE})
	TARGET_INCLUDE_DIRECTORIES(common_ded PUBLIC ${SDL2_INCLUDE_DIRS_SAFE})
ENDIF()

IF(USE_CURL)
# Add telemetry system
list(APPEND QCOMMON_SRCS src/common/q_telemetry.c)

# Add syscall registry system
list(APPEND QCOMMON_SRCS src/common/syscall_registry.c)
	
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
		TARGET_INCLUDE_DIRECTORIES(common PUBLIC ${CURL_INCLUDE_DIR})
		TARGET_INCLUDE_DIRECTORIES(common_ded PUBLIC ${CURL_INCLUDE_DIR})
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
		TARGET_INCLUDE_DIRECTORIES(common PUBLIC ${SQLite3_INCLUDE_DIRS})
		TARGET_INCLUDE_DIRECTORIES(common_ded PUBLIC ${SQLite3_INCLUDE_DIRS})
		TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_SQLITE)
		TARGET_COMPILE_DEFINITIONS(common PRIVATE USE_SQLITE)
		TARGET_COMPILE_DEFINITIONS(common_ded PRIVATE USE_SQLITE)
		MESSAGE(STATUS "SQLite found: ${SQLite3_LIBRARIES}")
	ELSE()
		MESSAGE(WARNING "SQLite not found, disabling SQLite support")
		MESSAGE(STATUS "To enable SQLite support, install: sudo apt-get install libsqlite3-dev")
		SET(USE_SQLITE OFF CACHE BOOL "" FORCE)
	ENDIF()
ENDIF()

IF(USE_BROTLI)
	# Try pkg-config first for Brotli
	pkg_check_modules(BROTLI libbrotlienc libbrotlidec QUIET)
	IF(BROTLI_FOUND)
		SET(BROTLI_INCLUDE_DIRS ${BROTLI_INCLUDE_DIRS})
		SET(BROTLI_LIBRARIES ${BROTLI_LIBRARIES})
		MESSAGE(STATUS "Brotli found: ${BROTLI_LIBRARIES}")
	ELSE()
		MESSAGE(WARNING "Brotli not found, WOFF2 font support will be disabled")
		SET(USE_BROTLI OFF CACHE BOOL "" FORCE)
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
		TARGET_INCLUDE_DIRECTORIES(common PUBLIC ${FREETYPE_INCLUDE_DIRS})
		TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_FREETYPE BUILD_FREETYPE)
		TARGET_COMPILE_DEFINITIONS(common PRIVATE USE_FREETYPE)
		IF(USE_BROTLI)
			TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_BROTLI)
			TARGET_COMPILE_DEFINITIONS(common PRIVATE USE_BROTLI)
		ENDIF()
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
		TARGET_INCLUDE_DIRECTORIES(common PUBLIC ${OPENSSL_INCLUDE_DIRS})
		TARGET_INCLUDE_DIRECTORIES(common_ded PUBLIC ${OPENSSL_INCLUDE_DIRS})
		TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_OPENSSL)
		TARGET_COMPILE_DEFINITIONS(common PRIVATE USE_OPENSSL)
		TARGET_COMPILE_DEFINITIONS(common_ded PRIVATE USE_OPENSSL)
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


# Theora video codec support
IF(USE_THEORA)
	# Try pkg-config first (common on Linux)
	find_package(PkgConfig QUIET)
	IF(PKG_CONFIG_FOUND)
		pkg_check_modules(THEORA libtheora QUIET)
		IF(THEORA_FOUND)
			SET(THEORA_INCLUDE_DIRS ${THEORA_INCLUDE_DIRS})
			SET(THEORA_LIBRARIES ${THEORA_LIBRARIES})
		ENDIF()
	ENDIF()
	
	# Try CMake's find_package
	IF(NOT THEORA_FOUND)
		find_package(Theora QUIET)
		IF(Theora_FOUND)
			SET(THEORA_FOUND TRUE)
			IF(DEFINED Theora_INCLUDE_DIRS)
				SET(THEORA_INCLUDE_DIRS ${Theora_INCLUDE_DIRS})
			ENDIF()
			IF(DEFINED Theora_LIBRARIES)
				SET(THEORA_LIBRARIES ${Theora_LIBRARIES})
			ENDIF()
		ENDIF()
	ENDIF()
	
	# Manual search as fallback
	IF(NOT THEORA_FOUND)
		FIND_PATH(THEORA_INCLUDE_DIR
			NAMES theora/theora.h
			PATHS
				/usr/include
				/usr/local/include
				/opt/local/include
		)
		# Theora is split into theoradec (decoder) and theoraenc (encoder)
		# We need theoradec for video playback (contains th_decode_* functions)
		FIND_LIBRARY(THEORA_LIBRARY
			NAMES theoradec theora
			PATHS
				/usr/lib
				/usr/lib/x86_64-linux-gnu
				/usr/local/lib
				/opt/local/lib
		)
		IF(THEORA_INCLUDE_DIR AND THEORA_LIBRARY)
			SET(THEORA_FOUND TRUE)
			SET(THEORA_INCLUDE_DIRS ${THEORA_INCLUDE_DIR})
			SET(THEORA_LIBRARIES ${THEORA_LIBRARY})
		ENDIF()
	ENDIF()
	
	IF(THEORA_FOUND AND THEORA_LIBRARIES)
		TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${THEORA_INCLUDE_DIRS})
		TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_THEORA)
		# Note: client is an OBJECT library, so libraries must be linked to final executable
		# libogg linking is handled at the final executable linking stage
		MESSAGE(STATUS "Theora found: ${THEORA_LIBRARIES}")
	ELSE()
		MESSAGE(WARNING "Theora not found, disabling Theora support")
		MESSAGE(STATUS "To enable Theora support, install: sudo apt-get install libtheora-dev")
		SET(USE_THEORA OFF CACHE BOOL "" FORCE)
	ENDIF()
ENDIF()

# VPX (VP8/VP9) video codec support
IF(USE_VPX)
	# Try pkg-config first
	find_package(PkgConfig QUIET)
	IF(PKG_CONFIG_FOUND)
		pkg_check_modules(VPX libvpx QUIET)
		IF(VPX_FOUND)
			SET(VPX_INCLUDE_DIRS ${VPX_INCLUDE_DIRS})
			SET(VPX_LIBRARIES ${VPX_LIBRARIES})
		ENDIF()
	ENDIF()
	
	# Manual search as fallback
	IF(NOT VPX_FOUND)
		FIND_PATH(VPX_INCLUDE_DIR
			NAMES vpx/vpx_decoder.h
			PATHS
				/usr/include
				/usr/local/include
				/opt/local/include
		)
		FIND_LIBRARY(VPX_LIBRARY
			NAMES vpx
			PATHS
				/usr/lib
				/usr/lib/x86_64-linux-gnu
				/usr/local/lib
				/opt/local/lib
		)
		IF(VPX_INCLUDE_DIR AND VPX_LIBRARY)
			SET(VPX_FOUND TRUE)
			SET(VPX_INCLUDE_DIRS ${VPX_INCLUDE_DIR})
			SET(VPX_LIBRARIES ${VPX_LIBRARY})
		ENDIF()
	ENDIF()
	
	IF(VPX_FOUND AND VPX_LIBRARIES)
		TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${VPX_INCLUDE_DIRS})
		TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_VPX)
		MESSAGE(STATUS "VPX found: ${VPX_LIBRARIES}")
	ELSE()
		MESSAGE(WARNING "VPX not found, disabling VP8/VP9 support")
		MESSAGE(STATUS "To enable VPX support, install: sudo apt-get install libvpx-dev")
		SET(USE_VPX OFF CACHE BOOL "" FORCE)
	ENDIF()
ENDIF()

# dav1d (AV1) video codec support
IF(USE_DAV1D)
	# Try pkg-config first
	find_package(PkgConfig QUIET)
	IF(PKG_CONFIG_FOUND)
		pkg_check_modules(DAV1D libdav1d QUIET)
		IF(DAV1D_FOUND)
			SET(DAV1D_INCLUDE_DIRS ${DAV1D_INCLUDE_DIRS})
			SET(DAV1D_LIBRARIES ${DAV1D_LIBRARIES})
		ENDIF()
	ENDIF()

	# Manual search as fallback
	IF(NOT DAV1D_FOUND)
		FIND_PATH(DAV1D_INCLUDE_DIR
			NAMES dav1d/dav1d.h
			PATHS
				/usr/include
				/usr/local/include
				/opt/local/include
		)
		FIND_LIBRARY(DAV1D_LIBRARY
			NAMES dav1d
			PATHS
				/usr/lib
				/usr/lib/x86_64-linux-gnu
				/usr/local/lib
				/opt/local/lib
		)
		IF(DAV1D_INCLUDE_DIR AND DAV1D_LIBRARY)
			SET(DAV1D_FOUND TRUE)
			SET(DAV1D_INCLUDE_DIRS ${DAV1D_INCLUDE_DIR})
			SET(DAV1D_LIBRARIES ${DAV1D_LIBRARY})
		ENDIF()
	ENDIF()
	
	IF(DAV1D_FOUND AND DAV1D_LIBRARIES)
		TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${DAV1D_INCLUDE_DIRS})
		TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_DAV1D)
		MESSAGE(STATUS "dav1d found: ${DAV1D_LIBRARIES}")
	ELSE()
		MESSAGE(WARNING "dav1d not found, disabling AV1 support")
		MESSAGE(STATUS "To enable dav1d support, install: sudo apt-get install libdav1d-dev")
		SET(USE_DAV1D OFF CACHE BOOL "" FORCE)
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
			TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_VULKAN USE_VULKAN_API)
			IF(TARGET ${RENDERER_PREFIX}_vulkan${RENDEXT})
				TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_VULKAN)
				TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${VULKAN_LIBRARY})
			ELSEIF(TARGET ${RENDERER_PREFIX}_vulkan)
				TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan PRIVATE USE_VULKAN)
				TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_vulkan PRIVATE ${VULKAN_LIBRARY})
			ENDIF()
		ELSE()
			MESSAGE(WARNING "Vulkan library not found, disabling Vulkan support")
			SET(USE_VULKAN OFF CACHE BOOL "" FORCE)
		ENDIF()
	ELSE()
		# Try CMake's find_package first
		find_package(Vulkan)
		IF(Vulkan_FOUND)
			TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_VULKAN)
			TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${Vulkan_INCLUDE_DIRS})
			IF(TARGET ${RENDERER_PREFIX}_vulkan${RENDEXT})
				TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_VULKAN)
				TARGET_INCLUDE_DIRECTORIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PUBLIC ${Vulkan_INCLUDE_DIRS})
				TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${Vulkan_LIBRARIES})
			ELSEIF(TARGET ${RENDERER_PREFIX}_vulkan)
				TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan PRIVATE USE_VULKAN)
				TARGET_INCLUDE_DIRECTORIES(${RENDERER_PREFIX}_vulkan PUBLIC ${Vulkan_INCLUDE_DIRS})
				TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_vulkan PRIVATE ${Vulkan_LIBRARIES})
			ENDIF()
		ELSE()
			# Try to find Vulkan manually if CMake's find_package fails
			FIND_PATH(VULKAN_INCLUDE_DIR NAMES vulkan/vulkan.h PATHS /usr/include /usr/local/include)
			FIND_LIBRARY(VULKAN_LIBRARY NAMES vulkan PATHS /usr/lib /usr/lib64 /usr/local/lib /usr/lib/x86_64-linux-gnu)
			IF(VULKAN_INCLUDE_DIR AND VULKAN_LIBRARY)
				MESSAGE(STATUS "Found Vulkan manually: ${VULKAN_INCLUDE_DIR}, ${VULKAN_LIBRARY}")
				TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_VULKAN)
				TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${VULKAN_INCLUDE_DIR})
				IF(TARGET ${RENDERER_PREFIX}_vulkan${RENDEXT})
					TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE USE_VULKAN)
					TARGET_INCLUDE_DIRECTORIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PUBLIC ${VULKAN_INCLUDE_DIR})
					TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${VULKAN_LIBRARY})
				ELSEIF(TARGET ${RENDERER_PREFIX}_vulkan)
					TARGET_COMPILE_DEFINITIONS(${RENDERER_PREFIX}_vulkan PRIVATE USE_VULKAN)
					TARGET_INCLUDE_DIRECTORIES(${RENDERER_PREFIX}_vulkan PUBLIC ${VULKAN_INCLUDE_DIR})
					TARGET_LINK_LIBRARIES(${RENDERER_PREFIX}_vulkan PRIVATE ${VULKAN_LIBRARY})
				ENDIF()
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
		if(TARGET ${CNAME}${BINEXT})
			if(OPENEXR_INCLUDE_DIRS)
				target_include_directories(${CNAME}${BINEXT} PRIVATE ${OPENEXR_INCLUDE_DIRS})
				if(BUILD_SERVER AND NOT ANDROID AND TARGET ${DNAME}${BINEXT})
					target_include_directories(${DNAME}${BINEXT} PRIVATE ${OPENEXR_INCLUDE_DIRS})
				endif()
			endif()
			target_link_libraries(${CNAME}${BINEXT} ${OPENEXR_LIBRARIES})
			if(BUILD_SERVER AND NOT ANDROID AND TARGET ${DNAME}${BINEXT})
				target_link_libraries(${DNAME}${BINEXT} ${OPENEXR_LIBRARIES})
			endif()
		endif()
	endif()
ENDIF()

IF(USE_NANOSVG)
	# Attach nanosvg include paths to the appropriate targets
	if(USE_RENDERER_DLOPEN)
		if(TARGET ${RENDERER_PREFIX}_opengl${RENDEXT})
			target_include_directories(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE libs/nanosvg)
		endif()
		if(TARGET ${RENDERER_PREFIX}_vulkan${RENDEXT})
			target_include_directories(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE libs/nanosvg)
		endif()
		if(WIN32 AND USE_D3D12 AND TARGET ${RENDERER_PREFIX}_d3d12${RENDEXT})
			target_include_directories(${RENDERER_PREFIX}_d3d12${RENDEXT} PRIVATE libs/nanosvg)
		endif()
		if(APPLE AND USE_METAL AND NOT ANDROID AND TARGET ${RENDERER_PREFIX}_metal${RENDEXT})
			target_include_directories(${RENDERER_PREFIX}_metal${RENDEXT} PRIVATE libs/nanosvg)
		endif()
	else()
		# Non-DLOPEN renderers are linked directly into the main executable(s)
		if(TARGET ${CNAME}${BINEXT})
			target_include_directories(${CNAME}${BINEXT} PRIVATE libs/nanosvg)
			if(BUILD_SERVER AND NOT ANDROID AND TARGET ${DNAME}${BINEXT})
				target_include_directories(${DNAME}${BINEXT} PRIVATE libs/nanosvg)
			endif()
		endif()
	endif()
	
	# Suppress warnings from nanosvg third-party library
	IF(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
		set_source_files_properties(src/renderercommon/tr_image_svg.c PROPERTIES 
			COMPILE_FLAGS "-Wno-shadow -Wno-missing-prototypes")
	ENDIF()
ENDIF()

IF(USE_ASSIMP AND ASSIMP_FOUND AND ASSIMP_LIBRARIES)
	# Attach Assimp include paths and libraries to the appropriate targets
	if(USE_RENDERER_DLOPEN)
		if(TARGET ${RENDERER_PREFIX}_opengl${RENDEXT})
			if(ASSIMP_INCLUDE_DIRS)
				target_include_directories(${RENDERER_PREFIX}_opengl${RENDEXT} PRIVATE ${ASSIMP_INCLUDE_DIRS})
			endif()
			# Use plain signature to match existing linkage for this target
			target_link_libraries(${RENDERER_PREFIX}_opengl${RENDEXT} ${ASSIMP_LIBRARIES})
		endif()
		if(TARGET ${RENDERER_PREFIX}_vulkan${RENDEXT})
			if(ASSIMP_INCLUDE_DIRS)
				target_include_directories(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${ASSIMP_INCLUDE_DIRS})
			endif()
			target_link_libraries(${RENDERER_PREFIX}_vulkan${RENDEXT} PRIVATE ${ASSIMP_LIBRARIES})
		endif()
		if(WIN32 AND USE_D3D12 AND TARGET ${RENDERER_PREFIX}_d3d12${RENDEXT})
			if(ASSIMP_INCLUDE_DIRS)
				target_include_directories(${RENDERER_PREFIX}_d3d12${RENDEXT} PRIVATE ${ASSIMP_INCLUDE_DIRS})
			endif()
			target_link_libraries(${RENDERER_PREFIX}_d3d12${RENDEXT} PRIVATE ${ASSIMP_LIBRARIES})
		endif()
	else()
		# Non-DLOPEN renderers are linked directly into the main executable(s)
		if(TARGET ${CNAME}${BINEXT})
			if(ASSIMP_INCLUDE_DIRS)
				target_include_directories(${CNAME}${BINEXT} PRIVATE ${ASSIMP_INCLUDE_DIRS})
				if(BUILD_SERVER AND NOT ANDROID AND TARGET ${DNAME}${BINEXT})
					target_include_directories(${DNAME}${BINEXT} PRIVATE ${ASSIMP_INCLUDE_DIRS})
				endif()
			endif()
			target_link_libraries(${CNAME}${BINEXT} ${ASSIMP_LIBRARIES})
			if(BUILD_SERVER AND NOT ANDROID AND TARGET ${DNAME}${BINEXT})
				target_link_libraries(${DNAME}${BINEXT} ${ASSIMP_LIBRARIES})
			endif()
		endif()
	endif()
ENDIF()

IF(ANDROID)
	ADD_LIBRARY(${CNAME}${BINEXT} SHARED ${Q3_SRCS})
ELSE()
	ADD_EXECUTABLE(${CNAME}${BINEXT} ${EXE_TYPE} ${Q3_SRCS})
ENDIF()
TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE common botlib client q3ui GL)
TARGET_LINK_OPTIONS(${CNAME}${BINEXT} PRIVATE -Wl,--export-dynamic)
TARGET_INCLUDE_DIRECTORIES(${CNAME}${BINEXT} PRIVATE src/common src)
IF(USE_UI2)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ui2)
ENDIF()
IF(Lua_FOUND)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE Lua::lua)
ENDIF()

# Non-SDL UNIX UI code (src/unix/linux_glimp.c, etc.) depends on X11 symbols.
if(UNIX AND NOT USE_SDL)
	find_package(X11 REQUIRED)
	if(X11_FOUND)
		if(X11_INCLUDE_DIR)
			target_include_directories(${CNAME}${BINEXT} PRIVATE ${X11_INCLUDE_DIR})
		endif()
		# FindX11 populates X11_LIBRARIES with all required link items.
		if(X11_LIBRARIES)
			target_link_libraries(${CNAME}${BINEXT} PRIVATE ${X11_LIBRARIES})
		endif()
	endif()
endif()

# When not using dlopen-based renderers, link the selected renderer directly.
if(NOT USE_RENDERER_DLOPEN)
	if(RENDERER_DEFAULT STREQUAL "vulkan" AND USE_VULKAN AND TARGET ${RENDERER_PREFIX}_vulkan)
		target_link_libraries(${CNAME}${BINEXT} PRIVATE ${RENDERER_PREFIX}_vulkan)
		target_compile_definitions(${CNAME}${BINEXT} PRIVATE USE_VULKAN RENDERER_DEFAULT=${RENDERER_DEFAULT})
		if(IDTECH3_VK_EXPERIMENTAL)
			target_compile_definitions(${CNAME}${BINEXT} PRIVATE IDTECH3_VK_EXPERIMENTAL)
		endif()
	elseif(TARGET ${RENDERER_PREFIX}_opengl)
		target_link_libraries(${CNAME}${BINEXT} PRIVATE ${RENDERER_PREFIX}_opengl)
		target_compile_definitions(${CNAME}${BINEXT} PRIVATE RENDERER_DEFAULT=${RENDERER_DEFAULT})
	elseif(USE_VULKAN AND TARGET ${RENDERER_PREFIX}_vulkan)
		# Fallback to Vulkan if OpenGL not available
		target_link_libraries(${CNAME}${BINEXT} PRIVATE ${RENDERER_PREFIX}_vulkan)
		target_compile_definitions(${CNAME}${BINEXT} PRIVATE USE_VULKAN RENDERER_DEFAULT=${RENDERER_DEFAULT})
		if(IDTECH3_VK_EXPERIMENTAL)
			target_compile_definitions(${CNAME}${BINEXT} PRIVATE IDTECH3_VK_EXPERIMENTAL)
		endif()
	endif()
endif()

# Security hardening options
OPTION(ENABLE_SECURITY_HARDENING "Enable comprehensive security hardening flags" ON)
OPTION(ENABLE_STACK_PROTECTOR "Enable stack canaries/protection" ON)
OPTION(ENABLE_STACK_CLASH_PROTECTION "Enable stack clash protection" ON)
OPTION(ENABLE_FORTIFY_SOURCE "Enable fortified source functions" OFF)
OPTION(ENABLE_RELRO "Enable read-only relocations" ON)
OPTION(ENABLE_PIE "Enable position independent executable" ON)

# Incremental build options
OPTION(ENABLE_CCACHE "Enable ccache for faster incremental builds" ON)
OPTION(ENABLE_NINJA "Prefer Ninja build system for faster incremental builds" ON)
OPTION(ENABLE_BUILD_CACHE "Enable build cache for incremental builds" ON)
OPTION(ENABLE_DEPENDENCY_GRAPH "Generate dependency graph for analysis" OFF)
OPTION(ENABLE_INCREMENTAL_STATS "Enable incremental build statistics" ON)

# Monolithic build support: link game module targets directly
OPTION(COMBINED_MONOLITH "Build monolithic executable with game modules linked statically" OFF)
OPTION(MONOLITH_USE_LTO "Enable link-time optimization for monolithic builds" OFF)
OPTION(MONOLITH_USE_GC_SECTIONS "Enable function/data section GC for monolithic builds" OFF)

# Build optimization options
OPTION(ENABLE_LTO "Enable Link-Time Optimization" OFF)
OPTION(ENABLE_BUILD_OPTIMIZATION "Enable comprehensive build optimizations" OFF)
OPTION(ENABLE_BINARY_SIZE_OPTIMIZATION "Enable binary size reduction techniques" OFF)
OPTION(ENABLE_DEAD_CODE_ELIMINATION "Enable aggressive dead code elimination" OFF)
OPTION(ENABLE_PROFILE_GUIDED_OPTIMIZATION "Enable Profile-Guided Optimization support" OFF)
OPTION(ENABLE_BINARY_COMPRESSION "Enable binary compression (UPX)" OFF)
OPTION(ENABLE_BUILD_PERFORMANCE_MONITORING "Monitor build performance and size" ON)

# ============================================================================
# Incremental Build System Implementation
# ============================================================================

# Function to detect and configure ccache
FUNCTION(CONFIGURE_CCACHE)
    IF(NOT ENABLE_CCACHE)
        RETURN()
    ENDIF()

    FIND_PROGRAM(CCACHE_PROGRAM ccache)
    IF(CCACHE_PROGRAM)
        MESSAGE(STATUS "ccache found: ${CCACHE_PROGRAM}")

        # Set ccache as compiler launcher
        SET(CMAKE_C_COMPILER_LAUNCHER ${CCACHE_PROGRAM} PARENT_SCOPE)
        SET(CMAKE_CXX_COMPILER_LAUNCHER ${CCACHE_PROGRAM} PARENT_SCOPE)

        # Configure ccache settings
        SET(ENV{CCACHE_MAXSIZE} "5G")
        SET(ENV{CCACHE_COMPRESS} "true")
        SET(ENV{CCACHE_COMPRESSLEVEL} "6")
        SET(ENV{CCACHE_CPP2} "true")
        SET(ENV{CCACHE_HARDLINK} "true")
        SET(ENV{CCACHE_SLOPPINESS} "include_file_ctime,include_file_mtime")

        # Set cache directory
        SET(CCACHE_DIR "${CMAKE_BINARY_DIR}/.ccache" CACHE PATH "ccache directory")
        SET(ENV{CCACHE_DIR} "${CCACHE_DIR}")

        # Create ccache directory
        FILE(MAKE_DIRECTORY ${CCACHE_DIR})

        MESSAGE(STATUS "ccache configured with cache directory: ${CCACHE_DIR}")
    ELSE()
        MESSAGE(WARNING "ccache not found. Install ccache for faster incremental builds.")
        SET(ENABLE_CCACHE OFF CACHE BOOL "" FORCE)
    ENDIF()
ENDFUNCTION()

# Function to configure Ninja build system
FUNCTION(CONFIGURE_NINJA)
    IF(NOT ENABLE_NINJA)
        RETURN()
    ENDIF()

    # Check if Ninja is available
    FIND_PROGRAM(NINJA_EXECUTABLE ninja)
    IF(NINJA_EXECUTABLE)
        MESSAGE(STATUS "Ninja found: ${NINJA_EXECUTABLE}")

        # Set Ninja as preferred generator
        SET(CMAKE_GENERATOR "Ninja" CACHE INTERNAL "" FORCE)

        # Configure Ninja-specific options
        SET(CMAKE_MAKE_PROGRAM ${NINJA_EXECUTABLE} CACHE FILEPATH "" FORCE)

        MESSAGE(STATUS "Using Ninja build system for faster incremental builds")
    ELSE()
        MESSAGE(WARNING "Ninja not found. Install ninja for faster incremental builds.")
        SET(ENABLE_NINJA OFF CACHE BOOL "" FORCE)
    ENDIF()
ENDFUNCTION()

# Function to configure build cache
FUNCTION(CONFIGURE_BUILD_CACHE)
    IF(NOT ENABLE_BUILD_CACHE)
        RETURN()
    ENDIF()

    # Enable CMake build cache
    SET(CMAKE_CACHEFILE_DIR "${CMAKE_BINARY_DIR}/.cmake_cache" CACHE PATH "CMake cache directory")
    FILE(MAKE_DIRECTORY ${CMAKE_CACHEFILE_DIR})

    # Configure cache variables for faster rebuilds
    SET(CMAKE_C_COMPILER_LAUNCHER ${CMAKE_C_COMPILER_LAUNCHER} CACHE STRING "" FORCE)
    SET(CMAKE_CXX_COMPILER_LAUNCHER ${CMAKE_CXX_COMPILER_LAUNCHER} CACHE STRING "" FORCE)
    SET(CMAKE_C_LINKER_LAUNCHER ${CMAKE_C_LINKER_LAUNCHER} CACHE STRING "" FORCE)
    SET(CMAKE_CXX_LINKER_LAUNCHER ${CMAKE_CXX_LINKER_LAUNCHER} CACHE STRING "" FORCE)

    MESSAGE(STATUS "Build cache enabled with directory: ${CMAKE_CACHEFILE_DIR}")
ENDFUNCTION()

# Function to generate dependency graph
FUNCTION(GENERATE_DEPENDENCY_GRAPH)
    IF(NOT ENABLE_DEPENDENCY_GRAPH)
        RETURN()
    ENDIF()

    SET(GRAPH_FILE "${CMAKE_BINARY_DIR}/dependency_graph.dot")
    SET(GRAPH_PNG "${CMAKE_BINARY_DIR}/dependency_graph.png")

    # Generate DOT file for dependency graph
    FILE(WRITE ${GRAPH_FILE} "digraph DependencyGraph {\n")
    FILE(APPEND ${GRAPH_FILE} "  rankdir=LR;\n")
    FILE(APPEND ${GRAPH_FILE} "  node [shape=box];\n\n")

    # Add main executable dependencies
    IF(TARGET ${CNAME}${BINEXT})
        FILE(APPEND ${GRAPH_FILE} "  \"${CNAME}${BINEXT}\" [fillcolor=lightblue, style=filled];\n")

        # Add renderer dependencies
        IF(TARGET ${RENDERER_PREFIX}_opengl${RENDEXT})
            FILE(APPEND ${GRAPH_FILE} "  \"${CNAME}${BINEXT}\" -> \"${RENDERER_PREFIX}_opengl${RENDEXT}\";\n")
        ENDIF()
        IF(TARGET ${RENDERER_PREFIX}_vulkan${RENDEXT})
            FILE(APPEND ${GRAPH_FILE} "  \"${CNAME}${BINEXT}\" -> \"${RENDERER_PREFIX}_vulkan${RENDEXT}\";\n")
        ENDIF()
    ENDIF()

    FILE(APPEND ${GRAPH_FILE} "}\n")

    # Try to generate PNG if graphviz is available
    FIND_PROGRAM(DOT_EXECUTABLE dot)
    IF(DOT_EXECUTABLE)
        ADD_CUSTOM_COMMAND(
            OUTPUT ${GRAPH_PNG}
            COMMAND ${DOT_EXECUTABLE} -Tpng ${GRAPH_FILE} -o ${GRAPH_PNG}
            DEPENDS ${GRAPH_FILE}
            COMMENT "Generating dependency graph PNG"
        )
        ADD_CUSTOM_TARGET(dependency_graph DEPENDS ${GRAPH_PNG})
        MESSAGE(STATUS "Dependency graph will be generated: ${GRAPH_PNG}")
    ELSE()
        MESSAGE(STATUS "Dependency graph DOT file: ${GRAPH_FILE}")
        MESSAGE(WARNING "Graphviz not found. Install graphviz to generate PNG dependency graph.")
    ENDIF()
ENDFUNCTION()

# Function to setup incremental build monitoring
FUNCTION(SETUP_INCREMENTAL_MONITORING)
    IF(NOT ENABLE_INCREMENTAL_STATS)
        RETURN()
    ENDIF()

    # Create build timing scripts
    SET(BUILD_START_SCRIPT "${CMAKE_BINARY_DIR}/build_start.sh")
    SET(BUILD_END_SCRIPT "${CMAKE_BINARY_DIR}/build_end.sh")
    SET(BUILD_STATS_FILE "${CMAKE_BINARY_DIR}/build_stats.txt")

    FILE(WRITE ${BUILD_START_SCRIPT}
        "#!/bin/bash\n"
        "echo \"Build started at: \$(date)\" > ${BUILD_STATS_FILE}\n"
        "echo \"Build start time: \$(date +%s)\" >> ${BUILD_STATS_FILE}\n"
    )

    FILE(WRITE ${BUILD_END_SCRIPT}
        "#!/bin/bash\n"
        "echo \"Build ended at: \$(date)\" >> ${BUILD_STATS_FILE}\n"
        "echo \"Build end time: \$(date +%s)\" >> ${BUILD_STATS_FILE}\n"
        "START_TIME=\$(grep 'Build start time:' ${BUILD_STATS_FILE} | cut -d: -f2)\n"
        "END_TIME=\$(grep 'Build end time:' ${BUILD_STATS_FILE} | tail -1 | cut -d: -f2)\n"
        "if [ -n \"\$START_TIME\" ] && [ -n \"\$END_TIME\" ]; then\n"
        "    DURATION=\$((END_TIME - START_TIME))\n"
        "    echo \"Build duration: \${DURATION} seconds\" >> ${BUILD_STATS_FILE}\n"
        "    echo \"Incremental build completed in \${DURATION} seconds\"\n"
        "fi\n"
    )

    FILE(CHMOD ${BUILD_START_SCRIPT} PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
    FILE(CHMOD ${BUILD_END_SCRIPT} PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)

    MESSAGE(STATUS "Incremental build monitoring enabled")
    MESSAGE(STATUS "Build statistics: ${BUILD_STATS_FILE}")
ENDFUNCTION()

# Configure incremental build components
CONFIGURE_CCACHE()
CONFIGURE_NINJA()
CONFIGURE_BUILD_CACHE()
GENERATE_DEPENDENCY_GRAPH()
SETUP_INCREMENTAL_MONITORING()

# ============================================================================
# Integrate Incremental Build Monitor Script
# ============================================================================

# Include the incremental build monitor script if it exists
if(EXISTS "${CMAKE_SOURCE_DIR}/tools/incremental_build_monitor.cmake")
    include(${CMAKE_SOURCE_DIR}/tools/incremental_build_monitor.cmake)
    setup_incremental_build_monitor()
else()
    message(WARNING "Incremental build monitor script not found: tools/incremental_build_monitor.cmake")
endif()

# ============================================================================
# Build Optimization System Implementation
# ============================================================================

# Function to configure Link-Time Optimization (LTO)
FUNCTION(CONFIGURE_LTO TARGET_NAME)
    IF(NOT ENABLE_LTO)
        RETURN()
    ENDIF()

    MESSAGE(STATUS "Configuring LTO for target: ${TARGET_NAME}")

    IF(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        # GCC LTO configuration
        TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE -flto -flto-partition=none)
        TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE -flto -flto-partition=none)

        # Additional GCC LTO optimizations
        TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE
            -fwhole-program         # Whole program optimization
            -fipa-pta               # Interprocedural points-to analysis
            -fipa-reference         # Interprocedural reference analysis
        )

    ELSEIF(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Clang LTO configuration
        TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE -flto)
        TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE -flto)

        # ThinLTO for faster compilation (Clang-specific)
        OPTION(USE_THINLTO "Use ThinLTO instead of full LTO for faster builds" ON)
        IF(USE_THINLTO)
            TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE -flto=thin)
            TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE -flto=thin)
            MESSAGE(STATUS "Using ThinLTO for faster incremental builds")
        ENDIF()

    ELSEIF(MSVC)
        # MSVC LTO configuration
        TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE /GL)  # Whole program optimization
        TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE /LTCG)   # Link-time code generation
    ENDIF()

    MESSAGE(STATUS "LTO configured for ${TARGET_NAME}")
ENDFUNCTION()

# Function to configure binary size optimizations
FUNCTION(CONFIGURE_BINARY_SIZE_OPTIMIZATION TARGET_NAME)
    IF(NOT ENABLE_BINARY_SIZE_OPTIMIZATION)
        RETURN()
    ENDIF()

    MESSAGE(STATUS "Configuring binary size optimization for target: ${TARGET_NAME}")

    # Common size optimization flags
    TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE
        -Os                    # Optimize for size
        -ffunction-sections    # Place each function in its own section
        -fdata-sections        # Place each data item in its own section
        -fno-exceptions        # Disable exception handling
        $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>  # Disable RTTI (C++ only)
        -fno-unwind-tables     # Remove unwind tables
        -fomit-frame-pointer   # Omit frame pointers where possible
        -fmerge-constants      # Merge identical constants
        -fmerge-all-constants  # Merge all constants
    )

    # Linker optimizations for size
    TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE
        # -Wl,--gc-sections      # Garbage collect unused sections (Disabled to prevent deleting stuff)
        # -Wl,--as-needed        # Only link libraries that are actually used (Keep this for now as it's common)
        # -Wl,--strip-all        # Strip all symbols (Disabled to prevent deleting stuff)
        -Wl,--sort-common      # Sort common symbols by size
        -Wl,--sort-section=alignment  # Sort sections by alignment
    )

    # Additional optimizations based on compiler
    IF(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE
            -fipa-ra                # Interprocedural register allocation
            -fipa-reference         # Interprocedural reference analysis
            -fipa-pure-const        # Interprocedural pure/const analysis
            -fshrink-wrap           # Shrink-wrap code around function calls
            -fshrink-wrap-separate  # Separate shrink-wrapping
        )

        TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE
            # -Wl,--print-gc-sections # Show garbage collected sections (Disabled)
        )

    ELSEIF(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE
            -fvectorize-slp        # Enable SLP vectorization
        )

        TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE
            # Identical Code Folding not supported by all linkers
        )
    ENDIF()

    MESSAGE(STATUS "Binary size optimization configured for ${TARGET_NAME}")
ENDFUNCTION()

# Function to configure dead code elimination
FUNCTION(CONFIGURE_DEAD_CODE_ELIMINATION TARGET_NAME)
    IF(NOT ENABLE_DEAD_CODE_ELIMINATION)
        RETURN()
    ENDIF()

    MESSAGE(STATUS "Configuring dead code elimination for target: ${TARGET_NAME}")

    # Aggressive dead code elimination
    TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE
        -ffunction-sections     # Place functions in separate sections
        -fdata-sections         # Place data in separate sections
        -fvisibility=hidden     # Hide symbols by default
        $<$<COMPILE_LANGUAGE:CXX>:-fvisibility-inlines-hidden>  # Hide inline functions (C++ only)
    )

    # Linker dead code elimination
    TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE
        # -Wl,--gc-sections       # Remove unused sections (Disabled to prevent deleting stuff)
        # -Wl,--discard-all       # Discard local symbols (Disabled to prevent deleting stuff)
        # -Wl,--discard-locals    # Discard temporary local symbols (Disabled to prevent deleting stuff)
    )

    # Additional compiler-specific optimizations
    IF(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE
            -fwhole-program       # Whole program optimization
            -fipa-pta             # Points-to analysis
            -fdevirtualize-at-ltrans # Devirtualization at link time
        )
    ENDIF()

    MESSAGE(STATUS "Dead code elimination configured for ${TARGET_NAME}")
ENDFUNCTION()

# Function to configure Profile-Guided Optimization (PGO)
FUNCTION(CONFIGURE_PGO TARGET_NAME)
    IF(NOT ENABLE_PROFILE_GUIDED_OPTIMIZATION)
        RETURN()
    ENDIF()

    MESSAGE(STATUS "Configuring PGO for target: ${TARGET_NAME}")

    # PGO requires two-stage compilation
    OPTION(PGO_INSTRUMENT "Build with PGO instrumentation" OFF)
    OPTION(PGO_USE_PROFILE "Use PGO profile data for optimization" OFF)

    IF(PGO_INSTRUMENT)
        # Instrumentation phase
        TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE -fprofile-generate)
        TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE -fprofile-generate)

        MESSAGE(STATUS "PGO: Instrumentation phase configured")

    ELSEIF(PGO_USE_PROFILE)
        # Optimization phase
        TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE -fprofile-use -fprofile-correction)
        TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE -fprofile-use)

        MESSAGE(STATUS "PGO: Optimization phase configured")

    ELSE()
        MESSAGE(WARNING "PGO enabled but neither PGO_INSTRUMENT nor PGO_USE_PROFILE is set")
    ENDIF()
ENDFUNCTION()

# Function to configure binary compression
FUNCTION(CONFIGURE_BINARY_COMPRESSION TARGET_NAME)
    IF(NOT ENABLE_BINARY_COMPRESSION)
        RETURN()
    ENDIF()

    # Find UPX (Ultimate Packer for eXecutables)
    FIND_PROGRAM(UPX_EXECUTABLE upx)
    IF(UPX_EXECUTABLE)
        MESSAGE(STATUS "UPX found: ${UPX_EXECUTABLE}")

        # Add UPX compression as post-build step
        ADD_CUSTOM_COMMAND(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${UPX_EXECUTABLE} --best --lzma $<TARGET_FILE:${TARGET_NAME}>
            COMMENT "Compressing binary with UPX"
        )

        MESSAGE(STATUS "Binary compression configured for ${TARGET_NAME}")
    ELSE()
        MESSAGE(WARNING "UPX not found. Install UPX for binary compression.")
        SET(ENABLE_BINARY_COMPRESSION OFF CACHE BOOL "" FORCE)
    ENDIF()
ENDFUNCTION()

# Function to monitor build performance and size
FUNCTION(MONITOR_BUILD_OPTIMIZATION TARGET_NAME)
    IF(NOT ENABLE_BUILD_PERFORMANCE_MONITORING)
        RETURN()
    ENDIF()

    # Get build start time
    STRING(TIMESTAMP BUILD_START_TIME "%s" UTC)

    # Add post-build analysis (disabled - script not found)
    # ADD_CUSTOM_COMMAND(TARGET ${TARGET_NAME} POST_BUILD
    #     COMMAND ${CMAKE_COMMAND}
    #         -D TARGET_FILE=$<TARGET_FILE:${TARGET_NAME}>
    #         -D BUILD_START_TIME=${BUILD_START_TIME}
    #         -D CMAKE_BINARY_DIR=${CMAKE_BINARY_DIR}
    #     -P ${CMAKE_SOURCE_DIR}/tools/analyze_build_optimization.cmake
    #     COMMENT "Analyzing build optimization results"
    # )
ENDFUNCTION()

# ============================================================================
# Apply Build Optimizations to All Targets
# ============================================================================

# Apply optimizations to main executable
IF(TARGET ${CNAME}${BINEXT})
    CONFIGURE_LTO(${CNAME}${BINEXT})
    CONFIGURE_BINARY_SIZE_OPTIMIZATION(${CNAME}${BINEXT})
    CONFIGURE_DEAD_CODE_ELIMINATION(${CNAME}${BINEXT})
    CONFIGURE_PGO(${CNAME}${BINEXT})
    CONFIGURE_BINARY_COMPRESSION(${CNAME}${BINEXT})
    MONITOR_BUILD_OPTIMIZATION(${CNAME}${BINEXT})
    MESSAGE(STATUS "Applied build optimizations to main executable: ${CNAME}${BINEXT}")
ENDIF()

# Apply optimizations to renderer libraries
IF(TARGET ${RENDERER_PREFIX}_opengl${RENDEXT})
    # Skip LTO for dlopen renderers as it conflicts with --unresolved-symbols=ignore-all
    # CONFIGURE_LTO(${RENDERER_PREFIX}_opengl${RENDEXT})
    CONFIGURE_BINARY_SIZE_OPTIMIZATION(${RENDERER_PREFIX}_opengl${RENDEXT})
    CONFIGURE_DEAD_CODE_ELIMINATION(${RENDERER_PREFIX}_opengl${RENDEXT})
    # MONITOR_BUILD_OPTIMIZATION(${RENDERER_PREFIX}_opengl${RENDEXT})  # Disabled - script missing
    MESSAGE(STATUS "Applied build optimizations to OpenGL renderer: ${RENDERER_PREFIX}_opengl${RENDEXT} (LTO disabled for dlopen compatibility)")
ENDIF()

IF(TARGET ${RENDERER_PREFIX}_vulkan${RENDEXT})
    # CONFIGURE_LTO(${RENDERER_PREFIX}_vulkan${RENDEXT})
    CONFIGURE_BINARY_SIZE_OPTIMIZATION(${RENDERER_PREFIX}_vulkan${RENDEXT})
    CONFIGURE_DEAD_CODE_ELIMINATION(${RENDERER_PREFIX}_vulkan${RENDEXT})
    MONITOR_BUILD_OPTIMIZATION(${RENDERER_PREFIX}_vulkan${RENDEXT})
    MESSAGE(STATUS "Applied build optimizations to Vulkan renderer: ${RENDERER_PREFIX}_vulkan${RENDEXT}")
ENDIF()

# Apply to other renderers if they exist
IF(TARGET ${RENDERER_PREFIX}_d3d12${RENDEXT})
    CONFIGURE_LTO(${RENDERER_PREFIX}_d3d12${RENDEXT})
    CONFIGURE_BINARY_SIZE_OPTIMIZATION(${RENDERER_PREFIX}_d3d12${RENDEXT})
    CONFIGURE_DEAD_CODE_ELIMINATION(${RENDERER_PREFIX}_d3d12${RENDEXT})
    MONITOR_BUILD_OPTIMIZATION(${RENDERER_PREFIX}_d3d12${RENDEXT})
    MESSAGE(STATUS "Applied build optimizations to D3D12 renderer: ${RENDERER_PREFIX}_d3d12${RENDEXT}")
ENDIF()

IF(TARGET ${RENDERER_PREFIX}_metal${RENDEXT})
    CONFIGURE_LTO(${RENDERER_PREFIX}_metal${RENDEXT})
    CONFIGURE_BINARY_SIZE_OPTIMIZATION(${RENDERER_PREFIX}_metal${RENDEXT})
    CONFIGURE_DEAD_CODE_ELIMINATION(${RENDERER_PREFIX}_metal${RENDEXT})
    MONITOR_BUILD_OPTIMIZATION(${RENDERER_PREFIX}_metal${RENDEXT})
    MESSAGE(STATUS "Applied build optimizations to Metal renderer: ${RENDERER_PREFIX}_metal${RENDEXT}")
ENDIF()

# Apply to static renderer objects (non-dlopen builds)
IF(NOT USE_RENDERER_DLOPEN)
    IF(TARGET ${RENDERER_PREFIX}_opengl)
        CONFIGURE_LTO(${RENDERER_PREFIX}_opengl)
        CONFIGURE_BINARY_SIZE_OPTIMIZATION(${RENDERER_PREFIX}_opengl)
        CONFIGURE_DEAD_CODE_ELIMINATION(${RENDERER_PREFIX}_opengl)
        MONITOR_BUILD_OPTIMIZATION(${RENDERER_PREFIX}_opengl)
    ENDIF()
    IF(TARGET ${RENDERER_PREFIX}_vulkan)
        CONFIGURE_LTO(${RENDERER_PREFIX}_vulkan)
        CONFIGURE_BINARY_SIZE_OPTIMIZATION(${RENDERER_PREFIX}_vulkan)
        CONFIGURE_DEAD_CODE_ELIMINATION(${RENDERER_PREFIX}_vulkan)
        MONITOR_BUILD_OPTIMIZATION(${RENDERER_PREFIX}_vulkan)
    ENDIF()
    IF(TARGET ${RENDERER_PREFIX}_d3d12)
        CONFIGURE_LTO(${RENDERER_PREFIX}_d3d12)
        CONFIGURE_BINARY_SIZE_OPTIMIZATION(${RENDERER_PREFIX}_d3d12)
        CONFIGURE_DEAD_CODE_ELIMINATION(${RENDERER_PREFIX}_d3d12)
        MONITOR_BUILD_OPTIMIZATION(${RENDERER_PREFIX}_d3d12)
    ENDIF()
    IF(TARGET ${RENDERER_PREFIX}_metal)
        CONFIGURE_LTO(${RENDERER_PREFIX}_metal)
        CONFIGURE_BINARY_SIZE_OPTIMIZATION(${RENDERER_PREFIX}_metal)
        CONFIGURE_DEAD_CODE_ELIMINATION(${RENDERER_PREFIX}_metal)
        MONITOR_BUILD_OPTIMIZATION(${RENDERER_PREFIX}_metal)
    ENDIF()
ENDIF()

# Display build optimization status
IF(ENABLE_BUILD_OPTIMIZATION)
    MESSAGE(STATUS "============================================================")
    MESSAGE(STATUS "Build Optimization Configuration:")
    MESSAGE(STATUS "  LTO: ${ENABLE_LTO}")
    MESSAGE(STATUS "  Binary Size Optimization: ${ENABLE_BINARY_SIZE_OPTIMIZATION}")
    MESSAGE(STATUS "  Dead Code Elimination: ${ENABLE_DEAD_CODE_ELIMINATION}")
    MESSAGE(STATUS "  PGO: ${ENABLE_PROFILE_GUIDED_OPTIMIZATION}")
    MESSAGE(STATUS "  Binary Compression: ${ENABLE_BINARY_COMPRESSION}")
    MESSAGE(STATUS "  Build Monitoring: ${ENABLE_BUILD_PERFORMANCE_MONITORING}")
    MESSAGE(STATUS "============================================================")
ELSE()
    MESSAGE(STATUS "Build optimizations disabled (ENABLE_BUILD_OPTIMIZATION=OFF)")
ENDIF()

# Display incremental build status
IF(ENABLE_CCACHE OR ENABLE_NINJA OR ENABLE_BUILD_CACHE)
    MESSAGE(STATUS "============================================================")
    MESSAGE(STATUS "Incremental Build Configuration:")
    MESSAGE(STATUS "  ccache: ${ENABLE_CCACHE}")
    MESSAGE(STATUS "  Ninja: ${ENABLE_NINJA}")
    MESSAGE(STATUS "  Build Cache: ${ENABLE_BUILD_CACHE}")
    MESSAGE(STATUS "  Dependency Graph: ${ENABLE_DEPENDENCY_GRAPH}")
    MESSAGE(STATUS "  Build Statistics: ${ENABLE_INCREMENTAL_STATS}")
    MESSAGE(STATUS "============================================================")
ENDIF()

# ============================================================================
# Security Hardening Implementation
# ============================================================================

# Function to apply security hardening flags to a target
FUNCTION(APPLY_SECURITY_HARDENING TARGET_NAME)
    IF(NOT ENABLE_SECURITY_HARDENING)
        RETURN()
    ENDIF()

    MESSAGE(STATUS "Applying security hardening to target: ${TARGET_NAME}")

    # Compiler-specific security flags
    IF(CMAKE_C_COMPILER_ID MATCHES "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        # GCC-specific flags
        IF(ENABLE_STACK_PROTECTOR)
            TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE -fstack-protector-strong)
            TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE -fstack-protector-strong)
        ENDIF()

        IF(ENABLE_STACK_CLASH_PROTECTION)
            TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE -fstack-clash-protection)
        ENDIF()

        IF(ENABLE_FORTIFY_SOURCE)
            TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE -D_FORTIFY_SOURCE=0)
        ENDIF()

        IF(ENABLE_PIE)
            GET_TARGET_PROPERTY(TARGET_TYPE ${TARGET_NAME} TYPE)
            IF(TARGET_TYPE STREQUAL "EXECUTABLE")
                TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE -fPIE)
                TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE -pie)
            ELSE()
                TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE -fPIC)
            ENDIF()
        ENDIF()

        # AddressSanitizer (mutually exclusive with other sanitizers for production)
        IF(ENABLE_ASAN_TEMPORARY_DISABLED)
            TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE -fsanitize=address -fsanitize-address-use-after-scope)
            TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE -fsanitize=address)
        ENDIF()

        # Additional GCC security flags
        TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE
            $<$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,8.0>:-fstack-clash-protection>
            $<$<VERSION_LESS:$<C_COMPILER_VERSION>,8.0>:-fstack-check>
            -fcf-protection=full    # Control flow protection
            -fexceptions           # Enable exception handling
            -fnon-call-exceptions  # Generate traps for invalid pointer dereferences
            -fno-delete-null-pointer-checks  # Keep null pointer checks
            -fno-omit-frame-pointer  # Keep frame pointers for stack traces
        )

        # Linker security flags
        IF(ENABLE_RELRO)
            TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE
                -Wl,-z,relro,-z,now      # Read-only relocations, full RELRO
                -Wl,-z,noexecstack       # Non-executable stack
                -Wl,-z,noexecstack        # Non-executable stack (if supported)
            )
        ENDIF()

        TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE
            -Wl,--as-needed            # Only link libraries that are actually used
            -Wl,--no-undefined         # Reject undefined symbols at link time
        )

    ELSEIF(CMAKE_C_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Clang-specific flags
        IF(ENABLE_STACK_PROTECTOR)
            TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE -fstack-protector-strong)
            TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE -fstack-protector-strong)
        ENDIF()

        IF(ENABLE_STACK_CLASH_PROTECTION)
            TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE -fstack-clash-protection)
        ENDIF()

        IF(ENABLE_FORTIFY_SOURCE)
            TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE -D_FORTIFY_SOURCE=0)
        ENDIF()

        IF(ENABLE_PIE)
            GET_TARGET_PROPERTY(TARGET_TYPE ${TARGET_NAME} TYPE)
            IF(TARGET_TYPE STREQUAL "EXECUTABLE")
                TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE -fPIE)
                TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE -pie)
            ELSE()
                TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE -fPIC)
            ENDIF()
        ENDIF()

        # AddressSanitizer
        IF(ENABLE_ASAN_TEMPORARY_DISABLED)
            TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE -fsanitize=address -fsanitize-address-use-after-scope)
            TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE -fsanitize=address)
        ENDIF()

        # Additional Clang security flags
        TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE
            $<$<VERSION_GREATER_EQUAL:$<C_COMPILER_VERSION>,8.0>:-fstack-clash-protection>
            $<$<VERSION_LESS:$<C_COMPILER_VERSION>,8.0>:-fstack-check>
            -fcf-protection=full    # Control flow protection
            -fexceptions           # Enable exception handling
            -fnon-call-exceptions  # Generate traps for invalid pointer dereferences
            -fno-omit-frame-pointer  # Keep frame pointers for stack traces
        )

        # Linker security flags
        IF(ENABLE_RELRO)
            TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE
                -Wl,-z,relro,-z,now      # Read-only relocations, full RELRO
                -Wl,-z,noexecstack       # Non-executable stack
                -Wl,-z,noexecstack        # Non-executable stack (if supported)
            )
        ENDIF()

        TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE
            -Wl,--as-needed            # Only link libraries that are actually used
            -Wl,--no-undefined         # Reject undefined symbols at link time
        )

    ELSEIF(MSVC)
        # MSVC-specific flags
        IF(ENABLE_STACK_PROTECTOR)
            TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE /GS)  # Buffer security check
        ENDIF()

        # MSVC security flags
        TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE
            /guard:cf                 # Control Flow Guard
            /guard:ehcont            # EH Continuation Guard
            /sdl                     # Security Development Lifecycle checks
            /DYNAMICBASE             # Address space layout randomization
            /HIGHENTROPYVA           # High entropy ASLR
        )

        TARGET_LINK_OPTIONS(${TARGET_NAME} PRIVATE
            /NXCOMPAT                # Data Execution Prevention
            /SAFESEH                 # Safe exception handlers
            /guard:cf                # Control Flow Guard
        )
    ENDIF()

    # Common security flags for all compilers
    IF(ENABLE_FORTIFY_SOURCE)
        TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE
            -U_FORTIFY_SOURCE  # Undefine first to avoid conflicts
            -D_FORTIFY_SOURCE=0
        )
    ENDIF()

    # Disable optimizations that might interfere with security
    TARGET_COMPILE_OPTIONS(${TARGET_NAME} PRIVATE
        -fno-strict-overflow     # Prevent undefined behavior from signed overflow
        -fno-delete-null-pointer-checks  # Keep null pointer checks
        -fwrapv                  # Defined signed overflow behavior
    )

    MESSAGE(STATUS "Security hardening applied to ${TARGET_NAME}")
ENDFUNCTION()

# ============================================================================
# Apply Security Hardening to All Targets
# ============================================================================

# Apply to main executable
IF(TARGET ${CNAME}${BINEXT})
    APPLY_SECURITY_HARDENING(${CNAME}${BINEXT})
    MESSAGE(STATUS "Applied security hardening to main executable: ${CNAME}${BINEXT}")
ENDIF()

# Apply to renderer libraries
IF(TARGET ${RENDERER_PREFIX}_opengl${RENDEXT})
    APPLY_SECURITY_HARDENING(${RENDERER_PREFIX}_opengl${RENDEXT})
    MESSAGE(STATUS "Applied security hardening to OpenGL renderer: ${RENDERER_PREFIX}_opengl${RENDEXT}")
ENDIF()

IF(TARGET ${RENDERER_PREFIX}_vulkan${RENDEXT})
    APPLY_SECURITY_HARDENING(${RENDERER_PREFIX}_vulkan${RENDEXT})
    MESSAGE(STATUS "Applied security hardening to Vulkan renderer: ${RENDERER_PREFIX}_vulkan${RENDEXT}")
ENDIF()

IF(TARGET ${RENDERER_PREFIX}_d3d12${RENDEXT})
    APPLY_SECURITY_HARDENING(${RENDERER_PREFIX}_d3d12${RENDEXT})
    MESSAGE(STATUS "Applied security hardening to D3D12 renderer: ${RENDERER_PREFIX}_d3d12${RENDEXT}")
ENDIF()

IF(TARGET ${RENDERER_PREFIX}_metal${RENDEXT})
    APPLY_SECURITY_HARDENING(${RENDERER_PREFIX}_metal${RENDEXT})
    MESSAGE(STATUS "Applied security hardening to Metal renderer: ${RENDERER_PREFIX}_metal${RENDEXT}")
ENDIF()

# Apply to static renderer objects (non-dlopen builds)
IF(NOT USE_RENDERER_DLOPEN)
    IF(TARGET ${RENDERER_PREFIX}_opengl)
        APPLY_SECURITY_HARDENING(${RENDERER_PREFIX}_opengl)
    ENDIF()
    IF(TARGET ${RENDERER_PREFIX}_vulkan)
        APPLY_SECURITY_HARDENING(${RENDERER_PREFIX}_vulkan)
    ENDIF()
    IF(TARGET ${RENDERER_PREFIX}_d3d12)
        APPLY_SECURITY_HARDENING(${RENDERER_PREFIX}_d3d12)
    ENDIF()
    IF(TARGET ${RENDERER_PREFIX}_metal)
        APPLY_SECURITY_HARDENING(${RENDERER_PREFIX}_metal)
    ENDIF()
ENDIF()

# Display security hardening status
IF(ENABLE_SECURITY_HARDENING)
    MESSAGE(STATUS "============================================================")
    MESSAGE(STATUS "Security Hardening Status:")
    MESSAGE(STATUS "  Stack Protector: ${ENABLE_STACK_PROTECTOR}")
    MESSAGE(STATUS "  Address Sanitizer: ${ENABLE_ASAN}")
    MESSAGE(STATUS "  Stack Clash Protection: ${ENABLE_STACK_CLASH_PROTECTION}")
    MESSAGE(STATUS "  Fortify Source: ${ENABLE_FORTIFY_SOURCE}")
    MESSAGE(STATUS "  RELRO: ${ENABLE_RELRO}")
    MESSAGE(STATUS "  PIE: ${ENABLE_PIE}")
    MESSAGE(STATUS "============================================================")
ELSE()
    MESSAGE(STATUS "Security hardening disabled (ENABLE_SECURITY_HARDENING=OFF)")
ENDIF()
IF(COMBINED_MONOLITH)
	ADD_DEFINITIONS(-DCOMBINED_MONOLITH)
	IF(BUILD_SHARED_LIBS)
		MESSAGE(FATAL_ERROR "COMBINED_MONOLITH requires BUILD_SHARED_LIBS=OFF")
	ENDIF()
	IF(NOT DEFINED MONOLITH_MOD_NAME)
		SET(MONOLITH_MOD_NAME "mymod")
	ENDIF()
	IF(NOT DEFINED MONOLITH_MOD_DIR)
		SET(MONOLITH_MOD_DIR "${CMAKE_SOURCE_DIR}/${MONOLITH_MOD_NAME}")
	ENDIF()
	SET(MONOLITH_GAMESRC_DIR "${MONOLITH_MOD_DIR}/gamesrc")
	IF(NOT EXISTS "${MONOLITH_GAMESRC_DIR}/CMakeLists.txt")
		MESSAGE(FATAL_ERROR "COMBINED_MONOLITH: gamesrc not found at ${MONOLITH_GAMESRC_DIR}")
	ENDIF()
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64")
		set(MONOLITH_ARCH_SUFFIX "x86_64")
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
		set(MONOLITH_ARCH_SUFFIX "arm64")
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
		set(MONOLITH_ARCH_SUFFIX "arm")
	else()
		set(MONOLITH_ARCH_SUFFIX "x86")
	endif()
	set(ARCH_SUFFIX "${MONOLITH_ARCH_SUFFIX}" CACHE STRING "Arch suffix for monolith modules" FORCE)
	add_subdirectory(${MONOLITH_GAMESRC_DIR} ${CMAKE_BINARY_DIR}/monolith_gamesrc)
	foreach(_t game_${MONOLITH_ARCH_SUFFIX} cgame_${MONOLITH_ARCH_SUFFIX} ui_${MONOLITH_ARCH_SUFFIX})
		if(NOT TARGET ${_t})
			message(FATAL_ERROR "COMBINED_MONOLITH: expected target '${_t}' not found")
		endif()
	endforeach()
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE
		game_${MONOLITH_ARCH_SUFFIX}
		cgame_${MONOLITH_ARCH_SUFFIX}
		ui_${MONOLITH_ARCH_SUFFIX}
	)
	IF(DEFINED MONOLITH_EXECUTABLE_NAME)
		SET_TARGET_PROPERTIES(${CNAME}${BINEXT} PROPERTIES OUTPUT_NAME "${MONOLITH_EXECUTABLE_NAME}")
	ENDIF()
	# Always embed the default mod name for monolithic builds so fs_game defaults correctly.
	TARGET_COMPILE_DEFINITIONS(${CNAME}${BINEXT} PRIVATE DEFAULT_FS_GAME="${MONOLITH_MOD_NAME}")
	# Propagate the default mod name to shared/object libs that include filesystem code.
	TARGET_COMPILE_DEFINITIONS(common PRIVATE DEFAULT_FS_GAME="${MONOLITH_MOD_NAME}")
	TARGET_COMPILE_DEFINITIONS(common_ded PRIVATE DEFAULT_FS_GAME="${MONOLITH_MOD_NAME}")
	# When linking statically in monolithic mode, allow duplicate symbols coming from
	# the statically linked game/cgame/ui modules (they provide their own syscall
	# trampolines and logging wrappers). The engine copies take precedence.
	if (UNIX AND NOT APPLE)
		target_link_options(${CNAME}${BINEXT} PRIVATE -Wl,--allow-multiple-definition)
	endif()

	# Optional size/speed optimizations for the monolithic binary
	if (MONOLITH_USE_GC_SECTIONS)
		# Enable section GC on the monolith and its statically linked modules
		target_compile_options(${CNAME}${BINEXT} PRIVATE -ffunction-sections -fdata-sections)
		# Linker section GC disabled as per user request to "not delete stuff"
		# target_link_options(${CNAME}${BINEXT} PRIVATE -Wl,--gc-sections)
		foreach(_mod game_${MONOLITH_ARCH_SUFFIX} cgame_${MONOLITH_ARCH_SUFFIX} ui_${MONOLITH_ARCH_SUFFIX})
			if (TARGET ${_mod})
				target_compile_options(${_mod} PRIVATE -ffunction-sections -fdata-sections)
			endif()
		endforeach()
	endif()

	if (MONOLITH_USE_LTO)
		# Enable LTO/IPO for the monolith and its statically linked modules
		set_property(TARGET ${CNAME}${BINEXT} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
		foreach(_mod game_${MONOLITH_ARCH_SUFFIX} cgame_${MONOLITH_ARCH_SUFFIX} ui_${MONOLITH_ARCH_SUFFIX})
			if (TARGET ${_mod})
				set_property(TARGET ${_mod} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
			endif()
		endforeach()
	endif()
	IF(WIN32)
		SET_TARGET_PROPERTIES(${CNAME}${BINEXT} PROPERTIES SUFFIX ".exe")
	ELSEIF(UNIX AND NOT APPLE AND NOT ANDROID)
		SET_TARGET_PROPERTIES(${CNAME}${BINEXT} PROPERTIES SUFFIX ".so")
	ENDIF()
ENDIF()

# Link Bullet Physics library after executable is created
IF(USE_BULLET)
	IF(BULLET_FOUND AND BULLET_LIBRARIES)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${BULLET_LIBRARIES})
		IF(BUILD_SERVER AND NOT ANDROID AND TARGET ${DNAME}${BINEXT})
			TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE ${BULLET_LIBRARIES})
		ENDIF()
	ENDIF()
ENDIF()
IF(USE_CIMGUI)
	set_property(TARGET ${CNAME}${BINEXT} PROPERTY LINKER_LANGUAGE CXX)
ENDIF()
IF(UNIX AND NOT ANDROID)
	TARGET_LINK_OPTIONS(${CNAME}${BINEXT} PRIVATE -rdynamic) # Export symbols for dynamically loaded libraries (Unix only)
ENDIF()

IF(USE_SDL)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${SDL2_LINK_LIBS})
ENDIF()

IF(USE_CURL)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${CURL_LIBRARIES})
ENDIF()

IF(USE_SQLITE)
	IF(SQLite3_FOUND)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${SQLite3_LIBRARIES})
	ENDIF()
ENDIF()

IF(USE_FREETYPE)
	IF(Freetype_FOUND)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${FREETYPE_LIBRARIES})
	ENDIF()
ENDIF()

IF(USE_BROTLI)
	IF(BROTLI_FOUND)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${BROTLI_LIBRARIES})
		MESSAGE(STATUS "Linking Brotli for WOFF2 support: ${BROTLI_LIBRARIES}")
	ENDIF()
ENDIF()

IF(USE_OPENSSL)
	IF(OpenSSL_FOUND)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${OPENSSL_LIBRARIES})
	ENDIF()
ENDIF()

# Lua::lua target is now created globally at the top of this file

IF(USE_WEBSOCKETS)
	IF(LibWebSockets_FOUND)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${LIBWEBSOCKETS_LIBRARIES})
	ENDIF()
ENDIF()

IF(USE_OPENAL)
	IF(OpenAL_FOUND AND OPENAL_LIBRARIES)
		IF(TARGET OpenAL::OpenAL)
			# Use modern CMake target
			TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE OpenAL::OpenAL)
		ELSE()
			# Use library variable
			TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${OPENAL_LIBRARIES})
		ENDIF()
	ENDIF()
ENDIF()

IF(USE_THEORA)
	IF(THEORA_FOUND AND THEORA_LIBRARIES)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${THEORA_LIBRARIES})
		# Also link libogg for Theora (Theora depends on it)
		find_library(OGG_LIBRARY ogg PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib /opt/local/lib)
		IF(OGG_LIBRARY)
			TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${OGG_LIBRARY})
		ELSE()
			# Try pkg-config for ogg
			find_package(PkgConfig QUIET)
			IF(PKG_CONFIG_FOUND)
				pkg_check_modules(OGG ogg QUIET)
				IF(OGG_FOUND)
					TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${OGG_LIBRARIES})
				ENDIF()
			ENDIF()
		ENDIF()
	ENDIF()
ENDIF()

IF(USE_VPX)
	IF(VPX_FOUND AND VPX_LIBRARIES)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${VPX_LIBRARIES})
	ENDIF()
ENDIF()

IF(USE_DAV1D)
	IF(DAV1D_FOUND AND DAV1D_LIBRARIES)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${DAV1D_LIBRARIES})
	ENDIF()
ENDIF()

IF(USE_ASSIMP)
	IF(ASSIMP_FOUND AND ASSIMP_LIBRARIES)
		TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE ${ASSIMP_LIBRARIES})
	ENDIF()
ENDIF()

IF(ANDROID)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE android log OpenSLES EGL GLESv1_CM GLESv2)
ENDIF()

IF(BUILD_SERVER AND NOT ANDROID)
	ADD_EXECUTABLE(${DNAME}${BINEXT} ${EXE_TYPE} ${Q3_DED_SRCS})
	TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE common_ded botlib)
	TARGET_COMPILE_DEFINITIONS(${DNAME}${BINEXT} PRIVATE DEDICATED)
TARGET_INCLUDE_DIRECTORIES(${DNAME}${BINEXT} PRIVATE src/common)

	IF(USE_SDL)
		TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE ${SDL2_LINK_LIBS})
	ENDIF()

	IF(USE_CURL)
		TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE ${CURL_LIBRARIES})
	ENDIF()

	IF(USE_SQLITE)
		IF(SQLite3_FOUND)
			TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE ${SQLite3_LIBRARIES})
		ENDIF()
	ENDIF()

	IF(USE_OPENSSL)
		IF(OpenSSL_FOUND)
			TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE ${OPENSSL_LIBRARIES})
		ENDIF()
	ENDIF()

IF(USE_LUA)
	IF(Lua_FOUND)
		TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE Lua::lua)
	ENDIF()
ENDIF()

	IF(USE_ASSIMP)
		IF(ASSIMP_FOUND AND ASSIMP_LIBRARIES)
			TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE ${ASSIMP_LIBRARIES})
		ENDIF()
	ENDIF()
ENDIF()

# Install targets so `make install` exists for Makefile generators.
# CI workflows commonly rely on `DESTDIR=... make install`.
if(NOT ANDROID)
	install(TARGETS ${CNAME}${BINEXT}
		RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
		ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	)
	if(BUILD_SERVER AND TARGET ${DNAME}${BINEXT})
		install(TARGETS ${DNAME}${BINEXT}
			RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
			LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
			ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
		)
	endif()
	# Install renderer shared libraries when building with dlopen renderer modules
	if(USE_RENDERER_DLOPEN)
		if(TARGET ${RENDERER_PREFIX}_vulkan${RENDEXT})
			install(TARGETS ${RENDERER_PREFIX}_vulkan${RENDEXT} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
		endif()
		if(TARGET ${RENDERER_PREFIX}_opengl${RENDEXT})
			install(TARGETS ${RENDERER_PREFIX}_opengl${RENDEXT} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
		endif()
		if(WIN32 AND USE_D3D12 AND TARGET ${RENDERER_PREFIX}_d3d12${RENDEXT})
			install(TARGETS ${RENDERER_PREFIX}_d3d12${RENDEXT} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
		endif()
		if(APPLE AND USE_METAL AND TARGET ${RENDERER_PREFIX}_metal${RENDEXT})
			install(TARGETS ${RENDERER_PREFIX}_metal${RENDEXT} LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
		endif()
	endif()
endif()

IF(USE_CIMGUI AND TARGET imgui_shared)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE imgui_shared)
	IF(BUILD_SERVER AND NOT ANDROID)
		TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE imgui_shared)
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
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE winmm comctl32 ws2_32)
	IF(BUILD_SERVER AND NOT ANDROID)
		TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE winmm comctl32 ws2_32)
	ENDIF()
ELSE()
	# Unix-specific libraries (math library and dynamic loading)
	TARGET_LINK_LIBRARIES(${CNAME}${BINEXT} PRIVATE m ${CMAKE_DL_LIBS})
	IF(BUILD_SERVER AND NOT ANDROID)
		TARGET_LINK_LIBRARIES(${DNAME}${BINEXT} PRIVATE m ${CMAKE_DL_LIBS})
	ENDIF()
ENDIF()

# Tracy Profiler Integration
IF(USE_TRACY)
	# Try to find Tracy headers
	FIND_PATH(TRACY_INCLUDE_DIR TracyC.h
		PATHS
		/usr/include
		/usr/local/include
		/opt/tracy/include
		${CMAKE_SOURCE_DIR}/libs/tracy
	)

	IF(TRACY_INCLUDE_DIR)
		MESSAGE(STATUS "Tracy Profiler enabled - found headers at: ${TRACY_INCLUDE_DIR}")

		# Add Tracy include directory to all relevant targets
		TARGET_INCLUDE_DIRECTORIES(common PUBLIC ${TRACY_INCLUDE_DIR})
		TARGET_INCLUDE_DIRECTORIES(client PUBLIC ${TRACY_INCLUDE_DIR})
		TARGET_COMPILE_DEFINITIONS(common PRIVATE USE_TRACY)
		TARGET_COMPILE_DEFINITIONS(client PRIVATE USE_TRACY)

		# Try to find Tracy library (optional - Tracy can be header-only)
		FIND_LIBRARY(TRACY_LIBRARY
			NAMES TracyClient TracyClientDebug
			PATHS
			/usr/lib
			/usr/lib64
			/usr/local/lib
			/opt/tracy/lib
			${CMAKE_SOURCE_DIR}/libs/tracy/build
		)

		IF(TRACY_LIBRARY)
			MESSAGE(STATUS "Found Tracy library: ${TRACY_LIBRARY}")
			TARGET_LINK_LIBRARIES(common PRIVATE ${TRACY_LIBRARY})
			TARGET_LINK_LIBRARIES(client PRIVATE ${TRACY_LIBRARY})
		ELSE()
			MESSAGE(STATUS "Tracy headers found but library not found - using header-only mode")
		ENDIF()
	ELSE()
		MESSAGE(WARNING "USE_TRACY enabled but Tracy headers not found. Install Tracy or set TRACY_INCLUDE_DIR.")
		MESSAGE(STATUS "To install Tracy: git clone https://github.com/wolfpld/tracy.git && cd tracy && make -j")
		SET(USE_TRACY OFF CACHE BOOL "" FORCE)
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
		set_property(TARGET q3ui PROPERTY POSITION_INDEPENDENT_CODE ON)
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

# Add include directories for q3ui
TARGET_INCLUDE_DIRECTORIES(q3ui PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR}/src/client)

# UI2 - Deterministic UI/Layout System (C++23)
OPTION(USE_UI2 "Enable UI2 deterministic UI/layout system" OFF)
IF(USE_UI2)
	AUX_SOURCE_DIRECTORY(src/ui2 UI2_SRCS)
ENDIF()
IF(UI2_SRCS)
	ADD_LIBRARY(ui2 OBJECT ${UI2_SRCS})
	set_property(TARGET ui2 PROPERTY POSITION_INDEPENDENT_CODE ON)
	TARGET_COMPILE_DEFINITIONS(ui2 PRIVATE UI2_ENABLED)
	# UI2 uses C++23 features
	SET_TARGET_PROPERTIES(ui2 PROPERTIES
		CXX_STANDARD 23
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
	)
	# UI2 depends on qcommon for Hunk_AllocateTempMemory, Cvar_Get, Cmd_AddCommand, etc.
	# Note: OBJECT libraries don't actually link, but this establishes the dependency
	# relationship for include directories and ensures proper linking order
	TARGET_LINK_LIBRARIES(ui2 PRIVATE common)
	MESSAGE(STATUS "UI2 module: enabled")
ELSE()
	# Create dummy source file if no UI2 sources found
	FILE(WRITE ${CMAKE_BINARY_DIR}/ui2_dummy.cpp "// Dummy file for ui2 when no sources are present\n")
	ADD_LIBRARY(ui2 OBJECT ${CMAKE_BINARY_DIR}/ui2_dummy.cpp)
	set_property(TARGET ui2 PROPERTY POSITION_INDEPENDENT_CODE ON)
	MESSAGE(STATUS "UI2 module: no sources found, using dummy library")
ENDIF()

# GtkRadiant editor/toolchain (optional)
IF(BUILD_RADIANT)
	ADD_SUBDIRECTORY(radiant)
ENDIF()

# Unit tests
OPTION(BUILD_TESTS "Build unit tests" ON)
IF(BUILD_TESTS)
	ENABLE_TESTING()
	# Minimal unit tests live under src/tests.
	# Start small and expand as tests are stabilized.
	ADD_EXECUTABLE(test_qcommon
		src/tests/test_qcommon.c
		src/common/q_shared.c
	)
	TARGET_LINK_LIBRARIES(test_qcommon PRIVATE m)
	TARGET_INCLUDE_DIRECTORIES(test_qcommon PRIVATE
		${CMAKE_SOURCE_DIR}/src
		${CMAKE_SOURCE_DIR}/src/tests
	)
	ADD_TEST(NAME test_qcommon COMMAND test_qcommon)
	SET_TARGET_PROPERTIES(test_qcommon PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
	)

	ADD_EXECUTABLE(test_qmath
		src/tests/test_qmath.c
		src/common/q_shared.c
		src/common/q_math.c
	)
	TARGET_LINK_LIBRARIES(test_qmath PRIVATE m)
	TARGET_INCLUDE_DIRECTORIES(test_qmath PRIVATE
		${CMAKE_SOURCE_DIR}/src
		${CMAKE_SOURCE_DIR}/src/tests
	)
	ADD_TEST(NAME test_qmath COMMAND test_qmath)
	SET_TARGET_PROPERTIES(test_qmath PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
	)

	ADD_EXECUTABLE(test_info
		src/tests/test_info.c
		src/common/q_shared.c
	)
	TARGET_LINK_LIBRARIES(test_info PRIVATE m)
	TARGET_INCLUDE_DIRECTORIES(test_info PRIVATE
		${CMAKE_SOURCE_DIR}/src
		${CMAKE_SOURCE_DIR}/src/tests
	)
	ADD_TEST(NAME test_info COMMAND test_info)
	SET_TARGET_PROPERTIES(test_info PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
	)

	ADD_EXECUTABLE(test_msg_secure
		src/tests/test_msg_secure.c
		src/common/q_shared.c
		src/common/msg.c
		src/common/msg_secure.c
		src/common/huffman_static.c
	)
	TARGET_LINK_LIBRARIES(test_msg_secure PRIVATE m)
	TARGET_INCLUDE_DIRECTORIES(test_msg_secure PRIVATE
		${CMAKE_SOURCE_DIR}/src
		${CMAKE_SOURCE_DIR}/src/tests
	)
	ADD_TEST(NAME test_msg_secure COMMAND test_msg_secure)
	SET_TARGET_PROPERTIES(test_msg_secure PROPERTIES
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
	)

	MESSAGE(STATUS "Tests enabled: test_qcommon")
ENDIF()
