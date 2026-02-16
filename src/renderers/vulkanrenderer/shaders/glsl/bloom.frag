#version 450

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

//layout(constant_id = 0) const float gamma = 1.0;
//layout(constant_id = 1) const float obScale = 2.0;
//layout(constant_id = 2) const float greyscale = 0.0;
layout(constant_id = 3) const float threshold = 0.6;
//layout(constant_id = 4) const float factor = 0.5;
layout(constant_id = 5) const int extract_mode = 0;
layout(constant_id = 6) const int base_modulate = 0;

//const vec3 sRGB = { 0.2126, 0.7152, 0.0722 };

void main() {
	const vec3 luma = vec3( 0.2126, 0.7152, 0.0722 );
	vec3 base = texture( texture0, frag_tex_coord ).rgb;
	float metric;

	if ( base_modulate != 0 ) {
		if ( base_modulate == 1 ) {
			base *= base;
		} else {
			base *= dot( luma, base );
		}
	}

	if ( extract_mode == 1 ) { // average RGB
		metric = ( base.r + base.g + base.b ) * 0.33333333;
	} else if ( extract_mode == 2 ) { // luma
		metric = dot( luma, base );
	} else { // max channel
		metric = max( base.r, max( base.g, base.b ) );
	}

	// Soft-knee extraction avoids hard threshold edges that show up as block artifacts.
	float knee = max( threshold * 0.5, 1e-5 );
	float soft = clamp( ( metric - threshold + knee ) / ( 2.0 * knee ), 0.0, 1.0 );
	float hard = max( metric - threshold, 0.0 );
	float contrib = ( hard + soft * soft * knee ) / max( metric, 1e-5 );

	out_color = vec4( base * contrib, 1.0 );
}
