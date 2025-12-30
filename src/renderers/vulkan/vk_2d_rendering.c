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

// 2D Rendering Functions for Vulkan Renderer
// Handles UI elements, HUD, console, and 2D graphics

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

    // Implement real 2D quad rendering with texture for real devices
    ri.Printf(PRINT_DEVELOPER, "vk_draw_stretch_pic: Drawing stretched pic with shader %d at (%.1f,%.1f) %.1fx%.1f\n", hShader, x, y, w, h);

    // For real Vulkan devices, implement proper 2D quad rendering
    // TODO: Implement actual Vulkan draw commands here with proper pipeline
    ri.Printf(PRINT_DEVELOPER, "2D Quad: pos(%.1f,%.1f) size(%.1f,%.1f) uv(%.3f,%.3f,%.3f,%.3f) color(%.2f,%.2f,%.2f,%.2f)\n",
              x, y, w, h, s1, t1, s2, t2,
              vk.currentColor[0], vk.currentColor[1], vk.currentColor[2], vk.currentColor[3]);

    // For now, we'll defer this to the immediate rendering system
    // This would queue the quad for rendering in the next frame
}

void vk_draw_stretch_raw(int x, int y, int w, int h, int cols, int rows, byte *data, int client, qboolean dirty) {
    // Draw raw image data (cinematic frames, screenshots, etc.)
    if (!vk.active || !data) {
        return;
    }

    // Implement raw RGBA data rendering
    // This function is typically used for:
    // - Cinematic playback frames
    // - Screenshot display
    // - Video playback

    ri.Printf(PRINT_DEVELOPER, "vk_draw_stretch_raw: Rendering raw data at (%d,%d) size (%dx%d), cols=%d rows=%d, client=%d, dirty=%d\n",
              x, y, w, h, cols, rows, client, dirty ? 1 : 0);

    // Validate data dimensions
    if (cols <= 0 || rows <= 0 || w <= 0 || h <= 0) {
        ri.Printf(PRINT_WARNING, "vk_draw_stretch_raw: Invalid dimensions\n");
        return;
    }

    // Calculate expected data size (RGBA = 4 bytes per pixel)
    size_t expected_size = (size_t)cols * (size_t)rows * 4;
    // Note: We can't easily validate data size without knowing the actual buffer size

    // For Vulkan implementation, this would:
    // 1. Create or update a texture with the raw RGBA data
    // 2. Set up appropriate sampler (likely linear filtering)
    // 3. Render a quad with the texture applied
    // 4. Handle proper aspect ratio and positioning

    // Convert screen coordinates to normalized device coordinates
    float ndc_x = ((float)x / glConfig.vidWidth) * 2.0f - 1.0f;
    float ndc_y = ((float)y / glConfig.vidHeight) * 2.0f - 1.0f;
    float ndc_w = ((float)w / glConfig.vidWidth) * 2.0f;
    float ndc_h = ((float)h / glConfig.vidHeight) * 2.0f;

    // Log rendering operation
    ri.Printf(PRINT_DEVELOPER, "Raw data quad: NDC pos(%.3f,%.3f) size(%.3f,%.3f), texture(%dx%d)\n",
              ndc_x, ndc_y, ndc_w, ndc_h, cols, rows);

    // TODO: Implement actual Vulkan texture upload and rendering
    // This would involve staging buffer operations and texture updates

    Q_UNUSED(client); Q_UNUSED(dirty); // These parameters might be used for caching logic
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