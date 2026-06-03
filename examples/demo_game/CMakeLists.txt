# Example demo mod: pack configs + minimal native ui.*.so/.dll into idtech3_demo.pk3 + optional helper binary.
# Included from top-level CMakeLists.txt when BUILD_EXAMPLE_DEMO_GAME=ON.

set(DEMO_MOD_DIR "${CMAKE_SOURCE_DIR}/examples/demo_game/mod")
set(DEMO_BOOTSTRAP_DIR "${CMAKE_SOURCE_DIR}/examples/demo_game/bootstrap_media")
set(DEMO_INTER_PACK_FONT "${CMAKE_SOURCE_DIR}/fonts/Inter_28pt-Regular.ttf")
set(DEMO_PK3 "${CMAKE_BINARY_DIR}/idtech3_demo.pk3")
set(DEMO_PACK_STAGING "${CMAKE_BINARY_DIR}/idtech3_demo_pack_staging")

set(DEMO_BOOTSTRAP_GFX2D "${DEMO_BOOTSTRAP_DIR}/gfx/2d/bigchars.png")
set(DEMO_BOOTSTRAP_GFX_DEMO
	"${DEMO_BOOTSTRAP_DIR}/gfx/demo/bootstrap_white.png"
	"${DEMO_BOOTSTRAP_DIR}/gfx/demo/bootstrap_console.png"
	"${DEMO_BOOTSTRAP_DIR}/gfx/demo/bootstrap_flare.png"
	"${DEMO_BOOTSTRAP_DIR}/gfx/demo/bootstrap_shadow.png"
	"${DEMO_BOOTSTRAP_DIR}/gfx/demo/demo_sprite_billboard.png"
	"${DEMO_BOOTSTRAP_DIR}/gfx/demo/demo_sprite_flipbook.png"
	"${DEMO_BOOTSTRAP_DIR}/gfx/demo/demo_sprite_imposter.png"
)
set(DEMO_BOOTSTRAP_FONTS
	"${DEMO_BOOTSTRAP_DIR}/fonts/LICENSE.txt"
	"${DEMO_BOOTSTRAP_DIR}/fonts/demo_console_sdf.png"
	"${DEMO_BOOTSTRAP_DIR}/fonts/demo_console_sdf.fnt"
)

if(NOT EXISTS "${DEMO_INTER_PACK_FONT}")
	message(FATAL_ERROR "Missing demo UI TTF: ${DEMO_INTER_PACK_FONT}\n  The idtech3_demo pack ships this file as fonts/Inter_28pt-Regular.ttf.\n  SDF console atlas: python3 examples/demo_game/tools/gen_demo_console_sdf.py \"${DEMO_INTER_PACK_FONT}\" examples/demo_game/bootstrap_media/fonts")
endif()

foreach(_demo_boot_chk IN LISTS DEMO_BOOTSTRAP_GFX2D DEMO_BOOTSTRAP_GFX_DEMO DEMO_BOOTSTRAP_FONTS)
	if(NOT EXISTS "${_demo_boot_chk}")
		message(FATAL_ERROR "Missing demo bootstrap file: ${_demo_boot_chk}\n  Regenerate PNGs: python3 examples/demo_game/tools/gen_demo_bootstrap_media.py examples/demo_game/bootstrap_media\n  SDF console font: python3 examples/demo_game/tools/gen_demo_console_sdf.py \"${DEMO_INTER_PACK_FONT}\" examples/demo_game/bootstrap_media/fonts")
	endif()
endforeach()

set(DEMO_MOD_FILES
	"${DEMO_MOD_DIR}/autoexec.cfg"
	"${DEMO_MOD_DIR}/gameinfo.txt"
	"${DEMO_MOD_DIR}/demo_features.cfg"
	"${DEMO_MOD_DIR}/demo_js.cfg"
	"${DEMO_MOD_DIR}/demo_lua.cfg"
	"${DEMO_MOD_DIR}/demo_csharp.cfg"
	"${DEMO_MOD_DIR}/demo_gameplay.cfg"
	"${DEMO_MOD_DIR}/demo_usd.cfg"
	"${DEMO_MOD_DIR}/demo_sprites.cfg"
	"${DEMO_MOD_DIR}/models/parity_geom_mesh.usda"
	"${DEMO_MOD_DIR}/models/parity_shade_preview.usda"
	"${DEMO_MOD_DIR}/readme_demo.txt"
	"${DEMO_MOD_DIR}/scripts/js/demo_hooks.js"
	"${DEMO_MOD_DIR}/scripts/js/README.txt"
	"${DEMO_MOD_DIR}/scripts/lua/demo_hooks.lua"
	"${DEMO_MOD_DIR}/scripts/csharp/IdTech3.Engine.cs"
	"${DEMO_MOD_DIR}/scripts/csharp/demo_hooks.cs"
	"${DEMO_MOD_DIR}/scripts/demo_bootstrap.shader"
	"${DEMO_MOD_DIR}/scripts/demo_sprites.shader"
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
	# Primary probe name from VM_BuildNativeModuleCandidates (ui.so first is skipped if missing).
	# Also ship dotted form so FS_LoadLibrary finds the same bytes under vm/ui.<arch>.so (matches docs/tests).
	set(DEMO_UI_IN_ZIP "vm/ui${DEMO_UI_ARCH}${CMAKE_SHARED_LIBRARY_SUFFIX}")
	set(DEMO_UI_DOT_ZIP "vm/ui.${DEMO_UI_ARCH}${CMAKE_SHARED_LIBRARY_SUFFIX}")
endif()

# Staging tree + zip so vm/ui*.so ships inside idtech3_demo.pk3 (pk3 is a zip).
add_custom_command(
	OUTPUT "${DEMO_PK3}"
	COMMAND ${CMAKE_COMMAND} -E rm -rf "${DEMO_PACK_STAGING}"
	COMMAND ${CMAKE_COMMAND} -E make_directory "${DEMO_PACK_STAGING}/scripts/js"
	COMMAND ${CMAKE_COMMAND} -E make_directory "${DEMO_PACK_STAGING}/scripts/lua"
	COMMAND ${CMAKE_COMMAND} -E make_directory "${DEMO_PACK_STAGING}/scripts/csharp"
	COMMAND ${CMAKE_COMMAND} -E make_directory "${DEMO_PACK_STAGING}/vm"
	COMMAND ${CMAKE_COMMAND} -E make_directory "${DEMO_PACK_STAGING}/gfx/2d"
	COMMAND ${CMAKE_COMMAND} -E make_directory "${DEMO_PACK_STAGING}/gfx/demo"
	COMMAND ${CMAKE_COMMAND} -E make_directory "${DEMO_PACK_STAGING}/fonts"
	COMMAND ${CMAKE_COMMAND} -E make_directory "${DEMO_PACK_STAGING}/models"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/autoexec.cfg" "${DEMO_PACK_STAGING}/autoexec.cfg"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/gameinfo.txt" "${DEMO_PACK_STAGING}/gameinfo.txt"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/demo_features.cfg" "${DEMO_PACK_STAGING}/demo_features.cfg"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/demo_js.cfg" "${DEMO_PACK_STAGING}/demo_js.cfg"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/demo_lua.cfg" "${DEMO_PACK_STAGING}/demo_lua.cfg"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/demo_csharp.cfg" "${DEMO_PACK_STAGING}/demo_csharp.cfg"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/demo_gameplay.cfg" "${DEMO_PACK_STAGING}/demo_gameplay.cfg"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/demo_usd.cfg" "${DEMO_PACK_STAGING}/demo_usd.cfg"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/demo_sprites.cfg" "${DEMO_PACK_STAGING}/demo_sprites.cfg"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/readme_demo.txt" "${DEMO_PACK_STAGING}/readme_demo.txt"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/scripts/js/demo_hooks.js" "${DEMO_PACK_STAGING}/scripts/js/demo_hooks.js"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/scripts/js/README.txt" "${DEMO_PACK_STAGING}/scripts/js/README.txt"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/scripts/lua/demo_hooks.lua" "${DEMO_PACK_STAGING}/scripts/lua/demo_hooks.lua"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/scripts/csharp/IdTech3.Engine.cs" "${DEMO_PACK_STAGING}/scripts/csharp/IdTech3.Engine.cs"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/scripts/csharp/demo_hooks.cs" "${DEMO_PACK_STAGING}/scripts/csharp/demo_hooks.cs"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/scripts/demo_bootstrap.shader" "${DEMO_PACK_STAGING}/scripts/demo_bootstrap.shader"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/scripts/demo_sprites.shader" "${DEMO_PACK_STAGING}/scripts/demo_sprites.shader"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/models/parity_geom_mesh.usda" "${DEMO_PACK_STAGING}/models/parity_geom_mesh.usda"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_MOD_DIR}/models/parity_shade_preview.usda" "${DEMO_PACK_STAGING}/models/parity_shade_preview.usda"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_BOOTSTRAP_GFX2D}" "${DEMO_PACK_STAGING}/gfx/2d/bigchars.png"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_BOOTSTRAP_DIR}/gfx/demo/bootstrap_white.png" "${DEMO_PACK_STAGING}/gfx/demo/bootstrap_white.png"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_BOOTSTRAP_DIR}/gfx/demo/bootstrap_console.png" "${DEMO_PACK_STAGING}/gfx/demo/bootstrap_console.png"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_BOOTSTRAP_DIR}/gfx/demo/bootstrap_flare.png" "${DEMO_PACK_STAGING}/gfx/demo/bootstrap_flare.png"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_BOOTSTRAP_DIR}/gfx/demo/bootstrap_shadow.png" "${DEMO_PACK_STAGING}/gfx/demo/bootstrap_shadow.png"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_BOOTSTRAP_DIR}/gfx/demo/demo_sprite_billboard.png" "${DEMO_PACK_STAGING}/gfx/demo/demo_sprite_billboard.png"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_BOOTSTRAP_DIR}/gfx/demo/demo_sprite_flipbook.png" "${DEMO_PACK_STAGING}/gfx/demo/demo_sprite_flipbook.png"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_BOOTSTRAP_DIR}/gfx/demo/demo_sprite_imposter.png" "${DEMO_PACK_STAGING}/gfx/demo/demo_sprite_imposter.png"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_INTER_PACK_FONT}" "${DEMO_PACK_STAGING}/fonts/Inter_28pt-Regular.ttf"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_BOOTSTRAP_DIR}/fonts/LICENSE.txt" "${DEMO_PACK_STAGING}/fonts/LICENSE.txt"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_BOOTSTRAP_DIR}/fonts/demo_console_sdf.png" "${DEMO_PACK_STAGING}/fonts/demo_console_sdf.png"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"${DEMO_BOOTSTRAP_DIR}/fonts/demo_console_sdf.fnt" "${DEMO_PACK_STAGING}/fonts/demo_console_sdf.fnt"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"$<TARGET_FILE:demo_ui_skeleton>" "${DEMO_PACK_STAGING}/${DEMO_UI_IN_ZIP}"
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
		"$<TARGET_FILE:demo_ui_skeleton>" "${DEMO_PACK_STAGING}/${DEMO_UI_DOT_ZIP}"
	COMMAND ${CMAKE_COMMAND} -E chdir "${DEMO_PACK_STAGING}"
		${CMAKE_COMMAND} -E tar cf "${DEMO_PK3}" --format=zip
		autoexec.cfg gameinfo.txt demo_features.cfg demo_js.cfg demo_lua.cfg demo_csharp.cfg demo_gameplay.cfg demo_usd.cfg demo_sprites.cfg readme_demo.txt
		scripts/js/demo_hooks.js scripts/js/README.txt
		scripts/lua/demo_hooks.lua
		scripts/csharp/IdTech3.Engine.cs scripts/csharp/demo_hooks.cs
		scripts/demo_bootstrap.shader scripts/demo_sprites.shader
		gfx/2d/bigchars.png
		gfx/demo/bootstrap_white.png
		gfx/demo/bootstrap_console.png
		gfx/demo/bootstrap_flare.png
		gfx/demo/bootstrap_shadow.png
		gfx/demo/demo_sprite_billboard.png
		gfx/demo/demo_sprite_flipbook.png
		gfx/demo/demo_sprite_imposter.png
		fonts/Inter_28pt-Regular.ttf
		fonts/LICENSE.txt
		fonts/demo_console_sdf.png
		fonts/demo_console_sdf.fnt
		"${DEMO_UI_IN_ZIP}"
		"${DEMO_UI_DOT_ZIP}"
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
