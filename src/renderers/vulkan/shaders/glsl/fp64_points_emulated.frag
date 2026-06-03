#version 450

layout(location = 0) in vec3 outHighColor;
layout(location = 1) in vec3 outLowColor;
layout(location = 0) out vec4 fragColor;

void main() {
	vec3 fullColor = outHighColor + outLowColor;
	fragColor = vec4(fullColor, 1.0);
}
