#version 450
#extension GL_ARB_gpu_shader_fp64 : enable

layout(push_constant) uniform PushConstants {
	dmat4 mvp;
	float pointSize;
} pc;

layout(location = 0) in dvec3 inPos;
layout(location = 1) in dvec3 inColor;

layout(location = 0) out flat dvec3 fragColor;

out gl_PerVertex {
	vec4 gl_Position;
	float gl_PointSize;
};

void main() {
	dvec4 clip = pc.mvp * dvec4(inPos, 1.0);
	gl_Position = vec4(clip);
	fragColor = inColor;
	gl_PointSize = max(pc.pointSize, 1.0);
}
