#version 450

layout(set = 0, binding = 0) uniform sampler2D worldColor;
layout(set = 1, binding = 0) uniform sampler2D resolvedWeapon;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

void main() {
	vec4 weapon = textureLod(resolvedWeapon, frag_tex_coord, 0.0);
	vec3 world = textureLod(worldColor, frag_tex_coord, 0.0).rgb;
	out_color = vec4(mix(world, weapon.rgb, clamp(weapon.a, 0.0, 1.0)), 1.0);
}
