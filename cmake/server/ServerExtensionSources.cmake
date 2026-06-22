# Server extension sources (open-world server hooks).

idtech3_require_layout()

file(RELATIVE_PATH _IDTECH3_SV_OPENWORLD "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_RUNTIME_SERVER}/sv_openworld.c")

set(_IDTECH3_SERVER_EXT_ALL ${_IDTECH3_SV_OPENWORLD})

macro(idtech3_strip_server_extension_sources)
	list(REMOVE_ITEM SERVER_SRCS ${_IDTECH3_SERVER_EXT_ALL})
endmacro()

macro(idtech3_append_server_extension_sources)
	if(USE_OPEN_WORLD)
		list(APPEND SERVER_SRCS ${_IDTECH3_SV_OPENWORLD})
	endif()
endmacro()
