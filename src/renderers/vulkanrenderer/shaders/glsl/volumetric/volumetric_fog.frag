#version 450

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D sceneColor;
layout(binding = 1) uniform sampler2D depthTexture;
layout(binding = 2) uniform sampler3D froxelVolume;

layout(std140, binding = 3) uniform VolumetricParams {
	mat4 invProj;
	mat4 invView;
	mat4 proj;
	mat4 viewProj;
	mat4 prevView;
	mat4 prevViewProj;
	vec4 viewOrigin;
	vec4 sunDirection;
	vec4 fogColor;
	vec4 densityParams;
	vec4 worldMin;
	vec4 worldMax;
	vec4 gridDim;
	vec4 miscParams;
	vec4 sliceParams;
	vec4 phaseParams;
	vec4 noiseParams;
	vec4 noiseScroll;
} params;

const int MAX_Z_SLICES = 128;

float hash12( vec2 p ) {
	vec3 p3 = fract( vec3( p.xyx ) * 0.1031 );
	p3 += dot( p3, p3.yzx + 33.33 );
	return fract( ( p3.x + p3.y ) * p3.z );
}

float decodeClipZ( float depthSample, int depthMode ) {
	if ( depthMode == 0 ) {
		return depthSample * 2.0 - 1.0;
	}
	if ( depthMode == 1 ) {
		return depthSample * 2.0 - 1.0;
	}
	return 0.0;
}

float getNearPlane() {
	return max( params.sliceParams.x, 0.001 );
}

float getFarPlane( float nearPlane ) {
	return max( params.sliceParams.y, nearPlane + 1.0 );
}

float getLogRatio( float nearPlane, float farPlane ) {
	return max( params.sliceParams.z, log( farPlane / nearPlane ) );
}

float sliceDepthFromNorm( float sliceNorm, float nearPlane, float logRatio ) {
	return nearPlane * exp( logRatio * clamp( sliceNorm, 0.0, 1.0 ) );
}

vec3 reconstructViewRay( vec2 uv ) {
	vec4 clipFar = vec4( uv * 2.0 - 1.0, 1.0, 1.0 );
	vec4 viewFar = params.invProj * clipFar;
	return viewFar.xyz / max( abs( viewFar.w ), 1e-6 );
}

void reconstructViewRayAndDepth(
	vec2 uv,
	float depthSample,
	int depthMode,
	float nearPlane,
	float farPlane,
	out vec3 viewDir,
	out float sceneDepth )
{
	vec3 viewRay = reconstructViewRay( uv );
	viewDir = normalize( viewRay );

	if ( depthMode == 2 ) {
		float linearDepth = depthSample;
		if ( linearDepth <= 1.0 ) {
			linearDepth = mix( nearPlane, farPlane, clamp( linearDepth, 0.0, 1.0 ) );
		}
		sceneDepth = clamp( linearDepth, nearPlane, farPlane );
		return;
	}

	vec4 clip = vec4( uv * 2.0 - 1.0, decodeClipZ( depthSample, depthMode ), 1.0 );
	vec4 view = params.invProj * clip;
	vec3 viewPos = view.xyz / max( abs( view.w ), 1e-6 );
	viewDir = normalize( viewPos );
	sceneDepth = clamp( max( -viewPos.z, nearPlane ), nearPlane, farPlane );
}

void main() {
	float depthSample = texture( depthTexture, v_UV ).r;
	int depthMode = int( clamp( floor( params.miscParams.y + 0.5 ), 0.0, 2.0 ) );
	int fogDebug = int( clamp( floor( params.gridDim.w + 0.5 ), 0.0, 5.0 ) );
	float frameIndex = params.miscParams.z;
	float nearPlane = getNearPlane();
	float farPlane = getFarPlane( nearPlane );
	float logRatio = getLogRatio( nearPlane, farPlane );
	int zSlices = int( clamp( floor( params.gridDim.z + 0.5 ), 1.0, float( MAX_Z_SLICES ) ) );
	float jitterStrength = max( params.densityParams.z, 0.0 );

	vec3 viewDir;
	float sceneDepth;
	reconstructViewRayAndDepth( v_UV, depthSample, depthMode, nearPlane, farPlane, viewDir, sceneDepth );

	vec3 scene = texture( sceneColor, v_UV ).rgb;
	float jitter = ( hash12( gl_FragCoord.xy + frameIndex ) - 0.5 ) * jitterStrength;

	if ( fogDebug == 1 ) {
		float sliceNorm = clamp( log( max( sceneDepth / nearPlane, 1e-4 ) ) / logRatio, 0.0, 1.0 );
		fragColor = vec4( vec3( v_UV, sliceNorm ), 1.0 );
		return;
	}
	if ( fogDebug == 2 ) {
		float sigma = texture( froxelVolume, vec3( v_UV, 0.5 ) ).a;
		fragColor = vec4( sigma, sigma, sigma, 1.0 );
		return;
	}
	if ( fogDebug == 3 ) {
		vec3 scatter = texture( froxelVolume, vec3( v_UV, 0.5 ) ).rgb;
		fragColor = vec4( scatter, 1.0 );
		return;
	}
	if ( fogDebug == 4 ) {
		float temporalWeight = clamp( params.densityParams.w, 0.0, 1.0 );
		float hasHistory = params.miscParams.w > 0.5 ? 1.0 : 0.0;
		fragColor = vec4( hasHistory, temporalWeight, 0.0, 1.0 );
		return;
	}

	float transmittance = 1.0;
	vec3 fogRadiance = vec3( 0.0 );
	for ( int i = 0; i < MAX_Z_SLICES; ++i ) {
		if ( i >= zSlices ) {
			break;
		}

		float s0 = float( i ) / float( zSlices );
		float s1 = float( i + 1 ) / float( zSlices );
		float z0 = sliceDepthFromNorm( s0, nearPlane, logRatio );
		float z1 = sliceDepthFromNorm( s1, nearPlane, logRatio );
		float t0 = max( z0, nearPlane );
		float t1 = min( z1, sceneDepth );
		if ( t1 <= t0 ) {
			continue;
		}

		float sCenter = ( float( i ) + 0.5 + jitter ) / float( zSlices );
		vec3 uvw = vec3( v_UV, clamp( sCenter, 0.0, 1.0 ) );
		vec4 media = texture( froxelVolume, uvw );
		float sigmaT = max( media.a, 0.0 );
		vec3 scatterSource = max( media.rgb, vec3( 0.0 ) );

		float dt = t1 - t0;
		float prevT = transmittance;
		transmittance *= exp( -sigmaT * dt );
		fogRadiance += prevT * scatterSource * dt;

		if ( transmittance <= 0.01 ) {
			break;
		}
	}

	if ( fogDebug == 5 ) {
		fragColor = vec4( vec3( clamp( transmittance, 0.0, 1.0 ) ), 1.0 );
		return;
	}

	vec3 outRgb = scene * transmittance + fogRadiance;
	fragColor = vec4( outRgb, 1.0 );
}
