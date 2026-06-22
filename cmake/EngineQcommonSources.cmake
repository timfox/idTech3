# Qcommon core source manifest (replaces AUX_SOURCE_DIRECTORY on src/qcommon/).
# Platform/optional net/vm backends stripped; re-appended from CMakeLists when enabled.
# Open-world qcommon helpers (cluster_graph, cm_stream_merge, com_openworld_smoke) stay in
# every profile — cm_load/cm_stream link them; src/world/*.cpp remains USE_OPEN_WORLD gated.

idtech3_require_layout()

set(_IDTECH3_QCOMMON_OPTIONAL
	engine/core/net_dtls.c
	engine/core/net_sdr.c
	engine/core/vm_aarch64.c
	engine/core/vm_armv7l.c
	engine/core/vm_powerpc.c
	engine/core/vm_x86.c
	src/qcommon/net_dtls.c
	src/qcommon/net_sdr.c
	src/qcommon/vm_aarch64.c
	src/qcommon/vm_armv7l.c
	src/qcommon/vm_powerpc.c
	src/qcommon/vm_x86.c
)

macro(idtech3_init_qcommon_sources)
	idtech3_glob_src_rel(QCOMMON_SRCS
		"engine/core/*.c"
		"engine/core/*.cpp"
		"src/qcommon/*.c"
		"src/qcommon/*.cpp"
	)
	list(REMOVE_ITEM QCOMMON_SRCS ${_IDTECH3_QCOMMON_OPTIONAL})
endmacro()
