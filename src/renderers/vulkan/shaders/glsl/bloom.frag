#version 450

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 3) const float threshold = 0.6;
layout(constant_id = 5) const int extract_mode = 0;
layout(constant_id = 6) const int base_modulate = 0;
layout(constant_id = 12) const float knee = 0.5;

const vec3 sRGB = vec3( 0.2126, 0.7152, 0.0722 );

/* Soft threshold: smooth transition from threshold to threshold+knee (Karis/UE4 style). */
float softWeight( float v ) {
	float k = max( knee, 0.001 );
	return smoothstep( threshold, threshold + k, v );
}

void main() {
	vec3 base = textureLod( texture0, frag_tex_coord, 0.0 ).rgb;

	float weight;
	if ( extract_mode == 1 ) {
		weight = softWeight( ( base.r + base.g + base.b ) * 0.33333333 );
	} else if ( extract_mode == 2 ) {
		weight = softWeight( dot( sRGB, base ) );
	} else {
		float brightest = max( max( base.r, base.g ), base.b );
		weight = softWeight( brightest );
	}

	if ( weight > 0.0 ) {
		if ( base_modulate != 0 ) {
			if ( base_modulate == 1 )
				base *= base;
			else
				base *= dot( sRGB, base );
		}
		out_color = vec4( base * weight, 1.0 );
	} else {
		out_color = vec4( 0.0 );
	}
}
