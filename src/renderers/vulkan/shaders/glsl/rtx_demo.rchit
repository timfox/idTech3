#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;

layout(set = 0, binding = 2, std140) uniform RtxFrame {
	mat4 invViewProj;
	vec4 viewOrigin;
	vec4 zNearFar;
	vec4 outputSize; /* xy = resolution; z = r_rtx mode (1-3); w = r_rtxComposite blend */
	vec4 traceParams;
} rtx;

void main()
{
	/* Base diffuse-style albedo (no texture fetch yet). */
	vec3 base = vec3( 0.72, 0.70, 0.66 );
	int mode = int( clamp( floor( rtx.outputSize.z + 0.5 ), 0.0, 3.0 ) );

	/* r_rtx: 1=shadows (darken), 2=reflections (cool tint), 3=full (warm accent). */
	if ( mode == 1 ) {
		hitValue = base * 0.35;
	} else if ( mode == 2 ) {
		hitValue = mix( base, vec3( 0.45, 0.62, 0.95 ), 0.55 );
	} else if ( mode == 3 ) {
		hitValue = mix( base, vec3( 0.95, 0.72, 0.42 ), 0.35 );
	} else {
		hitValue = base;
	}
}
