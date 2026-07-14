#version 450

layout(set = 0, binding = 0) uniform sampler2D texture0;
layout(set = 1, binding = 0) uniform sampler2D depthTex;
layout(set = 2, binding = 0) uniform PostFXParams {
	mat4 invViewProj;
	mat4 prevViewProj;
	mat4 viewMatrix;
	vec4 motionBlur;   /* enabled, strength, samples, maxRadius */
	vec4 depthOfField; /* enabled, aperture, focusDistance, focusRange */
	vec4 frameInfo;    /* dofMaxBlur, texelSize.x, texelSize.y, motionValid */
	vec4 depthParams;  /* zNear, zFar */
	vec4 toneMapParams0;   /* toe, shoulder, whitePoint, blackClip */
	vec4 toneMapParams1;   /* highlightDesat, contrast, contrastPivot, legacyTonemapMode */
	vec4 colorBalance;     /* temperature, tint, exposureBias, preExposureScale */
	vec4 colorGrade;       /* saturation, vibrance, legacyContrast, legacySaturation */
	vec4 colorGrade2;      /* hueDegrees, reserved */
	vec4 shadowsLift;      /* rgb lift */
	vec4 midsGamma;        /* rgb gamma */
	vec4 highlightsGain;   /* rgb gain */
	vec4 splitShadow;      /* rgb tint, balance */
	vec4 splitHighlight;   /* rgb tint, strength */
	vec4 lensEffects0;     /* vignette, vignetteRadius, chromaticAberration, filmGrain */
	vec4 lensEffects1;     /* outlineStrength, outlineThreshold, filmLook, sharpen */
	vec4 runtimeFlags;     /* greyscale, dither, postDebug, postEnabled */
	vec4 lutParams;        /* lutIntensity, lutEnabled, lutStripDim, invGamma */
	vec4 autoExposureParams; /* avgLogLum, targetLum, minExposure, maxExposure */
	vec4 localExposureParams; /* enabled, strength, shadowClampEV, highlightClampEV */
	vec4 taaParams;        /* validHistory, stationaryFeedback, motionFeedback, sharpen */
} postfx;
layout(set = 3, binding = 0) uniform sampler2D lutTexture;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 3) const float bloom_threshold = 0.6;
layout(constant_id = 4) const float bloom_intensity = 0.5;
layout(constant_id = 5) const int bloom_threshold_mode = 0;
layout(constant_id = 6) const int bloom_modulate = 0;
layout(constant_id = 8) const int depth_r = 255;
layout(constant_id = 9) const int depth_g = 255;
layout(constant_id = 10) const int depth_b = 255;
layout(constant_id = 12) const float bloom_knee = 0.5;
layout(constant_id = 14) const int apply_srgb_gamma = 0;

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
	float exposure;  /* per-frame (eye adaptation or r_exposure) */
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

float postPreExposureScale( void ) { return max( postfx.colorBalance.w, 0.001 ); }
float postGreyscale( void ) { return postfx.runtimeFlags.x; }
int postDitherMode( void ) { return int( floor( postfx.runtimeFlags.y + 0.5 ) ); }
int postDebugMode( void ) { return int( floor( postfx.runtimeFlags.z + 0.5 ) ); }
bool postEnabled( void ) { return postfx.runtimeFlags.w > 0.5; }
int postTonemapMode( void ) { return int( floor( postfx.toneMapParams1.w + 0.5 ) ); }
float postContrast( void ) { return max( postfx.toneMapParams1.y, 0.0 ); }
float postContrastPivot( void ) { return clamp( postfx.toneMapParams1.z, 0.0, 1.0 ); }
float postLegacyContrast( void ) { return max( postfx.colorGrade.z, 0.0 ); }
float postSaturation( void ) { return max( postfx.colorGrade.x, 0.0 ) * max( postfx.colorGrade.w, 0.0 ); }
float postVibrance( void ) { return clamp( postfx.colorGrade.y, -1.0, 1.0 ); }
float postHueDegrees( void ) { return clamp( postfx.colorGrade2.x, -180.0, 180.0 ); }
float postInvGamma( void ) { return max( postfx.lutParams.w, 1e-6 ); }
float postVignetteIntensity( void ) { return max( postfx.lensEffects0.x, 0.0 ); }
float postVignetteRadius( void ) { return max( postfx.lensEffects0.y, 0.0 ); }
float postChromaticAberration( void ) { return max( postfx.lensEffects0.z, 0.0 ); }
float postFilmGrain( void ) { return max( postfx.lensEffects0.w, 0.0 ); }
float postOutlineStrength( void ) { return max( postfx.lensEffects1.x, 0.0 ); }
float postOutlineThreshold( void ) { return max( postfx.lensEffects1.y, 0.0 ); }
int postFilmLook( void ) { return int( floor( postfx.lensEffects1.z + 0.5 ) ); }
float postSharpenStrength( void ) { return max( postfx.lensEffects1.w, 0.0 ); }
float postAvgLogLum( void ) { return postfx.autoExposureParams.x; }
bool postLocalExposureEnabled( void ) { return postfx.localExposureParams.x > 0.5; }
float postLocalExposureStrength( void ) { return clamp( postfx.localExposureParams.y, 0.0, 1.0 ); }
float postLocalExposureShadowClamp( void ) { return max( postfx.localExposureParams.z, 0.0 ); }
float postLocalExposureHighlightClamp( void ) { return max( postfx.localExposureParams.w, 0.0 ); }

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

float FilmicLuminanceCurve( float x, float toe, float shoulder, float whitePoint ) {
	float safeWhite = max( whitePoint, 1e-4 );
	float t = max( x / safeWhite, 0.0 );
	float toePow = mix( 1.0, 2.4, clamp( toe, 0.0, 1.0 ) );
	float shoulderStrength = mix( 0.45, 2.6, clamp( shoulder, 0.0, 1.0 ) );
	float mapped = pow( t, toePow );
	float normalizedWhite = 1.0 / ( 1.0 + shoulderStrength );
	return clamp( ( mapped / ( mapped + shoulderStrength ) ) / normalizedWhite, 0.0, 1.0 );
}

vec3 Tonemap_Filmic( vec3 x ) {
	float lum = max( dot( x, sRGB ), 1e-6 );
	float blackClip = max( postfx.toneMapParams0.w, 0.0 );
	float toe = clamp( postfx.toneMapParams0.x, 0.0, 1.0 );
	float shoulder = clamp( postfx.toneMapParams0.y, 0.0, 1.0 );
	float whitePoint = max( postfx.toneMapParams0.z, 0.5 );
	float highlightDesat = clamp( postfx.toneMapParams1.x, 0.0, 1.0 );
	float clippedLum = max( lum - blackClip, 0.0 );
	float mappedLum = FilmicLuminanceCurve( clippedLum, toe, shoulder, whitePoint );
	float scale = mappedLum / lum;
	vec3 mapped = clamp( x * scale, 0.0, 1.0 );
	float compression = clamp( 1.0 - mappedLum / max( clippedLum, 1e-5 ), 0.0, 1.0 );
	float desat = compression * highlightDesat;
	return mix( mapped, vec3( dot( mapped, sRGB ) ), desat );
}

/* AgX-inspired tonemap - punchy, saturated, modern look.
   Attempt to preserve chrominance through the curve. */
vec3 Tonemap_AgX( vec3 x ) {
	/* Per-channel curve with luminance-guided saturation recovery */
	float lum = dot( x, vec3( 0.2126, 0.7152, 0.0722 ) );
	vec3 mapped = x / ( x + vec3( 0.7 ) );
	float lumMapped = lum / ( lum + 0.7 );
	/* Recover saturation lost by per-channel mapping */
	vec3 satRecovered = lumMapped + 1.2 * ( mapped - lumMapped );
	return clamp( satRecovered, 0.0, 1.0 );
}

vec3 linearToDisplay( vec3 x ) {
	return pow( max( x, vec3( 0.0 ) ), vec3( postInvGamma() ) );
}

vec3 sanitizeHdr( vec3 hdr ) {
	if ( isnan( hdr.r ) || isnan( hdr.g ) || isnan( hdr.b ) ||
	     isinf( hdr.r ) || isinf( hdr.g ) || isinf( hdr.b ) ) {
		return vec3( 0.0 );
	}
	return hdr;
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

vec3 applyWhiteBalance( vec3 color ) {
	float temperature = clamp( postfx.colorBalance.x, -1.0, 1.0 );
	float tint = clamp( postfx.colorBalance.y, -1.0, 1.0 );
	vec3 balance = vec3(
		1.0 + temperature * 0.12 - tint * 0.02,
		1.0 + tint * 0.10,
		1.0 - temperature * 0.12 - tint * 0.02 );
	return max( color * balance, vec3( 0.0 ) );
}

float sampleHdrLogLum( vec2 uv ) {
	vec3 sampleHdr = applyWhiteBalance( sanitizeHdr( textureLod( texture0, clamp( uv, 0.0, 1.0 ), 0.0 ).rgb ) );
	return log2( max( dot( sampleHdr, sRGB ), 1e-4 ) );
}

vec3 applyLocalExposure( vec2 uv, vec3 hdr ) {
	if ( !postLocalExposureEnabled() || postLocalExposureStrength() <= 0.0 ) {
		return hdr;
	}

	vec2 texel = postfx.frameInfo.yz;
	vec2 nearOffset = texel * 6.0;
	vec2 farOffset = texel * 14.0;
	float localLogLum = sampleHdrLogLum( uv ) * 0.34;
	localLogLum += sampleHdrLogLum( uv + vec2( nearOffset.x, 0.0 ) ) * 0.14;
	localLogLum += sampleHdrLogLum( uv - vec2( nearOffset.x, 0.0 ) ) * 0.14;
	localLogLum += sampleHdrLogLum( uv + vec2( 0.0, nearOffset.y ) ) * 0.14;
	localLogLum += sampleHdrLogLum( uv - vec2( 0.0, nearOffset.y ) ) * 0.14;
	localLogLum += sampleHdrLogLum( uv + farOffset ) * 0.06;
	localLogLum += sampleHdrLogLum( uv - farOffset ) * 0.06;
	localLogLum += sampleHdrLogLum( uv + vec2( farOffset.x, -farOffset.y ) ) * 0.06;
	localLogLum += sampleHdrLogLum( uv + vec2( -farOffset.x, farOffset.y ) ) * 0.06;

	float deltaEv = ( postAvgLogLum() - localLogLum ) * postLocalExposureStrength();
	deltaEv = clamp( deltaEv, -postLocalExposureHighlightClamp(), postLocalExposureShadowClamp() );
	return hdr * exp2( deltaEv );
}

vec3 applyLiftGammaGain( vec3 ldr ) {
	float lum = dot( ldr, sRGB );
	float shadowMask = 1.0 - smoothstep( 0.12, 0.45, lum );
	float highlightMask = smoothstep( 0.45, 0.88, lum );
	float midMask = max( 0.0, 1.0 - shadowMask - highlightMask );
	vec3 lifted = clamp( ldr + postfx.shadowsLift.rgb * shadowMask, 0.0, 1.0 );
	vec3 gammaAdjusted = pow( max( lifted, vec3( 1e-5 ) ), 1.0 / max( postfx.midsGamma.rgb, vec3( 1e-3 ) ) );
	lifted = mix( lifted, gammaAdjusted, midMask );
	lifted *= mix( vec3( 1.0 ), max( postfx.highlightsGain.rgb, vec3( 0.0 ) ), highlightMask );
	return clamp( lifted, 0.0, 1.0 );
}

vec3 applySplitToning( vec3 ldr ) {
	float strength = clamp( postfx.splitHighlight.w, 0.0, 1.0 );
	float balance = clamp( postfx.splitShadow.w, 0.0, 1.0 );
	if ( strength <= 0.0 ) {
		return ldr;
	}

	float lum = dot( ldr, sRGB );
	float shadowMask = 1.0 - smoothstep( max( balance - 0.25, 0.0 ), min( balance + 0.05, 1.0 ), lum );
	float highlightMask = smoothstep( max( balance - 0.05, 0.0 ), min( balance + 0.25, 1.0 ), lum );
	vec3 tinted = ldr;
	tinted = mix( tinted, tinted * postfx.splitShadow.rgb, shadowMask * strength );
	tinted = mix( tinted, tinted * postfx.splitHighlight.rgb, highlightMask * strength );
	return clamp( tinted, 0.0, 1.0 );
}

vec3 applyVibrance( vec3 ldr ) {
	float vibrance = postVibrance();
	float minC = min( min( ldr.r, ldr.g ), ldr.b );
	float maxC = max( max( ldr.r, ldr.g ), ldr.b );
	float sat = maxC - minC;
	float lum = dot( ldr, sRGB );
	float amount = vibrance * ( 1.0 - sat ) * smoothstep( 0.05, 0.95, lum );
	return clamp( vec3( lum ) + ( 1.0 + amount ) * ( ldr - vec3( lum ) ), 0.0, 1.0 );
}

vec3 applyHueShift( vec3 ldr ) {
	float hue = postHueDegrees();
	if ( abs( hue ) <= 0.001 ) {
		return ldr;
	}

	float angle = radians( hue );
	float s = sin( angle );
	float c = cos( angle );
	vec3 yiq;
	yiq.x = dot( ldr, vec3( 0.299, 0.587, 0.114 ) );
	yiq.y = dot( ldr, vec3( 0.596, -0.274, -0.322 ) );
	yiq.z = dot( ldr, vec3( 0.211, -0.523, 0.312 ) );
	vec2 chroma = vec2( yiq.y * c - yiq.z * s, yiq.y * s + yiq.z * c );
	vec3 shifted;
	shifted.r = yiq.x + 0.956 * chroma.x + 0.621 * chroma.y;
	shifted.g = yiq.x - 0.272 * chroma.x - 0.647 * chroma.y;
	shifted.b = yiq.x - 1.106 * chroma.x + 1.703 * chroma.y;
	return clamp( shifted, 0.0, 1.0 );
}

vec3 sampleLUTStrip( vec3 ldr ) {
	float lutIntensity = clamp( postfx.lutParams.x, 0.0, 1.0 );
	float lutEnabled = postfx.lutParams.y;
	if ( lutIntensity <= 0.0 || lutEnabled < 0.5 ) {
		return ldr;
	}

	vec2 size = vec2( textureSize( lutTexture, 0 ) );
	float dim = max( size.y, 1.0 );
	if ( abs( size.x - dim * dim ) > 0.5 ) {
		return ldr;
	}

	vec3 coord = clamp( ldr, 0.0, 1.0 ) * ( dim - 1.0 );
	float slice = coord.b;
	float slice0 = floor( slice );
	float slice1 = min( slice0 + 1.0, dim - 1.0 );
	float frac = slice - slice0;
	vec2 uv0 = vec2( ( coord.r + slice0 * dim + 0.5 ) / size.x, ( coord.g + 0.5 ) / size.y );
	vec2 uv1 = vec2( ( coord.r + slice1 * dim + 0.5 ) / size.x, ( coord.g + 0.5 ) / size.y );
	vec3 graded = mix( textureLod( lutTexture, uv0, 0.0 ).rgb, textureLod( lutTexture, uv1, 0.0 ).rgb, frac );
	return mix( ldr, graded, lutIntensity );
}

bool finite2( vec2 v ) {
	return !( isnan( v.x ) || isnan( v.y ) || isinf( v.x ) || isinf( v.y ) );
}

vec2 to_src_uv( vec2 uv01 ) {
	vec2 scale = paniniPC.srcUVScaleBias.xy;
	vec2 bias = paniniPC.srcUVScaleBias.zw;
	if ( scale.x <= 0.0 || scale.y <= 0.0 )
		return uv01;
	return uv01 * scale + bias;
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
	dir.y = tan( phi ) * max( -dir.z, 1e-6 );
	float invLen = inversesqrt( max( dot( dir, dir ), 1e-8 ) );
	return dir * invLen;
}

vec3 doTonemap( vec3 value ) {
	int tonemapMode = postTonemapMode();
	if ( tonemapMode == 2 ) {
		return ACESFilm( value );
	} else if ( tonemapMode == 1 ) {
		return Tonemap_Reinhard( value );
	} else if ( tonemapMode == 3 ) {
		return Tonemap_Filmic( value );
	} else if ( tonemapMode == 4 ) {
		return Tonemap_AgX( value );
	}
	return value;
}

vec3 applyPostColorAdjust( vec3 ldr, bool postActive ) {
	if ( postActive ) {
		float c = clamp( postContrast() * postLegacyContrast(), 0.0, 4.0 );
		float pivot = postContrastPivot();
		ldr = ( ldr - pivot ) * c + pivot;
		ldr = clamp( ldr, 0.0, 1.0 );
		ldr = applyHueShift( ldr );
		ldr = applyVibrance( ldr );
		float sat = clamp( postSaturation(), 0.0, 3.0 );
		float lum = dot( ldr, sRGB );
		ldr = clamp( vec3( lum ) + sat * ( ldr - vec3( lum ) ), 0.0, 1.0 );
		ldr = applyLiftGammaGain( ldr );
		ldr = applySplitToning( ldr );
		ldr = sampleLUTStrip( ldr );
	}

	return ldr;
}

vec3 samplePostLdr( vec2 uv, bool hdrResolve, bool postGrade ) {
	vec3 sampleHdr = sanitizeHdr( textureLod( texture0, uv, 0.0 ).rgb );
	if ( hdrResolve ) {
		sampleHdr = applyWhiteBalance( sampleHdr );
	}
	if ( postGrade ) {
		sampleHdr = applyLocalExposure( uv, sampleHdr );
	}
	vec3 sampleExposed = sampleHdr * max( paniniPC.brightness, 0.0 );

	if ( hdrResolve ) {
		float exposureScale = exp2( postfx.colorBalance.z );
		sampleExposed *= max( paniniPC.exposure * exposureScale, 0.01 );
		sampleExposed *= postPreExposureScale();
		sampleExposed = applyBloomKnee( sampleExposed );
		sampleExposed = doTonemap( sampleExposed );
	}

	return applyPostColorAdjust( clamp( sampleExposed, 0.0, 1.0 ), postGrade );
}

/* Linear depth from depth buffer (0=near, 1=far). Assumes perspective depth. */
float linearDepthFromBuffer( float depthNdc ) {
	float zNear = postfx.depthParams.x;
	float zFar = postfx.depthParams.y;
	if ( zFar <= zNear ) return zNear;
	return zNear * zFar / ( zFar - depthNdc * ( zFar - zNear ) );
}

/* Camera motion blur: sample along velocity vector. */
vec3 applyMotionBlur( vec3 ldr, vec2 uv, bool postGrade ) {
	if ( !postGrade || postfx.motionBlur.x < 0.5 || postfx.frameInfo.w < 0.5 )
		return ldr;
	float depthNdc = textureLod( depthTex, uv, 0.0 ).r;
	if ( depthNdc <= 0.0 || depthNdc >= 1.0 )
		return ldr;
	vec4 posClip = vec4( uv * 2.0 - 1.0, depthNdc, 1.0 );
	vec4 posView = postfx.invViewProj * posClip;
	posView /= posView.w;
	vec4 prevClip = postfx.prevViewProj * posView;
	vec2 prevUV = prevClip.xy / prevClip.w * 0.5 + 0.5;
	vec2 velocity = uv - prevUV;
	float velLen = length( velocity );
	vec2 texel = postfx.frameInfo.yz;
	float maxRadius = postfx.motionBlur.w * max( texel.x, texel.y );
	int samples = int( clamp( postfx.motionBlur.z, 4.0, 32.0 ) );
	float strength = postfx.motionBlur.y * min( 1.0, velLen * 100.0 );
	if ( strength < 0.01 || velLen < 1e-5 )
		return ldr;
	vec2 step = velocity * strength / float( samples );
	vec3 acc = ldr;
	float wacc = 1.0;
	for ( int i = 1; i <= samples; i++ ) {
		float t = float( i ) / float( samples ) - 0.5;
		vec2 suv = uv + step * t * 2.0;
		if ( any( lessThan( suv, vec2( 0.0 ) ) ) || any( greaterThan( suv, vec2( 1.0 ) ) ) )
			continue;
		float w = 1.0 - abs( t ) * 2.0;
		acc += samplePostLdr( suv, true, postGrade ) * w;
		wacc += w;
	}
	return acc / wacc;
}

/* Depth of field: simple separable blur weighted by circle-of-confusion. */
vec3 applyDepthOfField( vec3 ldr, vec2 uv, bool postGrade ) {
	if ( !postGrade || postfx.depthOfField.x < 0.5 )
		return ldr;
	float depthNdc = textureLod( depthTex, uv, 0.0 ).r;
	if ( depthNdc <= 0.0 || depthNdc >= 1.0 )
		return ldr;
	float linearDepth = linearDepthFromBuffer( depthNdc );
	float focusDist = postfx.depthOfField.z;
	float focusRange = max( postfx.depthOfField.w, 1.0 );
	float aperture = postfx.depthOfField.y;
	float coc = abs( linearDepth - focusDist ) / focusRange * aperture;
	float maxBlur = postfx.frameInfo.x;
	coc = min( coc, 1.0 ) * maxBlur * 0.01;
	if ( coc < 0.5 )
		return ldr;
	vec2 texel = postfx.frameInfo.yz;
	int taps = 8;
	vec3 acc = ldr;
	float wacc = 1.0;
	float angle = 0.0;
	for ( int i = 0; i < taps; i++ ) {
		float a = angle + float( i ) * 6.28318530718 / float( taps );
		vec2 offset = vec2( cos( a ), sin( a ) ) * coc;
		vec2 suv = uv + offset;
		if ( any( lessThan( suv, vec2( 0.0 ) ) ) || any( greaterThan( suv, vec2( 1.0 ) ) ) )
			continue;
		acc += samplePostLdr( suv, true, postGrade );
		wacc += 1.0;
	}
	return acc / wacc;
}

void main() {
	vec2 uv = frag_tex_coord;
	float paniniAmount = clamp( paniniPC.paniniAmount, 0.0, 1.0 );
	int borderMode = int( clamp( floor( paniniPC.paniniBorderMode + 0.5 ), 0.0, 1.0 ) );
	int paniniDebug = int( clamp( floor( paniniPC.paniniDebugMode + 0.5 ), 0.0, 1.0 ) );
	int debugMode = postDebugMode();
	bool debugInverse = postEnabled() && ( debugMode == 97 || debugMode == 98 || debugMode == 99 );
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
	bool noWorldLdr = paniniPC.paniniPad1 > 0.5;
	bool hdrResolveActive = !noWorldLdr;
	bool postGradeActive = postEnabled() && hdrResolveActive;
	vec3 hdr = hdrResolveActive ?
		applyWhiteBalance( sanitizeHdr( textureLod( texture0, uv, 0.0 ).rgb ) ) :
		sanitizeHdr( textureLod( texture0, uv, 0.0 ).rgb );
	if ( postGradeActive ) {
		hdr = applyLocalExposure( uv, hdr );
	}
	vec3 hdr_exposed = hdr * max( paniniPC.brightness, 0.0 );
	if ( hdrResolveActive ) {
		float exposureScale = exp2( postfx.colorBalance.z );
		hdr_exposed *= max( paniniPC.exposure * exposureScale, 0.01 );
		hdr_exposed *= postPreExposureScale();
		hdr_exposed = applyBloomKnee( hdr_exposed );
	}

	vec3 tonemapped = hdr_exposed;
	if ( hdrResolveActive ) {
		tonemapped = doTonemap( tonemapped );
	}

	vec3 ldr;
	if ( hdrResolveActive && postGradeActive && debugMode != 0 ) {
		if ( debugMode == 1 ) {
			ldr = clamp( hdr_exposed, 0.0, 10.0 ) * 0.1;
		} else {
			float lum = max( max( hdr_exposed.r, hdr_exposed.g ), hdr_exposed.b );
			float logLum = log2( max( lum, 1e-4 ) );
			float heat = clamp( ( logLum + 6.0 ) / 8.0, 0.0, 1.0 );
			ldr = vec3( heat, clamp( heat * 0.25, 0.0, 1.0 ), 1.0 - heat );
		}
	} else {
		ldr = applyPostColorAdjust( clamp( tonemapped, 0.0, 1.0 ), postGradeActive );

		if ( postGradeActive && postSharpenStrength() > 0.0 ) {
			vec2 texel = 1.0 / vec2( textureSize( texture0, 0 ) );
			vec3 ldrL = samplePostLdr( clamp( uv + vec2( -texel.x, 0.0 ), 0.0, 1.0 ), hdrResolveActive, postGradeActive );
			vec3 ldrR = samplePostLdr( clamp( uv + vec2(  texel.x, 0.0 ), 0.0, 1.0 ), hdrResolveActive, postGradeActive );
			vec3 ldrU = samplePostLdr( clamp( uv + vec2( 0.0, -texel.y ), 0.0, 1.0 ), hdrResolveActive, postGradeActive );
			vec3 ldrD = samplePostLdr( clamp( uv + vec2( 0.0,  texel.y ), 0.0, 1.0 ), hdrResolveActive, postGradeActive );
			vec3 blur = ( ldrL + ldrR + ldrU + ldrD ) * 0.25;
			float edge = max( dot( ldr, sRGB ) - dot( blur, sRGB ), 0.0 );
			float adaptive = smoothstep( 0.01, 0.20, edge );
			float strength = clamp( postSharpenStrength(), 0.0, 1.5 ) * mix( 0.25, 1.0, adaptive );
			vec3 localMin = min( min( ldrL, ldrR ), min( ldrU, ldrD ) );
			vec3 localMax = max( max( ldrL, ldrR ), max( ldrU, ldrD ) );
			vec3 localRange = max( localMax - localMin, vec3( 0.02 ) );
			vec3 sharpened = ldr + ( ldr - blur ) * strength;
			sharpened = clamp( sharpened, localMin - localRange * 0.25, localMax + localRange * 0.25 );
			ldr = clamp( mix( ldr, sharpened, adaptive ), 0.0, 1.0 );
		}

		ldr = applyMotionBlur( ldr, uv, postGradeActive );
		ldr = applyDepthOfField( ldr, uv, postGradeActive );

		if ( postGreyscale() == 1.0 ) {
			ldr = vec3( dot( ldr, sRGB ) );
		} else if ( postGreyscale() != 0.0 ) {
			vec3 luma = vec3( dot( ldr, sRGB ) );
			ldr = mix( ldr, luma, postGreyscale() );
		}
		if ( postDitherMode() == 1 ) {
			ldr = dither( ldr );
		}
	}

	if ( postChromaticAberration() > 0.0 && postGradeActive ) {
		vec2 caUV = uvLogical;
		float exposureScale = exp2( postfx.colorBalance.z );
		vec2 caOffset = (caUV - 0.5) * postChromaticAberration() * 0.01;
			vec2 srcR = to_src_uv( caUV + caOffset );
			vec2 srcB = to_src_uv( caUV - caOffset );
			vec3 caHdr;
			caHdr.r = applyWhiteBalance( textureLod( texture0, srcR, 0.0 ).rgb ).r;
			caHdr.g = ldr.g;
			caHdr.b = applyWhiteBalance( textureLod( texture0, srcB, 0.0 ).rgb ).b;
			if ( postLocalExposureEnabled() ) {
				caHdr.r *= exp2( clamp( ( postAvgLogLum() - sampleHdrLogLum( srcR ) ) * postLocalExposureStrength(),
					-postLocalExposureHighlightClamp(), postLocalExposureShadowClamp() ) );
				caHdr.b *= exp2( clamp( ( postAvgLogLum() - sampleHdrLogLum( srcB ) ) * postLocalExposureStrength(),
					-postLocalExposureHighlightClamp(), postLocalExposureShadowClamp() ) );
			}
			caHdr.r *= max( paniniPC.brightness, 0.0 ) * max( paniniPC.exposure * exposureScale, 0.01 ) * postPreExposureScale();
			caHdr.b *= max( paniniPC.brightness, 0.0 ) * max( paniniPC.exposure * exposureScale, 0.01 ) * postPreExposureScale();
		vec3 caTone;
		caTone.r = doTonemap( vec3( caHdr.r ) ).r;
		caTone.g = ldr.g;
		caTone.b = doTonemap( vec3( caHdr.b ) ).b;
		ldr = clamp( caTone, 0.0, 1.0 );
	}

	if ( postOutlineStrength() > 0.0 && postGradeActive ) {
		vec2 outlineTexel = 1.0 / vec2( textureSize( texture0, 0 ) );
		float lumC  = dot( textureLod( texture0, uv, 0.0 ).rgb, sRGB );
		float lumL  = dot( textureLod( texture0, uv + vec2(-outlineTexel.x, 0.0), 0.0 ).rgb, sRGB );
		float lumR  = dot( textureLod( texture0, uv + vec2( outlineTexel.x, 0.0), 0.0 ).rgb, sRGB );
		float lumU  = dot( textureLod( texture0, uv + vec2(0.0, -outlineTexel.y), 0.0 ).rgb, sRGB );
		float lumD  = dot( textureLod( texture0, uv + vec2(0.0,  outlineTexel.y), 0.0 ).rgb, sRGB );
		float edgeH = abs( lumL - lumR );
		float edgeV = abs( lumU - lumD );
		float edge  = sqrt( edgeH * edgeH + edgeV * edgeV );
		float outlineMask = smoothstep( postOutlineThreshold() * 0.5, postOutlineThreshold(), edge );
		ldr = mix( ldr, vec3(0.0), outlineMask * postOutlineStrength() );
	}

	if ( postVignetteIntensity() > 0.0 && postGradeActive ) {
		vec2 vigUV = uvLogical * 2.0 - 1.0;
		float vigDist = length( vigUV );
		float vig = 1.0 - smoothstep( postVignetteRadius(), postVignetteRadius() + 0.5, vigDist );
		vig = mix( 1.0, vig, postVignetteIntensity() );
		ldr *= vig;
	}

	/* Film grain: film_look = Source-style (luminance-dependent, soft-light);
	   else film_grain = simple additive. */
	if ( postFilmLook() != 0 && postFilmGrain() > 0.0 && postGradeActive ) {
		/* Source Engine–style film grain (DoD:S, L4D): luminance-dependent, fine-grained,
		   soft-light blend. Grain peaks in mid-tones, fades in shadows/highlights.
		   r_filmGrain scales intensity (0.5–1.5x) when film_look is on. */
		float t = paniniPC.paniniPad0;
		vec2 px = gl_FragCoord.xy;
		/* Multi-octave hash for fine grain; higher scale = finer grain; temporal for animation */
		vec2 seedA = px * 8.0 + vec2( t * 47.0, t * 31.0 );
		vec2 seedB = px * 16.0 + vec2( t * 73.0, t * 59.0 );
		float n0 = fract( sin( dot( floor( seedA ), vec2( 12.9898, 78.233 ) ) ) * 43758.5453 );
		float n1 = fract( sin( dot( floor( seedB ), vec2( 127.1, 311.7 ) ) ) * 43758.5453 );
		float grainRaw = ( n0 + n1 ) * 0.5 - 0.5;  /* -0.5 .. 0.5 */
		/* Luminance mask: peak in mid-tones, fade in shadows and highlights (like real film) */
		float lum = dot( ldr, sRGB );
		float midTone = lum * ( 1.0 - lum ) * 4.0;  /* 0 at 0/1, max 1 at 0.5 */
		float lumMask = smoothstep( 0.0, 0.06, lum ) * smoothstep( 1.0, 0.35, lum );
		float intensity = clamp( postFilmGrain(), 0.0, 1.0 );
		float grainStrength = 0.12 * intensity * midTone * lumMask;
		/* Soft-light blend: blend = 0.5 + grain, so 0.5 = neutral */
		vec3 blend = vec3( 0.5 + grainRaw * grainStrength );
		vec3 base = ldr;
		vec3 result;
		result.r = ( blend.r < 0.5 )
			? ( 2.0 * base.r * blend.r + base.r * base.r * ( 1.0 - 2.0 * blend.r ) )
			: ( sqrt( base.r ) * ( 2.0 * blend.r - 1.0 ) + 2.0 * base.r * ( 1.0 - blend.r ) );
		result.g = ( blend.g < 0.5 )
			? ( 2.0 * base.g * blend.g + base.g * base.g * ( 1.0 - 2.0 * blend.g ) )
			: ( sqrt( base.g ) * ( 2.0 * blend.g - 1.0 ) + 2.0 * base.g * ( 1.0 - blend.g ) );
		result.b = ( blend.b < 0.5 )
			? ( 2.0 * base.b * blend.b + base.b * base.b * ( 1.0 - 2.0 * blend.b ) )
			: ( sqrt( base.b ) * ( 2.0 * blend.b - 1.0 ) + 2.0 * base.b * ( 1.0 - blend.b ) );
		ldr = clamp( result, 0.0, 1.0 );
	} else if ( postFilmGrain() > 0.0 && postGradeActive ) {
		/* Simple additive grain when film_look is off */
		float t = paniniPC.paniniPad0;
		float grainSeed = fract( sin( dot( gl_FragCoord.xy + vec2( t * 173.0, t * 79.0 ), vec2( 12.9898, 78.233 ) ) ) * 43758.5453 );
		float grain = ( grainSeed - 0.5 ) * postFilmGrain() * 0.1;
		ldr += grain;
		ldr = clamp( ldr, 0.0, 1.0 );
	}

	if ( apply_srgb_gamma != 0 ) {
		ldr = linearToDisplay( ldr );
	}

	out_color = vec4( ldr, 1.0 );
}
