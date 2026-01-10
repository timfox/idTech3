/*
=============================================================================
Vulkan Shader Validation System - Header
=============================================================================
*/

#ifndef __VK_SHADER_VALIDATION_H__
#define __VK_SHADER_VALIDATION_H__

#include "tr_local.h"
#include "vk.h"

#ifdef USE_VULKAN

// Check if a shader name is problematic
qboolean vk_is_problematic_shader_name(const char *shader_name);

// Check if a shader type is problematic
qboolean vk_is_problematic_shader_type(int shader_type);

// Comprehensive validation before pipeline creation
qboolean vk_validate_shader_before_pipeline(const char *shader_name, int shader_type, 
											const Vk_Pipeline_Def *def);

// Handle pipeline creation errors
void vk_handle_pipeline_creation_error(VkResult result, const char *shader_name, int shader_type);

// Get validation statistics
void vk_get_shader_validation_stats(int *total_checked, int *skipped, 
									int *device_lost, int *pipeline_failures);

// Reset validation statistics
void vk_reset_shader_validation_stats(void);

// Print validation report
void vk_print_shader_validation_report(void);

// Get count of problematic shaders detected
int vk_get_problematic_shader_count(void);

#endif // USE_VULKAN

#endif // __VK_SHADER_VALIDATION_H__
