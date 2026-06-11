# Client extension sources (generative ML + open-world client hooks).
# Strip from AUX_SOURCE_DIRECTORY picks, then re-append when profile flags allow.

set(_IDTECH3_CLIENT_EXT_ALL
	src/extensions/generative/cl_flux.c
	src/extensions/generative/cl_trellis.c
	src/extensions/generative/cl_genetic_gan.c
	src/extensions/generative/cl_ml_worker.c
	src/extensions/generative/cl_generative.c
	src/client/cl_district.cpp
	src/client/cl_openworld.cpp
	src/client/cl_proc.cpp
	src/client/cl_usd.cpp
)

macro(idtech3_strip_client_extension_sources)
	list(REMOVE_ITEM CLIENT_SRCS ${_IDTECH3_CLIENT_EXT_ALL})
endmacro()

macro(idtech3_append_client_extension_sources)
	if(USE_FLUX)
		list(APPEND CLIENT_SRCS src/extensions/generative/cl_flux.c)
	endif()
	if(USE_TRELLIS)
		list(APPEND CLIENT_SRCS src/extensions/generative/cl_trellis.c)
	endif()
	if(USE_GENETIC_GAN)
		list(APPEND CLIENT_SRCS
			src/extensions/generative/cl_genetic_gan.c
			src/extensions/generative/cl_ml_worker.c
		)
	endif()
	if(USE_FLUX OR USE_TRELLIS OR USE_GENETIC_GAN)
		list(APPEND CLIENT_SRCS src/extensions/generative/cl_generative.c)
	endif()
	if(USE_OPEN_WORLD)
		list(APPEND CLIENT_SRCS
			src/client/cl_district.cpp
			src/client/cl_openworld.cpp
			src/client/cl_proc.cpp
		)
	endif()
	if(USE_FREEUSD)
		list(APPEND CLIENT_SRCS src/client/cl_usd.cpp)
	endif()
endmacro()
