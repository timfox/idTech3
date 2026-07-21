#version 450
/* Dedicated weapon bloom extract: bright weapon-class pixels only. */

layout(set = 0, binding = 0) uniform sampler2D colorTex;
layout(set = 1, binding = 0) uniform sampler2D classTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

layout(constant_id = 3) const float threshold = 0.55;
layout(constant_id = 12) const float knee = 0.45;

const float CLASS_WEAPON_THRESH = 0.5;
const vec3 sRGB = vec3(0.2126, 0.7152, 0.0722);

float softWeight(float v) {
	float k = max(knee, 0.001);
	return smoothstep(threshold, threshold + k, v);
}

void main() {
	float weapon = textureLod(classTex, frag_tex_coord, 0.0).r;
	if (weapon <= CLASS_WEAPON_THRESH) {
		out_color = vec4(0.0);
		return;
	}
	vec3 base = textureLod(colorTex, frag_tex_coord, 0.0).rgb;
	float weight = softWeight(dot(sRGB, base));
	out_color = weight > 0.0 ? vec4(base * weight, 1.0) : vec4(0.0);
}
