#version 450
/*
 * Loop & Blinn 2005 glyphlet fragment shader.
 * Discards fragments outside the canonical quadratic (u^2 - v).
 */
layout(location = 0) in vec4 frag_color;
layout(location = 1) flat in float frag_triType;
layout(location = 2) in vec2 frag_canon;
layout(location = 13) in vec4 var_CurrentClip;
layout(location = 14) in vec4 var_PrevClip;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_motion;

void main() {
	out_motion = vec2( 0.0 );
	if ( abs( var_CurrentClip.w ) > 1e-6 && abs( var_PrevClip.w ) > 1e-6 ) {
		vec2 currUV = var_CurrentClip.xy / var_CurrentClip.w * 0.5 + 0.5;
		vec2 prevUV = var_PrevClip.xy / var_PrevClip.w * 0.5 + 0.5;
		out_motion = currUV - prevUV;
	}

	int triType = int( frag_triType + 0.5 );
	if ( triType == 1 ) {
		/* convex curve */
		if ( frag_canon.x * frag_canon.x < frag_canon.y ) {
			discard;
		}
	} else if ( triType == 2 ) {
		/* concave curve */
		if ( frag_canon.x * frag_canon.x > frag_canon.y ) {
			discard;
		}
	}

	out_color = frag_color;
}
