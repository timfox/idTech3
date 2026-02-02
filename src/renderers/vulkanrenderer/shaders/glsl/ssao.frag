#version 450

layout(set = 0, binding = 0) uniform sampler2D depthTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform SSAOParams {
	vec4 projInfo; // invProj00, invProj11, proj10, proj14
	vec4 params;   // radius, bias, intensity, power
	vec4 misc;     // samples, invWidth, invHeight, depthIsReversed
} pc;

float rand01(vec2 co)
{
	return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

float viewZFromDepth(float depth)
{
	// Vulkan depth is 0..1, view space Z is negative forward.
	return -pc.projInfo.w / max(depth + pc.projInfo.z, 1e-6);
}

void main()
{
	float depth = texture(depthTex, frag_tex_coord).r;
	if ((pc.misc.w > 0.5 && depth <= 0.001) || (pc.misc.w <= 0.5 && depth >= 0.999)) {
		out_color = vec4(1.0);
		return;
	}

	float viewZ = viewZFromDepth(depth);
	float negZ = -viewZ;
	vec2 ndc = frag_tex_coord * 2.0 - 1.0;
	vec3 viewPos = vec3(ndc.x * negZ * pc.projInfo.x, ndc.y * negZ * pc.projInfo.y, viewZ);

	// normal from derivatives
	vec3 dx = dFdx(viewPos);
	vec3 dy = dFdy(viewPos);
	vec3 normal = normalize(cross(dx, dy));
	if (normal.z < 0.0) {
		normal = -normal;
	}

	// build TBN for hemisphere sampling
	vec3 randVec = normalize(vec3(
		rand01(frag_tex_coord * 13.1),
		rand01(frag_tex_coord * 31.7),
		rand01(frag_tex_coord * 57.3)
	) * 2.0 - 1.0);

	vec3 tangent = normalize(randVec - normal * dot(randVec, normal));
	vec3 bitangent = cross(normal, tangent);
	mat3 tbn = mat3(tangent, bitangent, normal);

	float radius = pc.params.x;
	float bias = pc.params.y;
	float intensity = pc.params.z;
	float power = pc.params.w;
	int samples = int(pc.misc.x);

	float occlusion = 0.0;
	for (int i = 0; i < 32; ++i) {
		if (i >= samples) {
			break;
		}

		float fi = float(i);
		vec2 noise = vec2(rand01(vec2(fi, frag_tex_coord.x + 0.1)), rand01(vec2(frag_tex_coord.y + 0.2, fi)));
		vec3 sampleDir = normalize(vec3(noise, rand01(vec2(fi + 0.3, frag_tex_coord.y))) * 2.0 - 1.0);
		if (sampleDir.z < 0.0) {
			sampleDir.z = -sampleDir.z;
		}

		vec3 samplePos = viewPos + (tbn * sampleDir) * radius;

		float proj00 = 1.0 / max(pc.projInfo.x, 1e-6);
		float proj11 = 1.0 / max(pc.projInfo.y, 1e-6);
		vec2 sampleNdc = vec2(samplePos.x * proj00 / -samplePos.z, samplePos.y * proj11 / -samplePos.z);
		vec2 sampleUV = sampleNdc * 0.5 + 0.5;

		if (sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0) {
			continue;
		}

		float sampleDepth = texture(depthTex, sampleUV).r;
		float sampleViewZ = viewZFromDepth(sampleDepth);
		float rangeCheck = smoothstep(0.0, 1.0, radius / max(abs(viewZ - sampleViewZ), 1e-3));
		if ((sampleViewZ - samplePos.z) > bias) {
			occlusion += rangeCheck;
		}
	}

	occlusion = clamp(occlusion / max(float(samples), 1.0), 0.0, 1.0);
	float ao = 1.0 - occlusion;
	ao = pow(ao, power);
	ao = clamp(1.0 - (1.0 - ao) * intensity, 0.0, 1.0);

	out_color = vec4(ao, ao, ao, 1.0);
}
