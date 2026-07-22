# UberToolsSources.cmake — clean-room Babble / TIKI gates (GPL-2.0; no Miles).

idtech3_require_layout()

macro(idtech3_append_ubertools_client_sources)
	if(USE_BABBLE)
		list(APPEND CLIENT_SRCS
			modules/dialogue/babble_parse.c
			modules/dialogue/babble_runtime.c
		)
	endif()
endmacro()

macro(idtech3_append_ubertools_renderer_sources)
	if(USE_TIKI)
		list(APPEND RENDERER_COMMON_SRCS
			renderers/common/tr_model_tiki.c
		)
	endif()
endmacro()

macro(idtech3_apply_ubertools_defs target)
	if(USE_UBERTOOLS_COMPAT)
		target_compile_definitions(${target} PRIVATE USE_UBERTOOLS_COMPAT=1)
	endif()
	if(USE_BABBLE)
		target_compile_definitions(${target} PRIVATE USE_BABBLE=1)
	endif()
	if(USE_TIKI)
		target_compile_definitions(${target} PRIVATE USE_TIKI=1)
	endif()
endmacro()
