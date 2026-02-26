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
layout(constant_id = 16) const float vignette_intensity = 0.0;
layout(constant_id = 17) const float vignette_radius = 0.75;
layout(constant_id = 18) const float chromatic_aberration = 0.0;
layout(constant_id = 19) const float film_grain = 0.0;
layout(constant_id = 20) const int postprocess_enabled = 1;

layout(push_constant) uniform PaniniPC {
	float paniniAmount;
	float paniniD;
	float paniniS;
	float aspect;
	float fovXDeg;
	float paniniBorderMode;
	float paniniDebugMode;
	float brightness;
	float paniniZoom;
	float paniniPad0;
	float paniniPad1;
	float paniniPad2;
	vec4 srcUVScaleBias; // scale.xy, bias.xy
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

vec2 to_src_uv( vec2 uv01 ) {
	return uv01 * paniniPC.srcUVScaleBias.xy + paniniPC.srcUVScaleBias.zw;
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

float paniniForwardX( float theta, float d ) {
	float denom = max( d + cos( theta ), 1e-6 );
	return ( d + 1.0 ) * sin( theta ) / denom;
}

float paniniForwardXDerivative( float theta, float d ) {
	float denom = max( d + cos( theta ), 1e-6 );
	return ( d + 1.0 ) * ( d * cos( theta ) + 1.0 ) / ( denom * denom );
}

vec3 paniniInverseDir( vec2 uvOut, float d, float s, float fovYRadians, float aspect ) {
	float safeD = max( d, 0.001 );
	float safeS = clamp( s, 0.0, 1.0 );
	float thetaMax = atan( tan( fovYRadians * 0.5 ) * aspect );
	float phiMax = fovYRadians * 0.5;

	float denomMax = max( safeD + cos( thetaMax ), 1e-6 );
	float kMax = ( safeD + 1.0 ) / denomMax;
	float fitX = kMax * sin( thetaMax );
	float fitY = tan( phiMax ) * mix( 1.0, kMax, safeS );
	vec2 fit = vec2(
		max( abs( fitX ), 1e-6 ),
		max( abs( fitY ), 1e-6 ) );

	vec2 pRaw = ( uvOut * 2.0 - 1.0 ) * fit;
	float theta = clamp( pRaw.x, -1.0, 1.0 ) * thetaMax;

	for ( int i = 0; i < 6; ++i ) {
		float f = paniniForwardX( theta, safeD ) - pRaw.x;
		float fp = paniniForwardXDerivative( theta, safeD );
		float safeFp = abs( fp ) > 1e-6 ? fp : ( fp < 0.0 ? -1e-6 : 1e-6 );
		theta = clamp( theta - f / safeFp, -thetaMax, thetaMax );
	}

	float denom = max( safeD + cos( theta ), 1e-6 );
	float k = ( safeD + 1.0 ) / denom;
	float m = max( mix( 1.0, k, safeS ), 1e-6 );
	float phi = clamp( atan( pRaw.y / m ), -phiMax, phiMax );

	vec3 dir;
	dir.x = sin( theta );
	dir.z = -cos( theta );
	dir.y = tan( phi ) * max( length( dir.xz ), 1e-6 );
	float invLen = inversesqrt( max( dot( dir, dir ), 1e-8 ) );
	return dir * invLen;
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
	int debugMode = post_debug;
	bool debugInverse = ( postprocess_enabled != 0 ) && ( debugMode == 97 || debugMode == 98 || debugMode == 99 );
	bool doPaniniPath = paniniAmount > 0.0001 || debugInverse;
	vec2 uvLogical = uv;

	if ( doPaniniPath ) {
		float aspect = max( paniniPC.aspect, 1e-6 );
		float fovX = radians( clamp( paniniPC.fovXDeg, 1.0, 179.0 ) );
		float fovY = 2.0 * atan( tan( 0.5 * fovX ) / aspect );
		float paniniZoom = max( paniniPC.paniniZoom, 1.0 );
		vec2 uvOut = ( uv - 0.5 ) / paniniZoom + 0.5;
		vec3 dirPan = paniniInverseDir( uvOut, paniniPC.paniniD, paniniPC.paniniS, fovY, aspect );

		vec2 persp = dirPan.xy / max( -dirPan.z, 1e-6 );
		float perspFitX = max( tan( 0.5 * fovX ), 1e-6 );
		float perspFitY = max( tan( 0.5 * fovY ), 1e-6 );
		vec2 perspN = vec2( persp.x / perspFitX, persp.y / perspFitY );
		vec2 uvSrcPan = perspN * 0.5 + 0.5;
		uvLogical = mix( uvOut, uvSrcPan, paniniAmount );
		vec2 uvSample = to_src_uv( uvLogical );
		bool projInvalid = !( finite2( perspN ) && finite2( uvSrcPan ) && finite2( uvLogical ) && finite2( uvSample ) );

		if ( projInvalid ) {
			if ( paniniDebug != 0 ) {
				out_color = vec4( 1.0, 0.0, 1.0, 1.0 );
				return;
			}
			out_color = vec4( 0.0, 0.0, 0.0, 1.0 );
			return;
		}

		bool logicalOob = any( lessThan( uvLogical, vec2( 0.0 ) ) ) || any( greaterThan( uvLogical, vec2( 1.0 ) ) );
		if ( debugInverse ) {
			if ( debugMode == 97 ) {
				out_color = vec4( uvLogical, 0.0, 1.0 );
				return;
			}
			if ( debugMode == 98 ) {
				out_color = vec4( uvSample, 0.0, 1.0 );
				return;
			}
			out_color = logicalOob ? vec4( 1.0, 0.0, 0.0, 1.0 ) : vec4( 0.0, 1.0, 0.0, 1.0 );
			return;
		}

		if ( logicalOob ) {
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
			uvLogical = clamp( uvLogical, 0.0, 1.0 );
		}
	}

	uv = to_src_uv( uvLogical );
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

	if ( chromatic_aberration > 0.0 && postprocess_enabled != 0 ) {
		vec2 caUV = uvLogical;
		vec2 caOffset = (caUV - 0.5) * chromatic_aberration * 0.01;
		vec2 srcR = to_src_uv( caUV + caOffset );
		vec2 srcB = to_src_uv( caUV - caOffset );
		vec3 caHdr;
		caHdr.r = texture( texture0, srcR ).r;
		caHdr.g = ldr.g;
		caHdr.b = texture( texture0, srcB ).b;
		caHdr.r *= max( paniniPC.brightness, 0.0 ) * exposure * preExposureScale;
		caHdr.b *= max( paniniPC.brightness, 0.0 ) * exposure * preExposureScale;
		vec3 caTone;
		caTone.r = doTonemap( vec3( caHdr.r ) ).r;
		caTone.g = ldr.g;
		caTone.b = doTonemap( vec3( caHdr.b ) ).b;
		ldr = clamp( caTone, 0.0, 1.0 );
	}

	if ( vignette_intensity > 0.0 && postprocess_enabled != 0 ) {
		vec2 vigUV = uvLogical * 2.0 - 1.0;
		float vigDist = length( vigUV );
		float vig = 1.0 - smoothstep( vignette_radius, vignette_radius + 0.5, vigDist );
		vig = mix( 1.0, vig, vignette_intensity );
		ldr *= vig;
	}

	if ( film_grain > 0.0 && postprocess_enabled != 0 ) {
		float grainSeed = fract( sin( dot( gl_FragCoord.xy, vec2( 12.9898, 78.233 ) ) ) * 43758.5453 );
		float grain = ( grainSeed - 0.5 ) * film_grain * 0.1;
		ldr += grain;
		ldr = clamp( ldr, 0.0, 1.0 );
	}

	if ( apply_srgb_gamma != 0 ) {
		ldr = linearToDisplay( ldr );
	}

	out_color = vec4( ldr, 1.0 );
}
