#pragma once


#include "../common/tr_types.h"

void vk_geometry_corruption_register( void );
void vk_geometry_corruption_begin_frame( void );
qboolean vk_geometry_corruption_allow_draw( void );
void vk_geometry_corruption_note_index_reject( const char *reason );
void vk_geometry_corruption_note_soft_ibo_reject( void );
cvar_t *vk_geometry_corruption_debug_cvar( void );

