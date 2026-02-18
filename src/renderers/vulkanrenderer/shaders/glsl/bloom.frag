#version 450

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 3) const float threshold = 1.0;
layout(constant_id = 5) const float knee = 0.5;

void main() {
	vec3 tex = texture(texture0, frag_tex_coord).rgb;
	float brightness = max(max(tex.r, tex.g), tex.b);
	float soft = smoothstep(threshold - knee, threshold + knee, brightness);
	out_color = vec4(tex * soft, 1.0);
}
