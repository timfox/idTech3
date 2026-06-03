#version 450
#extension GL_ARB_gpu_shader_fp64 : enable

layout(location = 0) in flat dvec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
	outColor = vec4(fragColor, 1.0);
}
