#version 450

/* Fast Approximate Anti-Aliasing (FXAA 3.11-style), Jimenez et al. SIGGRAPH 2011.
 * Lightweight post-pass complement to MSAA (AMBF-Vulkan, Allison et al. arXiv:2410.05095). */

layout(set = 0, binding = 0) uniform sampler2D colorTexture;

layout(push_constant) uniform FXAAParams {
	float invResolutionX;
	float invResolutionY;
	float subpixQuality;
	float edgeThreshold;
} pc;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

#define FXAA_REDUCE_MIN   (1.0 / 128.0)
#define FXAA_REDUCE_MUL   (1.0 / 8.0)
#define FXAA_SPAN_MAX     8.0

float rgb2luma(vec3 rgb) {
	return dot(rgb, vec3(0.299, 0.587, 0.114));
}

void main() {
	vec2 uv = frag_tex_coord;
	vec2 rcpFrame = vec2(pc.invResolutionX, pc.invResolutionY);

	vec3 rgbM = textureLod(colorTexture, uv, 0.0).rgb;
	vec3 rgbN = textureLod(colorTexture, uv + vec2(0.0, -rcpFrame.y), 0.0).rgb;
	vec3 rgbW = textureLod(colorTexture, uv + vec2(-rcpFrame.x, 0.0), 0.0).rgb;
	vec3 rgbE = textureLod(colorTexture, uv + vec2(rcpFrame.x, 0.0), 0.0).rgb;
	vec3 rgbS = textureLod(colorTexture, uv + vec2(0.0, rcpFrame.y), 0.0).rgb;

	float lumaM = rgb2luma(rgbM);
	float lumaN = rgb2luma(rgbN);
	float lumaW = rgb2luma(rgbW);
	float lumaE = rgb2luma(rgbE);
	float lumaS = rgb2luma(rgbS);
	float lumaMin = min(lumaM, min(min(lumaN, lumaW), min(lumaE, lumaS)));
	float lumaMax = max(lumaM, max(max(lumaN, lumaW), max(lumaE, lumaS)));

	float lumaRange = lumaMax - lumaMin;
	if (lumaRange < max(pc.edgeThreshold, lumaMax * 0.03125)) {
		out_color = vec4(rgbM, 1.0);
		return;
	}

	vec2 dir;
	dir.x = -((lumaN + lumaS) - (lumaM + lumaM));
	dir.y = ((lumaW + lumaE) - (lumaM + lumaM));

	float dirReduce = max((lumaW + lumaE + lumaN + lumaS) * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
	float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
	dir = clamp(dir * rcpDirMin, vec2(-FXAA_SPAN_MAX), vec2(FXAA_SPAN_MAX)) * rcpFrame;

	vec3 rgbA = 0.5 * (
		textureLod(colorTexture, uv + dir * (1.0 / 3.0 - 0.5), 0.0).rgb +
		textureLod(colorTexture, uv + dir * (2.0 / 3.0 - 0.5), 0.0).rgb);
	vec3 rgbB = rgbA * 0.5 + 0.25 * (
		textureLod(colorTexture, uv + dir * -0.5, 0.0).rgb +
		textureLod(colorTexture, uv + dir * 0.5, 0.0).rgb);

	float lumaB = rgb2luma(rgbB);
	if ((lumaB < lumaMin) || (lumaB > lumaMax)) {
		out_color = vec4(rgbA, 1.0);
	} else {
		out_color = vec4(rgbB, 1.0);
	}
}
