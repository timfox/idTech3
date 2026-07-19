#version 450
/* Moment Transparency / MBOIT pass 1: accumulate optical-depth-weighted power moments.
 * RT0: moments b1..b4 = d * (z, z^2, z^3, z^4)
 * RT1: b0 = total optical depth d = -log(1 - alpha)
 * Additive blend. Depth test vs opaque; no depth write.
 */
layout (constant_id = 0) const int manual_depth_test = 0;

layout(set = 0, binding = 0) uniform sampler2D tex0;
layout(set = 1, binding = 0) uniform sampler2D opaqueDepthTex;

layout(location = 0) in vec2 frag_tex_coord0;
layout(location = 1) in vec4 frag_color0;

layout(location = 0) out vec4 out_moments;
layout(location = 1) out float out_b0;

void main() {
	vec4 base = textureLod(tex0, frag_tex_coord0, 0.0) * frag_color0;
	float alpha = clamp(base.a, 0.0, 0.999);
	if (alpha < 0.01) discard;

	if ( manual_depth_test != 0 ) {
		ivec2 depthSize = textureSize( opaqueDepthTex, 0 );
		vec2 depthUv = gl_FragCoord.xy / vec2( depthSize );
		float opaqueDepth = textureLod( opaqueDepthTex, depthUv, 0.0 ).r;
		/* Reversed-Z: discard fragments farther than opaque (lower depth). */
		if ( gl_FragCoord.z + 1e-5 < opaqueDepth ) discard;
	}

	float d = -log(max(1.0 - alpha, 1e-5));
	float z = clamp(gl_FragCoord.z, 0.0, 1.0);
	float z2 = z * z;
	out_moments = d * vec4(z, z2, z2 * z, z2 * z2);
	out_b0 = d;
	if ( any( isnan( out_moments ) ) || any( isinf( out_moments ) ) || isnan( out_b0 ) || isinf( out_b0 ) ) {
		out_moments = vec4( 0.0 );
		out_b0 = 0.0;
	}
}
