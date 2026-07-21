#version 450

/*
 * Screen-Space Reflections with confidence output.
 * Alpha channel encodes validated hit confidence (0 = miss / rejected).
 * Selective Hybrid Reflections uses this for SSR fallback weighting.
 */

layout(set = 0, binding = 0) uniform sampler2D colorTexture;
layout(set = 1, binding = 0) uniform sampler2D depthTexture;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform SSR_PC {
	mat4 projection;
	mat4 invProjection;
	vec4 params;
	vec4 params2;
} ssr;

const int MAX_STEPS = 64;
const int BINARY_STEPS = 8;

vec3 viewFromDepth(vec2 uv, float depth) {
	vec4 clip = vec4(uv * 2.0 - 1.0, depth, 1.0);
	vec4 view = ssr.invProjection * clip;
	return view.xyz / view.w;
}

vec2 projectToScreen(vec3 viewPos) {
	vec4 clip = ssr.projection * vec4(viewPos, 1.0);
	return (clip.xy / clip.w) * 0.5 + 0.5;
}

vec3 normalFromDepth(vec2 uv, vec2 invSize, float maxDepthGradient, out bool valid) {
	float d = texture(depthTexture, uv).r;
	float dx = texture(depthTexture, uv + vec2(invSize.x, 0.0)).r;
	float dy = texture(depthTexture, uv + vec2(0.0, invSize.y)).r;
	float dxm = texture(depthTexture, uv - vec2(invSize.x, 0.0)).r;
	float dym = texture(depthTexture, uv - vec2(0.0, invSize.y)).r;

	float maxDiff = max(max(abs(dx - d), abs(dxm - d)), max(abs(dy - d), abs(dym - d)));
	if (maxDiff > maxDepthGradient) {
		valid = false;
		return vec3(0.0);
	}

	vec3 p = viewFromDepth(uv, d);
	vec3 px = viewFromDepth(uv + vec2(invSize.x, 0.0), dx);
	vec3 py = viewFromDepth(uv + vec2(0.0, invSize.y), dy);
	vec3 pxm = viewFromDepth(uv - vec2(invSize.x, 0.0), dxm);
	vec3 pym = viewFromDepth(uv - vec2(0.0, invSize.y), dym);

	vec3 ddx = (px - pxm) * 0.5;
	vec3 ddy = (py - pym) * 0.5;
	vec3 normal = normalize(cross(ddx, ddy));
	valid = true;
	return normal;
}

void main() {
	float rawDepth = texture(depthTexture, frag_tex_coord).r;
	vec3 sceneColor = texture(colorTexture, frag_tex_coord).rgb;

	float maxDistance = ssr.params.x;
	float stepSize = ssr.params.y;
	float thickness = ssr.params.z;
	float fadeEdge = ssr.params.w;
	float roughnessThreshold = clamp(ssr.params2.x, 0.0, 1.0);
	float intensity = ssr.params2.y;
	float maxDepthGradient = ssr.params2.z;
	float fresnelExponent = ssr.params2.w;

	/* Negative fresnelExponent encodes r_temporalDebug while TAA is off. */
	if (fresnelExponent < -0.5) {
		int dbg = int(-fresnelExponent + 0.5);
		/* DEPTH_RANGE_WEAPON maps to viewport minDepth=0.6 (reverse-Z near). */
		float weaponMask = smoothstep(0.58, 0.62, rawDepth);
		if (dbg == 5) {
			vec3 overlay = mix(sceneColor, vec3(1.0, 0.15, 0.08), weaponMask);
			out_color = vec4(overlay, 1.0);
			return;
		}
		if (dbg == 2 || dbg == 4) {
			/* Depth / disocclusion proxy: current depth grayscale + weapon tint. */
			vec3 depthVis = vec3(rawDepth);
			out_color = vec4(mix(depthVis, vec3(1.0, 0.2, 0.2), weaponMask), 1.0);
			return;
		}
		if (dbg == 12 || dbg == 13) {
			bool bad = any(isnan(sceneColor)) || any(isinf(sceneColor)) || isnan(rawDepth) || isinf(rawDepth);
			out_color = bad ? vec4(1.0, 0.0, 1.0, 1.0) : vec4(sceneColor, 1.0);
			return;
		}
		/* Fall through for other modes — still run SSR so confidence can be shown. */
		fresnelExponent = max(abs(fresnelExponent), 0.5);
	} else {
		fresnelExponent = max(fresnelExponent, 0.5);
	}

	if (rawDepth <= 0.0 || rawDepth >= 1.0) {
		out_color = vec4(sceneColor, 0.0);
		return;
	}

	vec3 viewPos = viewFromDepth(frag_tex_coord, rawDepth);
	vec2 invSize = vec2(1.0) / textureSize(depthTexture, 0);
	bool normalValid;
	vec3 normal = normalFromDepth(frag_tex_coord, invSize, maxDepthGradient, normalValid);

	if (!normalValid || length(normal) < 0.1) {
		out_color = vec4(sceneColor, 0.0);
		return;
	}
	normal = normalize(normal);

	vec3 viewDir = normalize(viewPos);
	float fresnelSSRWeight = 1.0;
	if (roughnessThreshold > 0.0) {
		float NdotV = clamp(dot(normal, normalize(-viewPos)), 0.0, 1.0);
		float grazing = pow(max(1.0 - NdotV, 0.0), fresnelExponent);
		fresnelSSRWeight = mix(1.0, grazing, roughnessThreshold);
	}
	vec3 reflectDir = reflect(viewDir, normal);
	/* Camera-facing / backface reject */
	if (dot(reflectDir, -viewDir) < 0.0) {
		out_color = vec4(sceneColor, 0.0);
		return;
	}

	vec3 rayPos = viewPos;
	vec3 rayStep = reflectDir * stepSize;

	vec2 hitUV = vec2(-1.0);
	float hitConfidence = 0.0;
	float hitDist = 0.0;

	for (int i = 0; i < MAX_STEPS; i++) {
		rayPos += rayStep;

		vec2 screenPos = projectToScreen(rayPos);
		if (screenPos.x < 0.0 || screenPos.x > 1.0 || screenPos.y < 0.0 || screenPos.y > 1.0) {
			break;
		}

		float sampleDepth = texture(depthTexture, screenPos).r;
		vec3 sampleViewPos = viewFromDepth(screenPos, sampleDepth);
		float diff = rayPos.z - sampleViewPos.z;

		if (diff > 0.0 && diff < thickness) {
			vec3 binaryPos = rayPos;
			vec3 binaryStep = rayStep * 0.5;

			for (int j = 0; j < BINARY_STEPS; j++) {
				binaryPos -= binaryStep;
				binaryStep *= 0.5;
				vec2 bUV = projectToScreen(binaryPos);
				float bDepth = texture(depthTexture, bUV).r;
				vec3 bViewPos = viewFromDepth(bUV, bDepth);
				if (binaryPos.z > bViewPos.z) {
					binaryPos += binaryStep * 2.0;
				}
			}

			hitUV = projectToScreen(binaryPos);
			hitDist = length(rayPos - viewPos);
			float distFade = 1.0 - clamp(hitDist / maxDistance, 0.0, 1.0);

			float edgeFade = 1.0;
			vec2 edgeDist = abs(hitUV - 0.5) * 2.0;
			edgeFade *= 1.0 - smoothstep(1.0 - fadeEdge, 1.0, edgeDist.x);
			edgeFade *= 1.0 - smoothstep(1.0 - fadeEdge, 1.0, edgeDist.y);

			/* Thickness / depth agreement */
			float thicknessConf = 1.0 - clamp(diff / max(thickness, 1e-3), 0.0, 1.0);
			/* Reject self-intersection */
			float selfConf = smoothstep(stepSize * 0.5, stepSize * 2.0, hitDist);

			hitConfidence = distFade * edgeFade * thicknessConf * selfConf * intensity * fresnelSSRWeight;
			break;
		}
	}

	if (hitConfidence > 0.001 && hitUV.x >= 0.0 && hitUV.x <= 1.0 && hitUV.y >= 0.0 && hitUV.y <= 1.0) {
		vec3 reflColor = texture(colorTexture, hitUV).rgb;
		float w = clamp(hitConfidence, 0.0, 1.0);
		out_color = vec4(mix(sceneColor, reflColor, w), w);
	} else {
		out_color = vec4(sceneColor, 0.0);
	}
}
