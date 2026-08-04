# Vulkan renderer extension manifest (2026 layout under extensions/{neural,splats,rtx,scaffold}/).
# Chocolate paths (splats, Hybrid1/Raygun, Arc Blanc) are gated separately from
# USE_EXPERIMENTAL_RENDERERS (neural + paper scaffolds). See docs/NEURAL_RENDERER_PHASES.md.

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

# Chocolate: always linked; cvars default off.
set(VK_CHOCOLATE_SPLAT_SRCS
	${_VK_EXT}/splats/vk_mgs.c
	${_VK_EXT}/splats/vk_wsp.c
	${_VK_EXT}/splats/vk_squeezeme.c
)

# Chocolate RT demos: always linked; real path when USE_VULKAN_RTX (internal #else stubs).
set(VK_CHOCOLATE_RTX_SRCS
	renderers/vulkan/vk_ambient_visibility.c
	${_VK_EXT}/rtx/vk_hybrid1.c
	${_VK_EXT}/rtx/vk_raygun.c
	${_VK_EXT}/rtx/vk_surfel_gi.c
	${_VK_EXT}/rtx/vk_rcgi.c
)

# Chocolate ocean: always linked; real path when USE_ARC_BLANC (internal #else stubs).
set(VK_ARC_BLANC_VK_SRCS
	${_VK_EXT}/scaffold/vk_arc_blanc.c
	${_VK_EXT}/scaffold/vk_arc_blanc_gpu.c
)

# Renderer-owned sidecars are linked in every profile because tr_init exposes
# their cvars/status contracts even when their opt-in execution gates are off.
set(VK_RENDERER_SIDECAR_SRCS
	${_VK_EXT}/scaffold/vk_hair_deferred.c
	${_VK_EXT}/scaffold/vk_vector_brush.c
	${_VK_EXT}/neural/vk_neural_deferred.c
)

set(VK_RTX_CORE_SRCS
	${_VK_EXT}/rtx/vk_rtx.c
	${_VK_EXT}/rtx/vk_rtx_material.c
	${_VK_EXT}/rtx/vk_rtx_world.c
	${_VK_EXT}/rtx/vk_rtx_entities.c
	${_VK_EXT}/rtx/vk_rtx_bindless.c
)

# Research-only RTX extras (still behind USE_EXPERIMENTAL_RENDERERS).
set(VK_RTX_RESEARCH_SRCS
	${_VK_EXT}/rtx/vk_pathtrace.c
	${_VK_EXT}/rtx/vk_grtx.c
	${_VK_EXT}/rtx/vk_fsa.c
)

# Paper / scaffold research (still behind USE_EXPERIMENTAL_RENDERERS).
set(VK_SCAFFOLD_RESEARCH_SRCS
	${_VK_EXT}/splats/vk_vksplat.c
	${_VK_EXT}/scaffold/vk_curast.c
	${_VK_EXT}/scaffold/vk_mimir.c
	${_VK_EXT}/scaffold/vk_iris.c
	${_VK_EXT}/scaffold/vk_vuda.c
	${_VK_EXT}/scaffold/vk_dressi.c
)

set(VK_EXPERIMENTAL_RENDERER_SRCS
	${VK_NEURAL_EXTENSION_SRCS}
	${VK_RTX_RESEARCH_SRCS}
	${VK_SCAFFOLD_RESEARCH_SRCS}
)

# Everything that may be stripped from the default glob before selective re-append.
set(VK_ALL_EXTENSION_SRCS
	${VK_CHOCOLATE_SPLAT_SRCS}
	${VK_CHOCOLATE_RTX_SRCS}
	${VK_ARC_BLANC_VK_SRCS}
	${VK_EXPERIMENTAL_RENDERER_SRCS}
)

# Legacy aliases used by docs/tests.
set(VK_SPLATS_EXTENSION_SRCS
	${VK_CHOCOLATE_SPLAT_SRCS}
	${_VK_EXT}/splats/vk_vksplat.c
)
set(VK_RTX_EXPERIMENTAL_SRCS
	${VK_CHOCOLATE_RTX_SRCS}
	${VK_RTX_RESEARCH_SRCS}
)
set(VK_SCAFFOLD_EXTENSION_SRCS
	${VK_ARC_BLANC_VK_SRCS}
	${VK_SCAFFOLD_RESEARCH_SRCS}
)
set(VK_PROFILE_EXTENSION_SRCS
	${VK_RTX_EXPERIMENTAL_SRCS}
	${VK_SCAFFOLD_EXTENSION_SRCS}
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
	list(APPEND RENDERER_VK_SRCS ${VK_CHOCOLATE_SPLAT_SRCS})
	list(APPEND RENDERER_VK_SRCS ${VK_CHOCOLATE_RTX_SRCS})
	list(APPEND RENDERER_VK_SRCS ${VK_ARC_BLANC_VK_SRCS})
	list(APPEND RENDERER_VK_SRCS ${VK_RENDERER_SIDECAR_SRCS})
	message(STATUS "Chocolate renderers: MGS/WSP/SqueezeMe + Hybrid1/Raygun + ArcBlanc (cvars off; ArcBlanc needs USE_ARC_BLANC, RT demos need USE_VULKAN_RTX)")

	if(USE_EXPERIMENTAL_RENDERERS)
		list(APPEND RENDERER_VK_SRCS ${VK_EXPERIMENTAL_RENDERER_SRCS})
		message(STATUS "Experimental renderers: ON (neural + research scaffolds)")
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
	set(_vk_webcam "renderers/vulkan/extensions/scaffold/vk_webcam_screen.c")
	if(NOT "${_vk_webcam}" IN_LIST RENDERER_VK_SRCS)
		list(APPEND RENDERER_VK_SRCS ${_vk_webcam})
	endif()
endmacro()
