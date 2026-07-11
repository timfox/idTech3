#version 450

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

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

const vec3 LUMA = vec3( 0.2126, 0.7152, 0.0722 );

vec2 texelSize( void ) {
	return postfx.frameInfo.yz;
}

vec3 sampleCurrent( vec2 uv ) {
	return textureLod( currentColor, clamp( uv, 0.0, 1.0 ), 0.0 ).rgb;
}

vec2 reprojectHistoryUV( vec2 uv, float depthNdc ) {
	vec4 posClip = vec4( uv * 2.0 - 1.0, depthNdc, 1.0 );
	vec4 posWorld = postfx.invViewProj * posClip;
	posWorld /= max( posWorld.w, 1e-6 );
	vec4 prevClip = postfx.prevViewProj * posWorld;
	return prevClip.xy / max( prevClip.w, 1e-6 ) * 0.5 + 0.5;
}

void neighborhoodMinMax( vec2 uv, out vec3 mn, out vec3 mx, out vec3 avg ) {
	vec2 texel = texelSize();
	mn = vec3( 1e30 );
	mx = vec3( -1e30 );
	avg = vec3( 0.0 );

	for ( int y = -1; y <= 1; ++y ) {
		for ( int x = -1; x <= 1; ++x ) {
			vec3 c = sampleCurrent( uv + vec2( x, y ) * texel );
			mn = min( mn, c );
			mx = max( mx, c );
			avg += c;
		}
	}

	avg *= ( 1.0 / 9.0 );
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
	/* Unjitter sample UV when temporal upscale Halton offset is active (lutParams.zw in pixels). */
	vec2 texel = texelSize();
	vec2 sampleUV = uv - postfx.lutParams.zw * texel;
	vec3 current = sampleCurrent( sampleUV );
	float depthNdc = textureLod( depthTex, sampleUV, 0.0 ).r;

	if ( postfx.taaParams.x < 0.5 || postfx.frameInfo.w < 0.5 || depthNdc <= 0.0 || depthNdc >= 1.0 ) {
		out_color = vec4( current, 1.0 );
		return;
	}

	vec2 historyUV;
	if ( postfx.depthParams.z > 0.5 ) {
		vec2 motion = textureLod( motionTex, sampleUV, 0.0 ).rg;
		historyUV = sampleUV - motion;
	} else {
		historyUV = reprojectHistoryUV( sampleUV, depthNdc );
	}
	if ( any( lessThan( historyUV, vec2( 0.0 ) ) ) || any( greaterThan( historyUV, vec2( 1.0 ) ) ) ) {
		out_color = vec4( current, 1.0 );
		return;
	}

	vec3 history = textureLod( historyColor, historyUV, 0.0 ).rgb;
	vec3 mn, mx, avg;
	neighborhoodMinMax( sampleUV, mn, mx, avg );
	vec2 texSize = vec2( textureSize( currentColor, 0 ) );
	vec2 velocity = ( sampleUV - historyUV ) * texSize;
	float motion = length( velocity );
	float motionFactor = smoothstep( 0.2, 8.0, motion );
	float feedback = mix( clamp( postfx.taaParams.y, 0.0, 0.99 ),
		clamp( postfx.taaParams.z, 0.0, 0.99 ), motionFactor );
	float colorDiff = abs( dot( current - history, LUMA ) );

	history = clamp( history, mn, mx );
	feedback *= 1.0 - smoothstep( 0.03, 0.25, colorDiff );
	feedback = clamp( feedback, 0.0, 0.98 );

	{
		vec3 expandedMin = mix( avg, mn, 0.85 );
		vec3 expandedMax = mix( avg, mx, 0.85 );
		history = clamp( history, expandedMin, expandedMax );
	}

	vec3 resolved = mix( current, history, feedback );
	resolved = applyResolveSharpen( sampleUV, resolved );
	out_color = vec4( resolved, 1.0 );
}
