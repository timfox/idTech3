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
layout(constant_id = 16) const float bloom_knee = 0.0;

const vec3 luma_weights = vec3( 0.2126, 0.7152, 0.0722 );

// Compute a brightness metric from the color based on the extraction mode.
float computeBrightness( in vec3 color )
{
	if ( extract_mode == 1 )
	{
		return ( color.r + color.g + color.b ) * 0.33333333;
	}
	else if ( extract_mode == 2 )
	{
		return dot( luma_weights, color );
	}
	return max( color.r, max( color.g, color.b ) );
}

// Soft-knee bloom extraction.
// knee=0: original hard threshold (step function).
// knee>0: smoothstep transition from (threshold-soft) to threshold,
//         eliminating the harsh pop-in of bloom on bright surfaces.
float bloomWeight( in float brightness )
{
	if ( bloom_knee <= 0.0 )
	{
		return brightness >= threshold ? 1.0 : 0.0;
	}
	float soft = max( threshold * bloom_knee, 1e-5 );
	return smoothstep( threshold - soft, threshold + soft, brightness );
}

void main() {
	vec3 base = texture(texture0, frag_tex_coord).rgb;

	float brightness = computeBrightness( base );
	float weight = bloomWeight( brightness );

	if ( weight > 0.0 )
	{
		if ( base_modulate != 0 )
		{
			if ( base_modulate == 1 )
			{
				base *= base;
			}
			else
			{
				base *= dot( luma_weights, base );
			}
		}
		out_color = vec4( base * weight, 1.0 );
	}
	else
	{
		out_color = vec4( 0.0 );
	}
}
