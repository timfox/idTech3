#pragma once


void vk_shading_compare_register( void );
void vk_shading_compare_begin_frame( void );
void vk_shading_compare_accumulate( float absError );
void vk_shading_compare_reset( void );

