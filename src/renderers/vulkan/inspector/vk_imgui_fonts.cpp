/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

ImGui font loading (Roboto/system TTF when available, default Proggy otherwise).
===========================================================================
*/

#ifdef USE_IMGUI

#include "vk_imgui_common.hpp"

#include <stdio.h>
#include <sys/stat.h>

static ImFont *vkImgDefaultFont = nullptr;

static qboolean VkImgui_FontPathReadable( const char *path )
{
	struct stat st;

	if ( !path || !path[0] ) {
		return qfalse;
	}
	return ( stat( path, &st ) == 0 && S_ISREG( st.st_mode ) ) ? qtrue : qfalse;
}

static ImFont *VkImgui_TryLoadFont( ImGuiIO &io, const char *path, float sizePx )
{
	if ( !VkImgui_FontPathReadable( path ) ) {
		return nullptr;
	}
	ImFontConfig cfg;
	cfg.OversampleH = 2;
	cfg.OversampleV = 2;
	cfg.PixelSnapH = qtrue;
	return io.Fonts->AddFontFromFileTTF( path, sizePx, &cfg );
}

extern "C" void VkImgui_LoadFonts( void )
{
	ImGuiIO &io = ImGui::GetIO();
	const float sizePx = 16.0f;
	static const char *candidates[] = {
		"fonts/Roboto-Medium.ttf",
		"Resources/Fonts/Roboto-Medium.ttf",
		"/usr/share/fonts/truetype/roboto/unhinted/RobotoTTF/Roboto-Medium.ttf",
		"/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
		"/usr/share/fonts/TTF/DejaVuSans.ttf",
		"/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
		"C:/Windows/Fonts/segoeui.ttf",
		nullptr
	};

	io.Fonts->Clear();
	for ( int i = 0; candidates[i]; i++ ) {
		vkImgDefaultFont = VkImgui_TryLoadFont( io, candidates[i], sizePx );
		if ( vkImgDefaultFont ) {
			ri.Printf( PRINT_ALL, "[VK][imgui] UI font: %s (%.0fpx)\n", candidates[i], sizePx );
			break;
		}
	}
	if ( !vkImgDefaultFont ) {
		ImFontConfig cfg;
		cfg.SizePixels = sizePx;
		vkImgDefaultFont = io.Fonts->AddFontDefault( &cfg );
		ri.Printf( PRINT_ALL, "[VK][imgui] UI font: ImGui default (%.0fpx)\n", sizePx );
	}
	io.FontDefault = vkImgDefaultFont;
}

extern "C" ImFont *VkImgui_GetDefaultFont( void )
{
	return vkImgDefaultFont;
}

#endif /* USE_IMGUI */
