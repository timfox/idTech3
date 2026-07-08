# Explicit client source manifest (2026 domain layout).
# Paths via IDTECH3_DIR_RUNTIME_CLIENT (Phase 5b — IdTech3Layout.cmake required).

idtech3_require_layout()

set(_IDTECH3_CLIENT "${IDTECH3_DIR_RUNTIME_CLIENT}")

set(IDTECH3_CLIENT_CORE_SRCS
	${_IDTECH3_CLIENT}/core/cl_main.c
	${_IDTECH3_CLIENT}/core/cl_frame.c
	${_IDTECH3_CLIENT}/core/cl_lifecycle.c
	${_IDTECH3_CLIENT}/core/cl_cvars.c
	${_IDTECH3_CLIENT}/core/cl_connect.c
	${_IDTECH3_CLIENT}/core/cl_cmds.c
	${_IDTECH3_CLIENT}/core/cl_ref.c
	${_IDTECH3_CLIENT}/core/cl_pipeline.c
	${_IDTECH3_CLIENT}/core/cl_gameframe.c
	${_IDTECH3_CLIENT}/core/cl_parse.c
	${_IDTECH3_CLIENT}/core/cl_net_chan.c
	${_IDTECH3_CLIENT}/core/cl_cgame.c
	${_IDTECH3_CLIENT}/core/cl_compat_math.c
	${_IDTECH3_CLIENT}/core/cl_input.c
	${_IDTECH3_CLIENT}/core/cl_keys.c
	${_IDTECH3_CLIENT}/core/cl_serverbrowser.c
	${_IDTECH3_CLIENT}/core/cl_app_crdt.c
)

set(IDTECH3_CLIENT_MEDIA_SRCS
	${_IDTECH3_CLIENT}/media/cl_demo.c
	${_IDTECH3_CLIENT}/media/cl_download.c
	${_IDTECH3_CLIENT}/media/cl_cin.c
	${_IDTECH3_CLIENT}/media/cl_cin_modern.c
	${_IDTECH3_CLIENT}/media/cl_cin_ffmpeg.c
	${_IDTECH3_CLIENT}/media/cl_cin_dav1d.c
	${_IDTECH3_CLIENT}/media/cl_cin_theora.c
	${_IDTECH3_CLIENT}/media/cl_cin_vpx.c
	${_IDTECH3_CLIENT}/media/cl_avi.c
	${_IDTECH3_CLIENT}/media/cl_jpeg.c
	${_IDTECH3_CLIENT}/media/cl_menuvideo.c
)

set(IDTECH3_CLIENT_PLATFORM_SRCS
	${_IDTECH3_CLIENT}/platform/cl_curl.c
	${_IDTECH3_CLIENT}/platform/cl_steam.c
	${_IDTECH3_CLIENT}/platform/cl_websocket.c
	${_IDTECH3_CLIENT}/platform/cl_mumble.c
	${_IDTECH3_CLIENT}/platform/cl_voip.c
)

file(GLOB IDTECH3_CLIENT_SHELL_SRCS "${_IDTECH3_CLIENT}/*.c")

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
	)
endmacro()

include(${CMAKE_SOURCE_DIR}/cmake/client/ClientExtensionSources.cmake)
