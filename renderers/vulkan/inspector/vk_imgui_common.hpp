/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

Shared C++ includes and symbols for Vulkan Dear ImGui inspector TUs.
===========================================================================
*/
#pragma once

#ifdef USE_IMGUI

#define IMGUI_DEFINE_MATH_OPERATORS

#include <float.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>
#include <imgui_internal.h>

extern "C" {
#include "q_shared.h"
#include "../../renderers/common/tr_types.h"
#include "../../renderers/common/tr_public.h"

extern glconfig_t glConfig;
extern cvar_t *r_imgui;
extern cvar_t *r_imguiTheme;
extern cvar_t *r_studio_tools;
}

#if defined( USE_SDL ) && !defined( ANDROID )
#	include <SDL3/SDL.h>
extern "C" struct SDL_Window *SDL_window;
#endif

#	define USE_VK_PBR
#	include "../vk.h"

#include "vk_imgui.h"

extern "C" {
#include "vk_imgui_scene.h"
}

extern vkImguiInspector_t vkInspector;
extern vkImguiWindows_t vkWindows;
extern vkImguiGlobal_t vkImguiState;

extern "C" bool VkImgui_InitVulkanBackend( ImGui_ImplVulkan_InitInfo *outInfo, char *errBuf, size_t errBufSize );
extern "C" void VkImgui_ShutdownVulkanBackend( void );
extern "C" void VkImgui_NewFrameVulkan( void );
extern "C" void VkImgui_RenderDrawDataVulkan( ImDrawData *drawData, VkCommandBuffer cmd );
extern "C" void VkImgui_UpdateMouseFromSDL( ImGuiIO *io, qboolean inspectorWantsInput );
extern "C" void VkImgui_NotifySwapchainRestart( void );
extern "C" void VkImgui_SetVulkanBackendReady( qboolean ready );

float VkImgui_CvarFloat( const char *name );
void VkImgui_CvarSlider( const char *label, const char *cvar, float v, float vMin, float vMax, const char *fmt = "%.3f" );

#endif /* USE_IMGUI */
