# Vulkan renderer extension manifest (neural pack + profile-gated RTX-adjacent scaffolds).

set(VK_EXPERIMENTAL_RENDERER_SRCS
	src/renderers/vulkan/vk_niv.c
	src/renderers/vulkan/vk_nist.c
	src/renderers/vulkan/vk_nvc.c
	src/renderers/vulkan/vk_vfgi.c
	src/renderers/vulkan/vk_vfgi_world.c
	src/renderers/vulkan/vk_ndgi.c
	src/renderers/vulkan/vk_nslm.c
	src/renderers/vulkan/vk_renderformer.c
	src/renderers/vulkan/vk_renderformer_scene.c
	src/renderers/vulkan/vk_vksplat.c
	src/renderers/vulkan/vk_wpt.c
	src/renderers/vulkan/vk_mgs.c
	src/renderers/vulkan/vk_wsp.c
)

# Additional extension paths compiled only when USE_EXPERIMENTAL_RENDERERS=ON
# (still under src/renderers/vulkan/ until Phase 3 physical moves).
set(VK_PROFILE_EXTENSION_SRCS
	src/renderers/vulkan/vk_hybrid1.c
	src/renderers/vulkan/vk_pathtrace.c
	src/renderers/vulkan/vk_grtx.c
	src/renderers/vulkan/vk_raygun.c
	src/renderers/vulkan/vk_squeezeme.c
	src/renderers/vulkan/vk_dressi.c
	src/renderers/vulkan/vk_curast.c
	src/renderers/vulkan/vk_mimir.c
	src/renderers/vulkan/vk_iris.c
	src/renderers/vulkan/vk_vuda.c
)

macro(idtech3_apply_vulkan_extension_sources)
	if(USE_EXPERIMENTAL_RENDERERS)
		message(STATUS "Experimental renderers: ON (NIV/NIST/NVC/VFGI/NDGI/NSLM/RenderFormer/VkSplat/WPT/MGS/WSP + extension pack)")
	else()
		list(REMOVE_ITEM RENDERER_VK_SRCS ${VK_EXPERIMENTAL_RENDERER_SRCS})
		list(REMOVE_ITEM RENDERER_VK_SRCS ${VK_PROFILE_EXTENSION_SRCS})
		if(NOT "src/renderers/vulkan/vk_experimental_renderer_stubs.c" IN_LIST RENDERER_VK_SRCS)
			list(APPEND RENDERER_VK_SRCS src/renderers/vulkan/vk_experimental_renderer_stubs.c)
		endif()
		message(STATUS "Experimental renderers: OFF (vk_experimental_renderer_stubs.c)")
	endif()
	if(USE_MIMIR_CUDA AND USE_RESEARCH_EXTENSIONS)
		list(APPEND RENDERER_VK_SRCS src/extensions/research/mimir/mimir_cuda.c)
	endif()
endmacro()
