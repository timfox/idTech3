#version 450

layout(location = 0) out vec4 fragColor;

void main() {
    // Simple fog effect - semi-transparent gray
    fragColor = vec4(0.5, 0.5, 0.5, 0.7);
}