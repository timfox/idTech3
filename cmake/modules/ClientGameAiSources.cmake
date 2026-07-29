# Game AI middleware (Director, GOAP, Horde, BT, …) — chocolate layer, OFF in core profile.
# Paths are CMAKE_SOURCE_DIR-relative canonical layout (Phase 5e prep).

idtech3_require_layout()

file(RELATIVE_PATH _g_dir "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_RUNTIME_GAME}/middleware/g_director.c")
file(RELATIVE_PATH _g_resp "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_RUNTIME_GAME}/middleware/g_response.c")
file(RELATIVE_PATH _g_cho "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_RUNTIME_GAME}/middleware/g_choreography.c")
file(RELATIVE_PATH _g_dis "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_RUNTIME_GAME}/middleware/g_dismember.c")
file(RELATIVE_PATH _g_fac "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_RUNTIME_GAME}/middleware/g_facial.c")
file(RELATIVE_PATH _g_anim "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_RUNTIME_GAME}/middleware/g_animgraph.c")
file(RELATIVE_PATH _g_hor "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_RUNTIME_GAME}/middleware/g_horde.c")
file(RELATIVE_PATH _g_goap "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_RUNTIME_GAME}/middleware/g_goap.c")
file(RELATIVE_PATH _g_aim "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_RUNTIME_GAME}/middleware/g_aiml.c")
file(RELATIVE_PATH _g_eda "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_RUNTIME_GAME}/middleware/g_eda.c")
file(RELATIVE_PATH _g_bt "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_RUNTIME_GAME}/middleware/g_bt.c")
file(RELATIVE_PATH _g_stub "${CMAKE_SOURCE_DIR}" "${IDTECH3_DIR_RUNTIME_GAME}/middleware/game_middleware_stubs.c")

set(IDTECH3_GAME_AI_MIDDLEWARE_SRCS
	${_g_dir} ${_g_resp} ${_g_cho} ${_g_dis} ${_g_anim} ${_g_hor}
	${_g_goap} ${_g_aim} ${_g_eda} ${_g_bt}
)

macro(idtech3_strip_game_ai_middleware_sources)
	list(REMOVE_ITEM CLIENT_SRCS ${IDTECH3_GAME_AI_MIDDLEWARE_SRCS})
	list(REMOVE_ITEM CLIENT_SRCS ${_g_fac})
endmacro()

macro(idtech3_append_game_ai_middleware_sources)
	# FACS facial animation is used by client VoIP/morph paths — always link.
	list(APPEND CLIENT_SRCS ${_g_fac})
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
