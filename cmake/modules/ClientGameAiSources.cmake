# Game AI middleware (Director, GOAP, Horde, BT, …) — chocolate layer, OFF in core profile.
# Sources remain under src/game/ until runtime/game/ split (Phase 5).
# Wiring: call idtech3_strip/append from CMakeLists client block once game_middleware_stubs.h lands.

set(IDTECH3_GAME_AI_MIDDLEWARE_SRCS
	src/game/g_director.c
	src/game/g_response.c
	src/game/g_choreography.c
	src/game/g_dismember.c
	src/game/g_facial.c
	src/game/g_horde.c
	src/game/g_goap.c
	src/game/g_aiml.c
	src/game/g_eda.c
	src/game/g_engine_systems.c
	src/game/g_bt.c
)

macro(idtech3_strip_game_ai_middleware_sources)
	list(REMOVE_ITEM CLIENT_SRCS ${IDTECH3_GAME_AI_MIDDLEWARE_SRCS})
endmacro()

macro(idtech3_append_game_ai_middleware_sources)
	if(USE_GAME_AI_MIDDLEWARE)
		list(APPEND CLIENT_SRCS ${IDTECH3_GAME_AI_MIDDLEWARE_SRCS})
	endif()
endmacro()
