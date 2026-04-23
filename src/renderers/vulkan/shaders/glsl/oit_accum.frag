#version 450
/* Weighted blended OIT:
 *  RT0 accumulates (color * alpha * weight, alpha * weight)
 *  RT1 tracks revealage = product(1 - alpha)
 * MSAA path samples resolved opaque depth so transparent layers still reject
 * against opaque geometry before accumulation.
 */
#define DEPTH_TO_WEIGHT(z) (z)

layout (constant_id = 0) const int manual_depth_test = 0;

layout(set = 0, binding = 0) uniform sampler2D tex0;
layout(set = 1, binding = 0) uniform sampler2D opaqueDepthTex;

layout(location = 0) in vec2 frag_tex_coord0;
layout(location = 1) in vec4 frag_color0;

layout(location = 0) out vec4 out_color;
layout(location = 1) out float out_reveal;

void main() {
	vec4 base = textureLod(tex0, frag_tex_coord0, 0.0) * frag_color0;
	float alpha = base.a;
	if (alpha < 0.01) discard;

	if ( manual_depth_test != 0 ) {
		ivec2 depthSize = textureSize( opaqueDepthTex, 0 );
		vec2 depthUv = gl_FragCoord.xy / vec2( depthSize );
		float opaqueDepth = textureLod( opaqueDepthTex, depthUv, 0.0 ).r;
		if ( gl_FragCoord.z + 1e-5 < opaqueDepth ) discard;
	}

	float d = DEPTH_TO_WEIGHT(gl_FragCoord.z);
	float w = alpha * pow(max(d, 0.01), 2.0);
	out_color = vec4(base.rgb * w, w);
	out_reveal = alpha;
}
