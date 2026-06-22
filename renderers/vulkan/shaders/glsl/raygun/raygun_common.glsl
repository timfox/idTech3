/* Raygun (Hirsch & Thoman, arXiv:2001.09792) shared uniforms */

layout( set = 0, binding = 2, std140 ) uniform RaygunFrame {
	mat4 invViewProj;
	vec4 viewOrigin;
	vec4 zNearFar;
	vec4 outputSize;   /* xy = resolution; z = r_raygun_fxaa; w = r_raygun_composite */
	vec4 sunDirection; /* xyz = L, w = r_raygun_reflection */
	vec4 traceParams;  /* x = samples, y = refraction, z = shadow strength, w = ior */
} rg;

vec3 rgSkyColor( vec3 dir )
{
	float t = clamp( dir.y * 0.5 + 0.5, 0.0, 1.0 );
	return mix( vec3( 0.35, 0.52, 0.85 ), vec3( 0.92, 0.94, 0.98 ), t );
}
