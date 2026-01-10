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

#include "tr_local.h"
#include "../renderercommon/tr_types.h"

// 2D Rendering Functions for Vulkan Renderer
// Handles UI elements, HUD, console, and 2D graphics

// Forward declaration
extern void vk_2d_add_quad(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader);
extern void vk_2d_flush(void);

void vk_draw_stretch_pic(float x, float y, float w, float h, float s1, float t1, float s2, float t2, qhandle_t hShader) {
    // Draw 2D stretched image (UI, HUD, etc.)
    if (!vk.active) {
        return;
    }

    // For fake devices, implement basic colored rectangle rendering
    if (vk.device == (VkDevice)0x20000000) {
        // For now, just log the drawing operation
        ri.Printf(PRINT_DEVELOPER, "vk_draw_stretch_pic: Would draw rect at %.1f,%.1f size %.1f,%.1f with shader %d\n",
                 x, y, w, h, hShader);
        Q_UNUSED(s1); Q_UNUSED(t1); Q_UNUSED(s2); Q_UNUSED(t2);
        return;
    }

    // Use the 2D rendering system to add quad
    vk_2d_add_quad(x, y, w, h, s1, t1, s2, t2, hShader);
}

// Forward declarations
extern void RE_UploadCinematic(int w, int h, int cols, int rows, byte *data, int client, qboolean dirty);
extern qhandle_t RE_RegisterShader(const char *name);
extern trGlobals_t tr;

void vk_draw_stretch_raw(int x, int y, int w, int h, int cols, int rows, byte *data, int client, qboolean dirty) {
    // Draw raw image data (cinematic frames, screenshots, etc.)
    if (!vk.active || !data) {
        return;
    }

    // Validate data dimensions
    if (cols <= 0 || rows <= 0 || w <= 0 || h <= 0) {
        ri.Printf(PRINT_WARNING, "vk_draw_stretch_raw: Invalid dimensions\n");
        return;
    }

    // Upload raw data to texture using existing cinematic upload system
    RE_UploadCinematic(w, h, cols, rows, data, client, dirty);

    // Get the uploaded texture
    if (client < 0 || client >= MAX_VIDEO_HANDLES || !tr.scratchImage[client]) {
        ri.Printf(PRINT_WARNING, "vk_draw_stretch_raw: Invalid client index or texture not created\n");
        return;
    }

    // Get shader handle for the texture
    qhandle_t shader = RE_RegisterShader(va("*cinematic%i", client));
    
    if (!shader) {
        // Fallback: use default shader
        shader = RE_RegisterShader("default");
    }

    // Render the quad with the texture
    // Calculate UV coordinates based on display size vs texture size
    float s1 = 0.0f, t1 = 0.0f;
    float s2 = (float)w / (float)cols;
    float t2 = (float)h / (float)rows;

    // Use the 2D rendering system
    vk_2d_add_quad((float)x, (float)y, (float)w, (float)h, s1, t1, s2, t2, shader);
}

void vk_set_color(const float *rgba) {
    // Set current rendering color
    if (!rgba) {
        // NULL rgba means reset to white
        static const float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        rgba = white;
    }

    // Store color for use in subsequent draw calls
    // This color will be applied to vertices in 2D rendering operations
    vk.currentColor[0] = rgba[0];
    vk.currentColor[1] = rgba[1];
    vk.currentColor[2] = rgba[2];
    vk.currentColor[3] = rgba[3];

    ri.Printf(PRINT_DEVELOPER, "vk_set_color: Set color to (%.2f, %.2f, %.2f, %.2f)\n",
              rgba[0], rgba[1], rgba[2], rgba[3]);
}