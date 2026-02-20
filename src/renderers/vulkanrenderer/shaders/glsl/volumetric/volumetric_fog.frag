#version 450

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D depthTexture;
layout(binding = 2) uniform sampler3D froxelVolume;

layout(std140, binding = 3) uniform VolumetricParams {
	mat4 invProj;
	mat4 invView;
	mat4 prevView;
	mat4 prevProj;
	vec4 viewOrigin;
	vec4 sunDirection;
	vec4 fogColor;
	vec4 densityParams;
	vec4 sliceParams;
	vec4 prevSliceParams;
	vec4 resolution;
	vec4 jitter;
	vec4 misc;
} params;

float hash12(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

void main() {
	float depthSample = texture(depthTexture, v_UV).r;
	float nearPlane = params.sliceParams.x;
	float farPlane = params.sliceParams.y;
	// Engine uses reversed depth and encodes depth approximately as zNear / viewZ.
	float sceneDepth = depthSample > 0.0 ? nearPlane / depthSample : farPlane;
	float logFar = max(params.sliceParams.z, 1e-4);
	float sliceCount = max(params.sliceParams.w, 1.0);

	int rawSteps = int(clamp(params.misc.x, 1.0, 64.0));
	int steps = max(1, rawSteps);
	float distance = max(sceneDepth - nearPlane, 0.0);
	float stepDistance = distance / float(steps);
	float transmittance = 1.0;
	vec3 fogAccum = vec3(0.0);

	for ( int i = 0; i < 64; ++i ) {
		if ( i >= steps ) {
			break;
		}

		// Sample along the ray in view-space distance, then map the distance to the froxel z-slice.
		// This reduces visible banding compared to stepping in slice-index space.
		float stepJitter = 0.0;
		if ( params.jitter.x > 0.0 ) {
			stepJitter = hash12( gl_FragCoord.xy + vec2( float(i) * 13.37, params.misc.z * 17.11 ) ) - 0.5;
		}
		float sampleDepth = nearPlane + (float(i) + 0.5 + stepJitter) * stepDistance;
		sampleDepth = clamp(sampleDepth, nearPlane, farPlane);

		float sliceNormalized = clamp(log(max(sampleDepth / nearPlane, 1e-4)) / logFar, 0.0, 1.0);
		vec4 vol = texture(froxelVolume, vec3(v_UV, sliceNormalized));
		fogAccum += transmittance * vol.rgb * stepDistance;
		float extinction = vol.a;
		transmittance *= exp( -extinction * stepDistance );
		if ( transmittance <= 0.01 ) {
			break;
		}
	}

	// Small screen-space dither helps hide low-amplitude quantization bands.
	float dither = hash12(gl_FragCoord.xy + params.misc.z) - 0.5;
	fogAccum += dither * (1.0 / 1024.0);
	fogAccum = max(fogAccum, vec3(0.0));

	float fogOpacity = clamp( 1.0 - transmittance, 0.0, 1.0 );
	fragColor = vec4( fogAccum, fogOpacity );
}
