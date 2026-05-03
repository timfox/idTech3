/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Bridge Dear ImGui to the engine's Vulkan loader (qvk* via ImGui_ImplVulkan_LoadFunctions).
SDL2 mouse position when the inspector wants input.
===========================================================================
*/

#ifdef USE_IMGUI

#define VK_NO_PROTOTYPES

#include <imgui.h>
#include <imgui_impl_vulkan.h>

extern "C" {
#include "../../qcommon/q_shared.h"
#include "../vk_instance.h"
#define USE_VK_PBR
#include "../vk.h"
#include "../vk_render_pass.h"
}

#if defined( USE_SDL ) && !defined( ANDROID )
#	if defined( USE_LOCAL_HEADERS )
#		include "SDL.h"
#	else
#		include <SDL2/SDL.h>
#	endif
extern "C" struct SDL_Window *SDL_window;
#endif

#include "vk_imgui.h"

static qboolean g_vkImguiBackendReady = qfalse;

extern "C" void VkImgui_SetVulkanBackendReady( qboolean ready )
{
	g_vkImguiBackendReady = ready;
}

extern "C" qboolean VkImgui_IsVulkanBackendReady( void )
{
	return g_vkImguiBackendReady;
}

static PFN_vkVoidFunction VkImgui_ResolveVulkanProc( const char *name, void *userData )
{
	PFN_vkVoidFunction p = nullptr;
	(void)userData;

	if ( ri.VK_GetInstanceProcAddr && vk_instance != VK_NULL_HANDLE ) {
		p = (PFN_vkVoidFunction)ri.VK_GetInstanceProcAddr( vk_instance, name );
	}
	if ( p == nullptr && qvkGetDeviceProcAddr != nullptr && vk.device != VK_NULL_HANDLE ) {
		p = (PFN_vkVoidFunction)qvkGetDeviceProcAddr( vk.device, name );
	}
	return p;
}

extern "C" bool VkImgui_InitVulkanBackend( ImGui_ImplVulkan_InitInfo *outInfo, char *errBuf, size_t errBufSize )
{
	ImGui_ImplVulkan_InitInfo info;

	if ( errBuf && errBufSize > 0 ) {
		errBuf[0] = '\0';
	}

	if ( vk_instance == VK_NULL_HANDLE || vk.device == VK_NULL_HANDLE || vk.queue == VK_NULL_HANDLE ||
		vk.physical_device == VK_NULL_HANDLE ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "Vulkan device not ready", errBufSize );
		}
		return false;
	}

	if ( !ImGui_ImplVulkan_LoadFunctions( 0, VkImgui_ResolveVulkanProc, nullptr ) ) {
		if ( errBuf && errBufSize > 0 ) {
			Q_strncpyz( errBuf, "ImGui_ImplVulkan_LoadFunctions failed", errBufSize );
		}
		return false;
	}

	Com_Memset( &info, 0, sizeof( info ) );
#ifdef VK_API_VERSION_1_4
	info.ApiVersion = VK_API_VERSION_1_4;
#elif defined( VK_API_VERSION_1_3 )
	info.ApiVersion = VK_API_VERSION_1_3;
#else
	info.ApiVersion = VK_API_VERSION_1_2;
#endif
	info.Instance = vk_instance;
	info.PhysicalDevice = vk.physical_device;
	info.Device = vk.device;
	info.QueueFamily = vk.queue_family_index;
	info.Queue = vk.queue;
	info.DescriptorPool = VK_NULL_HANDLE;
	info.DescriptorPoolSize = 64;
	info.MinImageCount = 2;
	info.ImageCount = vk.swapchain_image_count > 0 ? vk.swapchain_image_count : 2;
	if ( info.ImageCount < info.MinImageCount ) {
		info.ImageCount = info.MinImageCount;
	}
	info.PipelineCache = VK_NULL_HANDLE;
	info.PipelineInfoMain.RenderPass = vk.render_pass.overlay_compose;
	info.PipelineInfoMain.Subpass = 0;
	info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	info.UseDynamicRendering = false;

	if ( ImGui_ImplVulkan_Init( &info ) ) {
		if ( outInfo ) {
			*outInfo = info;
		}
		return true;
	}

	if ( errBuf && errBufSize > 0 ) {
		Q_strncpyz( errBuf, "ImGui_ImplVulkan_Init failed", errBufSize );
	}
	return false;
}

extern "C" void VkImgui_ShutdownVulkanBackend( void )
{
	ImGui_ImplVulkan_Shutdown();
}

extern "C" void VkImgui_NewFrameVulkan( void )
{
	ImGui_ImplVulkan_NewFrame();
}

extern "C" void VkImgui_RenderDrawDataVulkan( ImDrawData *drawData, VkCommandBuffer cmd )
{
	if ( drawData != nullptr && cmd != VK_NULL_HANDLE ) {
		ImGui_ImplVulkan_RenderDrawData( drawData, cmd );
	}
}

extern "C" void VkImgui_UpdateMouseFromSDL( ImGuiIO *io, qboolean inspectorWantsInput )
{
#if defined( USE_SDL ) && !defined( ANDROID )
	if ( !inspectorWantsInput || io == nullptr || SDL_window == nullptr ) {
		return;
	}
	{
		int mx = 0;
		int my = 0;
		const Uint32 buttons = SDL_GetMouseState( &mx, &my );
		io->MousePos = ImVec2( (float)mx, (float)my );
		io->MouseDown[0] = ( buttons & SDL_BUTTON( SDL_BUTTON_LEFT ) ) != 0;
		io->MouseDown[1] = ( buttons & SDL_BUTTON( SDL_BUTTON_RIGHT ) ) != 0;
		io->MouseDown[2] = ( buttons & SDL_BUTTON( SDL_BUTTON_MIDDLE ) ) != 0;
	}
#else
	(void)io;
	(void)inspectorWantsInput;
#endif
}

extern "C" void VkImgui_NotifySwapchainRestart( void )
{
	if ( vk.swapchain_image_count >= 2 ) {
		ImGui_ImplVulkan_SetMinImageCount( vk.swapchain_image_count );
	}
}

extern "C" void VkImgui_RecordOverlayPass( void )
{
	extern cvar_t *r_imgui;

	if ( !g_vkImguiBackendReady || !vkImguiState.active ) {
		return;
	}
	if ( !r_imgui || !r_imgui->integer ) {
		return;
	}
	if ( ImGui::GetCurrentContext() == nullptr ) {
		return;
	}
	if ( vk.cmd == nullptr || vk.cmd->command_buffer == VK_NULL_HANDLE ) {
		return;
	}
	if ( vk.render_pass.overlay_compose == VK_NULL_HANDLE ||
		vk.cmd->swapchain_image_index >= MAX_SWAPCHAIN_IMAGES ||
		vk.framebuffers.overlay_compose[ vk.cmd->swapchain_image_index ] == VK_NULL_HANDLE ||
		vk.renderWidth == 0 || vk.renderHeight == 0 ) {
		return;
	}

	ImDrawData *dd = ImGui::GetDrawData();
	if ( dd == nullptr || !dd->Valid ) {
		return;
	}

	vk_begin_render_pass_tracked( vk.render_pass.overlay_compose,
		vk.framebuffers.overlay_compose[ vk.cmd->swapchain_image_index ],
		qfalse, vk.renderWidth, vk.renderHeight );
	ImGui_ImplVulkan_RenderDrawData( dd, vk.cmd->command_buffer );
	vk_end_render_pass_tracked();
}

#endif /* USE_IMGUI */
