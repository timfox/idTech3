#version 450

layout(set = 0, binding = 0) uniform sampler2D sceneBaseTex;
layout(set = 0, binding = 1) uniform sampler2D litTex;

layout(push_constant) uniform CompositePC {
	uint additive;
} pc;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

void main() {
	vec3 dynamic = texture( litTex, frag_tex_coord ).rgb;
	if ( pc.additive != 0u ) {
		vec3 base = texture( sceneBaseTex, frag_tex_coord ).rgb;
		out_color = vec4( base + dynamic, 1.0 );
		return;
	}
	out_color = vec4( dynamic, 1.0 );
}
