#version 450
/* Stamp TEMPORAL_CLASS_WEAPON into R8 where depth is in DEPTH_RANGE_WEAPON
 * (viewport minDepth=0.6 reverse-Z near). MAX-blend so world stays WORLD. */

layout(set = 0, binding = 0) uniform sampler2D depthTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out float out_class;

void main() {
	float d = texelFetch( depthTex, ivec2( gl_FragCoord.xy ), 0 ).r;
	/* Weapon viewport [0.6, 1.0] reverse-Z; soft edge avoids halo. */
	float weapon = smoothstep( 0.58, 0.62, d );
	/* Encode TEMPORAL_CLASS_WEAPON = 1 → 1/255... use 1.0 so threshold is easy. */
	out_class = weapon > 0.5 ? 1.0 : 0.0;
}
