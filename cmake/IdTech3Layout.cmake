# 2026 repository layout path aliases (Phase 5c).
# Canonical sources live under engine/, runtime/, modules/, extensions/, renderers/, third_party/.
# src/* forwarding shims remain for one release (DEPRECATION_POLICY.md).

function(idtech3_layout_dir out_var canonical_rel legacy_shim_rel)
	if(EXISTS "${CMAKE_SOURCE_DIR}/${canonical_rel}" AND NOT IS_SYMLINK "${CMAKE_SOURCE_DIR}/${canonical_rel}")
		set(${out_var} "${CMAKE_SOURCE_DIR}/${canonical_rel}" PARENT_SCOPE)
	elseif(EXISTS "${CMAKE_SOURCE_DIR}/${legacy_shim_rel}")
		get_filename_component(_resolved "${CMAKE_SOURCE_DIR}/${legacy_shim_rel}" REALPATH)
		set(${out_var} "${_resolved}" PARENT_SCOPE)
	else()
		set(${out_var} "${CMAKE_SOURCE_DIR}/${canonical_rel}" PARENT_SCOPE)
	endif()
endfunction()

idtech3_layout_dir(IDTECH3_DIR_ENGINE_CORE "engine/core" "src/qcommon")
idtech3_layout_dir(IDTECH3_DIR_ENGINE_PLATFORM "engine/platform" "src/platform")
idtech3_layout_dir(IDTECH3_DIR_RUNTIME_CLIENT "runtime/client" "src/client")
idtech3_layout_dir(IDTECH3_DIR_RUNTIME_SERVER "runtime/server" "src/server")
idtech3_layout_dir(IDTECH3_DIR_RUNTIME_GAME "runtime/game" "src/game")
idtech3_layout_dir(IDTECH3_DIR_MODULE_WORLD "modules/world" "src/world")
idtech3_layout_dir(IDTECH3_DIR_MODULE_NAVIGATION "modules/navigation" "src/navigation")
idtech3_layout_dir(IDTECH3_DIR_MODULE_PHYSICS "modules/physics" "src/physics")
idtech3_layout_dir(IDTECH3_DIR_MODULE_AUDIO "modules/audio" "src/audio")
idtech3_layout_dir(IDTECH3_DIR_MODULE_BOTLIB "modules/botlib" "src/botlib")
idtech3_layout_dir(IDTECH3_DIR_MODULE_RTS "modules/rts" "src/rts")
idtech3_layout_dir(IDTECH3_DIR_MODULE_MATH "modules/math" "src/math")
idtech3_layout_dir(IDTECH3_DIR_RUNTIME_CGAME "runtime/cgame" "src/cgame")
idtech3_layout_dir(IDTECH3_DIR_RUNTIME_UI "runtime/ui" "src/ui")
idtech3_layout_dir(IDTECH3_DIR_ENGINE_ASM "engine/asm" "src/asm")
idtech3_layout_dir(IDTECH3_DIR_EXTENSIONS "extensions" "src/extensions")
idtech3_layout_dir(IDTECH3_DIR_RENDERERS "renderers" "src/renderers")

if(EXISTS "${CMAKE_SOURCE_DIR}/third_party" AND NOT IS_SYMLINK "${CMAKE_SOURCE_DIR}/third_party")
	set(IDTECH3_DIR_THIRD_PARTY "${CMAKE_SOURCE_DIR}/third_party")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/src/external")
	get_filename_component(IDTECH3_DIR_THIRD_PARTY "${CMAKE_SOURCE_DIR}/src/external" REALPATH)
else()
	set(IDTECH3_DIR_THIRD_PARTY "${CMAKE_SOURCE_DIR}/third_party")
endif()

message(STATUS "Layout 2026: engine/core=${IDTECH3_DIR_ENGINE_CORE} runtime/client=${IDTECH3_DIR_RUNTIME_CLIENT}")

# Phase 5c: cross-domain #include symlinks (runtime/qcommon, modules/client, …).
if(EXISTS "${CMAKE_SOURCE_DIR}/engine/core/common.c")
	set(_IDTECH3_LAYOUT_FWD "${CMAKE_SOURCE_DIR}/scripts/layout_forwarding_symlinks.sh")
	if(EXISTS "${_IDTECH3_LAYOUT_FWD}")
		execute_process(
			COMMAND "${_IDTECH3_LAYOUT_FWD}"
			WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
			RESULT_VARIABLE _idtech3_layout_fwd_rc
			OUTPUT_VARIABLE _idtech3_layout_fwd_out
			ERROR_VARIABLE _idtech3_layout_fwd_err
		)
		if(NOT _idtech3_layout_fwd_rc EQUAL 0)
			message(WARNING "layout_forwarding_symlinks.sh failed (${_idtech3_layout_fwd_rc}): ${_idtech3_layout_fwd_err}")
		else()
			message(STATUS "Layout 2026: cross-domain forwarding symlinks ready")
		endif()
	endif()
endif()

# Phase 5b: manifests include this file first, then use IDTECH3_DIR_* instead of bare src/* paths.
macro(idtech3_require_layout)
	if(NOT IDTECH3_DIR_ENGINE_CORE OR NOT IDTECH3_DIR_RUNTIME_CLIENT)
		message(FATAL_ERROR "IdTech3Layout.cmake must be included before extension manifests")
	endif()
endmacro()

# Legacy-relative path for strip/append (Phase 5c: still src/* via forwarding shims).
function(idtech3_legacy_src out_var relpath)
	set(${out_var} "${relpath}" PARENT_SCOPE)
endfunction()

# Glob sources; accepts src/* shim paths or canonical layout paths.
# Dedupes by realpath so physical dirs + forwarding shims do not double-compile.
macro(idtech3_glob_src_rel out_var)
	set(_idtech3_glob_abs "")
	foreach(_pat ${ARGN})
		file(GLOB _hits "${CMAKE_SOURCE_DIR}/${_pat}")
		list(APPEND _idtech3_glob_abs ${_hits})
	endforeach()
	set(_idtech3_glob_rel "")
	set(_idtech3_glob_seen "")
	foreach(_abs ${_idtech3_glob_abs})
		get_filename_component(_real "${_abs}" REALPATH)
		list(FIND _idtech3_glob_seen "${_real}" _dup_idx)
		if(_dup_idx EQUAL -1)
			list(APPEND _idtech3_glob_seen "${_real}")
			file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}" "${_abs}")
			list(APPEND _idtech3_glob_rel "${_rel}")
		endif()
	endforeach()
	set(${out_var} ${_idtech3_glob_rel})
endmacro()
