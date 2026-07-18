/* Requires hybrid1_ubo.glsl included before this file.
 * Parent rchit must enable GL_EXT_nonuniform_qualifier.
 */

layout( set = 0, binding = 9, std430 ) readonly buffer WorldAlbedoSSBO {
	float rgb[];
} worldAlbedo;

layout( set = 0, binding = 12, std430 ) readonly buffer WorldNormalSSBO {
	float nxyz[];
} worldNormal;

layout( set = 0, binding = 13, std430 ) readonly buffer EntityAlbedoSSBO {
	float rgb[];
} entityAlbedo;

layout( set = 0, binding = 14, std430 ) readonly buffer EntityNormalSSBO {
	float nxyz[];
} entityNormal;

/* D2 Phase A: per-prim material indirection (textureIndex 0xFFFFFFFF = SSBO RGB). */
struct Hybrid1PrimMaterial {
	uint textureIndex;
	uint uvSetFlags; /* low 16 = uvSet, high 16 = flags */
};

layout( set = 0, binding = 16, std430 ) readonly buffer PrimMaterialSSBO {
	Hybrid1PrimMaterial mats[];
} primMat;

/* Phase A.1b: bindless diffuse array (centroid UV until AS attrs land). */
layout( set = 0, binding = 15 ) uniform sampler2D bindlessDiffuse[];

vec3 hybrid1_defaultAlbedo( void )
{
	return vec3( 0.72, 0.70, 0.66 );
}

vec3 hybrid1_sampleWorldAlbedoSSBO( void )
{
	uint n = uint( worldAlbedo.rgb.length() ) / 3u;
	if ( gl_InstanceCustomIndexEXT != 0 || n == 0u ) {
		return vec3( -1.0 );
	}
	if ( gl_PrimitiveID < 0 || uint( gl_PrimitiveID ) >= n ) {
		return vec3( -1.0 );
	}
	uint i = uint( gl_PrimitiveID ) * 3u;
	return vec3( worldAlbedo.rgb[i], worldAlbedo.rgb[i + 1u], worldAlbedo.rgb[i + 2u] );
}

vec3 hybrid1_sampleEntityAlbedoSSBO( void )
{
	uint n = uint( entityAlbedo.rgb.length() ) / 3u;
	if ( gl_InstanceCustomIndexEXT != 1 || n == 0u ) {
		return vec3( -1.0 );
	}
	if ( gl_PrimitiveID < 0 || uint( gl_PrimitiveID ) >= n ) {
		return vec3( -1.0 );
	}
	uint i = uint( gl_PrimitiveID ) * 3u;
	return vec3( entityAlbedo.rgb[i], entityAlbedo.rgb[i + 1u], entityAlbedo.rgb[i + 2u] );
}

vec3 hybrid1_sampleWorldNormalSSBO( void )
{
	uint n = uint( worldNormal.nxyz.length() ) / 3u;
	if ( gl_InstanceCustomIndexEXT != 0 || n == 0u ) {
		return vec3( 0.0 );
	}
	if ( gl_PrimitiveID < 0 || uint( gl_PrimitiveID ) >= n ) {
		return vec3( 0.0 );
	}
	uint i = uint( gl_PrimitiveID ) * 3u;
	vec3 N = vec3( worldNormal.nxyz[i], worldNormal.nxyz[i + 1u], worldNormal.nxyz[i + 2u] );
	float len2 = dot( N, N );
	if ( len2 < 1e-8 ) {
		return vec3( 0.0 );
	}
	return N * inversesqrt( len2 );
}

vec3 hybrid1_sampleEntityNormalSSBO( void )
{
	uint n = uint( entityNormal.nxyz.length() ) / 3u;
	if ( gl_InstanceCustomIndexEXT != 1 || n == 0u ) {
		return vec3( 0.0 );
	}
	if ( gl_PrimitiveID < 0 || uint( gl_PrimitiveID ) >= n ) {
		return vec3( 0.0 );
	}
	uint i = uint( gl_PrimitiveID ) * 3u;
	vec3 N = vec3( entityNormal.nxyz[i], entityNormal.nxyz[i + 1u], entityNormal.nxyz[i + 2u] );
	float len2 = dot( N, N );
	if ( len2 < 1e-8 ) {
		return vec3( 0.0 );
	}
	return N * inversesqrt( len2 );
}

/* Geometric world normal facing the incoming ray; falls back to -rayDir. */
vec3 hybrid1_sampleHitNormal( void )
{
	vec3 N = hybrid1_sampleWorldNormalSSBO();
	if ( dot( N, N ) < 1e-8 ) {
		N = hybrid1_sampleEntityNormalSSBO();
	}
	vec3 towardRay = normalize( -gl_WorldRayDirectionEXT );
	if ( dot( N, N ) < 1e-8 ) {
		return towardRay;
	}
	if ( dot( N, towardRay ) < 0.0 ) {
		N = -N;
	}
	return N;
}

vec3 hybrid1_sampleHitAlbedo( sampler2D albedoTex )
{
	/*
	 * D2 Phase A.1b: when bindlessMeta.x > 0, sample the bindless array at
	 * centroid UV (0.5) until AS vertex UVs land. Otherwise SSBO / G-buffer.
	 */
	{
		uint nMat = uint( primMat.mats.length() );
		uint primIdx = uint( max( gl_PrimitiveID, 0 ) );
		uint texCount = uint( max( h1.bindlessMeta.x, 0.0 ) );
		if ( gl_InstanceCustomIndexEXT == 1 ) {
			primIdx += uint( max( h1.viewOrigin.w, 0.0 ) );
		}
		if ( texCount > 0u && nMat > 0u && primIdx < nMat ) {
			uint texIdx = primMat.mats[primIdx].textureIndex;
			if ( texIdx != 0xFFFFFFFFu && texIdx < texCount ) {
				vec3 c = texture( nonuniformEXT( bindlessDiffuse[texIdx] ), vec2( 0.5 ) ).rgb;
				if ( dot( c, c ) > 1e-8 ) {
					return c;
				}
			}
		}
	}

	vec3 ssbo = hybrid1_sampleWorldAlbedoSSBO();
	if ( ssbo.x < 0.0 ) {
		ssbo = hybrid1_sampleEntityAlbedoSSBO();
	}
	if ( ssbo.x >= 0.0 ) {
		return ssbo;
	}

	if ( h1.params1.z <= 0.5 ) {
		return hybrid1_defaultAlbedo();
	}

	float t = gl_RayTmaxEXT;
	vec3 hitPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * t;
	vec4 clip = h1.viewProj * vec4( hitPos, 1.0 );
	if ( abs( clip.w ) < 1e-6 ) {
		return hybrid1_defaultAlbedo();
	}

	vec2 uv = clip.xy / clip.w * 0.5 + 0.5;
	if ( uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 ) {
		return hybrid1_defaultAlbedo();
	}

	vec3 albedo = texture( albedoTex, uv ).rgb;
	if ( dot( albedo, albedo ) < 1e-6 ) {
		return hybrid1_defaultAlbedo();
	}
	return albedo;
}
