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

#ifndef __GLW_LINUX_H__
#define __GLW_LINUX_H__

#ifdef USE_LOCAL_HEADERS
#	include "SDL.h"
#else
#	include <SDL.h>
#endif

#include "../renderercommon/tr_public.h"

//#define USE_JOYSTICK

typedef struct
{
	FILE *log_fp;

	qboolean isFullscreen;

	glconfig_t *config; // feedback to renderer module

	int desktop_width;
	int desktop_height;

	int window_width;
	int window_height;

	int monitorCount;

} glwstate_t;

extern SDL_Window *SDL_window;
extern glwstate_t glw_state;

extern cvar_t *in_nograb;

void IN_Init( void );
void IN_Shutdown( void );
void IN_Frame( void );

// signals.c
void InitSig( void );

// GLimp functions
void GLimp_Init( glconfig_t *config );
void GLimp_Shutdown( qboolean unloadDLL );
void GLimp_Minimize( void );
void GLimp_LogComment( const char *comment );
void GLimp_EndFrame( void );
void *GL_GetProcAddress( const char *symbol );
void GLW_HideFullscreenWindow( void );
void GLW_RestoreGamma( void );

// Vulkan functions
#ifdef USE_VULKAN
void VKimp_Init( glconfig_t *config );
void VKimp_Shutdown( qboolean unloadDLL );
void *VK_GetInstanceProcAddr( VkInstance instance, const char *name );
qboolean VK_CreateSurface( VkInstance instance, VkSurfaceKHR *surface );
#endif

#endif
