#version 450
#extension GL_GOOGLE_include_directive : require

#include "depth_view.glsl"

layout(set = 0, binding = 0) uniform sampler2D ssaoTex;
layout(set = 1, binding = 0) uniform sampler2D depthTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform SSAOParams {
	vec4 projInfo; // invProj00, invProj11, proj10, proj14 (same as SSAO gen)
	vec4 params;   // blurRadius, depthSharpness, unused, unused
	vec4 misc;     // samples, invWidth, invHeight, depthIsReversed
} pc;

float positiveViewDepth( float deviceDepth )
{
	/* Matches AV / SSAO projInfo reconstruction: meters along +view. */
	return max( pc.projInfo.w / max( deviceDepth + pc.projInfo.z, 1e-6 ), 0.0 );
}

void main()
{
	int radius = int( pc.params.x );
	float centerAo = textureLod( ssaoTex, frag_tex_coord, 0.0 ).r;
	if ( radius <= 0 ) {
		out_color = vec4( centerAo, centerAo, centerAo, 1.0 );
		return;
	}

	float centerDepth = textureLod( depthTex, frag_tex_coord, 0.0 ).r;
	if ( centerDepth <= 0.0 || centerDepth >= 0.999999 ) {
		out_color = vec4( centerAo, centerAo, centerAo, 1.0 );
		return;
	}

	float centerView = positiveViewDepth( centerDepth );
	float depthSharp = max( pc.params.y, 1.0 );
	vec2 texel = vec2( pc.misc.y, pc.misc.z );
	float sum = 0.0;
	float weightSum = 0.0;

	for ( int y = -4; y <= 4; ++y ) {
		if ( abs( y ) > radius ) {
			continue;
		}
		for ( int x = -4; x <= 4; ++x ) {
			if ( abs( x ) > radius ) {
				continue;
			}
			vec2 suv = frag_tex_coord + vec2( x, y ) * texel;
			float ao = textureLod( ssaoTex, suv, 0.0 ).r;
			float sd = textureLod( depthTex, suv, 0.0 ).r;
			float spatial = 1.0 - ( abs( float( x ) ) + abs( float( y ) ) ) / ( float( radius ) * 2.0 + 1.0 );
			float depthW = 1.0;
			if ( sd > 0.0 && sd < 0.999999 ) {
				float sv = positiveViewDepth( sd );
				depthW = Depth_BilateralWeight( centerView, sv, depthSharp );
			} else {
				depthW = 0.0;
			}
			float w = spatial * depthW;
			sum += ao * w;
			weightSum += w;
		}
	}

	float result = ( weightSum < 1e-5 ) ? centerAo : ( sum / weightSum );
	out_color = vec4( result, result, result, 1.0 );
}
