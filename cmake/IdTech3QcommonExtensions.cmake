# Optional qcommon sources gated by USE_RESEARCH_EXTENSIONS and USE_OPEN_WORLD.
# Source lists use legacy-relative paths (AUX / monolithic QCOMMON_SRCS compatibility).

idtech3_require_layout()

set(_IDTECH3_RESEARCH "${IDTECH3_DIR_EXTENSIONS}/research")
set(_IDTECH3_WORLD "${IDTECH3_DIR_MODULE_WORLD}")
set(_IDTECH3_QCOMMON "${IDTECH3_DIR_ENGINE_CORE}")

macro(idtech3_append_research_qcommon_sources)
	if(USE_RESEARCH_EXTENSIONS)
		list(APPEND QCOMMON_SRCS
			${_IDTECH3_RESEARCH}/vuda/vuda_model.c
			${_IDTECH3_RESEARCH}/vuda/vuda_console.c
			${_IDTECH3_RESEARCH}/vksplat/vksplat_model.c
			${_IDTECH3_RESEARCH}/vksplat/vksplat_console.c
			${_IDTECH3_RESEARCH}/curast/curast_model.c
			${_IDTECH3_RESEARCH}/curast/curast_console.c
			${_IDTECH3_RESEARCH}/infernux/infernux_model.c
			${_IDTECH3_RESEARCH}/infernux/infernux_console.c
			${_IDTECH3_RESEARCH}/mimir/mimir_model.c
			${_IDTECH3_RESEARCH}/mimir/mimir_console.c
			${_IDTECH3_RESEARCH}/iris/iris_model.c
			${_IDTECH3_RESEARCH}/iris/iris_console.c
			${_IDTECH3_RESEARCH}/iris/iris_io.c
			${_IDTECH3_RESEARCH}/radiusfps/radiusfps_cpu.c
			${_IDTECH3_RESEARCH}/radiusfps/radiusfps_console.c
			${_IDTECH3_RESEARCH}/gccfer/gccfer_dataset.c
			${_IDTECH3_RESEARCH}/gccfer/gccfer_cafer.c
			${_IDTECH3_RESEARCH}/gccfer/gccfer_model.c
			${_IDTECH3_RESEARCH}/gccfer/gccfer_console.c
			${_IDTECH3_RESEARCH}/dax/dax_benchmark.c
			${_IDTECH3_RESEARCH}/dax/dax_model.c
			${_IDTECH3_RESEARCH}/dax/dax_eval.c
			${_IDTECH3_RESEARCH}/dax/dax_console.c
			${_IDTECH3_RESEARCH}/x3dpra/x3dpra_physics.c
			${_IDTECH3_RESEARCH}/x3dpra/x3dpra_scene.c
			${_IDTECH3_RESEARCH}/x3dpra/x3dpra_opt.c
			${_IDTECH3_RESEARCH}/x3dpra/x3dpra_forward.c
			${_IDTECH3_RESEARCH}/x3dpra/x3dpra_console.c
			${_IDTECH3_RESEARCH}/dk_qsd/dk_qsd.c
			${_IDTECH3_RESEARCH}/dk_qsd/dk_qsd_kernels.c
			${_IDTECH3_RESEARCH}/dk_qsd/dk_qsd_dense.c
			${_IDTECH3_RESEARCH}/dk_qsd/dk_qsd_mps.c
			${_IDTECH3_RESEARCH}/dk_qsd/dk_qsd_observables.c
			${_IDTECH3_RESEARCH}/dk_qsd/dk_qsd_console.c
			${_IDTECH3_RESEARCH}/dlm/dlm_matrix.c
			${_IDTECH3_RESEARCH}/dlm/dlm_eval.c
			${_IDTECH3_RESEARCH}/dlm/dlm_console.c
			${_IDTECH3_RESEARCH}/sfca/sfca_core.c
			${_IDTECH3_RESEARCH}/sfca/sfca_run.c
			${_IDTECH3_RESEARCH}/sfca/sfca_scan.c
			${_IDTECH3_RESEARCH}/sfca/sfca_console.c
		)
		if(USE_RADIUSFPS_CUDA)
			enable_language(CUDA)
			find_package(CUDAToolkit REQUIRED)
			list(APPEND QCOMMON_SRCS ${_IDTECH3_RESEARCH}/radiusfps/radiusfps_cuda.cu)
			message(STATUS "RadiusFPS-G: CUDA acceleration enabled (cl_radiusfps_backend gpu)")
		endif()
	endif()
endmacro()

macro(idtech3_append_open_world_qcommon_sources)
	if(USE_OPEN_WORLD)
		list(APPEND QCOMMON_SRCS
			${_IDTECH3_WORLD}/world_district.cpp
			${_IDTECH3_WORLD}/world_open.cpp
			${_IDTECH3_WORLD}/world_residency.cpp
			${_IDTECH3_WORLD}/sector_graph.cpp
			${_IDTECH3_WORLD}/fog_biology.cpp
			${_IDTECH3_WORLD}/genetic_gan.cpp
			${_IDTECH3_WORLD}/world_proc.cpp
		)
	endif()
	if(USE_ARC_BLANC)
		list(APPEND QCOMMON_SRCS
			${_IDTECH3_WORLD}/arc_blanc/arc_blanc.c
			${_IDTECH3_WORLD}/arc_blanc/arc_blanc_fft.c
			${_IDTECH3_WORLD}/arc_blanc/arc_blanc_spectrum.c
			${_IDTECH3_WORLD}/arc_blanc/arc_blanc_ocean.c
			${_IDTECH3_WORLD}/arc_blanc/arc_blanc_velocity.c
			${_IDTECH3_WORLD}/arc_blanc/arc_blanc_coupling.c
		)
	endif()
endmacro()
