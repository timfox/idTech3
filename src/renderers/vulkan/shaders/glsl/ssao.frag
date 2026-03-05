#version 450

layout(set = 0, binding = 0) uniform sampler2D depthTex;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

layout(push_constant) uniform SSAOParams {
	vec4 projInfo; // invProj00, invProj11, proj10, proj14
	vec4 params;   // radius, bias, intensity, power
	vec4 misc;     // samples, invWidth, invHeight, depthIsReversed
} pc;

/* Halton(2,3) low-discrepancy sequence for better sample distribution than pure random. */
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

/* Random rotation per pixel to break up banding (spatial variation). */
float rand01( vec2 co ) {
	return fract( sin( dot( co, vec2( 12.9898, 78.233 ) ) ) * 43758.5453 );
}

float viewZFromDepth( float depth ) {
	return -pc.projInfo.w / max( depth + pc.projInfo.z, 1e-6 );
}

void main() {
	float depth = textureLod( depthTex, frag_tex_coord, 0.0 ).r;
	if ( ( pc.misc.w > 0.5 && depth <= 0.001 ) || ( pc.misc.w <= 0.5 && depth >= 0.999 ) ) {
		out_color = vec4( 1.0 );
		return;
	}

	float viewZ = viewZFromDepth( depth );
	float negZ = -viewZ;
	vec2 ndc = frag_tex_coord * 2.0 - 1.0;
	vec3 viewPos = vec3( ndc.x * negZ * pc.projInfo.x, ndc.y * negZ * pc.projInfo.y, viewZ );

	vec3 dx = dFdx( viewPos );
	vec3 dy = dFdy( viewPos );
	vec3 normal = normalize( cross( dx, dy ) );
	if ( normal.z < 0.0 )
		normal = -normal;

	/* Per-pixel random vector for TBN orientation (breaks up banding). */
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
	float intensity = pc.params.z;
	float power = pc.params.w;
	int samples = int( pc.misc.x );

	float occlusion = 0.0;
	for ( int i = 0; i < 32; ++i ) {
		if ( i >= samples )
			break;

		/* Halton sequence for hemisphere: phi from halton2, theta from halton3. */
		float u1 = halton2( i + 1 );
		float u2 = halton3( i + 1 );
		float phi = u1 * 6.283185;
		float cosTheta = 1.0 - u2;
		float sinTheta = sqrt( 1.0 - cosTheta * cosTheta );
		vec3 sampleDir = vec3( cos( phi ) * sinTheta, sin( phi ) * sinTheta, cosTheta );
		sampleDir = tbn * sampleDir;

		vec3 samplePos = viewPos + sampleDir * radius;

		float proj00 = 1.0 / max( pc.projInfo.x, 1e-6 );
		float proj11 = 1.0 / max( pc.projInfo.y, 1e-6 );
		vec2 sampleNdc = vec2( samplePos.x * proj00 / -samplePos.z, samplePos.y * proj11 / -samplePos.z );
		vec2 sampleUV = sampleNdc * 0.5 + 0.5;

		if ( sampleUV.x < 0.0 || sampleUV.x > 1.0 || sampleUV.y < 0.0 || sampleUV.y > 1.0 )
			continue;

		float sampleDepth = textureLod( depthTex, sampleUV, 0.0 ).r;
		float sampleViewZ = viewZFromDepth( sampleDepth );
		float rangeCheck = smoothstep( 0.0, 1.0, radius / max( abs( viewZ - sampleViewZ ), 1e-3 ) );
		if ( ( sampleViewZ - samplePos.z ) > bias )
			occlusion += rangeCheck;
	}

	occlusion = clamp( occlusion / max( float( samples ), 1.0 ), 0.0, 1.0 );
	float ao = 1.0 - occlusion;
	ao = pow( ao, power );
	ao = clamp( 1.0 - ( 1.0 - ao ) * intensity, 0.0, 1.0 );

	out_color = vec4( ao, ao, ao, 1.0 );
}
