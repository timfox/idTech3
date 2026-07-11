/* Shared Hybrid1 per-frame UBO (std140). */
layout( set = 0, binding = 2, std140 ) uniform Hybrid1Frame {
	mat4 invViewProj;
	mat4 prevViewProj;
	mat4 viewProj;
	vec4 viewOrigin;
	vec4 sunDirection; /* xyz = dir, w = angular radius (degrees) */
	vec4 outputSize;   /* xy = extent, z = sunLight scale, w = dlightShadows count */
	vec4 params0; /* x=history gamma, y=temporal alpha, z=reinhard mul, w=frame hash */
	vec4 params1; /* x=shadow str, y=spec str, z=hasGBuffer, w=ibl on */
	vec4 params2; /* x=rayBias, y=tMin, z=contactHarden, w=specRoughMax */
	vec4 params3; /* x=ggx, y=iblMode, z=diffuseDirect, w=unused */
	vec4 dlightDir; /* xyz = first dlight world origin, w = weight (0=off) */
} h1;
