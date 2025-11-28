/*
===========================================================================
Copyright (C) 1999-2005 Id Software, Inc.

This file is part of Quake III Arena source code.

Quake III Arena source code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the License,
or (at your option) any later version.

Quake III Arena source code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Quake III Arena source code; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
===========================================================================
*/

#ifndef __CL_IMGUI_DEBUG_H__
#define __CL_IMGUI_DEBUG_H__

#ifdef USE_CIMGUI

// Debug overlay windows
void CL_ImGui_Debug_ShowPerformanceOverlay(void);
void CL_ImGui_Debug_ShowMemoryOverlay(void);
void CL_ImGui_Debug_ShowNetworkOverlay(void);
void CL_ImGui_Debug_ShowRendererOverlay(void);
void CL_ImGui_Debug_ShowCVarBrowser(void);
void CL_ImGui_Debug_ShowConsoleOverlay(void);
void CL_ImGui_Debug_ShowMainMenu(void);

// Initialize debug overlay system
void CL_ImGui_Debug_Init(void);
void CL_ImGui_Debug_Shutdown(void);

// Render all debug overlays
void CL_ImGui_Debug_RenderAll(void);

#endif // USE_CIMGUI

#endif // __CL_IMGUI_DEBUG_H__

