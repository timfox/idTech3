#version 450
/* Stamp Temporal Reconstruction reactive mask from DEPTH_RANGE_WEAPON
 * (viewport [0.6,1] reverse-Z). Depth-aware 1px dilation: expand only when
 * neighbors are also near so the gun silhouette does not wipe distant world.
 */

layout(set = 0, binding = 0) uniform sampler2D depthTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out float out_reactive;

void main() {
	ivec2 px = ivec2( gl_FragCoord.xy );
	float d = texelFetch( depthTex, px, 0 ).r;
	float weapon = smoothstep( 0.58, 0.62, d );

	float dilated = weapon;
	for ( int y = -1; y <= 1; ++y ) {
		for ( int x = -1; x <= 1; ++x ) {
			if ( x == 0 && y == 0 ) {
				continue;
			}
			float nd = texelFetch( depthTex, px + ivec2( x, y ), 0 ).r;
			float nWeapon = smoothstep( 0.58, 0.62, nd );
			/* Reversed-Z: larger = nearer. Only dilate onto pixels not much farther. */
			if ( nWeapon > 0.5 && nd >= d - 0.015 ) {
				dilated = max( dilated, nWeapon * 0.95 );
			}
		}
	}

	out_reactive = dilated > 0.45 ? max( dilated, 0.96 ) : 0.0;
}
