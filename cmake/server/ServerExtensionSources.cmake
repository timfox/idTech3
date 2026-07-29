# Server extension sources (open-world server hooks).

idtech3_require_layout()

file(RELATIVE_PATH _IDTECH3_SV_OPENWORLD "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_RUNTIME_SERVER}/world/sv_openworld.c")
file(RELATIVE_PATH _IDTECH3_SV_WORLD_CONFIG "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_RUNTIME_SERVER}/world/sv_world_config.c")

set(_IDTECH3_SERVER_EXT_ALL ${_IDTECH3_SV_OPENWORLD} ${_IDTECH3_SV_WORLD_CONFIG})

macro(idtech3_strip_server_extension_sources)
	list(REMOVE_ITEM SERVER_SRCS ${_IDTECH3_SERVER_EXT_ALL})
endmacro()

macro(idtech3_append_server_extension_sources)
	if(USE_OPEN_WORLD)
		list(APPEND SERVER_SRCS ${_IDTECH3_SV_OPENWORLD} ${_IDTECH3_SV_WORLD_CONFIG})
	endif()
endmacro()
