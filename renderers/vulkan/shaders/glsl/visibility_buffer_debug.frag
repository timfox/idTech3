#version 450

/*
 * Visibility-buffer / material-class debug visualization.
 * mode 1=drawId hash, 2=primId hash, 3=bary RG, 4=class colormap
 */

layout(set = 0, binding = 0) uniform usampler2D visIds;
layout(set = 0, binding = 1) uniform sampler2D baryTex;
layout(set = 0, binding = 2) uniform usampler2D classMap;

layout(push_constant) uniform DebugPC {
	int mode;
} pc;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

vec3 hashColor( uint v ) {
	float r = fract( float( v ) * 0.0019677 );
	float g = fract( float( v >> 8u ) * 0.00389105 );
	float b = fract( float( v >> 16u ) * 0.002713 );
	return vec3( r, g, b );
}

vec3 classColor( uint c ) {
	if ( c == 0u ) return vec3( 0.05, 0.05, 0.08 );
	if ( c == 1u ) return vec3( 0.35, 0.55, 0.85 ); /* simple opaque */
	if ( c == 2u ) return vec3( 0.85, 0.55, 0.20 ); /* layered */
	if ( c == 3u ) return vec3( 0.25, 0.85, 0.75 ); /* transmission */
	if ( c == 4u ) return vec3( 0.95, 0.90, 0.35 ); /* emissive */
	if ( c == 5u ) return vec3( 0.75, 0.30, 0.70 ); /* alpha test */
	if ( c == 6u ) return vec3( 0.55, 0.85, 0.40 ); /* transparent fwd */
	return vec3( 1.0, 0.0, 1.0 );
}

void main() {
	ivec2 pix = ivec2( textureSize( visIds, 0 ) * frag_tex_coord );

	if ( pc.mode == 3 ) {
		vec2 b = texture( baryTex, frag_tex_coord ).rg;
		out_color = vec4( b, 0.0, 1.0 );
		return;
	}
	if ( pc.mode == 4 ) {
		uint c = texelFetch( classMap, pix, 0 ).r;
		out_color = vec4( classColor( c ), 1.0 );
		return;
	}
	if ( pc.mode == 2 ) {
		uvec2 ids = texelFetch( visIds, pix, 0 ).rg;
		out_color = vec4( hashColor( ids.g ), 1.0 );
		return;
	}

	/* mode 1 default: drawId */
	uvec2 ids = texelFetch( visIds, pix, 0 ).rg;
	if ( ids.r == 0xFFFFFFFFu ) {
		out_color = vec4( 0.02, 0.02, 0.04, 1.0 );
		return;
	}
	out_color = vec4( hashColor( ids.r ), 1.0 );
}
