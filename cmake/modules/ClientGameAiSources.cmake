# Game AI middleware (Director, GOAP, Horde, BT, …) — chocolate layer, OFF in core profile.

idtech3_require_layout()

idtech3_legacy_src(_g_dir "src/game/g_director.c")
idtech3_legacy_src(_g_resp "src/game/g_response.c")
idtech3_legacy_src(_g_cho "src/game/g_choreography.c")
idtech3_legacy_src(_g_dis "src/game/g_dismember.c")
idtech3_legacy_src(_g_fac "src/game/g_facial.c")
idtech3_legacy_src(_g_hor "src/game/g_horde.c")
idtech3_legacy_src(_g_goap "src/game/g_goap.c")
idtech3_legacy_src(_g_aim "src/game/g_aiml.c")
idtech3_legacy_src(_g_eda "src/game/g_eda.c")
idtech3_legacy_src(_g_bt "src/game/g_bt.c")
idtech3_legacy_src(_g_stub "src/game/game_middleware_stubs.c")

set(IDTECH3_GAME_AI_MIDDLEWARE_SRCS
	${_g_dir} ${_g_resp} ${_g_cho} ${_g_dis} ${_g_fac} ${_g_hor}
	${_g_goap} ${_g_aim} ${_g_eda} ${_g_bt}
)

macro(idtech3_strip_game_ai_middleware_sources)
	list(REMOVE_ITEM CLIENT_SRCS ${IDTECH3_GAME_AI_MIDDLEWARE_SRCS})
endmacro()

macro(idtech3_append_game_ai_middleware_sources)
	if(USE_GAME_AI_MIDDLEWARE)
		list(APPEND CLIENT_SRCS ${IDTECH3_GAME_AI_MIDDLEWARE_SRCS})
	else()
		list(APPEND CLIENT_SRCS ${_g_stub})
	endif()
endmacro()

macro(idtech3_apply_game_ai_middleware_defs target)
	if(USE_GAME_AI_MIDDLEWARE)
		target_compile_definitions(${target} PRIVATE USE_GAME_AI_MIDDLEWARE=1)
	endif()
endmacro()
