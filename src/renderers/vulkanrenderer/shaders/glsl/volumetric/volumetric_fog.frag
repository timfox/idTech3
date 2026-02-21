#version 450

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D sceneColor;
layout(binding = 1) uniform sampler2D depthTexture;
layout(binding = 2) uniform sampler3D froxelVolume;

layout(std140, binding = 3) uniform VolumetricParams {
	mat4 invProj;
	mat4 invView;
	vec4 viewOrigin;
	vec4 sunDirection;
	vec4 fogColor;
	vec4 densityParams;
	vec4 worldMin;
	vec4 worldMax;
	vec4 gridDim;
	vec4 miscParams;
	vec4 phaseParams;
} params;

const float PI = 3.14159265359;

float hash12(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

float phaseHG(float cosTheta, float g) {
	float gg = g * g;
	float denom = max(1.0 + gg - 2.0 * g * cosTheta, 1e-5);
	return (1.0 - gg) / (4.0 * PI * pow(denom, 1.5));
}

float decodeClipZ(float depthSample, int depthMode) {
	if ( depthMode == 0 ) {
		return depthSample * 2.0 - 1.0;
	}
	if ( depthMode == 1 ) {
		return ( 1.0 - depthSample ) * 2.0 - 1.0;
	}
	return 0.0;
}

vec3 reconstructViewRay(vec2 uv) {
	vec4 clipFar = vec4( uv * 2.0 - 1.0, 1.0, 1.0 );
	vec4 viewFar = params.invProj * clipFar;
	return viewFar.xyz / max( abs( viewFar.w ), 1e-6 );
}

vec3 reconstructViewPos(
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
		return viewDir * sceneDepth;
	}

	vec4 clip = vec4( uv * 2.0 - 1.0, decodeClipZ( depthSample, depthMode ), 1.0 );
	vec4 view = params.invProj * clip;
	vec3 viewPos = view.xyz / max( abs( view.w ), 1e-6 );
	viewDir = normalize( viewPos );
	sceneDepth = clamp( length( viewPos ), nearPlane, farPlane );
	return viewPos;
}

bool boundsValid(out vec3 extent) {
	extent = params.worldMax.xyz - params.worldMin.xyz;
	return all( greaterThan( extent, vec3( 1e-4 ) ) );
}

void main() {
	const float nearPlane = 0.05;
	const float maxFar = 16384.0;
	float depthSample = texture( depthTexture, v_UV ).r;
	int depthMode = int( clamp( floor( params.miscParams.y + 0.5 ), 0.0, 2.0 ) );
	float frameIndex = params.miscParams.z;
	int fogDebug = int( clamp( floor( params.gridDim.w + 0.5 ), 0.0, 5.0 ) );

	if ( fogDebug == 1 ) {
		vec3 scene = texture( sceneColor, v_UV ).rgb;
		fragColor = vec4( mix( scene, vec3( 1.0, 0.0, 1.0 ), 0.35 ), 1.0 );
		return;
	}
	if ( fogDebug == 2 ) {
		fragColor = texture( froxelVolume, vec3( v_UV, 0.5 ) );
		return;
	}
	if ( fogDebug == 3 ) {
		float a = texture( froxelVolume, vec3( v_UV, 0.5 ) ).a;
		fragColor = vec4( a, a, a, 1.0 );
		return;
	}

	int rawSteps = int( clamp( params.miscParams.x, 1.0, 128.0 ) );
	int steps = max( 1, rawSteps );
	float transmittance = 1.0;
	vec3 fogAccum = vec3( 0.0 );
	vec3 extent;
	bool worldBounds = boundsValid( extent );
	vec3 viewDir;
	float sceneDepth = maxFar;
	reconstructViewPos( v_UV, depthSample, depthMode, nearPlane, maxFar, viewDir, sceneDepth );
	sceneDepth = clamp( sceneDepth, nearPlane, maxFar );
	if ( fogDebug == 5 ) {
		float depthNumer = log2( 1.0 + max( sceneDepth, 0.0 ) );
		float depthDenom = max( log2( 1.0 + maxFar ), 1e-5 );
		float depthGray = clamp( depthNumer / depthDenom, 0.0, 1.0 );
		fragColor = vec4( depthGray, depthGray, depthGray, 1.0 );
		return;
	}
	if ( fogDebug == 4 ) {
		if ( !worldBounds ) {
			fragColor = vec4( 1.0, 0.0, 0.0, 1.0 );
			return;
		}
		float nearTDbg = nearPlane;
		float hitTDbg = sceneDepth;
		float midTDbg = mix( nearTDbg, hitTDbg, 0.5 );
		vec3 worldDirDbg = normalize( ( params.invView * vec4( viewDir, 0.0 ) ).xyz );
		vec3 worldPosDbg = params.viewOrigin.xyz + worldDirDbg * midTDbg;
		vec3 uvwDbg = ( worldPosDbg - params.worldMin.xyz ) / extent;
		bool inRange = all( greaterThanEqual( uvwDbg, vec3( 0.0 ) ) ) && all( lessThanEqual( uvwDbg, vec3( 1.0 ) ) );
		fragColor = inRange ? vec4( 0.0, 1.0, 0.0, 1.0 ) : vec4( 1.0, 0.0, 0.0, 1.0 );
		return;
	}

	if ( worldBounds ) {
		float nearT = nearPlane;
		float hitT = sceneDepth;
		float totalDistance = max( hitT - nearT, 0.0 );
		float ds = totalDistance / float( steps );
		vec3 worldDir = normalize( ( params.invView * vec4( viewDir, 0.0 ) ).xyz );
		vec3 worldCam = params.viewOrigin.xyz;

		for ( int i = 0; i < 128; ++i ) {
			if ( i >= steps ) {
				break;
			}

			float jitterStep = ( hash12( gl_FragCoord.xy + vec2( frameIndex, float( i ) * 17.0 ) ) - 0.5 ) * clamp( params.densityParams.z, 0.0, 1.0 );

			float t = nearT + ( float( i ) + 0.5 + jitterStep ) * ds;
			t = clamp( t, nearT, hitT );
			vec3 worldPos = worldCam + worldDir * t;
			vec3 uvw = ( worldPos - params.worldMin.xyz ) / extent;
			if ( any( lessThan( uvw, vec3( 0.0 ) ) ) || any( greaterThan( uvw, vec3( 1.0 ) ) ) ) {
				continue;
			}

			vec4 vol = texture( froxelVolume, uvw );
			float sigma_t = max( vol.a, 0.0 );
			vec3 inscatter = max( vol.rgb, vec3( 0.0 ) );
			if ( dot( inscatter, inscatter ) < 1e-8 && sigma_t > 0.0 ) {
				float cosTheta = dot( normalize( params.sunDirection.xyz ), -worldDir );
				float phase = phaseHG( cosTheta, clamp( params.phaseParams.x, -0.999, 0.999 ) );
				vec3 ambient = params.fogColor.rgb * params.phaseParams.z;
				vec3 directional = params.fogColor.rgb * params.phaseParams.y * phase;
				inscatter = ( ambient + directional ) * sigma_t * params.fogColor.a;
			}

			fogAccum += transmittance * inscatter * ds;
			transmittance *= exp( -sigma_t * ds );
			if ( transmittance <= 0.01 ) {
				break;
			}
		}
	} else {
		float stepDistance = max( sceneDepth - nearPlane, 0.0 ) / float( steps );

		for ( int i = 0; i < 128; ++i ) {
			if ( i >= steps ) {
				break;
			}

			float stepJitter = ( hash12( gl_FragCoord.xy + vec2( frameIndex, float( i ) * 17.0 ) ) - 0.5 ) * clamp( params.densityParams.z, 0.0, 1.0 );
			float sampleDepth = nearPlane + ( float( i ) + 0.5 + stepJitter ) * stepDistance;
			float sliceNormalized = clamp( sampleDepth / max( sceneDepth, 1e-4 ), 0.0, 1.0 );
			vec4 vol = texture( froxelVolume, vec3( v_UV, sliceNormalized ) );
			fogAccum += transmittance * vol.rgb * stepDistance;
			transmittance *= exp( -max( vol.a, 0.0 ) * stepDistance );
			if ( transmittance <= 0.01 ) {
				break;
			}
		}
	}

	float dither = hash12( gl_FragCoord.xy + frameIndex ) - 0.5;
	fogAccum = max( fogAccum + dither * ( 1.0 / 1024.0 ), vec3( 0.0 ) );

	float fogOpacity = clamp( 1.0 - transmittance, 0.0, 1.0 );
	vec3 scene = texture( sceneColor, v_UV ).rgb;
	vec3 outRgb = scene * ( 1.0 - fogOpacity ) + fogAccum;
	fragColor = vec4( outRgb, 1.0 );
}
