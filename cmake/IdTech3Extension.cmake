# IdTech3Extension.cmake — register opt-in extension sources behind CMake gates.
# Used by qcommon/client/server/renderer module manifests (2026 layercake layout).

include_guard(GLOBAL)

# idtech3_gate_append(LIST_VAR GATE src/foo.c src/bar.c)
# Appends sources to LIST_VAR only when GATE is true.
macro(idtech3_gate_append list_var gate)
	if(${gate})
		list(APPEND ${list_var} ${ARGN})
	endif()
endmacro()

# idtech3_register_extension(NAME generative GATE USE_FLUX DEST CLIENT_SRCS
#   SOURCES src/extensions/generative/cl_flux.c ...)
macro(idtech3_register_extension)
	set(_opts "")
	set(_one NAME GATE DEST)
	set(_multi SOURCES)
	cmake_parse_arguments(_ext "${_opts}" "${_one}" "${_multi}" ${ARGN})
	if(NOT _ext_NAME OR NOT _ext_GATE OR NOT _ext_DEST)
		message(FATAL_ERROR "idtech3_register_extension requires NAME, GATE, DEST")
	endif()
	if(${_ext_GATE})
		list(APPEND ${_ext_DEST} ${_ext_SOURCES})
		message(STATUS "Extension ${_ext_NAME}: ON (${_ext_GATE}) — ${CMAKE_MATCH_COUNT} sources")
	endif()
endmacro()

# Strip then re-append pattern for AUX_SOURCE_DIRECTORY hygiene.
macro(idtech3_strip_sources list_var)
	list(REMOVE_ITEM ${list_var} ${ARGN})
endmacro()
