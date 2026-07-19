#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;

layout(set = 0, binding = 2, std140) uniform RtxFrame {
	mat4 invViewProj;
	vec4 viewOrigin;
	vec4 zNearFar;
	vec4 outputSize; /* xy = resolution; z = r_rtx mode (0-3); w = composite blend */
	vec4 traceParams;
	vec4 sunDir;     /* xyz = world-space direction toward sun */
	vec4 primCounts; /* x = world albedo prims, y = world normal prims */
} rtx;

layout(set = 0, binding = 6, scalar) readonly buffer WorldAlbedo {
	float albedo[]; /* RGB per world primitive */
};

layout(set = 0, binding = 7, scalar) readonly buffer WorldNormal {
	float normals[]; /* XYZ per world primitive */
};

vec3 fetchAlbedo( uint prim )
{
	uint count = uint( max( rtx.primCounts.x, 0.0 ) );
	if ( count == 0u || prim >= count ) {
		return vec3( 0.72, 0.70, 0.66 );
	}
	uint i = prim * 3u;
	return vec3( albedo[i], albedo[i + 1u], albedo[i + 2u] );
}

vec3 fetchNormal( uint prim )
{
	uint count = uint( max( rtx.primCounts.y, 0.0 ) );
	if ( count == 0u || prim >= count ) {
		return vec3( 0.0, 0.0, 1.0 );
	}
	uint i = prim * 3u;
	vec3 n = vec3( normals[i], normals[i + 1u], normals[i + 2u] );
	float len = length( n );
	return ( len > 1e-5 ) ? ( n / len ) : vec3( 0.0, 0.0, 1.0 );
}

float traceSunShadow( vec3 worldPos, vec3 L )
{
	const uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT
		| gl_RayFlagsOpaqueEXT
		| gl_RayFlagsSkipClosestHitShaderEXT;
	/* 0 = shadowed until shadow-miss (index 1) sets 1. */
	hitValue = vec3( 0.0 );
	traceRayEXT( topLevelAS, rayFlags, 0xFFu,
		0u, 0u, 1u,
		worldPos + L * 0.05, 0.05, L, 1.0e5, 0 );
	return clamp( hitValue.x, 0.0, 1.0 );
}

void main()
{
	uint inst = gl_InstanceCustomIndexEXT & 0xFFu;
	uint prim = uint( gl_PrimitiveID );
	int mode = int( clamp( floor( rtx.outputSize.z + 0.5 ), 0.0, 3.0 ) );

	vec3 base = fetchAlbedo( prim );
	vec3 N = fetchNormal( prim );

	/* Entity proxy BLAS uses instanceCustomIndex 1 — tint until entity albedo is bound. */
	if ( inst == 1u ) {
		base = mix( base, vec3( 0.55, 0.82, 0.62 ), 0.45 );
		N = normalize( -gl_WorldRayDirectionEXT );
	}

	if ( dot( N, -gl_WorldRayDirectionEXT ) < 0.0 ) {
		N = -N;
	}

	vec3 L = normalize( rtx.sunDir.xyz );
	if ( length( rtx.sunDir.xyz ) < 1e-4 ) {
		L = normalize( vec3( 0.35, 0.75, 0.45 ) );
	}
	float ndl = max( dot( N, L ), 0.0 );
	float ambient = 0.22;
	vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;

	if ( mode == 1 ) {
		float shadow = ( ndl > 1e-4 ) ? traceSunShadow( worldPos, L ) : 0.0;
		hitValue = base * ( ambient + ( 1.0 - ambient ) * ndl * shadow );
	} else if ( mode == 2 ) {
		float lit = ambient + ( 1.0 - ambient ) * ndl;
		hitValue = mix( base * lit, vec3( 0.45, 0.62, 0.95 ), 0.45 );
	} else if ( mode == 3 ) {
		float shadow = ( ndl > 1e-4 ) ? traceSunShadow( worldPos, L ) : 0.0;
		vec3 shaded = base * ( ambient + ( 1.0 - ambient ) * ndl * shadow );
		hitValue = mix( shaded, vec3( 0.95, 0.72, 0.42 ), 0.22 );
	} else {
		float lit = ambient + ( 1.0 - ambient ) * ndl;
		hitValue = base * lit;
	}
}
