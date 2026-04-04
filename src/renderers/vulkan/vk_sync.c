#include "tr_local.h"
#include "vk.h"
#include "vk_sync.h"
#include "vk_util.h"

void vk_create_sync_primitives( void )
{
	VkSemaphoreCreateInfo desc;
	VkFenceCreateInfo fence_desc;
	uint32_t i;

	desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	desc.pNext = NULL;
	desc.flags = 0;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ )
	{
		desc.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		desc.pNext = NULL;
		desc.flags = 0;

		VK_CHECK( qvkCreateSemaphore( vk.device, &desc, NULL, &vk.tess[i].image_acquired ) );

		fence_desc.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_desc.pNext = NULL;
		fence_desc.flags = 0;

		VK_CHECK( qvkCreateFence( vk.device, &fence_desc, NULL, &vk.tess[i].rendering_finished_fence ) );
		vk.tess[i].waitForFence = qfalse;

		vk_set_object_name( (uint64_t)vk.tess[i].image_acquired, va( "image_acquired semaphore %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT );
		vk_set_object_name( (uint64_t)vk.tess[i].rendering_finished_fence, va( "rendering_finished fence %i", i ), VK_DEBUG_REPORT_OBJECT_TYPE_FENCE_EXT );
	}
}

void vk_destroy_sync_primitives( void )
{
	uint32_t i;

	for ( i = 0; i < NUM_COMMAND_BUFFERS; i++ ) {
		qvkDestroySemaphore( vk.device, vk.tess[i].image_acquired, NULL );
		qvkDestroyFence( vk.device, vk.tess[i].rendering_finished_fence, NULL );
		vk.tess[i].waitForFence = qfalse;
		vk.tess[i].swapchain_image_acquired = qfalse;
	}
}
