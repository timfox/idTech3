#version 450

// 2D Rendering Vertex Shader
// Handles 2D quad rendering with texture coordinates and colors

layout(location = 0) in vec2 in_position;     // Position (x, y)
layout(location = 1) in vec2 in_tex_coord;    // Texture coordinates (u, v)
layout(location = 2) in vec4 in_color;        // Color (r, g, b, a)

layout(location = 0) out vec2 frag_tex_coord;
layout(location = 1) out vec4 frag_color;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    // Convert screen coordinates to NDC (-1 to 1)
    // Assuming input coordinates are in pixels
    vec2 ndc_pos = (in_position * 2.0 - vec2(800.0, 600.0)) / vec2(800.0, 600.0);

    gl_Position = vec4(ndc_pos, 0.0, 1.0);
    frag_tex_coord = in_tex_coord;
    frag_color = in_color;
}