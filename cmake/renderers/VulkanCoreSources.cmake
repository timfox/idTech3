# Vulkan shipping core manifest (Forward+, deferred, BSP, post, fonts, volumetrics).
# Files remain under src/renderers/vulkan/; AUX_SOURCE_DIRECTORY picks them up.
# Extension/neural pack is listed in VulkanExtensionSources.cmake and stripped when OFF.

set(VK_CORE_RENDERER_PATTERNS
  tr_init.c
  tr_backend.c
  tr_bsp.c
  tr_shade.c
  tr_surface.c
  vk_deferred_gbuffer.c
  vk_forward_plus.c
  vk_temporal.c
  vk_vector_font.c
  vk_nanovdb_decode.c
  tr_font
  tr_vector_font
)

# Document-only; not used to filter AUX picks (would risk omitting required TU).
function(idtech3_log_vulkan_core_manifest)
  message(STATUS "Vulkan core manifest: ${VK_CORE_RENDERER_PATTERNS}")
endfunction()
