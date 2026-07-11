/* Shared Hybrid1 per-frame UBO (std140). */
layout( set = 0, binding = 2, std140 ) uniform Hybrid1Frame {
	mat4 invViewProj;
	mat4 prevViewProj;
	mat4 viewProj;
	vec4 viewOrigin;
	vec4 sunDirection;
	vec4 outputSize;
	vec4 params0; /* x=history gamma, y=temporal alpha, z=reinhard mul, w=frame hash */
	vec4 params1; /* x=shadow str, y=spec str, z=hasGBuffer, w=ibl on */
	vec4 params2; /* x=rayBias, y=tMin, z=unused, w=specRoughMax */
} h1;
