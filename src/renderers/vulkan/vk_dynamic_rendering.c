/*
=============================================================================
Vulkan Dynamic Rendering Implementation

Uses VK_KHR_dynamic_rendering for modern render pass replacement.
=============================================================================
*/

#include "tr_local.h"
// Renderer import interface - defined in renderer main file
extern refimport_t ri;
#include "vk.h"
#include "vk_dynamic_rendering.h"

#ifdef USE_VULKAN

// CVars
extern cvar_t *r_vk_dynamicRendering;

static qboolean dynamic_rendering_supported = qfalse;

/*
=============================================================================
Dynamic Rendering Extension Detection
=============================================================================
*/

qboolean vk_dynamic_rendering_check_support(void) {
	if (!r_vk_dynamicRendering || !r_vk_dynamicRendering->integer) {
		dynamic_rendering_supported = qfalse;
		return qfalse;
	}

	// Vulkan 1.4: VK_KHR_dynamic_rendering is now core
	// Check if the device supports dynamic rendering
	VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
		.pNext = NULL
	};

	VkPhysicalDeviceFeatures2 device_features = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = &dynamic_rendering_features
	};

	if (qvkGetPhysicalDeviceFeatures2KHR) {
		qvkGetPhysicalDeviceFeatures2KHR(vk.physical_device, &device_features);

		if (dynamic_rendering_features.dynamicRendering) {
			dynamic_rendering_supported = qtrue;
			ri.Printf(PRINT_ALL, "Vulkan: Dynamic rendering supported (Vulkan 1.4 core)\n");
			return qtrue;
		}
	}

	dynamic_rendering_supported = qfalse;
	ri.Printf(PRINT_ALL, "Vulkan: Dynamic rendering not supported by device\n");
	return qfalse;
}

qboolean vk_dynamic_rendering_enabled(void) {
	return dynamic_rendering_supported;
}

/*
=============================================================================
Dynamic Rendering Implementation
=============================================================================
*/

void vk_begin_dynamic_rendering(const VkRenderingInfo *rendering_info) {
	if (!dynamic_rendering_supported || !qvkCmdBeginRenderingKHR) {
		ri.Printf(PRINT_ERROR, "Vulkan: Dynamic rendering not supported or function not available\n");
		return;
	}

	qvkCmdBeginRenderingKHR(vk.cmd->command_buffer, rendering_info);
}

void vk_end_dynamic_rendering(void) {
	if (!dynamic_rendering_supported || !qvkCmdEndRenderingKHR) {
		ri.Printf(PRINT_ERROR, "Vulkan: Dynamic rendering not supported or function not available\n");
		return;
	}

	qvkCmdEndRenderingKHR(vk.cmd->command_buffer);
}

// Helper function to create VkRenderingInfo for common use cases
void vk_setup_rendering_info(VkRenderingInfo *info, VkImageView color_view, VkImageView depth_view,
                            uint32_t width, uint32_t height, VkClearValue *clear_values) {
	if (!info) return;

	VkRenderingAttachmentInfo color_attachment = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.pNext = NULL,
		.imageView = color_view,
		.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		.loadOp = clear_values ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = clear_values ? clear_values[0] : (VkClearValue){0}
	};

	VkRenderingAttachmentInfo depth_attachment = {
		.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
		.pNext = NULL,
		.imageView = depth_view,
		.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		.loadOp = clear_values ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD,
		.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
		.clearValue = clear_values ? clear_values[1] : (VkClearValue){0}
	};

	*info = (VkRenderingInfo){
		.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
		.pNext = NULL,
		.flags = 0,
		.renderArea = {
			.offset = {0, 0},
			.extent = {width, height}
		},
		.layerCount = 1,
		.viewMask = 0,
		.colorAttachmentCount = color_view ? 1 : 0,
		.pColorAttachments = color_view ? &color_attachment : NULL,
		.pDepthAttachment = depth_view ? &depth_attachment : NULL,
		.pStencilAttachment = NULL
	};
}

#endif // USE_VULKAN