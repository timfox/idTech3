#pragma once

#include "tr_local.h"

void R_Iris_Init( void );
void R_Iris_Shutdown( void );

qboolean R_Iris_Active( void );
qboolean R_Iris_PanNewFOV( void );

qboolean vk_iris_overlay_active( void );
void vk_iris_record_overlay( VkCommandBuffer cmd );
