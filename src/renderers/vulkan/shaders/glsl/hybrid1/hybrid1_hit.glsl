/* Requires hybrid1_ubo.glsl included before this file. */

vec3 hybrid1_defaultAlbedo( void )
{
	return vec3( 0.72, 0.70, 0.66 );
}

vec3 hybrid1_sampleHitAlbedo( sampler2D albedoTex )
{
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
