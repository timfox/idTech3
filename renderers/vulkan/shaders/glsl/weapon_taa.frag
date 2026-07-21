#version 450

layout(set = 0, binding = 0) uniform sampler2D currentCombinedColor;
layout(set = 1, binding = 0) uniform sampler2D currentDepth;
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
	vec4 temporalValidity;
	vec4 weaponTemporalParams;
	vec4 temporalDebugParams;
} postfx;
layout(set = 3, binding = 0) uniform sampler2D previousWeaponHistory;
layout(set = 4, binding = 0) uniform sampler2D motionTex;
layout(set = 5, binding = 0) uniform sampler2D reactiveMaskTex;
layout(set = 6, binding = 0) uniform sampler2D previousClassTex;
layout(set = 7, binding = 0) uniform sampler2D previousWeaponDepth;
layout(set = 8, binding = 0) uniform sampler2D currentClassTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

const float CLASS_WEAPON_THRESH = 0.5;

vec3 sampleCurrent(vec2 uv) {
	return textureLod(currentCombinedColor, clamp(uv, 0.0, 1.0), 0.0).rgb;
}

vec3 visualizeClass(float value) {
	if (value > 0.75) {
		return vec3(1.0);
	}
	if (value > 0.25) {
		return vec3(0.15, 0.35, 1.0);
	}
	if (value >= 0.0) {
		return vec3(0.35);
	}
	return vec3(1.0, 0.0, 1.0);
}

void main() {
	vec2 uv = frag_tex_coord;
	float currentClass = textureLod(currentClassTex, uv, 0.0).r;
	vec3 current = sampleCurrent(uv);
	vec2 motion = textureLod(motionTex, uv, 0.0).rg;
	bool validMotion = !(any(isnan(motion)) || any(isinf(motion)));
	vec2 historyUV = uv - motion;
	bool inBounds = all(greaterThanEqual(historyUV, vec2(0.0))) &&
		all(lessThanEqual(historyUV, vec2(1.0)));
	vec2 safeHistoryUV = validMotion ? clamp(historyUV, 0.0, 1.0) : uv;
	float previousClass = textureLod(previousClassTex, safeHistoryUV, 0.0).r;
	float depthNow = textureLod(currentDepth, uv, 0.0).r;
	float depthPrev = textureLod(previousWeaponDepth, safeHistoryUV, 0.0).r;
	float depthError = abs(depthNow - depthPrev);
	float depthThreshold = max(postfx.weaponTemporalParams.z, 1e-5);
	float depthConfidence = 1.0 - smoothstep(depthThreshold * 0.25, depthThreshold, depthError);
	float rawReactive = textureLod(reactiveMaskTex, uv, 0.0).r;
	float reactive = rawReactive * max(postfx.weaponTemporalParams.w, 0.0);
	float debugMode = postfx.shadowsLift.w;

	if (debugMode >= 15.5 && debugMode < 33.5) {
		vec2 texel = postfx.frameInfo.yz;
		float currentWeapon = currentClass > CLASS_WEAPON_THRESH ? 1.0 : 0.0;
		float previousWeapon = previousClass > CLASS_WEAPON_THRESH ? 1.0 : 0.0;
		float vectorScale = max(postfx.temporalDebugParams.x, 1.0);
		vec2 signedVelocity = motion * vectorScale;
		bool outOfRange = any(greaterThan(abs(signedVelocity), vec2(1.0)));
		vec3 velocityColor = !validMotion ? vec3(1.0, 0.0, 1.0) :
			outOfRange ? vec3(1.0, 1.0, 0.0) :
			vec3(0.5 + signedVelocity.x * 0.5, 0.5 + signedVelocity.y * 0.5, 0.25);
		float dilatedReactive = rawReactive;
		dilatedReactive = max(dilatedReactive, textureLod(reactiveMaskTex, uv + vec2(texel.x, 0.0), 0.0).r);
		dilatedReactive = max(dilatedReactive, textureLod(reactiveMaskTex, uv - vec2(texel.x, 0.0), 0.0).r);
		dilatedReactive = max(dilatedReactive, textureLod(reactiveMaskTex, uv + vec2(0.0, texel.y), 0.0).r);
		dilatedReactive = max(dilatedReactive, textureLod(reactiveMaskTex, uv - vec2(0.0, texel.y), 0.0).r);
		float confidence = postfx.temporalValidity.w * (validMotion && inBounds ? 1.0 : 0.0) *
			previousWeapon * depthConfidence * (1.0 - clamp(reactive, 0.0, 1.0));
		if (debugMode < 16.5) {
			out_color = vec4(visualizeClass(currentClass), 1.0); /* 16 current class */
		} else if (debugMode < 17.5) {
			out_color = vec4(visualizeClass(textureLod(previousClassTex, uv, 0.0).r), 1.0); /* 17 previous */
		} else if (debugMode < 18.5) {
			out_color = vec4(visualizeClass(previousClass), 1.0); /* 18 reprojected previous */
		} else if (debugMode < 19.5) {
			bool reject = !validMotion || !inBounds || currentWeapon != previousWeapon;
			out_color = reject ? vec4(1.0, 0.0, 0.0, 1.0) : vec4(0.0, 1.0, 0.0, 1.0);
		} else if (debugMode < 20.5) {
			out_color = vec4(mix(vec3(0.0), velocityColor, 1.0 - currentWeapon), 1.0); /* 20 world velocity */
		} else if (debugMode < 21.5) {
			out_color = vec4(mix(vec3(0.0), velocityColor, currentWeapon), 1.0); /* 21 weapon MVP */
		} else if (debugMode < 22.5) {
			out_color = vec4(velocityColor, 1.0); /* 22 merged */
		} else if (debugMode < 23.5) {
			out_color = vec4(rawReactive, rawReactive, rawReactive, 1.0);
		} else if (debugMode < 24.5) {
			out_color = vec4(dilatedReactive, dilatedReactive, dilatedReactive, 1.0);
		} else if (debugMode < 25.5) {
			out_color = vec4(confidence, confidence, confidence, 1.0);
		} else if (debugMode < 26.5) {
			out_color = vec4(postfx.temporalValidity.www, 1.0);
		} else if (debugMode < 27.5) {
			out_color = vec4(currentWeapon, currentWeapon, currentWeapon, 1.0);
		} else if (debugMode < 28.5) {
			out_color = vec4(depthNow, depthNow, depthNow, 1.0);
		} else if (debugMode < 29.5) {
			float d = textureLod(previousWeaponDepth, uv, 0.0).r;
			out_color = vec4(d, d, d, 1.0);
		} else if (debugMode < 30.5) {
			out_color = vec4(depthPrev, depthPrev, depthPrev, 1.0);
		} else if (debugMode < 31.5) {
			float d = clamp(depthError * 50.0, 0.0, 1.0);
			out_color = vec4(d, d, d, 1.0);
		} else if (debugMode < 32.5) {
			float relativeError = depthError / max(max(abs(depthNow), abs(depthPrev)), 1e-4);
			out_color = vec4(clamp(relativeError, 0.0, 1.0), 0.0, 0.0, 1.0);
		} else {
			float reject = depthConfidence < 0.5 ? 1.0 : 0.0;
			out_color = vec4(reject, 0.0, 0.0, 1.0);
		}
		return;
	}

	if (currentClass <= CLASS_WEAPON_THRESH) {
		out_color = vec4(0.0);
		return;
	}

	if (postfx.temporalValidity.w < 0.5 || !validMotion || !inBounds) {
		out_color = vec4(current, 1.0);
		return;
	}

	if (previousClass <= CLASS_WEAPON_THRESH) {
		out_color = vec4(current, 1.0);
		return;
	}

	vec3 history = textureLod(previousWeaponHistory, historyUV, 0.0).rgb;

	vec2 texel = postfx.frameInfo.yz;
	vec3 lo = current;
	vec3 hi = current;
	for (int y = -1; y <= 1; ++y) {
		for (int x = -1; x <= 1; ++x) {
			vec2 suv = uv + vec2(x, y) * texel;
			if (textureLod(currentClassTex, suv, 0.0).r > CLASS_WEAPON_THRESH) {
				vec3 c = sampleCurrent(suv);
				lo = min(lo, c);
				hi = max(hi, c);
			}
		}
	}
	float gamma = max(postfx.weaponTemporalParams.y, 0.1);
	vec3 center = (lo + hi) * 0.5;
	vec3 extent = (hi - lo) * 0.5 * gamma;
	history = clamp(history, center - extent, center + extent);

	float weight = clamp(postfx.weaponTemporalParams.x, 0.0, 0.9) *
		depthConfidence * (1.0 - clamp(reactive, 0.0, 1.0));
	out_color = vec4(mix(current, history, weight), 1.0);
}
