# Bot AI library manifest (replaces AUX_SOURCE_DIRECTORY on src/botlib/).

idtech3_require_layout()

macro(idtech3_init_botlib_sources)
	idtech3_glob_src_rel(BOTLIB_SRCS
		"modules/botlib/*.c"
		"src/botlib/*.c"
	)
endmacro()
