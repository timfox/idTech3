// Vertex shader for SDF vector texture rendering
// Simple pass-through shader for 2D rendering

#version 450

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec2 a_texCoord;

layout(location = 0) out vec2 v_texCoord;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(a_position, 1.0);
    v_texCoord = a_texCoord;
}
