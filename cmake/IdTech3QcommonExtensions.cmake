# Optional qcommon sources gated by USE_RESEARCH_EXTENSIONS and USE_OPEN_WORLD.

macro(idtech3_append_research_qcommon_sources)
	if(USE_RESEARCH_EXTENSIONS)
		list(APPEND QCOMMON_SRCS
			src/extensions/research/vuda/vuda_model.c
			src/extensions/research/vuda/vuda_console.c
			src/extensions/research/vksplat/vksplat_model.c
			src/extensions/research/vksplat/vksplat_console.c
			src/extensions/research/curast/curast_model.c
			src/extensions/research/curast/curast_console.c
			src/extensions/research/infernux/infernux_model.c
			src/extensions/research/infernux/infernux_console.c
			src/extensions/research/mimir/mimir_model.c
			src/extensions/research/mimir/mimir_console.c
			src/extensions/research/iris/iris_model.c
			src/extensions/research/iris/iris_console.c
			src/extensions/research/iris/iris_io.c
			src/extensions/research/radiusfps/radiusfps_cpu.c
			src/extensions/research/radiusfps/radiusfps_console.c
			src/extensions/research/gccfer/gccfer_dataset.c
			src/extensions/research/gccfer/gccfer_cafer.c
			src/extensions/research/gccfer/gccfer_model.c
			src/extensions/research/gccfer/gccfer_console.c
			src/extensions/research/dax/dax_benchmark.c
			src/extensions/research/dax/dax_model.c
			src/extensions/research/dax/dax_eval.c
			src/extensions/research/dax/dax_console.c
			src/extensions/research/x3dpra/x3dpra_physics.c
			src/extensions/research/x3dpra/x3dpra_scene.c
			src/extensions/research/x3dpra/x3dpra_opt.c
			src/extensions/research/x3dpra/x3dpra_forward.c
			src/extensions/research/x3dpra/x3dpra_console.c
		)
		if(USE_RADIUSFPS_CUDA)
			enable_language(CUDA)
			find_package(CUDAToolkit REQUIRED)
			list(APPEND QCOMMON_SRCS src/extensions/research/radiusfps/radiusfps_cuda.cu)
			message(STATUS "RadiusFPS-G: CUDA acceleration enabled (cl_radiusfps_backend gpu)")
		endif()
	endif()
endmacro()

macro(idtech3_append_open_world_qcommon_sources)
	if(USE_OPEN_WORLD)
		list(APPEND QCOMMON_SRCS
			src/world/world_district.cpp
			src/world/world_open.cpp
			src/world/world_residency.cpp
			src/world/sector_graph.cpp
			src/world/fog_biology.cpp
			src/world/genetic_gan.cpp
			src/qcommon/cluster_graph.cpp
			src/world/world_proc.cpp
			src/qcommon/cm_stream_merge.c
			src/qcommon/com_openworld_smoke.c
		)
	endif()
endmacro()
