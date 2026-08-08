#pragma once


void R_VFGI_Init( void );
void R_VFGI_Shutdown( void );
void R_VFGI_OnMapLoad( const char *mapBaseName );

qboolean R_VFGI_Active( void );

void vk_vfgi_apply_after_geometry( void );

