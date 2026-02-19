#version 450

layout(location = 0) in vec2 v_UV;
layout(location = 0) out vec4 fragColor;

layout(binding = 0) uniform sampler2D hdrColor;
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

void main() {
	vec3 hdr = texture(hdrColor, v_UV).rgb;
	float depthSample = texture(depthTexture, v_UV).r;
	float nearPlane = params.sliceParams.x;
	float farPlane = params.sliceParams.y;
	float sceneDepth = depthSample > 0.0 ? nearPlane / depthSample : farPlane;
	float logFar = max(params.sliceParams.z, 1e-4);
	float sliceCount = max(params.sliceParams.w, 1.0);
	float maxSlice = clamp(log(max(sceneDepth / nearPlane, 1e-4)) / logFar, 0.0, 1.0) * max(sliceCount - 1.0, 1.0);

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
		float sliceIdx = steps == 1 ? maxSlice : min(maxSlice, float(i) * (maxSlice / float(max(steps - 1, 1))));
		float normalizedSlice = sliceIdx / max(sliceCount - 1.0, 1.0);
		vec4 vol = texture(froxelVolume, vec3(v_UV, normalizedSlice));
		fogAccum += transmittance * vol.rgb * stepDistance;
		float extinction = vol.a;
		transmittance *= exp( -extinction * stepDistance );
		if ( transmittance <= 0.01 ) {
			break;
		}
	}

	vec3 result = hdr + fogAccum;
	fragColor = vec4(result, 1.0);
}
