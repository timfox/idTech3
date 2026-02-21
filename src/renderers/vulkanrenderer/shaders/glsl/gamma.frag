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
layout(constant_id = 13) const int tonemap_mode = 2;
layout(constant_id = 14) const int apply_srgb_gamma = 0;
layout(constant_id = 15) const int post_debug = 0;
layout(constant_id = 36) const int postprocess_enabled = 1;

layout(push_constant) uniform PaniniPC {
	mat4 invProj;
	float paniniAmount;
	float paniniD;
	float paniniS;
	float paniniThetaDeg;
	float paniniBorderMode;
	float paniniDebugMode;
	float brightness;
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

vec3 ACESFilm( vec3 x ) {
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

vec3 linearToDisplay( vec3 x ) {
	return pow( max( x, vec3( 0.0 ) ), vec3( max( gamma, 1e-6 ) ) );
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

bool finite2( vec2 v ) {
	return !( isnan( v.x ) || isnan( v.y ) || isinf( v.x ) || isinf( v.y ) );
}

vec3 reconstructRay( vec2 uv, float fovYRadians, float aspect ) {
	vec2 ndc = uv * 2.0 - 1.0;
	float tanHalfFovY = tan( 0.5 * fovYRadians );

	vec3 ray;
	ray.x = ndc.x * aspect * tanHalfFovY;
	ray.y = ndc.y * tanHalfFovY;
	ray.z = -1.0;

	float invLen = inversesqrt( max( dot( ray, ray ), 1e-8 ) );
	return ray * invLen;
}

vec2 paniniProjectStable(
	vec3 dir,
	float d,
	float s,
	float fovYRadians,
	float aspect )
{
	float safeD = max( d, 0.001 );
	float safeS = clamp( s, 0.0, 1.0 );
	float thetaMax = atan( tan( fovYRadians * 0.5 ) * aspect );
	float phiMax = fovYRadians * 0.5;

	float xzLen = max( length( dir.xz ), 1e-6 );
	float theta = atan( dir.x, -dir.z );
	float phi = atan( dir.y, xzLen );

	theta = clamp( theta, -thetaMax, thetaMax );
	phi = clamp( phi, -phiMax, phiMax );

	float denom = max( safeD + cos( theta ), 1e-4 );
	float k = ( safeD + 1.0 ) / denom;

	vec2 p;
	p.x = k * sin( theta );
	p.y = tan( phi );
	p.y *= mix( 1.0, k, safeS );

	float denomMax = max( safeD + cos( thetaMax ), 1e-4 );
	float kMax = ( safeD + 1.0 ) / denomMax;

	float fitX = kMax * sin( thetaMax );
	float fitY = tan( phiMax ) * mix( 1.0, kMax, safeS );
	float fit = max( max( abs( fitX ), abs( fitY ) ), 1e-4 );

	return p / fit;
}

vec3 doTonemap( vec3 value ) {
	if ( tonemap_mode == 2 ) {
		return ACESFilm( value );
	} else if ( tonemap_mode == 1 ) {
		return Tonemap_Reinhard( value );
	}
	return value;
}

void main() {
	vec2 uv = frag_tex_coord;
	float paniniAmount = clamp( paniniPC.paniniAmount, 0.0, 1.0 );
	int borderMode = int( clamp( floor( paniniPC.paniniBorderMode + 0.5 ), 0.0, 1.0 ) );
	int paniniDebug = int( clamp( floor( paniniPC.paniniDebugMode + 0.5 ), 0.0, 1.0 ) );

	if ( paniniAmount > 0.0001 ) {
		ivec2 texSize = textureSize( texture0, 0 );
		float aspect = float( texSize.x ) / max( float( texSize.y ), 1.0 );
		float fovY = radians( paniniPC.paniniThetaDeg );
		vec3 dir = reconstructRay( uv, fovY, aspect );

		vec2 persp = dir.xy / max( -dir.z, 1e-4 );
		vec2 panini = paniniProjectStable(
			dir,
			paniniPC.paniniD,
			paniniPC.paniniS,
			fovY,
			aspect );
		vec2 proj = mix( persp, panini, paniniAmount );
		bool projInvalid = !finite2( proj );

		if ( projInvalid ) {
			if ( paniniDebug != 0 ) {
				out_color = vec4( 1.0, 0.0, 1.0, 1.0 );
				return;
			}
			out_color = vec4( 0.0, 0.0, 0.0, 1.0 );
			return;
		}

		vec2 uv2 = proj * 0.5 + 0.5;
		bool oob = any( lessThan( uv2, vec2( 0.0 ) ) ) || any( greaterThan( uv2, vec2( 1.0 ) ) );
		if ( oob ) {
			if ( borderMode == 0 ) {
				if ( paniniDebug != 0 ) {
					out_color = vec4( 1.0, 0.4, 0.0, 1.0 );
					return;
				}
				out_color = vec4( 0.0, 0.0, 0.0, 1.0 );
				return;
			}
			if ( paniniDebug != 0 ) {
				out_color = vec4( 0.0, 1.0, 1.0, 1.0 );
				return;
			}
			uv = clamp( uv2, 0.0, 1.0 );
		} else {
			uv = uv2;
		}
	}

	vec3 hdr = texture( texture0, uv ).rgb;
	vec3 hdr_exposed = hdr * max( paniniPC.brightness, 0.0 );
	if ( postprocess_enabled != 0 ) {
		hdr_exposed *= exposure;
		hdr_exposed *= preExposureScale;
		hdr_exposed = applyBloomKnee( hdr_exposed );
	}

	vec3 tonemapped = hdr_exposed;
	if ( postprocess_enabled != 0 ) {
		tonemapped = doTonemap( tonemapped );
	}

	vec3 ldr;
	if ( postprocess_enabled != 0 && post_debug != 0 ) {
		if ( post_debug == 1 ) {
			ldr = clamp( hdr_exposed, 0.0, 10.0 ) * 0.1;
		} else {
			float lum = max( max( hdr_exposed.r, hdr_exposed.g ), hdr_exposed.b );
			float logLum = log2( max( lum, 1e-4 ) );
			float heat = clamp( ( logLum + 6.0 ) / 8.0, 0.0, 1.0 );
			ldr = vec3( heat, clamp( heat * 0.25, 0.0, 1.0 ), 1.0 - heat );
		}
	} else {
		ldr = clamp( tonemapped, 0.0, 1.0 );
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

	if ( apply_srgb_gamma != 0 ) {
		ldr = linearToDisplay( ldr );
	}

	out_color = vec4( ldr, 1.0 );
}
