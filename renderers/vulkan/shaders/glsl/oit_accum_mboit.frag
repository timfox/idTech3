#version 450
/* Moment Transparency / MBOIT pass 2: WBOIT-style accum weighted by moment T(z).
 * Samples pass-1 moments + b0, reconstructs transmittance (Cantelli/MSM-style),
 * then accumulates (color * alpha * T, alpha * T) and revealage.
 */
layout (constant_id = 0) const int manual_depth_test = 0;

layout(set = 0, binding = 0) uniform sampler2D tex0;
layout(set = 1, binding = 0) uniform sampler2D opaqueDepthTex;
layout(set = 2, binding = 0) uniform sampler2D momentsTex;
layout(set = 3, binding = 0) uniform sampler2D b0Tex;

layout(location = 0) in vec2 frag_tex_coord0;
layout(location = 1) in vec4 frag_color0;

layout(location = 0) out vec4 out_color;
layout(location = 1) out float out_reveal;

/* Fraction of optical depth closer than z (biased). β=0.25 overestimation. */
float AbsorbanceCloser( float b0, vec4 b, float z )
{
	float inv = 1.0 / max(b0, 1e-5);
	float mean = b.x * inv;
	float mean2 = b.y * inv;
	float var = max(mean2 - mean * mean, 1e-6);
	float t = z - mean;
	float pGe;
	if ( t <= 0.0 ) {
		/* Cantelli: most mass is at/behind mean when querying in front */
		pGe = var / (var + t * t + 1e-6);
		pGe = 1.0 - clamp(pGe, 0.0, 1.0);
	} else {
		pGe = var / (var + t * t);
		pGe = 1.0 - clamp(pGe, 0.0, 1.0);
	}
	/* Overestimation weight β = 0.25 (Münstermann / Moment Transparency) */
	pGe = mix(pGe, 1.0, 0.25);
	return clamp(pGe, 0.0, 1.0) * b0;
}

void main() {
	vec4 base = textureLod(tex0, frag_tex_coord0, 0.0) * frag_color0;
	float alpha = clamp(base.a, 0.0, 0.999);
	if (alpha < 0.01) discard;

	if ( manual_depth_test != 0 ) {
		ivec2 depthSize = textureSize( opaqueDepthTex, 0 );
		vec2 depthUv = gl_FragCoord.xy / vec2( depthSize );
		float opaqueDepth = textureLod( opaqueDepthTex, depthUv, 0.0 ).r;
		if ( gl_FragCoord.z + 1e-5 < opaqueDepth ) discard;
	}

	ivec2 px = ivec2(gl_FragCoord.xy);
	vec4 b = texelFetch(momentsTex, px, 0);
	float b0 = texelFetch(b0Tex, px, 0).r;
	float z = clamp(gl_FragCoord.z, 0.0, 1.0);
	float absCloser = AbsorbanceCloser(b0, b, z);
	float T = exp(-absCloser);
	T = clamp(T, 0.0, 1.0);

	float w = alpha * T;
	out_color = vec4(base.rgb * w, w);
	out_reveal = alpha;
}
