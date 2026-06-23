# Client extension sources (generative ML + open-world client hooks).

idtech3_require_layout()

idtech3_legacy_src(_flux "src/extensions/generative/cl_flux.c")
idtech3_legacy_src(_trellis "src/extensions/generative/cl_trellis.c")
idtech3_legacy_src(_ggan "src/extensions/generative/cl_genetic_gan.c")
idtech3_legacy_src(_mlw "src/extensions/generative/cl_ml_worker.c")
idtech3_legacy_src(_gen "src/extensions/generative/cl_generative.c")
idtech3_legacy_src(_dist "src/client/world/cl_district.cpp")
idtech3_legacy_src(_ow "src/client/world/cl_openworld.cpp")
idtech3_legacy_src(_proc "src/client/world/cl_proc.cpp")
idtech3_legacy_src(_usd "src/client/cl_usd.cpp")
idtech3_legacy_src(_emu_proc "src/extensions/emulator/emulator_process.c")
idtech3_legacy_src(_emu_frame "src/extensions/emulator/emulator_frame.c")
idtech3_legacy_src(_emu_console "src/extensions/emulator/emulator_console.c")
idtech3_legacy_src(_emu_input "src/extensions/emulator/emulator_input.c")

set(_IDTECH3_CLIENT_EXT_ALL
	${_flux} ${_trellis} ${_ggan} ${_mlw} ${_gen}
	${_dist} ${_ow} ${_proc} ${_usd}
	${_emu_proc} ${_emu_frame} ${_emu_console} ${_emu_input}
)

macro(idtech3_strip_client_extension_sources)
	list(REMOVE_ITEM CLIENT_SRCS ${_IDTECH3_CLIENT_EXT_ALL})
endmacro()

macro(idtech3_append_client_extension_sources)
	if(USE_FLUX)
		list(APPEND CLIENT_SRCS ${_flux})
	endif()
	if(USE_TRELLIS)
		list(APPEND CLIENT_SRCS ${_trellis})
	endif()
	if(USE_GENETIC_GAN)
		list(APPEND CLIENT_SRCS ${_ggan} ${_mlw})
	endif()
	if(USE_FLUX OR USE_TRELLIS OR USE_GENETIC_GAN)
		list(APPEND CLIENT_SRCS ${_gen})
	endif()
	if(USE_OPEN_WORLD)
		list(APPEND CLIENT_SRCS ${_dist} ${_ow} ${_proc})
	endif()
	if(USE_FREEUSD)
		list(APPEND CLIENT_SRCS ${_usd})
	endif()
	if(USE_IDTECH3_EMULATOR)
		list(APPEND CLIENT_SRCS ${_emu_proc} ${_emu_frame} ${_emu_console} ${_emu_input})
	endif()
endmacro()
