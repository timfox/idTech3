# Example demo mod: pack configs + minimal native ui.*.so/.dll into idtech3_demo.pk3 + optional helper binary.
# Included from top-level CMakeLists.txt when BUILD_EXAMPLE_DEMO_GAME=ON.

set(DEMO_MOD_DIR "${CMAKE_SOURCE_DIR}/examples/demo_game/mod")
set(DEMO_PK3 "${CMAKE_BINARY_DIR}/idtech3_demo.pk3")
set(DEMO_PACK_STAGING "${CMAKE_BINARY_DIR}/idtech3_demo_pack_staging")

set(DEMO_MOD_FILES
	"${DEMO_MOD_DIR}/autoexec.cfg"
	"${DEMO_MOD_DIR}/demo_features.cfg"
	"${DEMO_MOD_DIR}/demo_js.cfg"
	"${DEMO_MOD_DIR}/demo_gameplay.cfg"
	"${DEMO_MOD_DIR}/readme_demo.txt"
	"${DEMO_MOD_DIR}/scripts/js/demo_hooks.js"
	"${DEMO_MOD_DIR}/scripts/js/README.txt"
)

# Minimal native UI for skeleton playfields (loads from pk3 vm/ before ui.qvm).
if(NOT ANDROID)
	if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64)$")
		set(DEMO_UI_ARCH "x86_64")
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ARM64|aarch64|arm64)$")
		set(DEMO_UI_ARCH "aarch64")
	elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i386|i686|x86)$")
		set(DEMO_UI_ARCH "i386")
	else()
		set(DEMO_UI_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
	endif()
	add_library(demo_ui_skeleton SHARED
		"${CMAKE_SOURCE_DIR}/examples/demo_game/native/ui_skeleton_stub.c")
	set_target_properties(demo_ui_skeleton PROPERTIES
		OUTPUT_NAME "ui${DEMO_UI_ARCH}"
		PREFIX ""
		LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/demo_ui_skeleton_out"
		RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/demo_ui_skeleton_out"
	)
	target_include_directories(demo_ui_skeleton PRIVATE "${CMAKE_SOURCE_DIR}/src")
	if(NOT MSVC)
		target_compile_options(demo_ui_skeleton PRIVATE -Wall -Wextra -pedantic)
	endif()
	set(DEMO_UI_IN_ZIP "vm/ui${DEMO_UI_ARCH}${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif()

# Staging tree + zip so vm/ui*.so ships inside idtech3_demo.pk3 (pk3 is a zip).
add_custom_command(
	OUTPUT "${DEMO_PK3}"
	COMMAND ${CMAKE_COMMAND} -E rm -rf "${DEMO_PACK_STAGING}"
	COMMAND ${CMAKE_COMMAND} -E make_directory "${DEMO_PACK_STAGING}/scripts/js"
	COMMAND ${CMAKE_COMMAND} -E make_directory "${DEMO_PACK_STAGING}/vm"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/autoexec.cfg" "${DEMO_PACK_STAGING}/autoexec.cfg"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/demo_features.cfg" "${DEMO_PACK_STAGING}/demo_features.cfg"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/demo_js.cfg" "${DEMO_PACK_STAGING}/demo_js.cfg"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/demo_gameplay.cfg" "${DEMO_PACK_STAGING}/demo_gameplay.cfg"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/readme_demo.txt" "${DEMO_PACK_STAGING}/readme_demo.txt"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/scripts/js/demo_hooks.js" "${DEMO_PACK_STAGING}/scripts/js/demo_hooks.js"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/scripts/js/README.txt" "${DEMO_PACK_STAGING}/scripts/js/README.txt"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"$<TARGET_FILE:demo_ui_skeleton>" "${DEMO_PACK_STAGING}/${DEMO_UI_IN_ZIP}"
	COMMAND ${CMAKE_COMMAND} -E chdir "${DEMO_PACK_STAGING}"
		${CMAKE_COMMAND} -E tar cf "${DEMO_PK3}" --format=zip
		autoexec.cfg demo_features.cfg demo_js.cfg demo_gameplay.cfg readme_demo.txt
		scripts/js/demo_hooks.js scripts/js/README.txt
		"${DEMO_UI_IN_ZIP}"
	DEPENDS ${DEMO_MOD_FILES} demo_ui_skeleton
	COMMENT "Packaging idtech3_demo.pk3 (configs + native UI stub)"
	VERBATIM
	COMMAND_EXPAND_LISTS
)

# Not ALL: avoids rebuilding the zip on every default engine build; use --target demo_game_pk3
add_custom_target(demo_game_pk3 DEPENDS "${DEMO_PK3}")

add_executable(idtech3_demo_helper
	"${CMAKE_SOURCE_DIR}/examples/demo_game/src/demo_helper.c")
set_target_properties(idtech3_demo_helper PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples"
)
if(NOT MSVC)
	target_compile_options(idtech3_demo_helper PRIVATE -Wall -Wextra -pedantic)
endif()

message(STATUS "Example demo game: idtech3_demo.pk3 -> ${DEMO_PK3} (target demo_game_pk3)")
