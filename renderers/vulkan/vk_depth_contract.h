#pragma once

#ifdef USE_VULKAN

void vk_depth_contract_register( void );
void vk_depth_contract_begin_frame( void );
void vk_depth_contract_note_writer( const char *passName );
void vk_depth_contract_note_reader( const char *passName );

#endif /* USE_VULKAN */
