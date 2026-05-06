/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.
===========================================================================
*/

#ifdef USE_IMGUI

#include "vk_imgui_common.hpp"

float VkImgui_CvarFloat( const char *name )
{
	const char *s = ri.Cvar_VariableString( name );
	return s && s[0] ? (float)Q_atof( s ) : 0.0f;
}

void VkImgui_CvarSlider( const char *label, const char *cvar, float v, float vMin, float vMax, const char *fmt )
{
	if ( ImGui::SliderFloat( label, &v, vMin, vMax, fmt ) ) {
		ri.Cvar_SetValue( cvar, v );
	}
}

#endif /* USE_IMGUI */
