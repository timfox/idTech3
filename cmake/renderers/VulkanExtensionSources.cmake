# Vulkan renderer extension manifest (2026 layout under extensions/{neural,splats,rtx,scaffold}/).
# Strip/append uses canonical renderers/ paths (Phase 5e prep).

idtech3_require_layout()

set(_VK_EXT "renderers/vulkan/extensions")

set(VK_NEURAL_EXTENSION_SRCS
	${_VK_EXT}/neural/vk_niv.c
	${_VK_EXT}/neural/vk_nist.c
	${_VK_EXT}/neural/vk_nvc.c
	${_VK_EXT}/neural/vk_vfgi.c
	${_VK_EXT}/neural/vk_vfgi_world.c
	${_VK_EXT}/neural/vk_ndgi.c
	${_VK_EXT}/neural/vk_nslm.c
	${_VK_EXT}/neural/vk_renderformer.c
	${_VK_EXT}/neural/vk_renderformer_scene.c
	${_VK_EXT}/neural/vk_wpt.c
	${_VK_EXT}/neural/vk_neural_io.c
)

set(VK_SPLATS_EXTENSION_SRCS
	${_VK_EXT}/splats/vk_vksplat.c
	${_VK_EXT}/splats/vk_mgs.c
	${_VK_EXT}/splats/vk_wsp.c
	${_VK_EXT}/splats/vk_squeezeme.c
)

set(VK_RTX_CORE_SRCS
	${_VK_EXT}/rtx/vk_rtx.c
	${_VK_EXT}/rtx/vk_rtx_world.c
	${_VK_EXT}/rtx/vk_rtx_entities.c
)

set(VK_RTX_EXPERIMENTAL_SRCS
	${_VK_EXT}/rtx/vk_hybrid1.c
	${_VK_EXT}/rtx/vk_pathtrace.c
	${_VK_EXT}/rtx/vk_grtx.c
	${_VK_EXT}/rtx/vk_raygun.c
	${_VK_EXT}/rtx/vk_fsa.c
)

set(VK_SCAFFOLD_EXTENSION_SRCS
	${_VK_EXT}/scaffold/vk_curast.c
	${_VK_EXT}/scaffold/vk_mimir.c
	${_VK_EXT}/scaffold/vk_iris.c
	${_VK_EXT}/scaffold/vk_vuda.c
	${_VK_EXT}/scaffold/vk_dressi.c
	${_VK_EXT}/scaffold/vk_arc_blanc.c
	${_VK_EXT}/scaffold/vk_arc_blanc_gpu.c
)

set(VK_EXPERIMENTAL_RENDERER_SRCS
	${VK_NEURAL_EXTENSION_SRCS}
	${VK_SPLATS_EXTENSION_SRCS}
)

set(VK_PROFILE_EXTENSION_SRCS
	${VK_RTX_EXPERIMENTAL_SRCS}
	${VK_SCAFFOLD_EXTENSION_SRCS}
)

set(VK_ALL_EXTENSION_SRCS
	${VK_EXPERIMENTAL_RENDERER_SRCS}
	${VK_PROFILE_EXTENSION_SRCS}
)

macro(idtech3_vulkan_extension_include_dirs target)
	target_include_directories(${target} PRIVATE
		${IDTECH3_DIR_RENDERERS}/vulkan
		${IDTECH3_DIR_RENDERERS}/vulkan/extensions/neural
		${IDTECH3_DIR_RENDERERS}/vulkan/extensions/splats
		${IDTECH3_DIR_RENDERERS}/vulkan/extensions/rtx
		${IDTECH3_DIR_RENDERERS}/vulkan/extensions/scaffold
	)
endmacro()

macro(idtech3_apply_vulkan_extension_sources)
	list(REMOVE_ITEM RENDERER_VK_SRCS ${VK_ALL_EXTENSION_SRCS})
	list(REMOVE_ITEM RENDERER_VK_SRCS ${VK_RTX_CORE_SRCS})

	list(APPEND RENDERER_VK_SRCS ${VK_RTX_CORE_SRCS})

	if(USE_EXPERIMENTAL_RENDERERS)
		list(APPEND RENDERER_VK_SRCS ${VK_ALL_EXTENSION_SRCS})
		message(STATUS "Experimental renderers: ON (neural/splats/rtx/scaffold extensions/)")
	else()
		set(_vk_stub "renderers/vulkan/vk_experimental_renderer_stubs.c")
		if(NOT "${_vk_stub}" IN_LIST RENDERER_VK_SRCS)
			list(APPEND RENDERER_VK_SRCS ${_vk_stub})
		endif()
		message(STATUS "Experimental renderers: OFF (vk_experimental_renderer_stubs.c)")
	endif()
	if(USE_MIMIR_CUDA AND USE_RESEARCH_EXTENSIONS)
		set(_mimir_cuda "extensions/research/mimir/mimir_cuda.c")
		list(APPEND RENDERER_VK_SRCS ${_mimir_cuda})
	endif()
	set(_vk_emulator "renderers/vulkan/extensions/scaffold/vk_emulator_screen.c")
	if(NOT "${_vk_emulator}" IN_LIST RENDERER_VK_SRCS)
		list(APPEND RENDERER_VK_SRCS ${_vk_emulator})
	endif()
endmacro()
