#version 450

layout(set = 0, binding = 0) uniform sampler2D depthTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform SSAOParams {
	vec4 projInfo; // invProj00, invProj11, proj10, proj14
	vec4 params;   // radius, bias, intensity, power
	vec4 misc;     // samples, invWidth, invHeight, depthIsReversed
	vec4 misc2;    // method, hbaoDirections, hbaoSteps, maxDepthGradient
} pc;

float halton2( int i ) {
	float r = 0.0;
	float f = 0.5;
	int n = i;
	while ( n > 0 ) {
		r += f * float( n & 1 );
		n /= 2;
		f *= 0.5;
	}
	return r;
}

float halton3( int i ) {
	float r = 0.0;
	float f = 1.0 / 3.0;
	int n = i;
	while ( n > 0 ) {
		r += f * float( n % 3 );
		n /= 3;
		f *= 1.0 / 3.0;
	}
	return r;
}

float rand01( vec2 co ) {
	return fract( sin( dot( co, vec2( 12.9898, 78.233 ) ) ) * 43758.5453 );
}

float viewZFromDepth( float depth ) {
	return -pc.projInfo.w / max( depth + pc.projInfo.z, 1e-6 );
}

vec3 reconstructViewPos( vec2 uv, float depth ) {
	float viewZ = viewZFromDepth( depth );
	float negZ = -viewZ;
	vec2 ndc = uv * 2.0 - 1.0;
	return vec3( ndc.x * negZ * pc.projInfo.x, ndc.y * negZ * pc.projInfo.y, viewZ );
}

float applyAOResponse( float visibility ) {
	float ao = clamp( visibility, 0.0, 1.0 );
	ao = pow( ao, pc.params.w );
	ao = clamp( 1.0 - ( 1.0 - ao ) * pc.params.z, 0.0, 1.0 );
	return ao;
}

float computeSSAO( vec3 viewPos, vec3 normal ) {
	vec3 randVec = normalize( vec3(
		rand01( frag_tex_coord * 13.1 ) * 2.0 - 1.0,
		rand01( frag_tex_coord * 31.7 ) * 2.0 - 1.0,
		rand01( frag_tex_coord * 57.3 ) * 2.0 - 1.0
	) );

	vec3 tangent = normalize( randVec - normal * dot( randVec, normal ) );
	vec3 bitangent = cross( normal, tangent );
	mat3 tbn = mat3( tangent, bitangent, normal );

	float radius = pc.params.x;
	float bias = pc.params.y;
	int samples = int( pc.misc.x );
	float occlusion = 0.0;

	for ( int i = 0; i < 32; ++i ) {
		if ( i >= samples ) {
			break;
		}

		float u1 = halton2( i + 1 );
		float u2 = halton3( i + 1 );
		float phi = u1 * 6.28318530718;
		float cosTheta = 1.0 - u2;
		float sinTheta = sqrt( 1.0 - cosTheta * cosTheta );
		vec3 sampleDir = vec3( cos( phi ) * sinTheta, sin( phi ) * sinTheta, cosTheta );
		sampleDir = tbn * sampleDir;

		vec3 samplePos = viewPos + sampleDir * radius;
		float proj00 = 1.0 / max( pc.projInfo.x, 1e-6 );
		float proj11 = 1.0 / max( pc.projInfo.y, 1e-6 );
		vec2 sampleNdc = vec2( samplePos.x * proj00 / -samplePos.z, samplePos.y * proj11 / -samplePos.z );
		vec2 sampleUV = sampleNdc * 0.5 + 0.5;

		if ( sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0 ) {
			continue;
		}

		float sampleDepth = textureLod( depthTex, sampleUV, 0.0 ).r;
		float sampleViewZ = viewZFromDepth( sampleDepth );
		float rangeCheck = smoothstep( 0.0, 1.0, radius / max( abs( viewPos.z - sampleViewZ ), 1e-3 ) );
		if ( ( sampleViewZ - samplePos.z ) > bias ) {
			occlusion += rangeCheck;
		}
	}

	occlusion = clamp( occlusion / max( float( samples ), 1.0 ), 0.0, 1.0 );
	return applyAOResponse( 1.0 - occlusion );
}

float computeHBAO( vec3 viewPos, vec3 normal ) {
	float radius = pc.params.x;
	float tangentBias = clamp( pc.params.y * 0.1, 0.0, 1.2 );
	int directions = clamp( int( pc.misc2.y ), 4, 16 );
	int steps = clamp( int( pc.misc2.z ), 2, 8 );
	float negZ = max( -viewPos.z, 1e-3 );
	float proj00 = 1.0 / max( pc.projInfo.x, 1e-6 );
	float proj11 = 1.0 / max( pc.projInfo.y, 1e-6 );
	vec2 radiusUV = vec2( 0.5 * radius * proj00 / negZ, 0.5 * radius * proj11 / negZ );
	float occlusion = 0.0;

	for ( int d = 0; d < 16; ++d ) {
		if ( d >= directions ) {
			break;
		}

		float angle = ( float( d ) + rand01( frag_tex_coord * 91.7 + vec2( float( d ), 17.0 ) ) ) *
			( 6.28318530718 / float( directions ) );
		vec2 dir = vec2( cos( angle ), sin( angle ) );
		float tangentAngle = atan( dot( normal.xy, dir ), max( normal.z, 1e-4 ) ) + tangentBias;
		float baseSin = sin( tangentAngle );
		float maxSinHorizon = baseSin;

		for ( int s = 0; s < 8; ++s ) {
			if ( s >= steps ) {
				break;
			}

			float stepT = ( float( s ) + 1.0 ) / float( steps );
			vec2 sampleUV = frag_tex_coord + dir * radiusUV * stepT;
			if ( sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0 ) {
				continue;
			}

			float sampleDepth = textureLod( depthTex, sampleUV, 0.0 ).r;
			if ( ( pc.misc.w > 0.5 && sampleDepth <= 0.001 ) || ( pc.misc.w <= 0.5 && sampleDepth >= 0.999 ) ) {
				continue;
			}

			vec3 samplePos = reconstructViewPos( sampleUV, sampleDepth );
			vec3 delta = samplePos - viewPos;
			float deltaLen = length( delta );
			float horizLen = length( delta.xy );

			if ( deltaLen <= 1e-4 || horizLen <= 1e-4 || deltaLen > radius * 1.25 ) {
				continue;
			}

			float sinElevation = clamp( ( viewPos.z - samplePos.z ) / deltaLen, -1.0, 1.0 );
			float distanceWeight = 1.0 - clamp( deltaLen / radius, 0.0, 1.0 );
			maxSinHorizon = max( maxSinHorizon, mix( baseSin, sinElevation, distanceWeight ) );
		}

		occlusion += clamp( maxSinHorizon - baseSin, 0.0, 1.0 );
	}

	return applyAOResponse( 1.0 - clamp( occlusion / float( directions ), 0.0, 1.0 ) );
}

void main() {
	float depth = textureLod( depthTex, frag_tex_coord, 0.0 ).r;
	if ( ( pc.misc.w > 0.5 && depth <= 0.001 ) || ( pc.misc.w <= 0.5 && depth >= 0.999 ) ) {
		out_color = vec4( 1.0 );
		return;
	}

	/* At depth discontinuities (object edges, silhouettes), dFdx/dFdy gradients are
	 * unreliable and produce halos. Reject pixels where neighbor depth differs too much. */
	float maxDepthGradient = pc.misc2.w;
	if ( maxDepthGradient > 0.0 ) {
		vec2 invSize = vec2( pc.misc.y, pc.misc.z );
		float dx = textureLod( depthTex, frag_tex_coord + vec2( invSize.x, 0.0 ), 0.0 ).r;
		float dy = textureLod( depthTex, frag_tex_coord + vec2( 0.0, invSize.y ), 0.0 ).r;
		float dxm = textureLod( depthTex, frag_tex_coord - vec2( invSize.x, 0.0 ), 0.0 ).r;
		float dym = textureLod( depthTex, frag_tex_coord - vec2( 0.0, invSize.y ), 0.0 ).r;
		float maxDiff = max( max( abs( dx - depth ), abs( dxm - depth ) ), max( abs( dy - depth ), abs( dym - depth ) ) );
		if ( maxDiff > maxDepthGradient ) {
			out_color = vec4( 1.0 );
			return;
		}
	}

	vec3 viewPos = reconstructViewPos( frag_tex_coord, depth );
	vec3 dx = dFdx( viewPos );
	vec3 dy = dFdy( viewPos );
	vec3 normal = normalize( cross( dx, dy ) );
	if ( normal.z < 0.0 ) {
		normal = -normal;
	}

	float ao = ( int( pc.misc2.x + 0.5 ) == 1 ) ? computeHBAO( viewPos, normal ) : computeSSAO( viewPos, normal );
	out_color = vec4( ao, ao, ao, 1.0 );
}
