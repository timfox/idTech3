# Server domain source manifest (replaces AUX_SOURCE_DIRECTORY).
# Open-world hooks: ServerExtensionSources.cmake strip/append.

idtech3_require_layout()

set(_IDTECH3_SERVER "${IDTECH3_DIR_RUNTIME_SERVER}")
file(RELATIVE_PATH _IDTECH3_SERVER_REL "${CMAKE_SOURCE_DIR}" "${_IDTECH3_SERVER}")

set(IDTECH3_SERVER_CORE_SRCS
	${_IDTECH3_SERVER_REL}/core/sv_main.c
	${_IDTECH3_SERVER_REL}/core/sv_init.c
	${_IDTECH3_SERVER_REL}/core/sv_ccmds.c
)

set(IDTECH3_SERVER_CLIENT_SRCS
	${_IDTECH3_SERVER_REL}/client/sv_client.c
	${_IDTECH3_SERVER_REL}/client/sv_snapshot.c
	${_IDTECH3_SERVER_REL}/client/sv_filter.c
)

set(IDTECH3_SERVER_GAMEPLAY_SRCS
	${_IDTECH3_SERVER_REL}/gameplay/sv_game.c
	${_IDTECH3_SERVER_REL}/gameplay/sv_bot.c
	${_IDTECH3_SERVER_REL}/gameplay/sv_engine_sprites.c
	${_IDTECH3_SERVER_REL}/gameplay/sv_engine_decals.c
	${_IDTECH3_SERVER_REL}/gameplay/sv_enhanced.c
	${_IDTECH3_SERVER_REL}/gameplay/sv_physics.c
)

set(IDTECH3_SERVER_NET_SRCS
	${_IDTECH3_SERVER_REL}/net/sv_net_chan.c
)

set(IDTECH3_SERVER_SERVICES_SRCS
	${_IDTECH3_SERVER_REL}/services/sv_app_crdt.c
	${_IDTECH3_SERVER_REL}/services/sv_auth.c
	${_IDTECH3_SERVER_REL}/services/sv_tv.c
	${_IDTECH3_SERVER_REL}/services/sv_tvstream.c
)

set(IDTECH3_SERVER_WORLD_SRCS
	${_IDTECH3_SERVER_REL}/world/sv_world.c
	${_IDTECH3_SERVER_REL}/world/sv_openworld.c
	${_IDTECH3_SERVER_REL}/world/sv_world_config.c
)

macro(idtech3_init_server_sources)
	set(SERVER_SRCS
		${IDTECH3_SERVER_CORE_SRCS}
		${IDTECH3_SERVER_CLIENT_SRCS}
		${IDTECH3_SERVER_GAMEPLAY_SRCS}
		${IDTECH3_SERVER_NET_SRCS}
		${IDTECH3_SERVER_SERVICES_SRCS}
		${IDTECH3_SERVER_WORLD_SRCS}
	)
endmacro()

macro(idtech3_server_include_domains target)
	target_include_directories(${target} PRIVATE
		${_IDTECH3_SERVER}
		${_IDTECH3_SERVER}/core
		${_IDTECH3_SERVER}/client
		${_IDTECH3_SERVER}/gameplay
		${_IDTECH3_SERVER}/net
		${_IDTECH3_SERVER}/services
		${_IDTECH3_SERVER}/world
		${IDTECH3_DIR_RUNTIME_GAME}
		${IDTECH3_DIR_MODULE_BOTLIB}
		${IDTECH3_DIR_MODULE_PHYSICS}
		${IDTECH3_DIR_MODULE_WORLD}
	)
endmacro()
