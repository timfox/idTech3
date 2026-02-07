#version 450

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

layout(set = 0, binding = 0) uniform samplerCube env_texture;

const float PI = 3.14159265358979323846;

void main() {
	vec2 uv = frag_tex_coord;
	float phi = uv.x * 2.0 * PI;
	float theta = (uv.y * 2.0 - 1.0) * (PI * 0.5);
	float cosTheta = cos(theta);

	vec3 dir = vec3(
		cosTheta * cos(phi),
		sin(theta),
		cosTheta * sin(phi)
	);

	out_color = vec4(texture(env_texture, dir).rgb, 1.0);
}
