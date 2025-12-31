#version 450

layout(location = 0) in vec4 position;
layout(location = 1) in vec2 texCoord;

layout(location = 0) out vec2 v_texCoord;

void main() {
    gl_Position = position;
    v_texCoord = texCoord;
}