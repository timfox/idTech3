# Server core source manifest (replaces AUX_SOURCE_DIRECTORY on src/server/).
# Open-world hooks: ServerExtensionSources.cmake strip/append.

idtech3_require_layout()

macro(idtech3_init_server_sources)
	idtech3_glob_src_rel(SERVER_SRCS
		"runtime/server/*.c"
		"src/server/*.c"
	)
endmacro()
