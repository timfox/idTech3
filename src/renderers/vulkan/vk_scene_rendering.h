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

#ifndef VK_SCENE_RENDERING_H
#define VK_SCENE_RENDERING_H

// Scene Rendering Functions for Vulkan Renderer
void vk_add_entity(const refEntity_t *re, int intShaderTime);
void vk_add_polygon(qhandle_t hShader, int numVerts, const polyVert_t *verts, int num);
void vk_clear_scene(void);
void vk_render_scene_vulkan(const refdef_t *fd);

#endif // VK_SCENE_RENDERING_H