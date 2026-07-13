#version 450

layout(set = 0, binding = 0) uniform sampler2D gbufTex;

layout(push_constant) uniform DebugPC {
	int mode;
} pc;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

void main() {
	vec4 s = texture( gbufTex, frag_tex_coord );

	if ( pc.mode == 2 ) {
		out_color = vec4( texture( gbufTex, frag_tex_coord ).xyz * 0.5 + 0.5, 1.0 );
		return;
	}
	if ( pc.mode == 3 ) {
		out_color = vec4( s.r, s.g, s.b, 1.0 );
		return;
	}
	if ( pc.mode == 4 ) {
		out_color = vec4( s.rgb, 1.0 );
		return;
	}
	if ( pc.mode == 5 ) {
		float confidence = clamp( s.a, 0.0, 1.0 );
		out_color = vec4( 1.0 - confidence, confidence, 0.0, 1.0 );
		return;
	}
	if ( pc.mode == 6 ) {
		vec2 motion = s.rg;
		vec2 direction = clamp( motion * 32.0 + 0.5, 0.0, 1.0 );
		float magnitude = clamp( length( motion ) * 64.0, 0.0, 1.0 );
		out_color = vec4( direction, magnitude, 1.0 );
		return;
	}

	out_color = vec4( s.rgb, 1.0 );
}
