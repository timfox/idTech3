# MP3 decoder sources (replaces AUX_SOURCE_DIRECTORY on src/audio/mp3/).

idtech3_require_layout()

macro(idtech3_append_mp3_client_sources)
	if(USE_MP3)
		idtech3_glob_src_rel(_idtech3_mp3_srcs "src/audio/mp3/*.c")
		list(APPEND CLIENT_SRCS ${_idtech3_mp3_srcs})
	endif()
endmacro()
