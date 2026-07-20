#version 450

/*
 * Temporal Reconstruction AA (confidence-guided).
 * History weight = base × confidence (depth × velocity × luma × reactive).
 * YCoCg variance clipping replaces naïve RGB min/max clamp.
 */

layout(set = 0, binding = 0) uniform sampler2D currentColor;
layout(set = 1, binding = 0) uniform sampler2D depthTex;
layout(set = 2, binding = 0) uniform PostFXParams {
	mat4 invViewProj;
	mat4 prevViewProj;
	mat4 viewMatrix;
	vec4 motionBlur;
	vec4 depthOfField;
	vec4 frameInfo;
	vec4 depthParams;
	vec4 toneMapParams0;
	vec4 toneMapParams1;
	vec4 colorBalance;
	vec4 colorGrade;
	vec4 colorGrade2;
	vec4 shadowsLift;
	vec4 midsGamma;
	vec4 highlightsGain;
	vec4 splitShadow;
	vec4 splitHighlight;
	vec4 lensEffects0;
	vec4 lensEffects1;
	vec4 runtimeFlags;
	vec4 lutParams;
	vec4 autoExposureParams;
	vec4 localExposureParams;
	vec4 taaParams;
} postfx;
layout(set = 3, binding = 0) uniform sampler2D historyColor;
layout(set = 4, binding = 0) uniform sampler2D motionTex;
layout(set = 5, binding = 0) uniform sampler2D reactiveMaskTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

const vec3 LUMA = vec3( 0.2126, 0.7152, 0.0722 );

vec2 texelSize( void ) {
	return postfx.frameInfo.yz;
}

vec3 sampleCurrent( vec2 uv ) {
	return textureLod( currentColor, clamp( uv, 0.0, 1.0 ), 0.0 ).rgb;
}

vec3 RGBToYCoCg( vec3 c ) {
	float Y = dot( c, vec3( 0.25, 0.5, 0.25 ) );
	float Co = dot( c, vec3( 0.5, 0.0, -0.5 ) );
	float Cg = dot( c, vec3( -0.25, 0.5, -0.25 ) );
	return vec3( Y, Co, Cg );
}

vec3 YCoCgToRGB( vec3 ycocg ) {
	float Y = ycocg.x;
	float Co = ycocg.y;
	float Cg = ycocg.z;
	return vec3( Y + Co - Cg, Y + Cg, Y - Co - Cg );
}

vec2 reprojectHistoryUV( vec2 uv, float depthNdc ) {
	vec4 posClip = vec4( uv * 2.0 - 1.0, depthNdc, 1.0 );
	vec4 posWorld = postfx.invViewProj * posClip;
	posWorld /= max( posWorld.w, 1e-6 );
	vec4 prevClip = postfx.prevViewProj * posWorld;
	return prevClip.xy / max( prevClip.w, 1e-6 ) * 0.5 + 0.5;
}

void neighborhoodYCoCgStats( vec2 uv, out vec3 meanY, out vec3 sigmaY ) {
	vec2 texel = texelSize();
	vec3 acc = vec3( 0.0 );
	vec3 acc2 = vec3( 0.0 );
	for ( int y = -1; y <= 1; ++y ) {
		for ( int x = -1; x <= 1; ++x ) {
			vec3 ycc = RGBToYCoCg( sampleCurrent( uv + vec2( x, y ) * texel ) );
			acc += ycc;
			acc2 += ycc * ycc;
		}
	}
	meanY = acc * ( 1.0 / 9.0 );
	vec3 varY = max( acc2 * ( 1.0 / 9.0 ) - meanY * meanY, vec3( 1e-6 ) );
	sigmaY = sqrt( varY );
}

vec3 applyResolveSharpen( vec2 uv, vec3 resolved ) {
	float sharpen = clamp( postfx.taaParams.w, 0.0, 1.0 );
	if ( sharpen <= 0.0 ) {
		return resolved;
	}
	vec2 texel = texelSize();
	vec3 blur = (
		sampleCurrent( uv + vec2( texel.x, 0.0 ) ) +
		sampleCurrent( uv - vec2( texel.x, 0.0 ) ) +
		sampleCurrent( uv + vec2( 0.0, texel.y ) ) +
		sampleCurrent( uv - vec2( 0.0, texel.y ) ) ) * 0.25;
	return max( resolved + ( resolved - blur ) * sharpen * 0.35, vec3( 0.0 ) );
}

/*
 * Edge-aware current-frame neighborhood (SMAA-class spatial fallback).
 * Used when history is rejected under Present-Time Adaptive Reconstruction —
 * not a full-frame SMAA pass over the resolve.
 */
vec3 spatialCurrentFallback( vec2 uv ) {
	vec2 texel = texelSize();
	vec3 c0 = sampleCurrent( uv );
	vec3 cL = sampleCurrent( uv + vec2( -texel.x, 0.0 ) );
	vec3 cR = sampleCurrent( uv + vec2( texel.x, 0.0 ) );
	vec3 cU = sampleCurrent( uv + vec2( 0.0, -texel.y ) );
	vec3 cD = sampleCurrent( uv + vec2( 0.0, texel.y ) );
	float l0 = max( dot( c0, LUMA ), 1e-4 );
	float wL = 1.0 / ( 1.0 + abs( max( dot( cL, LUMA ), 0.0 ) - l0 ) * 8.0 );
	float wR = 1.0 / ( 1.0 + abs( max( dot( cR, LUMA ), 0.0 ) - l0 ) * 8.0 );
	float wU = 1.0 / ( 1.0 + abs( max( dot( cU, LUMA ), 0.0 ) - l0 ) * 8.0 );
	float wD = 1.0 / ( 1.0 + abs( max( dot( cD, LUMA ), 0.0 ) - l0 ) * 8.0 );
	float wSum = 1.0 + wL + wR + wU + wD;
	return ( c0 + cL * wL + cR * wR + cU * wU + cD * wD ) / wSum;
}

void main() {
	vec2 uv = frag_tex_coord;
	vec2 texel = texelSize();
	vec2 sampleUV = uv - postfx.lutParams.zw * texel;
	vec3 current = sampleCurrent( sampleUV );
	float depthNdc = textureLod( depthTex, sampleUV, 0.0 ).r;

	/* colorGrade2.yzw: Temporal Reconstruction flags (gamma uses .x = hue only).
	 * shadowsLift.a: debug mode (gamma uses .rgb only).
	 * highlightsGain.a: adaptive recon pack (>0.5 on). */
	float useVarClip = postfx.colorGrade2.y;
	float useDisocc = postfx.colorGrade2.z;
	float useReactive = postfx.colorGrade2.w;
	float debugMode = postfx.shadowsLift.a;
	float adaptivePack = postfx.highlightsGain.a;
	bool adaptive = adaptivePack >= 1.0;
	bool adaptSpatial = adaptivePack >= 2.0;
	float adaptBudget = adaptive ? clamp( adaptivePack - ( adaptSpatial ? 2.0 : 1.0 ), 0.0, 1.0 ) : 0.0;

	if ( postfx.taaParams.x < 0.5 || postfx.frameInfo.w < 0.5 || depthNdc <= 0.0 || depthNdc >= 1.0 ) {
		if ( debugMode > 1.5 ) {
			out_color = vec4( 1.0, 1.0, 1.0, 1.0 ); /* white = cut/reset */
			return;
		}
		out_color = vec4( adaptive && adaptSpatial ? spatialCurrentFallback( sampleUV ) : current, 1.0 );
		return;
	}

	vec2 historyUV;
	vec2 motion = vec2( 0.0 );
	bool mvValid = false;
	if ( postfx.depthParams.z > 0.5 ) {
		motion = textureLod( motionTex, sampleUV, 0.0 ).rg;
		historyUV = sampleUV - motion;
		mvValid = !( any( isnan( motion ) ) || any( isinf( motion ) ) );
		if ( !mvValid ) {
			historyUV = reprojectHistoryUV( sampleUV, depthNdc );
		}
	} else {
		historyUV = reprojectHistoryUV( sampleUV, depthNdc );
	}

	if ( any( lessThan( historyUV, vec2( 0.0 ) ) ) || any( greaterThan( historyUV, vec2( 1.0 ) ) ) ) {
		if ( debugMode > 1.5 ) {
			out_color = vec4( 1.0, 0.0, 1.0, 1.0 ); /* magenta = invalid MV / OOB */
			return;
		}
		out_color = vec4( current, 1.0 );
		return;
	}

	if ( debugMode > 0.5 && debugMode < 1.5 ) {
		/* Motion vector debug: encode velocity as color */
		vec2 texSize = vec2( textureSize( currentColor, 0 ) );
		vec2 vel = ( sampleUV - historyUV ) * texSize * 0.05;
		out_color = vec4( abs( vel.x ), abs( vel.y ), 0.15, 1.0 );
		return;
	}

	vec3 history = textureLod( historyColor, historyUV, 0.0 ).rgb;
	float histDepth = textureLod( depthTex, historyUV, 0.0 ).r;

	vec2 texSize = vec2( textureSize( currentColor, 0 ) );
	vec2 velocity = ( sampleUV - historyUV ) * texSize;
	float motionLen = length( velocity );
	float motionFactor = smoothstep( 0.2, 8.0, motionLen );

	/* Confidence factors */
	float depthConf = 1.0;
	if ( useDisocc > 0.5 ) {
		float depthDelta = abs( depthNdc - histDepth );
		/* Reversed-Z: larger = nearer. Tighter reject kills skyline / silhouette bleed. */
		float dLo = adaptive ? 0.00025 : 0.0004;
		float dHi = adaptive ? 0.008 : 0.012;
		depthConf = 1.0 - smoothstep( dLo, dHi, depthDelta );
		/* Extra reject when current is near-far (sky/background) vs history nearer geo. */
		if ( depthNdc < 0.08 && histDepth > depthNdc + 0.02 ) {
			depthConf *= 0.25;
		}
	}

	float velocityConf = 1.0 - smoothstep( adaptive ? 2.5 : 4.0, adaptive ? 16.0 : 24.0, motionLen );
	if ( !mvValid && postfx.depthParams.z > 0.5 ) {
		velocityConf *= adaptive ? 0.15 : 0.35;
	}

	float currentLuma = max( dot( current, LUMA ), 0.0 );
	float historyLuma = max( dot( history, LUMA ), 0.0 );
	float lumaDiff = abs( currentLuma - historyLuma );
	float lumaConf = 1.0 - smoothstep( adaptive ? 0.018 : 0.028, adaptive ? 0.18 : 0.26, lumaDiff );

	/*
	 * Heuristic reactive always runs with Temporal Reconstruction.
	 * colorGrade2.w / midsGamma.a only gate the stamped OIT/transparent mask texture —
	 * not near-weapon, flash, or silhouette bleed (those caused the echoing glow when
	 * r_temporalReactiveMask was 0 or the mask was not allocated).
	 */
	float nearWeapon = smoothstep( 0.82, 0.995, depthNdc ); /* near in reversed-Z */
	float fastMotion = smoothstep( 3.5, 14.0, motionLen );
	float flash = smoothstep( 0.06, 0.28, lumaDiff );
	/* View-dependent / emissive peaks: current much brighter than history. */
	float highlightGhost = smoothstep( 0.10, 0.60, currentLuma - historyLuma ) *
		smoothstep( 0.15, 1.10, currentLuma );
	/* Dark geo over former bright history (skyline / HOST banner trails). */
	float historyBleed = smoothstep( 0.06, 0.45, historyLuma - currentLuma ) *
		smoothstep( 0.12, 0.85, historyLuma );
	float reactive = clamp( max( nearWeapon,
		max( fastMotion * 0.90,
		max( flash, max( highlightGhost * 0.95, historyBleed * 0.98 ) ) ) ), 0.0, 1.0 );
	if ( !mvValid && postfx.depthParams.z > 0.5 ) {
		reactive = max( reactive, adaptive ? 0.98 : 0.90 );
	}
	if ( useReactive > 0.5 && postfx.midsGamma.a > 0.5 ) {
		float stamped = textureLod( reactiveMaskTex, sampleUV, 0.0 ).r;
		/* Any meaningful transparent/OIT stamp fully prefers current. */
		if ( stamped > 0.02 ) {
			reactive = max( reactive, max( stamped, 0.95 ) );
		}
	}
	float reactiveConf = 1.0 - reactive;

	float confidence = clamp( depthConf * velocityConf * lumaConf * reactiveConf, 0.0, 1.0 );
	/* Hard reject when reactive is high — do not blend a fixed global weight. */
	float reactiveHard = adaptive ? 0.65 : 0.82;
	if ( reactive > reactiveHard ) {
		confidence = 0.0;
	}
	if ( adaptive && depthConf < 0.35 ) {
		confidence = 0.0; /* immediate disocclusion → current-frame spatial */
	}

	/* Difficult-pixel mask for bounded extra current-frame samples. */
	float difficult = clamp( max( 1.0 - confidence,
		max( reactive, max( 1.0 - depthConf, motionFactor ) ) ), 0.0, 1.0 );
	bool wantExtraSamples = adaptive && difficult > ( 1.0 - adaptBudget * 0.85 );

	if ( debugMode > 1.5 ) {
		/* Rejection reason / ownership viz (r_debugHistoryRejection / r_temporalDebugView). */
		float dbg = debugMode;
		if ( dbg > 8.5 && dbg < 9.5 ) {
			/* 9 = adaptive current-frame sample mask */
			float m = wantExtraSamples ? 1.0 : difficult;
			out_color = vec4( m, m * 0.4, 1.0 - m, 1.0 );
			return;
		}
		if ( dbg > 9.5 && dbg < 10.5 ) {
			/* 10 = current (R) vs history (G) contribution proxy */
			float histW = clamp( confidence * 0.5, 0.0, 1.0 );
			out_color = vec4( 1.0 - histW, histW, 0.15, 1.0 );
			return;
		}
		if ( dbg > 10.5 && dbg < 11.5 ) {
			/* 11 = neighborhood luminance variance */
			vec3 meanY, sigmaY;
			neighborhoodYCoCgStats( sampleUV, meanY, sigmaY );
			float v = clamp( sigmaY.x * 4.0, 0.0, 1.0 );
			out_color = vec4( v, v * 0.5, 0.1, 1.0 );
			return;
		}
		if ( dbg > 11.5 && dbg < 12.5 ) {
			/* 12 = unclipped history vs current delta */
			float d = clamp( lumaDiff * 3.0, 0.0, 1.0 );
			out_color = vec4( d, 0.2, 1.0 - d, 1.0 );
			return;
		}
		if ( dbg > 2.5 && dbg < 3.5 ) {
			/* 3 = reactive mask */
			out_color = vec4( reactive, reactive * 0.35, 1.0 - reactive, 1.0 );
			return;
		}
		if ( dbg > 3.5 && dbg < 4.5 ) {
			/* 4 = history weight / confidence */
			out_color = vec4( confidence, confidence, confidence, 1.0 );
			return;
		}
		if ( dbg > 4.5 && dbg < 5.5 ) {
			/* 5 = disocclusion (1 - depthConf) */
			out_color = vec4( 1.0 - depthConf, 0.2, 0.2, 1.0 );
			return;
		}
		if ( dbg > 5.5 && dbg < 6.5 ) {
			/* 6 = reprojected history UV */
			out_color = vec4( historyUV, 0.0, 1.0 );
			return;
		}
		if ( dbg > 6.5 && dbg < 7.5 ) {
			/* 7 = near-weapon heuristic mask */
			float nearWeaponDbg = smoothstep( 0.90, 0.998, depthNdc );
			out_color = vec4( nearWeaponDbg, 0.15, 0.15, 1.0 );
			return;
		}
		if ( dbg > 7.5 && dbg < 8.5 ) {
			/* 8 = world vs reactive ownership */
			out_color = vec4( reactive > 0.5 ? vec3( 1.0, 0.85, 0.1 ) : vec3( 0.15, 0.35, 1.0 ), 1.0 );
			return;
		}
		if ( reactive > 0.55 ) {
			out_color = vec4( 1.0, 1.0, 0.0, 1.0 ); /* yellow reactive */
			return;
		}
		if ( depthConf < 0.45 ) {
			out_color = vec4( 1.0, 0.0, 0.0, 1.0 ); /* red depth */
			return;
		}
		if ( !mvValid && postfx.depthParams.z > 0.5 ) {
			out_color = vec4( 1.0, 0.0, 1.0, 1.0 ); /* magenta MV */
			return;
		}
		if ( lumaConf < 0.45 ) {
			out_color = vec4( 0.0, 1.0, 1.0, 1.0 ); /* cyan luma */
			return;
		}
		if ( velocityConf < 0.45 ) {
			out_color = vec4( 0.2, 0.4, 1.0, 1.0 ); /* blue velocity proxy */
			return;
		}
		out_color = vec4( 0.0, 1.0, 0.0, 1.0 ); /* green accept */
		return;
	}

	float baseStationary = clamp( postfx.taaParams.y, 0.0, 0.95 );
	float baseMotion = clamp( postfx.taaParams.z, 0.0, 0.95 );
	float baseWeight = mix( baseStationary, baseMotion, motionFactor );
	/* Cap via depthParams.w carrying r_temporalHistoryWeight */
	float maxHist = clamp( postfx.depthParams.w, 0.0, 0.95 );
	baseWeight = min( baseWeight, maxHist );
	if ( adaptive ) {
		baseWeight = min( baseWeight, 0.42 );
	}

	float feedback = clamp( baseWeight * confidence, 0.0, adaptive ? 0.42 : 0.95 );

	vec3 historyClipped = history;
	if ( useVarClip > 0.5 ) {
		vec3 meanY, sigmaY;
		neighborhoodYCoCgStats( sampleUV, meanY, sigmaY );
		/* Tighten further on highlight / reactive pixels so specular trails cannot stick. */
		float highlightTighten = smoothstep( 0.20, 1.10, currentLuma );
		float gamma = mix( 1.25, 0.55, max( motionFactor, reactive ) );
		gamma = mix( gamma, gamma * 0.72, highlightTighten );
		if ( adaptive ) {
			gamma *= 0.78; /* tighter clip — current neighborhood owns bounds */
		}
		vec3 lo = meanY - gamma * sigmaY;
		vec3 hi = meanY + gamma * sigmaY;
		/* Tighter luminance (Y) than chroma */
		lo.x = meanY.x - gamma * sigmaY.x * 0.85;
		hi.x = meanY.x + gamma * sigmaY.x * 0.85;
		vec3 histY = RGBToYCoCg( historyClipped );
		histY = clamp( histY, lo, hi );
		historyClipped = max( YCoCgToRGB( histY ), vec3( 0.0 ) );
	} else {
		/* Legacy RGB neighborhood clamp fallback */
		vec3 mn = vec3( 1e30 );
		vec3 mx = vec3( -1e30 );
		for ( int y = -1; y <= 1; ++y ) {
			for ( int x = -1; x <= 1; ++x ) {
				vec3 c = sampleCurrent( sampleUV + vec2( x, y ) * texel );
				mn = min( mn, c );
				mx = max( mx, c );
			}
		}
		historyClipped = clamp( historyClipped, mn, mx );
	}

	vec3 currentSample = current;
	if ( wantExtraSamples && adaptSpatial ) {
		currentSample = spatialCurrentFallback( sampleUV );
	} else if ( confidence < 0.08 && adaptSpatial ) {
		currentSample = spatialCurrentFallback( sampleUV );
	}

	vec3 resolved = mix( currentSample, historyClipped, feedback );
	resolved = applyResolveSharpen( sampleUV, resolved );
	out_color = vec4( resolved, 1.0 );
}
