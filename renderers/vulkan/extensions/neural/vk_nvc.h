#pragma once

#ifdef USE_VULKAN

void R_NVC_Init( void );
void R_NVC_Shutdown( void );
void R_NVC_OnMapLoad( const char *mapBaseName );

qboolean R_NVC_Active( void );

void vk_nvc_apply_after_geometry( void );

#endif /* USE_VULKAN */
