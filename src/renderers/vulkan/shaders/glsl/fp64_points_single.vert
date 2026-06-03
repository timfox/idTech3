#version 450

layout(push_constant) uniform PushConstants {
	mat4 mvp;
	float pointSize;
} pc;

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inColor;

layout(location = 0) out vec3 fragColor;

out gl_PerVertex {
	vec4 gl_Position;
	float gl_PointSize;
};

void main() {
	gl_Position = pc.mvp * vec4(inPos, 1.0);
	fragColor = inColor;
	gl_PointSize = max(pc.pointSize, 1.0);
}
