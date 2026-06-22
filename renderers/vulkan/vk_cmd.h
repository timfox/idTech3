/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan one-time command buffer helpers: allocate, record, submit, wait, free.
Used for staging uploads, texture copies, and other one-off GPU work.
===========================================================================
*/

#pragma once

#include "../common/vulkan/vulkan.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Allocate a primary command buffer, begin recording with ONE_TIME_SUBMIT.
 * Caller records commands, then calls vk_end_command_buffer to submit and free. */
VkCommandBuffer vk_begin_command_buffer( void );

/* End recording, submit to queue, wait idle, free command buffer. */
void vk_end_command_buffer( VkCommandBuffer command_buffer, const char *location );

#ifdef __cplusplus
}
#endif
