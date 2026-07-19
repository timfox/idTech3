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

void main() {
	vec2 uv = frag_tex_coord;
	vec2 texel = texelSize();
	vec2 sampleUV = uv - postfx.lutParams.zw * texel;
	vec3 current = sampleCurrent( sampleUV );
	float depthNdc = textureLod( depthTex, sampleUV, 0.0 ).r;

	/* colorGrade2.yzw: Temporal Reconstruction flags (gamma uses .x = hue only).
	 * shadowsLift.a: debug mode (gamma uses .rgb only). */
	float useVarClip = postfx.colorGrade2.y;
	float useDisocc = postfx.colorGrade2.z;
	float useReactive = postfx.colorGrade2.w;
	float debugMode = postfx.shadowsLift.a;

	if ( postfx.taaParams.x < 0.5 || postfx.frameInfo.w < 0.5 || depthNdc <= 0.0 || depthNdc >= 1.0 ) {
		if ( debugMode > 1.5 ) {
			out_color = vec4( 1.0, 1.0, 1.0, 1.0 ); /* white = cut/reset */
			return;
		}
		out_color = vec4( current, 1.0 );
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
		/* Reversed-Z: larger = nearer; relative mismatch rejects. */
		depthConf = 1.0 - smoothstep( 0.002, 0.04, depthDelta );
	}

	float velocityConf = 1.0 - smoothstep( 4.0, 24.0, motionLen );
	if ( !mvValid && postfx.depthParams.z > 0.5 ) {
		velocityConf *= 0.35;
	}

	float lumaDiff = abs( dot( current - history, LUMA ) );
	float lumaConf = 1.0 - smoothstep( 0.04, 0.35, lumaDiff );

	/* Reactive: near-camera weapon/HUD-ish depth, fast motion, large luma change,
	 * plus stamped transparent/OIT/stochastic coverage when maskBound (midsGamma.a). */
	float reactive = 0.0;
	if ( useReactive > 0.5 ) {
		float nearWeapon = smoothstep( 0.92, 0.995, depthNdc ); /* near in reversed-Z */
		float fastMotion = smoothstep( 6.0, 20.0, motionLen );
		float flash = smoothstep( 0.15, 0.5, lumaDiff );
		reactive = clamp( max( nearWeapon, max( fastMotion * 0.75, flash ) ), 0.0, 1.0 );
		if ( postfx.midsGamma.a > 0.5 ) {
			float stamped = textureLod( reactiveMaskTex, sampleUV, 0.0 ).r;
			reactive = max( reactive, clamp( stamped, 0.0, 1.0 ) );
		}
	}
	float reactiveConf = 1.0 - reactive;

	float confidence = clamp( depthConf * velocityConf * lumaConf * reactiveConf, 0.0, 1.0 );

	if ( debugMode > 1.5 ) {
		/* Rejection reason viz */
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

	float feedback = clamp( baseWeight * confidence, 0.0, 0.95 );

	if ( useVarClip > 0.5 ) {
		vec3 meanY, sigmaY;
		neighborhoodYCoCgStats( sampleUV, meanY, sigmaY );
		float gamma = mix( 1.25, 0.55, max( motionFactor, reactive ) );
		vec3 lo = meanY - gamma * sigmaY;
		vec3 hi = meanY + gamma * sigmaY;
		/* Tighter luminance (Y) than chroma */
		lo.x = meanY.x - gamma * sigmaY.x * 0.85;
		hi.x = meanY.x + gamma * sigmaY.x * 0.85;
		vec3 histY = RGBToYCoCg( history );
		histY = clamp( histY, lo, hi );
		history = max( YCoCgToRGB( histY ), vec3( 0.0 ) );
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
		history = clamp( history, mn, mx );
	}

	vec3 resolved = mix( current, history, feedback );
	resolved = applyResolveSharpen( sampleUV, resolved );
	out_color = vec4( resolved, 1.0 );
}
