#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec3 hitValue;

layout(set = 0, binding = 2, std140) uniform GrtxFrame {
	mat4 invViewProj;
	vec4 viewOrigin;
	vec4 zNearFar;
	vec4 outputSize;
	vec4 traceParams;
} grtx;

struct GaussianPrim {
	vec3 position;
	float opacity;
	vec3 scale;
	float sigmaScale;
	vec4 rotation;
	vec3 color;
	float pad;
};

layout(set = 0, binding = 5, std430) readonly buffer GaussianBuffer {
	GaussianPrim g[];
} gaussians;

const uint GRTX_TRIS_PER_BOX = 12u;

void main()
{
	uint gIdx = gl_PrimitiveID / GRTX_TRIS_PER_BOX;
	if ( gIdx >= gaussians.g.length() ) {
		hitValue = vec3( 0.5, 0.2, 0.8 );
		return;
	}

	GaussianPrim gp = gaussians.g[gIdx];
	vec3 base = gp.color * clamp( gp.opacity, 0.0, 1.0 );
	int mode = int( clamp( floor( grtx.outputSize.z + 0.5 ), 0.0, 3.0 ) );

	if ( mode == 1 ) {
		hitValue = base * 0.45;
	} else if ( mode == 2 ) {
		hitValue = mix( base, vec3( 0.55, 0.85, 0.95 ), 0.5 );
	} else if ( mode == 3 ) {
		hitValue = mix( base, vec3( 0.95, 0.55, 0.75 ), 0.4 );
	} else {
		hitValue = base;
	}
}
