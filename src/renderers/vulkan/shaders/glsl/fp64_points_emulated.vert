#version 450

layout(push_constant) uniform PushConstants {
	mat4 mvp;
	float pointSize;
} pc;

layout(location = 0) in vec3 highPos;
layout(location = 1) in vec3 lowPos;
layout(location = 2) in vec3 highColor;
layout(location = 3) in vec3 lowColor;

layout(location = 0) out vec3 outHighColor;
layout(location = 1) out vec3 outLowColor;

out gl_PerVertex {
	vec4 gl_Position;
	float gl_PointSize;
};

void main() {
	outHighColor = highColor;
	outLowColor = lowColor;
	gl_Position = pc.mvp * vec4(highPos + lowPos, 1.0);
	gl_PointSize = max(pc.pointSize, 1.0);
}
