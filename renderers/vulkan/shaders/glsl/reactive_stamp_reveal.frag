#version 450
/* Stamp Temporal Reconstruction reactive mask from OIT revealage:
 * coverage ≈ 1 - reveal (reveal starts at 1 and multiplies by (1-alpha)).
 * Prefer current frame over history for any meaningful transparent coverage
 * (noise over trails). Blend MAX into R8 so repeated buckets accumulate.
 */

layout(set = 0, binding = 0) uniform sampler2D revealTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out float out_reactive;

void main() {
	/* texelFetch: match OIT resolve — no LINEAR bleed across rows into the reactive mask. */
	ivec2 px = ivec2( gl_FragCoord.xy );
	float reveal = texelFetch( revealTex, px, 0 ).r;
	float coverage = clamp( 1.0 - reveal, 0.0, 1.0 );
	/* Soft floor: even light glass must reject stale history strongly. */
	if ( coverage > 0.02 ) {
		coverage = max( coverage, 0.92 );
	}
	out_reactive = coverage;
}
