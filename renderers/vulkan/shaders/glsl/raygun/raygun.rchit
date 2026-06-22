#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : enable
#include "raygun_common.glsl"

layout( location = 0 ) rayPayloadInEXT vec3 primaryColor;
layout( location = 1 ) rayPayloadInEXT float shadowVis;

layout( set = 0, binding = 0 ) uniform accelerationStructureEXT topLevelAS;

int rgMaterialType( void )
{
	return int( gl_PrimitiveID ) & 3;
}

vec3 rgBaseAlbedo( int mat )
{
	if ( mat == 0 ) {
		return vec3( 0.72, 0.70, 0.66 );
	}
	if ( mat == 1 ) {
		return vec3( 0.85, 0.32, 0.28 );
	}
	if ( mat == 2 ) {
		return vec3( 0.92, 0.92, 0.95 );
	}
	return vec3( 0.35, 0.55, 0.82 );
}

vec3 rgTraceReflection( vec3 ro, vec3 rd )
{
	primaryColor = vec3( 0.0 );
	traceRayEXT( topLevelAS, gl_RayFlagsOpaqueEXT, 0xFFu, 0u, 0u, 0u, ro, 0.01, rd, 1.0e5, 0 );
	return primaryColor;
}

vec3 rgTraceRefraction( vec3 ro, vec3 rd )
{
	primaryColor = vec3( 0.0 );
	traceRayEXT( topLevelAS, gl_RayFlagsOpaqueEXT, 0xFFu, 0u, 0u, 0u, ro, 0.01, rd, 1.0e5, 0 );
	return primaryColor;
}

float rgTraceShadow( vec3 ro, vec3 lightDir )
{
	shadowVis = 1.0;
	traceRayEXT( topLevelAS, gl_RayFlagsOpaqueEXT, 0xFFu, 1u, 0u, 0u, ro, 0.02, lightDir, 1.0e5, 1 );
	return shadowVis;
}

void main()
{
	int mat = rgMaterialType();
	vec3 albedo = rgBaseAlbedo( mat );
	vec3 N = normalize( -gl_WorldRayDirectionEXT );
	vec3 V = normalize( -gl_WorldRayDirectionEXT );
	vec3 L = normalize( rg.sunDirection.xyz );
	vec3 ro = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_RayTmaxEXT + N * 0.02;

	float NdotL = max( dot( N, L ), 0.0 );
	float shadow = mix( 1.0, rgTraceShadow( ro, L ), rg.traceParams.z * NdotL );
	vec3 diffuse = albedo * ( 0.15 + 0.85 * shadow * NdotL );

	vec3 color = diffuse;

	if ( mat == 2 && rg.sunDirection.w > 0.01 ) {
		vec3 R = reflect( V, N );
		vec3 spec = rgTraceReflection( ro, R );
		color = mix( diffuse, spec, 0.75 );
	}

	if ( mat == 3 && rg.traceParams.y > 0.01 ) {
		float eta = 1.0 / max( rg.traceParams.w, 1.01 );
		vec3 refrDir = refract( V, N, eta );
		if ( dot( refrDir, refrDir ) > 1e-6 ) {
			vec3 refr = rgTraceRefraction( ro, normalize( refrDir ) );
			color = mix( diffuse, refr, 0.65 );
		}
	}

	primaryColor = color;
}
