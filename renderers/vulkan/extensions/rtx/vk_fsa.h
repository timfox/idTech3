#pragma once


void R_FSA_Init( void );
void R_FSA_Shutdown( void );
void R_FSA_OnMapLoad( const char *mapBaseName );

qboolean R_FSA_Active( void );
qboolean vk_fsa_rtx_adaptive_wanted( void );

void vk_fsa_build_importance_after_geometry( void );
void vk_fsa_denoise_after_rtx( VkCommandBuffer cmd );

/* RTX integration (USE_VULKAN_RTX): importance sampler + adaptive traceParams */
VkImageView vk_fsa_get_importance_view( void );
void vk_fsa_patch_rtx_trace_params( float traceParams[4], uint32_t frameSeed );
void vk_fsa_write_rtx_importance_descriptor( VkDescriptorSet rtxSet );

