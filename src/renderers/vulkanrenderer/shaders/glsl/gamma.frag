#version 450

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 0) const float gamma = 1.0;
layout(constant_id = 1) const float obScale = 2.0;
layout(constant_id = 2) const float greyscale = 0.0;
//
layout(constant_id = 7) const int ditherMode = 0; // 0 - disabled, 1 - ordered
layout(constant_id = 8) const int depth_r = 255;
layout(constant_id = 9) const int depth_g = 255;
layout(constant_id = 10) const int depth_b = 255;
layout(constant_id = 11) const int tonemapMode = 0; // 0=off,1=Reinhard,2=ACES,3=Hable,4=AgX,5=KhronosPBR
layout(constant_id = 13) const int lutEnabled = 0;
layout(constant_id = 14) const float lutIntensity = 1.0;
layout(constant_id = 15) const float lutSize = 32.0;

layout(set = 1, binding = 0) uniform sampler2D lut_texture;

const vec3 sRGB = { 0.2126, 0.7152, 0.0722 };

const int bayerSize = 8;
const float bayerMatrix[bayerSize * bayerSize] = {
	0,  32, 8,  40, 2,  34, 10, 42,
	48, 16, 56, 24, 50, 18, 58, 26,
	12, 44, 4,  36, 14, 46, 6,  38,
	60, 28, 52, 20, 62, 30, 54, 22,
	3,  35, 11, 43, 1,  33, 9,  41,
	51, 19, 59, 27, 49, 17, 57, 25,
	15, 47, 7,  39, 13, 45, 5,  37,
	63, 31, 55, 23, 61, 29, 53, 21
};

float threshold() {
	ivec2 coordDenormalized = ivec2(gl_FragCoord.xy);
	ivec2 bayerCoord = coordDenormalized % bayerSize;
	float bayerSample = bayerMatrix[bayerCoord.x + bayerCoord.y * bayerSize];
	float threshold = (bayerSample + 0.5) / float(bayerSize * bayerSize);
	return threshold;
}

vec3 dither(vec3 color) {
	ivec3 depth = ivec3(depth_r, depth_g, depth_b);
	vec3 cDenormalized = color * depth;
	vec3 cLow = floor(cDenormalized);
	vec3 cFractional = cDenormalized - cLow;
	vec3 cDithered = cLow + step(threshold(), cFractional);
	return cDithered / depth;
}

vec3 reinhardTonemap(vec3 color) {
	return color / (vec3(1.0) + color);
}

vec3 acesTonemap(vec3 color) {
	const vec3 a = vec3(2.51);
	const vec3 b = vec3(0.03);
	const vec3 c = vec3(2.43);
	const vec3 d = vec3(0.59);
	const vec3 e = vec3(0.14);
	vec3 x = clamp(color, 0.0, 1e6);
	vec3 numerator = x * (a * x + b);
	vec3 denominator = x * (c * x + d) + e;
	return clamp(numerator / denominator, 0.0, 1.0);
}

vec3 hableTonemap(vec3 color) {
const float Uncharted2A = 0.15;
const float Uncharted2B = 0.50;
const float Uncharted2C = 0.10;
const float Uncharted2D = 0.20;
const float Uncharted2E = 0.02;
const float Uncharted2F = 0.30;
	const float Uncharted2White = 11.2;
	vec3 curr = ((color * (Uncharted2A * color + Uncharted2B) + Uncharted2C) * color + Uncharted2D);
	vec3 denom = ((color * (Uncharted2A * color + Uncharted2B) + Uncharted2C) * color + Uncharted2E);
	vec3 tonemapped = clamp(curr / denom, 0.0, 1.0);
	float whiteScale = clamp(((Uncharted2White * (Uncharted2A * Uncharted2White + Uncharted2B) + Uncharted2C) * Uncharted2White + Uncharted2D) /
		((Uncharted2White * (Uncharted2A * Uncharted2White + Uncharted2B) + Uncharted2C) * Uncharted2White + Uncharted2E), 0.0, 1.0);
	return clamp(tonemapped / whiteScale * (Uncharted2F + 1.0), 0.0, 1.0);
}

// AgX tonemapper by Troy Sobotka, approximation by Benjamin Wrensch
// Ref: https://iolite-engine.com/blog_posts/minimal_agx_implementation
// Better color handling than ACES for saturated colors; avoids "ACES orange" artifact
vec3 agxTonemap(vec3 color) {
	// AgX input transform (log2 space encoding)
	const mat3 agx_mat = mat3(
		0.842479062253094, 0.0423282422610123, 0.0423756549057051,
		0.0784335999999992, 0.878468636469772, 0.0784336,
		0.0792237451477643, 0.0791661274605434, 0.879142973793104
	);
	color = agx_mat * color;

	// Log2 space encoding
	const float min_ev = -12.47393;
	const float max_ev = 4.026069;
	color = clamp(log2(max(color, vec3(1e-10))), min_ev, max_ev);
	color = (color - min_ev) / (max_ev - min_ev);

	// 6th-order sigmoid function approximation
	vec3 v = 15.5 * color - 40.14;
	v = color * v + 31.96;
	v = color * v - 6.868;
	v = color * v + 0.4298;
	v = color * v + 0.1191;
	v = color * v - 0.0023;

	// AgX inverse output transform
	const mat3 agx_mat_inv = mat3(
		1.19687900512017, -0.0528968517574562, -0.0529716355144438,
		-0.0980208811401368, 1.15190312990417, -0.0980434501171241,
		-0.0990297440797205, -0.0989611768448433, 1.15107367264116
	);
	return clamp(agx_mat_inv * v, 0.0, 1.0);
}

// Khronos PBR Neutral tone mapper
// Ref: https://github.com/KhronosGroup/ToneMapping/blob/main/PBR_Neutral/pbrNeutral.glsl
// The official Khronos recommendation for PBR content display
vec3 khronosPbrTonemap(vec3 color) {
	const float startCompression = 0.8 - 0.04;
	const float desaturation = 0.15;

	float x = min(color.r, min(color.g, color.b));
	float peak = max(color.r, max(color.g, color.b));

	float offset = (x < 0.08) ? x * (-6.25 * x + 1.0) : 0.04;
	color -= offset;

	if ( peak >= startCompression ) {
		const float d = 1.0 - startCompression;
		float newPeak = 1.0 - d * d / (peak + d - startCompression);
		color *= newPeak / peak;

		float g = 1.0 - 1.0 / (desaturation * (peak - newPeak) + 1.0);
		color = mix(color, vec3(newPeak), g);
	}

	// Apply linear-to-sRGB conversion
	vec3 low = color * 12.92;
	vec3 high = 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055;
	return mix(low, high, step(vec3(0.0031308), color));
}

layout(push_constant) uniform GammaPush {
	float exposure;
} gammaPush;

vec3 applyToneMapping(vec3 color) {
	color = clamp(color, 0.0, 1e6);
	if ( tonemapMode == 1 ) {
		return reinhardTonemap(color);
	} else if ( tonemapMode == 2 ) {
		return acesTonemap(color);
	} else if ( tonemapMode == 3 ) {
		return hableTonemap(color);
	} else if ( tonemapMode == 4 ) {
		return agxTonemap(color);
	} else if ( tonemapMode == 5 ) {
		return khronosPbrTonemap(color);
	}
	return clamp(color, 0.0, 1.0);
}

vec3 lutLookup(vec3 color) {
	float size = max(lutSize, 1.0);
	vec3 scaled = clamp(color, 0.0, 1.0) * (size - 1.0);
	float slice = floor(scaled.b);
	float sliceFrac = fract(scaled.b);
	float x = scaled.r;
	float y = scaled.g;
	float texWidth = size * size;
	float u0 = (x + slice * size + 0.5) / texWidth;
	float v = (y + 0.5) / size;
	vec3 sampleA = texture( lut_texture, vec2( u0, v ) ).rgb;
	if ( sliceFrac <= 0.0 ) {
		return sampleA;
	}
	float nextSlice = min( slice + 1.0, size - 1.0 );
	float u1 = (x + nextSlice * size + 0.5) / texWidth;
	vec3 sampleB = texture( lut_texture, vec2( u1, v ) ).rgb;
	return mix( sampleA, sampleB, sliceFrac );
}

void main() {
	vec3 color = texture(texture0, frag_tex_coord).rgb * gammaPush.exposure;
	color = applyToneMapping( color );

	if ( lutEnabled != 0 && lutIntensity > 0.0 ) {
		vec3 lutColor = lutLookup( color );
		color = mix( color, lutColor, clamp( lutIntensity, 0.0, 1.0 ) );
	}

	if ( greyscale == 1 )
	{
		color = vec3(dot(color, sRGB));
	}
	else if ( greyscale != 0 )
	{
		vec3 luma = vec3(dot(color, sRGB));
		color = mix(color, luma, greyscale);
	}

	if ( gamma != 1.0 )
	{
		out_color = vec4(pow(color, vec3(gamma)) * obScale, 1);
	}
	else
	{
		out_color = vec4(color * obScale, 1);
	}

	if ( ditherMode == 1 ) {
		out_color.rgb = dither(out_color.rgb);
	}
}
