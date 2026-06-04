#pragma once

#ifdef USE_VULKAN

void R_RenderFormer_Init( void );
void R_RenderFormer_Shutdown( void );
void R_RenderFormer_OnMapLoad( const char *mapBaseName );

qboolean R_RenderFormer_Active( void );

void vk_renderformer_apply_after_geometry( void );

#endif /* USE_VULKAN */
