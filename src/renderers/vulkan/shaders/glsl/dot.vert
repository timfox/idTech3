#version 450

// 128 bytes
layout(push_constant) uniform Transform {
	mat4 mvp;
};

layout(set = 0, binding = 0) buffer SSBO {
	int sampled;
};

layout(location = 0) in vec2 in_position;
layout(location = 1) in vec2 in_tex_coord0;
layout(location = 2) in vec4 in_color;

out gl_PerVertex {
	vec4 gl_Position;
	float gl_PointSize;
};

void main() {
	sampled = 0;
	gl_Position = mvp * vec4(in_position, 0.0, 1.0);
	gl_PointSize = 1.0;
}
