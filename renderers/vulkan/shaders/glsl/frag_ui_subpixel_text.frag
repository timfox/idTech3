#version 450
/*
 * Rougier JCGT 2013 / HAL-00821839 — subpixel glyph positioning (Listing 2).
 * subpixelShift in [0,1), invTexWidth = 1/atlasTexWidth (texel UV step).
 * fontGamma = linearize coverage before display gamma (HAL-05430837).
 */
layout(location = 0) centroid in vec4 frag_color0;
layout(location = 1) centroid in vec2 frag_tex_coord0;
layout(location = 13) in vec4 var_CurrentClip;
layout(location = 14) in vec4 var_PrevClip;

layout(set = 1, binding = 0) uniform sampler2D texture0;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_motion;

layout(push_constant) uniform Transform {
	mat4 mvp;
	mat4 prevMvp;
	float subpixelShift;
	float invTexWidth;
	float fontGamma;
	float lcdWeight;
	float _pad[7];
} pc;

vec3 mixSubpixelRgb( vec3 current, vec3 previous, float shift ) {
	float r = current.r;
	float g = current.g;
	float b = current.b;

	if ( shift <= ( 1.0 / 3.0 ) ) {
		float z = 3.0 * shift;
		r = mix( current.r, previous.b, z );
		g = mix( current.g, current.r, z );
		b = mix( current.b, current.g, z );
	} else if ( shift <= ( 2.0 / 3.0 ) ) {
		float z = 3.0 * shift - 1.0;
		r = mix( previous.b, previous.g, z );
		g = mix( current.r, previous.b, z );
		b = mix( current.g, current.r, z );
	} else if ( shift < 1.0 ) {
		float z = 3.0 * shift - 2.0;
		r = mix( previous.g, previous.r, z );
		g = mix( previous.b, previous.g, z );
		b = mix( current.r, previous.b, z );
	}

	return vec3( r, g, b );
}

float mixSubpixelAlpha( float current, float previous, float shift ) {
	if ( shift <= ( 1.0 / 3.0 ) ) {
		return mix( current, previous, 3.0 * shift );
	}
	if ( shift <= ( 2.0 / 3.0 ) ) {
		return mix( previous, current, 3.0 * shift - 1.0 );
	}
	if ( shift < 1.0 ) {
		return mix( current, previous, 3.0 * shift - 2.0 );
	}
	return current;
}

void main() {
	vec3 coverageRgb;
	float monoCov;
	float coverageAlpha;
	float lcdWeight;

	out_motion = vec2( 0.0 );
	if ( abs( var_CurrentClip.w ) > 1e-6 && abs( var_PrevClip.w ) > 1e-6 ) {
		vec2 currUV = var_CurrentClip.xy / var_CurrentClip.w * 0.5 + 0.5;
		vec2 prevUV = var_PrevClip.xy / var_PrevClip.w * 0.5 + 0.5;
		out_motion = currUV - prevUV;
	}

	vec2 pixel = vec2( max( pc.invTexWidth, 1e-6 ), 0.0 );
	vec2 uv = frag_tex_coord0;
	vec4 current = texture( texture0, uv );
	vec4 previous = texture( texture0, uv - pixel );
	float shift = clamp( pc.subpixelShift, 0.0, 0.999 );

	vec3 rgb = mixSubpixelRgb( current.rgb, previous.rgb, shift );
	float alpha = mixSubpixelAlpha( current.a, previous.a, shift );
	float chromaDelta = max( abs( rgb.r - rgb.g ), abs( rgb.g - rgb.b ) );

	coverageRgb = clamp( rgb, 0.0, 1.0 );
	monoCov = clamp( alpha, 0.0, 1.0 );

	if ( abs(pc.fontGamma - 1.0) > 1e-3 ) {
		float invGamma = 1.0 / max(pc.fontGamma, 0.001);
		coverageRgb = pow( coverageRgb, vec3( invGamma ) );
		monoCov = pow( monoCov, invGamma );
	}

	lcdWeight = clamp( pc.lcdWeight, 0.0, 1.0 );
	if ( chromaDelta <= 0.02 ) {
		lcdWeight = 0.0;
	}

	coverageRgb = mix( vec3( monoCov ), coverageRgb, lcdWeight );
	coverageAlpha = max( max( coverageRgb.r, coverageRgb.g ), coverageRgb.b );

	out_color = vec4( frag_color0.rgb * coverageRgb, frag_color0.a * coverageAlpha );
}
