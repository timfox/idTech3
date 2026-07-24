# Explicit client source manifest (2026 domain layout).
# Paths are CMAKE_SOURCE_DIR-relative under runtime/client (Phase 5e prep).

idtech3_require_layout()

set(_IDTECH3_CLIENT "${IDTECH3_DIR_RUNTIME_CLIENT}")
file(RELATIVE_PATH _IDTECH3_CLIENT_REL "${CMAKE_SOURCE_DIR}" "${_IDTECH3_CLIENT}")

set(IDTECH3_CLIENT_CORE_SRCS
	${_IDTECH3_CLIENT_REL}/core/cl_main.c
	${_IDTECH3_CLIENT_REL}/core/cl_frame.c
	${_IDTECH3_CLIENT_REL}/core/cl_lifecycle.c
	${_IDTECH3_CLIENT_REL}/core/cl_cvars.c
	${_IDTECH3_CLIENT_REL}/core/cl_connect.c
	${_IDTECH3_CLIENT_REL}/core/cl_p2p_session.c
	${_IDTECH3_CLIENT_REL}/core/cl_cmds.c
	${_IDTECH3_CLIENT_REL}/core/cl_ref.c
	${_IDTECH3_CLIENT_REL}/core/cl_pipeline.c
	${_IDTECH3_CLIENT_REL}/core/cl_gameframe.c
	${_IDTECH3_CLIENT_REL}/core/cl_parse.c
	${_IDTECH3_CLIENT_REL}/core/cl_net_chan.c
	${_IDTECH3_CLIENT_REL}/core/cl_cgame.c
	${_IDTECH3_CLIENT_REL}/core/cl_tv.c
	${_IDTECH3_CLIENT_REL}/core/cl_fonts.c
	${_IDTECH3_CLIENT_REL}/core/cl_compat_math.c
	${_IDTECH3_CLIENT_REL}/core/cl_input.c
	${_IDTECH3_CLIENT_REL}/core/cl_keys.c
	${_IDTECH3_CLIENT_REL}/core/cl_serverbrowser.c
	${_IDTECH3_CLIENT_REL}/core/cl_app_crdt.c
	${_IDTECH3_CLIENT_REL}/core/cl_oscar.c
	${_IDTECH3_CLIENT_REL}/core/cl_discord.c
	${_IDTECH3_CLIENT_REL}/core/cl_discord_proto.c
	${_IDTECH3_CLIENT_REL}/core/cl_rconset.c
)

set(IDTECH3_CLIENT_MEDIA_SRCS
	${_IDTECH3_CLIENT_REL}/media/cl_demo.c
	${_IDTECH3_CLIENT_REL}/media/cl_download.c
	${_IDTECH3_CLIENT_REL}/media/cl_cin.c
	${_IDTECH3_CLIENT_REL}/media/cl_cin_modern.c
	${_IDTECH3_CLIENT_REL}/media/cl_cin_colors.c
	${_IDTECH3_CLIENT_REL}/media/cl_cin_ffmpeg.c
	${_IDTECH3_CLIENT_REL}/media/cl_cin_dav1d.c
	${_IDTECH3_CLIENT_REL}/media/cl_cin_dav2d.c
	${_IDTECH3_CLIENT_REL}/media/cl_cin_vvdec.c
	${_IDTECH3_CLIENT_REL}/media/cl_cin_theora.c
	${_IDTECH3_CLIENT_REL}/media/cl_cin_vpx.c
	${_IDTECH3_CLIENT_REL}/media/cl_avi.c
	${_IDTECH3_CLIENT_REL}/media/cl_jpeg.c
	${_IDTECH3_CLIENT_REL}/media/cl_menuvideo.c
)

set(IDTECH3_CLIENT_PLATFORM_SRCS
	${_IDTECH3_CLIENT_REL}/platform/cl_curl.c
	${_IDTECH3_CLIENT_REL}/platform/cl_torrent.cpp
	${_IDTECH3_CLIENT_REL}/platform/cl_steam.c
	${_IDTECH3_CLIENT_REL}/platform/cl_openhmd.c
	${_IDTECH3_CLIENT_REL}/platform/cl_websocket.c
	${_IDTECH3_CLIENT_REL}/platform/cl_mumble.c
	${_IDTECH3_CLIENT_REL}/platform/cl_voip.c
	${_IDTECH3_CLIENT_REL}/platform/cl_streaming.c
)

# Shell: console / UI / HUD / engine sprites (was root *.c; now shell/).
file(GLOB IDTECH3_CLIENT_SHELL_SRCS_ABS "${_IDTECH3_CLIENT}/shell/*.c")
set(IDTECH3_CLIENT_SHELL_SRCS "")
foreach(_abs ${IDTECH3_CLIENT_SHELL_SRCS_ABS})
	file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}" "${_abs}")
	list(APPEND IDTECH3_CLIENT_SHELL_SRCS "${_rel}")
endforeach()

macro(idtech3_init_client_sources)
	set(CLIENT_SRCS
		${IDTECH3_CLIENT_CORE_SRCS}
		${IDTECH3_CLIENT_MEDIA_SRCS}
		${IDTECH3_CLIENT_PLATFORM_SRCS}
		${IDTECH3_CLIENT_SHELL_SRCS}
	)
endmacro()

macro(idtech3_client_include_domains target)
	target_include_directories(${target} PRIVATE
		${_IDTECH3_CLIENT}/core
		${_IDTECH3_CLIENT}/world
		${_IDTECH3_CLIENT}/media
		${_IDTECH3_CLIENT}/platform
		${_IDTECH3_CLIENT}/shell
	)
endmacro()

include(${CMAKE_SOURCE_DIR}/cmake/client/ClientExtensionSources.cmake)
