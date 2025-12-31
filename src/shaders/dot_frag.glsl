#version 450

layout(location = 0) in vec2 v_texCoord;

layout(location = 0) out vec4 fragColor;

void main() {
    // Simple white dot for testing
    fragColor = vec4(1.0, 1.0, 1.0, 1.0);
}