#version 450

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 0) const float gamma = 1.0;
layout(constant_id = 1) const float preExposureScale = 1.0;
layout(constant_id = 2) const float greyscale = 0.0;
layout(constant_id = 3) const float bloom_threshold = 0.6;
layout(constant_id = 4) const float bloom_intensity = 0.5;
layout(constant_id = 5) const int bloom_threshold_mode = 0;
layout(constant_id = 6) const int bloom_modulate = 0;
layout(constant_id = 7) const int ditherMode = 0;
layout(constant_id = 8) const int depth_r = 255;
layout(constant_id = 9) const int depth_g = 255;
layout(constant_id = 10) const int depth_b = 255;
layout(constant_id = 11) const float exposure = 1.0;
layout(constant_id = 12) const float bloom_knee = 0.5;
layout(constant_id = 13) const int tonemap_mode = 1;
layout(constant_id = 14) const int apply_srgb_gamma = 0;
layout(constant_id = 15) const int post_debug = 0;
layout(constant_id = 36) const int postprocess_enabled = 1;

layout(push_constant) uniform PaniniPC {
	float aspect;
	float paniniD;
	float paniniS;
	float padding;
} paniniPC;

const vec3 sRGB = vec3( 0.2126, 0.7152, 0.0722 );

const int bayerSize = 8;
const float bayerMatrix[bayerSize * bayerSize] = float[](
	0,  32, 8,  40, 2,  34, 10, 42,
	48, 16, 56, 24, 50, 18, 58, 26,
	12, 44, 4,  36, 14, 46, 6,  38,
	60, 28, 52, 20, 62, 30, 54, 22,
	3,  35, 11, 43, 1,  33, 9,  41,
	51, 19, 59, 27, 49, 17, 57, 25,
	15, 47, 7,  39, 13, 45, 5,  37,
	63, 31, 55, 23, 61, 29, 53, 21
);

float threshold() {
	ivec2 coord = ivec2( gl_FragCoord.xy );
	ivec2 bayerCoord = coord % bayerSize;
	float bayerSample = bayerMatrix[bayerCoord.x + bayerCoord.y * bayerSize];
	return (bayerSample + 0.5) / float( bayerSize * bayerSize );
}

vec3 dither( vec3 color ) {
	ivec3 depth = ivec3( depth_r, depth_g, depth_b );
	vec3 denormalized = color * depth;
	vec3 low = floor( denormalized );
	vec3 frac = denormalized - low;
	vec3 dithered = low + step( threshold(), frac );
	return dithered / depth;
}

vec3 Tonemap_ACES( vec3 x ) {
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return clamp( (x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0 );
}

vec3 Tonemap_Reinhard( vec3 x ) {
	return x / ( x + vec3( 1.0 ) );
}

vec3 applyBloomKnee( vec3 color ) {
	float knee = max( bloom_knee, 0.0 );
	if ( knee <= 0.0 ) {
		return color;
	}

	float brightest = max( max( color.r, color.g ), color.b );
	float factor = smoothstep( knee, knee + 0.5, brightest );
	return mix( color, color * (1.0 - factor * 0.5), factor );
}

vec3 linear_to_srgb( vec3 x ) {
	x = max( x, vec3( 0.0 ) );
	vec3 lo = x * 12.92;
	vec3 hi = 1.055 * pow( x, vec3( 1.0 / 2.4 ) ) - 0.055;
	bvec3 cut = lessThanEqual( x, vec3( 0.0031308 ) );
	return mix( hi, lo, vec3( cut ) );
}

vec2 panini_project( vec2 uv, float aspect, float d, float s ) {
	float safeAspect = max( aspect, 0.0001 );

	vec2 p = uv * 2.0 - 1.0;
	p.x *= safeAspect;

	float x2 = p.x * p.x;
	float y2 = p.y * p.y;

	float invLen = inversesqrt( 1.0 + x2 + y2 );
	float c = ( d + 1.0 ) / ( d + invLen );
	vec2 pp = p * c;

	pp.y = mix( pp.y, pp.y * ( 1.0 + s * ( abs( pp.x ) / safeAspect ) ), s );

	pp.x /= safeAspect;

	return pp * 0.5 + 0.5;
}

void main() {
	vec2 uv = frag_tex_coord;

	if ( paniniPC.paniniD > 0.0001 ) {
		uv = panini_project( uv, paniniPC.aspect, paniniPC.paniniD, paniniPC.paniniS );
	}

	uv = clamp( uv, 0.0, 1.0 );

	vec3 hdr = texture( texture0, uv ).rgb;
	vec3 hdr_exposed = hdr;
	if ( postprocess_enabled != 0 ) {
		hdr_exposed *= exposure;
		hdr_exposed *= preExposureScale;
	}

	vec3 tonemapped = hdr_exposed;
	if ( postprocess_enabled != 0 ) {
		tonemapped = applyBloomKnee( tonemapped );
		if ( tonemap_mode == 2 ) {
			tonemapped = Tonemap_ACES( tonemapped );
		} else if ( tonemap_mode == 1 ) {
			tonemapped = Tonemap_Reinhard( tonemapped );
		}
	}

	vec3 base = tonemapped;
	base = max( base, vec3( 0.0 ) );
	base = pow( base, vec3( gamma ) );
	if ( apply_srgb_gamma != 0 ) {
		base = linear_to_srgb( base );
	}

	vec3 ldr;
	if ( postprocess_enabled != 0 ) {
		if ( post_debug == 1 ) {
			ldr = clamp( hdr_exposed, 0.0, 10.0 ) * 0.1;
		} else if ( post_debug == 2 ) {
			float lum = max( max( hdr_exposed.r, hdr_exposed.g ), hdr_exposed.b );
			float logLum = log2( max( lum, 1e-4 ) );
			float heat = clamp( ( logLum + 6.0 ) / 8.0, 0.0, 1.0 );
			ldr = vec3( heat, clamp( heat * 0.25, 0.0, 1.0 ), 1.0 - heat );
		} else {
			ldr = clamp( base, 0.0, 1.0 );
			if ( greyscale == 1.0 ) {
				ldr = vec3( dot( ldr, sRGB ) );
			} else if ( greyscale != 0.0 ) {
				vec3 luma = vec3( dot( ldr, sRGB ) );
				ldr = mix( ldr, luma, greyscale );
			}
			if ( ditherMode == 1 ) {
				ldr = dither( ldr );
			}
		}
	} else {
		ldr = clamp( base, 0.0, 1.0 );
		if ( ditherMode == 1 ) {
			ldr = dither( ldr );
		}
	}

	out_color = vec4( ldr, 1.0 );
}
