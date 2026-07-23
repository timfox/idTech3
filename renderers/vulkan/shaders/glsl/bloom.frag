#version 450

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 3) const float threshold = 0.6;
layout(constant_id = 5) const int extract_mode = 0;
layout(constant_id = 6) const int base_modulate = 0;
layout(constant_id = 12) const float knee = 0.5;
/* IQ P1-C firefly suppression (bloom extract only — SceneHDR untouched). */
layout(constant_id = 29) const int firefly_clamp = 1;
layout(constant_id = 30) const float firefly_ratio = 4.0;
layout(constant_id = 31) const float firefly_absolute = 0.25;
layout(constant_id = 32) const int firefly_neighborhood = 1;
layout(constant_id = 33) const int firefly_debug = 0;

const vec3 sRGB = vec3( 0.2126, 0.7152, 0.0722 );

float luma( vec3 c ) {
	return max( dot( sRGB, c ), 0.0 );
}

/* Soft threshold: smooth transition from threshold to threshold+knee (Karis/UE4 style). */
float softWeight( float v ) {
	float k = max( knee, 0.001 );
	return smoothstep( threshold, threshold + k, v );
}

/* Robust local luminance: cross or 3×3 (median-of-sort / trimmed mean approximation). */
float robustNeighborhoodLuma( vec2 uv ) {
	vec2 texel = 1.0 / vec2( textureSize( texture0, 0 ) );
	float samples[9];
	int n = 0;

	if ( firefly_neighborhood <= 0 ) {
		/* Cross: center + 4-neighbors. */
		samples[n++] = luma( textureLod( texture0, uv, 0.0 ).rgb );
		samples[n++] = luma( textureLod( texture0, uv + vec2( texel.x, 0.0 ), 0.0 ).rgb );
		samples[n++] = luma( textureLod( texture0, uv - vec2( texel.x, 0.0 ), 0.0 ).rgb );
		samples[n++] = luma( textureLod( texture0, uv + vec2( 0.0, texel.y ), 0.0 ).rgb );
		samples[n++] = luma( textureLod( texture0, uv - vec2( 0.0, texel.y ), 0.0 ).rgb );
	} else {
		for ( int y = -1; y <= 1; y++ ) {
			for ( int x = -1; x <= 1; x++ ) {
				samples[n++] = luma( textureLod( texture0, uv + vec2( float( x ), float( y ) ) * texel, 0.0 ).rgb );
			}
		}
	}

	/* Insertion sort first n samples; take median (or trimmed mean for mode 2). */
	for ( int i = 1; i < n; i++ ) {
		float key = samples[i];
		int j = i - 1;
		while ( j >= 0 && samples[j] > key ) {
			samples[j + 1] = samples[j];
			j--;
		}
		samples[j + 1] = key;
	}

	if ( firefly_neighborhood >= 2 && n >= 5 ) {
		/* Trimmed mean: drop lowest and highest. */
		float sum = 0.0;
		for ( int i = 1; i < n - 1; i++ ) {
			sum += samples[i];
		}
		return sum / float( n - 2 );
	}
	return samples[n / 2];
}

vec3 applyFireflyClamp( vec3 source, out float localRef, out float clampedLuma, out float removed ) {
	float centerLuma = luma( source );
	localRef = robustNeighborhoodLuma( frag_tex_coord );
	float allowed = localRef * max( firefly_ratio, 1.0 ) + max( firefly_absolute, 0.0 );
	clampedLuma = min( centerLuma, allowed );
	removed = max( centerLuma - clampedLuma, 0.0 );
	if ( centerLuma > 1e-6 ) {
		return source * ( clampedLuma / centerLuma );
	}
	return vec3( 0.0 );
}

void main() {
	vec3 base = textureLod( texture0, frag_tex_coord, 0.0 ).rgb;
	vec3 original = base;
	float localRef = 0.0;
	float clampedLuma = 0.0;
	float removed = 0.0;

	if ( firefly_clamp != 0 ) {
		base = applyFireflyClamp( base, localRef, clampedLuma, removed );
	} else {
		localRef = luma( base );
		clampedLuma = localRef;
	}

	if ( firefly_debug == 1 ) {
		out_color = vec4( original, 1.0 );
		return;
	}
	if ( firefly_debug == 2 ) {
		out_color = vec4( vec3( localRef ), 1.0 );
		return;
	}
	if ( firefly_debug == 3 ) {
		out_color = vec4( removed > 1e-4 ? vec3( 1.0, 0.2, 0.0 ) : vec3( 0.0 ), 1.0 );
		return;
	}
	if ( firefly_debug == 4 ) {
		out_color = vec4( base, 1.0 );
		return;
	}
	if ( firefly_debug == 5 ) {
		out_color = vec4( vec3( removed ), 1.0 );
		return;
	}

	float weight;
	if ( extract_mode == 1 ) {
		weight = softWeight( ( base.r + base.g + base.b ) * 0.33333333 );
	} else if ( extract_mode == 2 ) {
		weight = softWeight( dot( sRGB, base ) );
	} else {
		float brightest = max( max( base.r, base.g ), base.b );
		weight = softWeight( brightest );
	}

	if ( firefly_debug == 6 ) {
		out_color = vec4( base * weight, 1.0 );
		return;
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
