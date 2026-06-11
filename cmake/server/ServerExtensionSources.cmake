# Server extension sources (open-world server hooks).

set(_IDTECH3_SERVER_EXT_ALL
	src/server/sv_openworld.c
)

macro(idtech3_strip_server_extension_sources)
	list(REMOVE_ITEM SERVER_SRCS ${_IDTECH3_SERVER_EXT_ALL})
endmacro()

macro(idtech3_append_server_extension_sources)
	if(USE_OPEN_WORLD)
		list(APPEND SERVER_SRCS src/server/sv_openworld.c)
	endif()
endmacro()
