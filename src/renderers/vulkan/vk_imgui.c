#ifndef USE_VULKAN
#define USE_VULKAN
#endif

#include "tr_local.h"
#include "vk.h"

#ifdef USE_CIMGUI

#include <stdbool.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "cimgui_impl.h"

static qboolean vk_imgui_initialized = qfalse;
static VkDescriptorPool vk_imgui_descriptor_pool = VK_NULL_HANDLE;

static void VK_ImGui_CheckResult( VkResult err )
{
	if ( err == VK_SUCCESS )
		return;

	ri.Error( ERR_FATAL, "ImGui Vulkan backend error: %s", vk_result_string( err ) );
}

static qboolean VK_ImGui_CreateDescriptorPool( void )
{
	if ( vk_imgui_descriptor_pool != VK_NULL_HANDLE )
		return qtrue;

	const VkDescriptorPoolSize pool_sizes[] = {
		{ VK_DESCRIPTOR_TYPE_SAMPLER, 32 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 64 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 32 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 32 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 32 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 32 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 32 }
	};

	VkDescriptorPoolCreateInfo info;
	Com_Memset( &info, 0, sizeof( info ) );
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	info.maxSets = 256;
	info.poolSizeCount = ARRAY_LEN( pool_sizes );
	info.pPoolSizes = pool_sizes;

	if ( qvkCreateDescriptorPool( vk.device, &info, NULL, &vk_imgui_descriptor_pool ) != VK_SUCCESS )
	{
		vk_imgui_descriptor_pool = VK_NULL_HANDLE;
		return qfalse;
	}

	return qtrue;
}

static void VK_ImGui_DestroyDescriptorPool( void )
{
	if ( vk_imgui_descriptor_pool != VK_NULL_HANDLE )
	{
		qvkDestroyDescriptorPool( vk.device, vk_imgui_descriptor_pool, NULL );
		vk_imgui_descriptor_pool = VK_NULL_HANDLE;
	}
}

qboolean VK_ImGui_InitBackend( void )
{
	if ( vk_imgui_initialized )
		return qtrue;

	if ( !vk.active || vk.device == VK_NULL_HANDLE || vk.render_pass.main == VK_NULL_HANDLE )
		return qfalse;

	if ( !VK_ImGui_CreateDescriptorPool() )
		return qfalse;

	ImGui_ImplVulkan_InitInfo init_info;
	Com_Memset( &init_info, 0, sizeof( init_info ) );
	init_info.Instance = VK_GetInstanceHandle();
	init_info.PhysicalDevice = vk.physical_device;
	init_info.Device = vk.device;
	init_info.QueueFamily = vk.queue_family_index;
	init_info.Queue = vk.queue;
	init_info.DescriptorPool = vk_imgui_descriptor_pool;
	init_info.MinImageCount = MAX( 2u, vk.swapchain_image_count );
	init_info.ImageCount = MAX( 2u, vk.swapchain_image_count );
	init_info.PipelineCache = vk.pipelineCache;
	init_info.ApiVersion = VK_MAKE_VERSION( 1, 1, 0 );
	init_info.PipelineInfoMain.RenderPass = vk.render_pass.main;
	init_info.PipelineInfoMain.Subpass = 0;
	init_info.PipelineInfoMain.MSAASamples = VK_GetMsaaSampleCount();
	init_info.CheckVkResultFn = VK_ImGui_CheckResult;
	init_info.MinAllocationSize = 1024 * 1024;

	if ( !ImGui_ImplVulkan_Init( &init_info ) )
	{
		VK_ImGui_ShutdownBackend();
		return qfalse;
	}

	vk_imgui_initialized = qtrue;
	return qtrue;
}

void VK_ImGui_ShutdownBackend( void )
{
	if ( !vk_imgui_initialized )
	{
		VK_ImGui_DestroyDescriptorPool();
		return;
	}

	ImGui_ImplVulkan_Shutdown();
	VK_ImGui_DestroyDescriptorPool();
	vk_imgui_initialized = qfalse;
}

void VK_ImGui_NewFrame( void )
{
	if ( !vk_imgui_initialized )
		return;

	ImGui_ImplVulkan_NewFrame();
}

void VK_ImGui_RenderDrawData( const ImDrawData *drawData )
{
	if ( !vk_imgui_initialized || !drawData )
		return;

	if ( drawData->CmdListsCount <= 0 )
		return;

	if ( !vk.cmd || vk.cmd->command_buffer == VK_NULL_HANDLE )
		return;

	ImGui_ImplVulkan_RenderDrawData( (ImDrawData *)drawData, vk.cmd->command_buffer, VK_NULL_HANDLE );
}

void VK_ImGui_NotifyRenderPassChanged( void )
{
	if ( !vk_imgui_initialized )
		return;

	ImGui_ImplVulkan_PipelineInfo info;
	Com_Memset( &info, 0, sizeof( info ) );
	info.RenderPass = vk.render_pass.main;
	info.Subpass = 0;
	info.MSAASamples = VK_GetMsaaSampleCount();
	ImGui_ImplVulkan_CreateMainPipeline( &info );
}

void VK_ImGui_NotifySwapchainChanged( void )
{
	if ( !vk_imgui_initialized )
		return;

	ImGui_ImplVulkan_SetMinImageCount( MAX( 2u, vk.swapchain_image_count ) );
}

#else

qboolean VK_ImGui_InitBackend( void )
{
	return qfalse;
}

void VK_ImGui_ShutdownBackend( void )
{
}

void VK_ImGui_NewFrame( void )
{
}

void VK_ImGui_RenderDrawData( const ImDrawData *drawData )
{
	(void)drawData;
}

void VK_ImGui_NotifyRenderPassChanged( void )
{
}

void VK_ImGui_NotifySwapchainChanged( void )
{
}

#endif

