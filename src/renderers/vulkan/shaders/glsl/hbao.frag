#version 450

/* Horizon-Based Ambient Occlusion (HBAO)
 * Raymarches the depth buffer in multiple directions, tracks horizon angles,
 * and integrates occlusion. Higher quality than hemisphere SSAO with fewer samples.
 */

layout(set = 0, binding = 0) uniform sampler2D depthTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform HBAOParams {
	vec4 projInfo; // invProj00, invProj11, proj10, proj14
	vec4 params;   // radius, angleBias, intensity, power
	vec4 misc;     // numDirections, numSteps, invWidth, invHeight (depthIsReversed: sign of invWidth)
} pc;

float viewZFromDepth( float depth ) {
	return -pc.projInfo.w / max( depth + pc.projInfo.z, 1e-6 );
}

/* Per-pixel random rotation to break up banding. */
float rand01( vec2 co ) {
	return fract( sin( dot( co, vec2( 12.9898, 78.233 ) ) ) * 43758.5453 );
}

void main() {
	float depth = textureLod( depthTex, frag_tex_coord, 0.0 ).r;
	bool depthReversed = pc.misc.z < 0.0;
	float invW = abs( pc.misc.z );
	float invH = pc.misc.w;
	if ( ( depthReversed && depth <= 0.001 ) || ( !depthReversed && depth >= 0.999 ) ) {
		out_color = vec4( 1.0 );
		return;
	}

	float viewZ = viewZFromDepth( depth );
	vec2 ndc = frag_tex_coord * 2.0 - 1.0;
	vec3 viewPos = vec3( ndc.x * (-viewZ) * pc.projInfo.x, ndc.y * (-viewZ) * pc.projInfo.y, viewZ );

	float radius = pc.params.x;
	float angleBias = pc.params.y;
	float intensity = pc.params.z;
	float power = pc.params.w;
	int numDirections = int( pc.misc.x );
	int numSteps = int( pc.misc.y );
	vec2 invRes = vec2( invW, invH );

	/* Random rotation per pixel (0..2*PI) */
	float angleOffset = rand01( frag_tex_coord * 17.3 ) * 6.283185;
	float angularStep = 6.283185 / max( float( numDirections ), 1.0 );

	float occlusion = 0.0;
	for ( int d = 0; d < 16; ++d ) {
		if ( d >= numDirections )
			break;

		float angle = angleOffset + angularStep * float( d );
		vec2 dir = vec2( cos( angle ), sin( angle ) );

		/* March in screen space along this direction.
		 * radius is in UV space (0.01-0.5 typical); step size = radius/numSteps per step. */
		float tanHorizon = tan( angleBias );
		float stepScale = radius / max( float( numSteps ), 1.0 );
		for ( int s = 1; s <= 16; ++s ) {
			if ( s > numSteps )
				break;

			vec2 offset = dir * float( s ) * stepScale;
			vec2 sampleUV = frag_tex_coord + offset;

			if ( sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0 )
				break;

			float sampleDepth = textureLod( depthTex, sampleUV, 0.0 ).r;
			float sampleViewZ = viewZFromDepth( sampleDepth );

			/* Height difference in view space (positive = sample is closer/occluding). */
			float deltaZ = viewZ - sampleViewZ;
			/* Horizontal distance in view space: UV delta -> NDC delta (x2) -> view = ndc*viewZ*projInfo. */
			float dist = length( offset ) * 2.0 * abs( viewZ ) * length( vec2( pc.projInfo.x, pc.projInfo.y ) );
			if ( dist < 1e-4 )
				dist = 1e-4;

			/* Horizon angle: atan(deltaZ / dist). tan = deltaZ / dist. */
			float tanAngle = deltaZ / dist;

			if ( tanAngle > tanHorizon ) {
				/* New horizon found; add occlusion for this angular segment. */
				float segmentOcclusion = tanAngle - tanHorizon;
				/* Distance falloff: reduce contribution from far samples. */
				float falloff = 1.0 - smoothstep( 0.0, 1.0, float( s ) / float( numSteps ) );
				occlusion += segmentOcclusion * falloff * angularStep;
				tanHorizon = tanAngle;
			}
		}
	}

	/* Normalize and shape output. */
	occlusion = occlusion / max( float( numDirections ), 1.0 );
	float ao = 1.0 - clamp( occlusion * intensity, 0.0, 1.0 );
	ao = pow( ao, power );
	ao = clamp( ao, 0.0, 1.0 );

	out_color = vec4( ao, ao, ao, 1.0 );
}
