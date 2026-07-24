/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Vulkan swapchain creation and query helpers.
Extracted from vk.c for incremental modularization.
===========================================================================
*/

#include "tr_local.h"
#include "vk.h"
#include "vk_cmd.h"
#include "vk_image_layout.h"
#include "vk_swapchain.h"
#include "vk_util.h"

void vk_create_swapchain( VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface,
	VkSurfaceFormatKHR surface_format, VkSwapchainKHR *swapchain, qboolean verbose )
{
	VkImageViewCreateInfo view;
	VkSurfaceCapabilitiesKHR surface_caps;
	VkExtent2D image_extent;
	uint32_t present_mode_count, i;
	VkPresentModeKHR present_mode;
	VkPresentModeKHR *present_modes;
	uint32_t image_count;
	VkSwapchainCreateInfoKHR desc;
	qboolean mailbox_supported = qfalse;
	qboolean immediate_supported = qfalse;
	qboolean fifo_relaxed_supported = qfalse;
	int v;

	VK_CHECK( qvkGetPhysicalDeviceSurfaceCapabilitiesKHR( physical_device, surface, &surface_caps ) );

	image_extent = surface_caps.currentExtent;
	if ( image_extent.width == 0xffffffff && image_extent.height == 0xffffffff ) {
		image_extent.width = MIN( surface_caps.maxImageExtent.width, MAX( surface_caps.minImageExtent.width, (uint32_t) glConfig.vidWidth ) );
		image_extent.height = MIN( surface_caps.maxImageExtent.height, MAX( surface_caps.minImageExtent.height, (uint32_t) glConfig.vidHeight ) );
	}

	vk.clearAttachment = qtrue;

	if ( !vk.fboActive ) {
		if ( ( surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT ) == 0 ) {
			vk.clearAttachment = qfalse;
			ri.Printf( PRINT_WARNING, "VK_IMAGE_USAGE_TRANSFER_DST_BIT is not supported by the swapchain, \\r_clear might not work\n" );
		}
		if ( ( surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT ) == 0 ) {
			ri.Error( ERR_FATAL, "create_swapchain: VK_IMAGE_USAGE_TRANSFER_SRC_BIT is not supported by the swapchain" );
		}
	}

	VK_CHECK( qvkGetPhysicalDeviceSurfacePresentModesKHR( physical_device, surface, &present_mode_count, NULL ) );

	present_modes = (VkPresentModeKHR *) ri.Malloc( present_mode_count * sizeof( VkPresentModeKHR ) );
	VK_CHECK( qvkGetPhysicalDeviceSurfacePresentModesKHR( physical_device, surface, &present_mode_count, present_modes ) );

	if ( verbose ) {
		ri.Printf( PRINT_ALL, "...presentation modes:" );
	}
	for ( i = 0; i < present_mode_count; i++ ) {
		if ( verbose ) {
			ri.Printf( PRINT_ALL, " %s", vk_present_mode_string( present_modes[i] ) );
		}
		if ( present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR )
			mailbox_supported = qtrue;
		else if ( present_modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR )
			immediate_supported = qtrue;
		else if ( present_modes[i] == VK_PRESENT_MODE_FIFO_RELAXED_KHR )
			fifo_relaxed_supported = qtrue;
	}
	if ( verbose ) {
		ri.Printf( PRINT_ALL, "\n" );
	}

	ri.Free( present_modes );

	if ( ( v = ri.Cvar_VariableIntegerValue( "r_swapInterval" ) ) != 0 ) {
		if ( v == 2 && mailbox_supported )
			present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
		else if ( fifo_relaxed_supported )
			present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
		else
			present_mode = VK_PRESENT_MODE_FIFO_KHR;
		image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
	} else {
		if ( immediate_supported ) {
			present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_IMM, surface_caps.minImageCount );
		} else if ( mailbox_supported ) {
			present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_MAILBOX, surface_caps.minImageCount );
		} else if ( fifo_relaxed_supported ) {
			present_mode = VK_PRESENT_MODE_FIFO_RELAXED_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
		} else {
			present_mode = VK_PRESENT_MODE_FIFO_KHR;
			image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO, surface_caps.minImageCount );
		}
	}

	if ( image_count < 2 ) {
		image_count = 2;
	}

	if ( surface_caps.maxImageCount == 0 && present_mode == VK_PRESENT_MODE_FIFO_KHR ) {
		image_count = MAX( MIN_SWAPCHAIN_IMAGES_FIFO_0, surface_caps.minImageCount );
	} else if ( surface_caps.maxImageCount > 0 ) {
		image_count = MIN( MIN( image_count, surface_caps.maxImageCount ), MAX_SWAPCHAIN_IMAGES );
	}

	if ( verbose ) {
		ri.Printf( PRINT_ALL, "...selected presentation mode: %s, image count: %i\n", vk_present_mode_string( present_mode ), image_count );
	}

	desc.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	desc.pNext = NULL;
	desc.flags = 0;
	desc.surface = surface;
	desc.minImageCount = image_count;
	desc.imageFormat = surface_format.format;
	desc.imageColorSpace = surface_format.colorSpace;
	desc.imageExtent = image_extent;
	vk.swapchain_extent = image_extent;
	vk.swapchain_extent_valid = qtrue;
	desc.imageArrayLayers = 1;
	desc.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	/* Reset before create: presentation restore reuses the live vk struct. */
	vk.swapchainTransferSrc = qfalse;
	if ( !vk.fboActive ) {
		desc.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		/* UI blur copies the tonemapped present image; keep the capability flag
		 * in sync with imageUsage (was missing here → blur died after restart). */
		vk.swapchainTransferSrc = qtrue;
	} else if ( ( surface_caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT ) != 0 ) {
		/* UI backdrop-filter blur copies the tonemapped swapchain into a transient
		 * pooled texture (never sampling the live image). Requires TRANSFER_SRC. */
		desc.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		vk.swapchainTransferSrc = qtrue;
	}
	desc.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	desc.queueFamilyIndexCount = 0;
	desc.pQueueFamilyIndices = NULL;
	desc.preTransform = surface_caps.currentTransform;
	desc.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	desc.presentMode = present_mode;
	desc.clipped = VK_TRUE;
	desc.oldSwapchain = VK_NULL_HANDLE;

	VK_CHECK( qvkCreateSwapchainKHR( device, &desc, NULL, swapchain ) );

	VK_CHECK( qvkGetSwapchainImagesKHR( vk.device, vk.swapchain, &vk.swapchain_image_count, NULL ) );
	vk.swapchain_image_count = MIN( vk.swapchain_image_count, MAX_SWAPCHAIN_IMAGES );
	VK_CHECK( qvkGetSwapchainImagesKHR( vk.device, vk.swapchain, &vk.swapchain_image_count, vk.swapchain_images ) );

	for ( i = 0; (uint32_t) i < vk.swapchain_image_count; i++ ) {
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.pNext = NULL;
		view.flags = 0;
		view.image = vk.swapchain_images[i];
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = vk.present_format.format;
		view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		view.subresourceRange.baseMipLevel = 0;
		view.subresourceRange.levelCount = 1;
		view.subresourceRange.baseArrayLayer = 0;
		view.subresourceRange.layerCount = 1;

		VK_CHECK( qvkCreateImageView( vk.device, &view, NULL, &vk.swapchain_image_views[i] ) );

		vk_set_object_name( (uint64_t)vk.swapchain_images[i], va( "swapchain image %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT );
		vk_set_object_name( (uint64_t)vk.swapchain_image_views[i], va( "swapchain image %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT );
	}

	for ( i = 0; (uint32_t) i < vk.swapchain_image_count; i++ ) {
		VkSemaphoreCreateInfo s;
		s.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		s.pNext = NULL;
		s.flags = 0;
		VK_CHECK( qvkCreateSemaphore( vk.device, &s, NULL, &vk.swapchain_rendering_finished[i] ) );
		vk_set_object_name( (uint64_t)vk.swapchain_rendering_finished[i], va( "swapchain_rendering_finished semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
	}

	if ( vk.initSwapchainLayout != VK_IMAGE_LAYOUT_UNDEFINED ) {
		VkCommandBuffer command_buffer = vk_begin_command_buffer();

		for ( i = 0; (uint32_t) i < vk.swapchain_image_count; i++ ) {
			record_image_layout_transition( command_buffer, vk.swapchain_images[i],
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_UNDEFINED, vk.initSwapchainLayout, 0, 0 );
		}

		vk_end_command_buffer( command_buffer, __func__ );
	}
}

qboolean vk_query_surface_extent( VkPhysicalDevice physical_device, VkSurfaceKHR surface, VkExtent2D *extent )
{
	VkSurfaceCapabilitiesKHR caps;

	if ( qvkGetPhysicalDeviceSurfaceCapabilitiesKHR( physical_device, surface, &caps ) != VK_SUCCESS ) {
		return qfalse;
	}

	if ( caps.currentExtent.width == UINT32_MAX && caps.currentExtent.height == UINT32_MAX ) {
		extent->width = (uint32_t) gls.windowWidth;
		extent->height = (uint32_t) gls.windowHeight;
	}
	else {
		extent->width = caps.currentExtent.width;
		extent->height = caps.currentExtent.height;
	}

	if ( extent->width == 0 || extent->height == 0 ) {
		return qfalse;
	}

	return qtrue;
}

void vk_log_swapchain_recreation( VkResult res, const VkExtent2D *old_extent, const VkExtent2D *new_extent )
{
	uint32_t old_width = vk.swapchain_extent_valid && old_extent ? old_extent->width : 0;
	uint32_t old_height = vk.swapchain_extent_valid && old_extent ? old_extent->height : 0;
	uint32_t new_width = new_extent ? new_extent->width : 0;
	uint32_t new_height = new_extent ? new_extent->height : 0;
	static int last_print_ms = -1;
	static VkResult last_res = VK_SUCCESS;
	static uint32_t last_old_width, last_old_height, last_new_width, last_new_height;
	static int last_fullscreen, last_refresh;
	const int now_ms = ri.Milliseconds();
	const qboolean same_as_last =
		( res == last_res ) &&
		( old_width == last_old_width ) &&
		( old_height == last_old_height ) &&
		( new_width == last_new_width ) &&
		( new_height == last_new_height ) &&
		( (int)glConfig.isFullscreen == last_fullscreen ) &&
		( glConfig.displayFrequency == last_refresh );

	if ( same_as_last && last_print_ms >= 0 ) {
		return;
	}

	ri.Printf( PRINT_WARNING, "vk_present_frame(): %s old=%ux%u new=%ux%u fullscreen=%d refresh=%d\n",
		vk_result_string( res ), old_width, old_height, new_width, new_height, glConfig.isFullscreen, glConfig.displayFrequency );

	last_print_ms = now_ms;
	last_res = res;
	last_old_width = old_width;
	last_old_height = old_height;
	last_new_width = new_width;
	last_new_height = new_height;
	last_fullscreen = (int)glConfig.isFullscreen;
	last_refresh = glConfig.displayFrequency;
}


void vk_destroy_swapchain( void )
{
	uint32_t i;

	vk.swapchain_extent_valid = qfalse;

	for ( i = 0; (uint32_t) i < vk.swapchain_image_count; i++ ) {
		if ( vk.swapchain_image_views[i] != VK_NULL_HANDLE ) {
			qvkDestroyImageView( vk.device, vk.swapchain_image_views[i], NULL );
			vk.swapchain_image_views[i] = VK_NULL_HANDLE;
		}
		if ( vk.swapchain_rendering_finished[i] != VK_NULL_HANDLE ) {
			qvkDestroySemaphore( vk.device, vk.swapchain_rendering_finished[i], NULL );
			vk.swapchain_rendering_finished[i] = VK_NULL_HANDLE;
		}
	}

	if ( vk.swapchain != VK_NULL_HANDLE && qvkDestroySwapchainKHR != NULL ) {
		qvkDestroySwapchainKHR( vk.device, vk.swapchain, NULL );
	}
	vk.swapchain = VK_NULL_HANDLE;
	vk.swapchain_image_count = 0;
}
