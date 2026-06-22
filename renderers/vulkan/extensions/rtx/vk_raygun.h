#pragma once

#include "../common/vulkan/vulkan.h"

void R_Raygun_Init( void );
void R_Raygun_Shutdown( void );
void vk_raygun_init( void );
void vk_raygun_shutdown( void );
void vk_raygun_frame_begin( void );
qboolean vk_raygun_active( void );
void vk_raygun_record_pass( VkCommandBuffer cmd );
