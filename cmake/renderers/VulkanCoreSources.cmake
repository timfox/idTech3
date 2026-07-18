# Vulkan shipping core manifest (root renderers/vulkan/*.c — not extensions/ or inspector/).
# Extension/neural pack: VulkanExtensionSources.cmake. ImGui inspector: CMakeLists USE_IMGUI block.

idtech3_require_layout()

set(VK_CORE_RENDERER_PATTERNS
	tr_init.c
	tr_backend.c
	tr_bsp.c
	tr_shade.c
	tr_surface.c
	vk_deferred_gbuffer.c
	vk_visibility_buffer.c
	vk_forward_plus.c
	vk_temporal.c
	vk_vector_font.c
	vk_nanovdb_decode.c
	tr_font
	tr_vector_font
)

macro(idtech3_init_vulkan_core_sources)
	idtech3_glob_src_rel(RENDERER_VK_SRCS
		"renderers/vulkan/*.c"
		"renderers/vulkan/*.cpp"
	)
	list(FILTER RENDERER_VK_SRCS EXCLUDE REGEX ".*/shaders/spirv/generated/shader_data\\.c$")
	list(FILTER RENDERER_VK_SRCS EXCLUDE REGEX ".*/shaders/spirv/generated/shader_binding\\.c$")
	list(FILTER RENDERER_VK_SRCS EXCLUDE REGEX ".*/vk_experimental_renderer_stubs\\.c$")
endmacro()

function(idtech3_log_vulkan_core_manifest)
	list(LENGTH VK_CORE_RENDERER_PATTERNS _vk_core_n)
	message(STATUS "Vulkan core manifest: ${_vk_core_n} documented pattern groups")
endfunction()
