# Client extension sources (generative ML + open-world client hooks).
# Paths are CMAKE_SOURCE_DIR-relative canonical layout (Phase 5e prep).

idtech3_require_layout()

file(RELATIVE_PATH _flux "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_EXTENSIONS}/generative/cl_flux.c")
file(RELATIVE_PATH _trellis "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_EXTENSIONS}/generative/cl_trellis.c")
file(RELATIVE_PATH _ggan "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_EXTENSIONS}/generative/cl_genetic_gan.c")
file(RELATIVE_PATH _mlw "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_EXTENSIONS}/generative/cl_ml_worker.c")
file(RELATIVE_PATH _gen "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_EXTENSIONS}/generative/cl_generative.c")
file(RELATIVE_PATH _dist "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_RUNTIME_CLIENT}/world/cl_district.cpp")
file(RELATIVE_PATH _ow "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_RUNTIME_CLIENT}/world/cl_openworld.cpp")
file(RELATIVE_PATH _proc "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_RUNTIME_CLIENT}/world/cl_proc.cpp")
file(RELATIVE_PATH _usd "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_RUNTIME_CLIENT}/cl_usd.cpp")
file(RELATIVE_PATH _emu_proc "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_EXTENSIONS}/emulator/emulator_process.c")
file(RELATIVE_PATH _emu_frame "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_EXTENSIONS}/emulator/emulator_frame.c")
file(RELATIVE_PATH _emu_console "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_EXTENSIONS}/emulator/emulator_console.c")
file(RELATIVE_PATH _emu_input "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_EXTENSIONS}/emulator/emulator_input.c")

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
	# Always compile cl_usd.cpp — CL_USD_Init is called from cl_main; USE_FREEUSD
	# only enables real FreeUSD commands inside the TU.
	list(APPEND CLIENT_SRCS ${_usd})
	if(USE_IDTECH3_EMULATOR)
		list(APPEND CLIENT_SRCS ${_emu_proc} ${_emu_frame} ${_emu_console} ${_emu_input})
	endif()
endmacro()
