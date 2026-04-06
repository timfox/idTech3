# Example demo mod: pack configs into idtech3_demo.pk3 + optional helper binary.
# Included from top-level CMakeLists.txt when BUILD_EXAMPLE_DEMO_GAME=ON.

set(DEMO_MOD_DIR "${CMAKE_SOURCE_DIR}/examples/demo_game/mod")
set(DEMO_PK3 "${CMAKE_BINARY_DIR}/idtech3_demo.pk3")

set(DEMO_MOD_FILES
	"${DEMO_MOD_DIR}/autoexec.cfg"
	"${DEMO_MOD_DIR}/demo_features.cfg"
	"${DEMO_MOD_DIR}/demo_js.cfg"
	"${DEMO_MOD_DIR}/demo_gameplay.cfg"
	"${DEMO_MOD_DIR}/readme_demo.txt"
	"${DEMO_MOD_DIR}/scripts/js/demo_hooks.js"
	"${DEMO_MOD_DIR}/scripts/js/README.txt"
)

# cmake -E tar requires running from the directory containing the files so paths inside the zip are flat.
add_custom_command(
	OUTPUT "${DEMO_PK3}"
	COMMAND ${CMAKE_COMMAND} -E chdir "${DEMO_MOD_DIR}"
		${CMAKE_COMMAND} -E tar cf "${DEMO_PK3}" --format=zip
		autoexec.cfg demo_features.cfg demo_js.cfg demo_gameplay.cfg readme_demo.txt
		scripts/js/demo_hooks.js scripts/js/README.txt
	DEPENDS ${DEMO_MOD_FILES}
	COMMENT "Packaging idtech3_demo.pk3"
	VERBATIM
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
