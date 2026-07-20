#version 460
#extension GL_EXT_ray_tracing : require

/* payload: x = visibility (0=shadowed), y = hit T */
layout( location = 0 ) rayPayloadInEXT vec2 shadowPayload;

void main()
{
	shadowPayload = vec2( 0.0, gl_HitTEXT );
}
