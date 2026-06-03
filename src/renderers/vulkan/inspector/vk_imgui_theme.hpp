/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

ImGui editor themes aligned with VEditor UIContext (Pablo dark / Spectrum light).
===========================================================================
*/
#pragma once

#ifdef USE_IMGUI

#include <imgui.h>

namespace VkImguiTheme {

enum class EditorTheme {
	DarkPablo,
	LightSpectrum
};

static inline ImVec4 U32ToColor( ImU32 c )
{
	return ImGui::ColorConvertU32ToFloat4( c );
}

/* Adobe Spectrum–style palette (ImGuiSpectrum-compatible constants). */
namespace Spectrum {
	static constexpr ImU32 GRAY50   = IM_COL32( 250, 250, 250, 255 );
	static constexpr ImU32 GRAY75   = IM_COL32( 244, 244, 244, 255 );
	static constexpr ImU32 GRAY100  = IM_COL32( 239, 239, 239, 255 );
	static constexpr ImU32 GRAY200  = IM_COL32( 234, 234, 234, 255 );
	static constexpr ImU32 GRAY300  = IM_COL32( 211, 211, 211, 255 );
	static constexpr ImU32 GRAY400  = IM_COL32( 188, 188, 188, 255 );
	static constexpr ImU32 GRAY500  = IM_COL32( 155, 155, 155, 255 );
	static constexpr ImU32 GRAY600  = IM_COL32( 116, 116, 116, 255 );
	static constexpr ImU32 GRAY700  = IM_COL32( 73, 73, 73, 255 );
	static constexpr ImU32 GRAY800  = IM_COL32( 44, 44, 44, 255 );
	static constexpr ImU32 GRAY900  = IM_COL32( 30, 30, 30, 255 );
	static constexpr ImU32 BLUE400  = IM_COL32( 59, 153, 252, 255 );
	static constexpr ImU32 BLUE500  = IM_COL32( 38, 128, 235, 255 );
	static constexpr ImU32 BLUE600  = IM_COL32( 20, 115, 230, 255 );
	static constexpr ImU32 NONE     = IM_COL32( 0, 0, 0, 0 );
}

static inline void ApplyEditorChrome( EditorTheme theme )
{
	ImGuiStyle &style = ImGui::GetStyle();

	style.WindowRounding    = 8.0f;
	style.ChildRounding     = 8.0f;
	style.FrameRounding     = 6.0f;
	style.PopupRounding     = 6.0f;
	style.ScrollbarRounding = 6.0f;
	style.GrabRounding      = 6.0f;
	style.TabRounding       = 6.0f;
	style.WindowBorderSize  = 1.0f;
	style.ChildBorderSize   = 1.0f;
	style.PopupBorderSize   = 1.0f;
	style.FrameBorderSize   = 0.0f;
	style.IndentSpacing     = 22.0f;
	style.ScrollbarSize     = 14.0f;
	style.GrabMinSize       = 10.0f;
	style.WindowPadding     = ImVec2( 10.0f, 8.0f );
	style.FramePadding      = ImVec2( 8.0f, 4.0f );
	style.ItemSpacing       = ImVec2( 8.0f, 6.0f );

	if ( theme == EditorTheme::LightSpectrum ) {
		style.GrabRounding = 4.0f;
	}
}

static inline void ApplyDarkPablo( void )
{
	ImGui::StyleColorsDark();
	ApplyEditorChrome( EditorTheme::DarkPablo );

	ImVec4 *colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text]                  = ImVec4( 0.90f, 0.90f, 0.90f, 1.00f );
	colors[ImGuiCol_TextDisabled]          = ImVec4( 0.50f, 0.50f, 0.50f, 1.00f );
	colors[ImGuiCol_WindowBg]              = ImVec4( 0.10f, 0.10f, 0.10f, 1.00f );
	colors[ImGuiCol_ChildBg]               = ImVec4( 0.10f, 0.10f, 0.10f, 1.00f );
	colors[ImGuiCol_PopupBg]               = ImVec4( 0.08f, 0.08f, 0.08f, 0.94f );
	colors[ImGuiCol_Border]                = ImVec4( 0.25f, 0.25f, 0.25f, 0.70f );
	colors[ImGuiCol_BorderShadow]          = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
	colors[ImGuiCol_FrameBg]               = ImVec4( 0.18f, 0.18f, 0.18f, 1.00f );
	colors[ImGuiCol_FrameBgHovered]        = ImVec4( 0.25f, 0.25f, 0.25f, 1.00f );
	colors[ImGuiCol_FrameBgActive]         = ImVec4( 0.30f, 0.30f, 0.30f, 1.00f );
	colors[ImGuiCol_TitleBg]               = ImVec4( 0.10f, 0.10f, 0.10f, 1.00f );
	colors[ImGuiCol_TitleBgActive]         = ImVec4( 0.15f, 0.15f, 0.15f, 1.00f );
	colors[ImGuiCol_TitleBgCollapsed]      = ImVec4( 0.08f, 0.08f, 0.08f, 1.00f );
	colors[ImGuiCol_MenuBarBg]             = ImVec4( 0.12f, 0.12f, 0.12f, 1.00f );
	colors[ImGuiCol_ScrollbarBg]           = ImVec4( 0.02f, 0.02f, 0.02f, 0.53f );
	colors[ImGuiCol_ScrollbarGrab]         = ImVec4( 0.30f, 0.30f, 0.30f, 1.00f );
	colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4( 0.35f, 0.35f, 0.35f, 1.00f );
	colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4( 0.40f, 0.40f, 0.40f, 1.00f );
	colors[ImGuiCol_CheckMark]             = ImVec4( 0.80f, 0.80f, 0.80f, 1.00f );
	colors[ImGuiCol_SliderGrab]            = ImVec4( 0.70f, 0.70f, 0.70f, 1.00f );
	colors[ImGuiCol_SliderGrabActive]      = ImVec4( 0.85f, 0.85f, 0.85f, 1.00f );
	colors[ImGuiCol_Button]                = ImVec4( 0.20f, 0.20f, 0.20f, 1.00f );
	colors[ImGuiCol_ButtonHovered]         = ImVec4( 0.30f, 0.30f, 0.30f, 1.00f );
	colors[ImGuiCol_ButtonActive]          = ImVec4( 0.35f, 0.35f, 0.35f, 1.00f );
	colors[ImGuiCol_Header]                = ImVec4( 0.25f, 0.25f, 0.25f, 0.55f );
	colors[ImGuiCol_HeaderHovered]         = ImVec4( 0.35f, 0.35f, 0.35f, 0.80f );
	colors[ImGuiCol_HeaderActive]          = ImVec4( 0.40f, 0.40f, 0.40f, 1.00f );
	colors[ImGuiCol_Separator]             = ImVec4( 0.30f, 0.30f, 0.30f, 0.50f );
	colors[ImGuiCol_SeparatorHovered]      = ImVec4( 0.45f, 0.45f, 0.45f, 0.78f );
	colors[ImGuiCol_SeparatorActive]       = ImVec4( 0.50f, 0.50f, 0.50f, 1.00f );
	colors[ImGuiCol_ResizeGrip]            = ImVec4( 0.30f, 0.30f, 0.30f, 0.25f );
	colors[ImGuiCol_ResizeGripHovered]     = ImVec4( 0.45f, 0.45f, 0.45f, 0.67f );
	colors[ImGuiCol_ResizeGripActive]      = ImVec4( 0.50f, 0.50f, 0.50f, 0.95f );
	colors[ImGuiCol_Tab]                   = ImVec4( 0.12f, 0.12f, 0.12f, 1.00f );
	colors[ImGuiCol_TabHovered]            = ImVec4( 0.30f, 0.30f, 0.30f, 0.80f );
	colors[ImGuiCol_TabSelected]           = ImVec4( 0.25f, 0.25f, 0.25f, 1.00f );
	colors[ImGuiCol_TabDimmed]             = ImVec4( 0.10f, 0.10f, 0.10f, 0.97f );
	colors[ImGuiCol_TabDimmedSelected]     = ImVec4( 0.18f, 0.18f, 0.18f, 1.00f );
	colors[ImGuiCol_DockingPreview]        = ImVec4( 0.30f, 0.30f, 0.30f, 0.70f );
	colors[ImGuiCol_DockingEmptyBg]        = ImVec4( 0.15f, 0.15f, 0.15f, 1.00f );
	colors[ImGuiCol_PlotLines]             = ImVec4( 0.70f, 0.70f, 0.70f, 1.00f );
	colors[ImGuiCol_PlotLinesHovered]      = ImVec4( 0.90f, 0.50f, 0.50f, 1.00f );
	colors[ImGuiCol_PlotHistogram]         = ImVec4( 0.80f, 0.65f, 0.00f, 1.00f );
	colors[ImGuiCol_PlotHistogramHovered]  = ImVec4( 0.90f, 0.50f, 0.00f, 1.00f );
	colors[ImGuiCol_TextSelectedBg]        = ImVec4( 0.50f, 0.50f, 0.50f, 0.35f );
	colors[ImGuiCol_DragDropTarget]        = ImVec4( 1.00f, 0.00f, 0.00f, 0.90f );
	colors[ImGuiCol_NavHighlight]          = ImVec4( 0.70f, 0.70f, 0.70f, 1.00f );
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4( 1.00f, 1.00f, 1.00f, 0.70f );
	colors[ImGuiCol_NavWindowingDimBg]     = ImVec4( 0.80f, 0.80f, 0.80f, 0.20f );
	colors[ImGuiCol_ModalWindowDimBg]      = ImVec4( 0.80f, 0.80f, 0.80f, 0.35f );
}

static inline void ApplyLightSpectrum( void )
{
	ImGui::StyleColorsLight();
	ApplyEditorChrome( EditorTheme::LightSpectrum );

	ImVec4 *colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text]                  = U32ToColor( Spectrum::GRAY800 );
	colors[ImGuiCol_TextDisabled]          = U32ToColor( Spectrum::GRAY500 );
	colors[ImGuiCol_WindowBg]              = U32ToColor( Spectrum::GRAY100 );
	colors[ImGuiCol_ChildBg]               = ImVec4( 0.00f, 0.00f, 0.00f, 0.00f );
	colors[ImGuiCol_PopupBg]               = U32ToColor( Spectrum::GRAY50 );
	colors[ImGuiCol_Border]                = U32ToColor( Spectrum::GRAY300 );
	colors[ImGuiCol_BorderShadow]          = U32ToColor( Spectrum::NONE );
	colors[ImGuiCol_FrameBg]               = U32ToColor( Spectrum::GRAY75 );
	colors[ImGuiCol_FrameBgHovered]        = U32ToColor( Spectrum::GRAY50 );
	colors[ImGuiCol_FrameBgActive]         = U32ToColor( Spectrum::GRAY200 );
	colors[ImGuiCol_TitleBg]               = U32ToColor( Spectrum::GRAY300 );
	colors[ImGuiCol_TitleBgActive]         = U32ToColor( Spectrum::GRAY200 );
	colors[ImGuiCol_TitleBgCollapsed]      = U32ToColor( Spectrum::GRAY400 );
	colors[ImGuiCol_MenuBarBg]             = U32ToColor( Spectrum::GRAY100 );
	colors[ImGuiCol_ScrollbarBg]           = U32ToColor( Spectrum::GRAY100 );
	colors[ImGuiCol_ScrollbarGrab]         = U32ToColor( Spectrum::GRAY400 );
	colors[ImGuiCol_ScrollbarGrabHovered]  = U32ToColor( Spectrum::GRAY600 );
	colors[ImGuiCol_ScrollbarGrabActive]   = U32ToColor( Spectrum::GRAY700 );
	colors[ImGuiCol_CheckMark]             = U32ToColor( Spectrum::BLUE500 );
	colors[ImGuiCol_SliderGrab]            = U32ToColor( Spectrum::GRAY700 );
	colors[ImGuiCol_SliderGrabActive]      = U32ToColor( Spectrum::GRAY800 );
	colors[ImGuiCol_Button]                = U32ToColor( Spectrum::GRAY75 );
	colors[ImGuiCol_ButtonHovered]         = U32ToColor( Spectrum::GRAY50 );
	colors[ImGuiCol_ButtonActive]          = U32ToColor( Spectrum::GRAY200 );
	colors[ImGuiCol_Header]                = U32ToColor( Spectrum::BLUE400 );
	colors[ImGuiCol_HeaderHovered]         = U32ToColor( Spectrum::BLUE500 );
	colors[ImGuiCol_HeaderActive]          = U32ToColor( Spectrum::BLUE600 );
	colors[ImGuiCol_Separator]             = U32ToColor( Spectrum::GRAY400 );
	colors[ImGuiCol_SeparatorHovered]      = U32ToColor( Spectrum::GRAY600 );
	colors[ImGuiCol_SeparatorActive]       = U32ToColor( Spectrum::GRAY700 );
	colors[ImGuiCol_ResizeGrip]            = U32ToColor( Spectrum::GRAY400 );
	colors[ImGuiCol_ResizeGripHovered]     = U32ToColor( Spectrum::GRAY600 );
	colors[ImGuiCol_ResizeGripActive]      = U32ToColor( Spectrum::GRAY700 );
	colors[ImGuiCol_Tab]                   = U32ToColor( Spectrum::GRAY200 );
	colors[ImGuiCol_TabHovered]            = U32ToColor( Spectrum::GRAY300 );
	colors[ImGuiCol_TabSelected]           = U32ToColor( Spectrum::GRAY100 );
	colors[ImGuiCol_TabDimmed]             = U32ToColor( Spectrum::GRAY300 );
	colors[ImGuiCol_TabDimmedSelected]     = U32ToColor( Spectrum::GRAY200 );
	colors[ImGuiCol_DockingPreview]        = U32ToColor( ( Spectrum::BLUE400 & 0x00FFFFFF ) | 0x99000000 );
	colors[ImGuiCol_DockingEmptyBg]        = U32ToColor( Spectrum::GRAY75 );
	colors[ImGuiCol_PlotLines]             = U32ToColor( Spectrum::BLUE400 );
	colors[ImGuiCol_PlotLinesHovered]      = U32ToColor( Spectrum::BLUE600 );
	colors[ImGuiCol_PlotHistogram]         = U32ToColor( Spectrum::BLUE400 );
	colors[ImGuiCol_PlotHistogramHovered]  = U32ToColor( Spectrum::BLUE600 );
	colors[ImGuiCol_TextSelectedBg]        = U32ToColor( ( Spectrum::BLUE400 & 0x00FFFFFF ) | 0x33000000 );
	colors[ImGuiCol_DragDropTarget]        = ImVec4( 1.00f, 1.00f, 0.00f, 0.90f );
	colors[ImGuiCol_NavHighlight]          = U32ToColor( ( Spectrum::GRAY900 & 0x00FFFFFF ) | 0x0A000000 );
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4( 1.00f, 1.00f, 1.00f, 0.70f );
	colors[ImGuiCol_NavWindowingDimBg]     = ImVec4( 0.80f, 0.80f, 0.80f, 0.20f );
	colors[ImGuiCol_ModalWindowDimBg]      = ImVec4( 0.20f, 0.20f, 0.20f, 0.35f );
}

static inline void Apply( int themeIndex )
{
	if ( themeIndex == 1 ) {
		ApplyLightSpectrum();
	} else {
		ApplyDarkPablo();
	}
}

} /* namespace VkImguiTheme */

#endif /* USE_IMGUI */
