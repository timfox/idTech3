/* Octahedral normal encode/decode for G-buffer 2.0 compact packing. */
#ifndef GBUFFER_OCTAHEDRAL_GLSL
#define GBUFFER_OCTAHEDRAL_GLSL

vec2 octahedron_wrap( vec2 v )
{
	return ( 1.0 - abs( v.yx ) ) * vec2( v.x >= 0.0 ? 1.0 : -1.0, v.y >= 0.0 ? 1.0 : -1.0 );
}

vec2 encode_octahedral( vec3 n )
{
	n = normalize( n );
	n.xy /= ( abs( n.x ) + abs( n.y ) + abs( n.z ) );
	n.xy = ( n.z >= 0.0 ) ? n.xy : octahedron_wrap( n.xy );
	return n.xy * 0.5 + 0.5;
}

vec3 decode_octahedral( vec2 e )
{
	vec2 f = e * 2.0 - 1.0;
	vec3 n = vec3( f.x, f.y, 1.0 - abs( f.x ) - abs( f.y ) );
	float t = clamp( -n.z, 0.0, 1.0 );
	n.xy += vec2( n.x >= 0.0 ? -t : t, n.y >= 0.0 ? -t : t );
	return normalize( n );
}

/* Legacy Gbuf* names used by deferred_gbuffer_fill.comp. */
vec2 GbufOctahedronWrap( vec2 v )
{
	return octahedron_wrap( v );
}

vec2 GbufEncodeOctahedral( vec3 n )
{
	return encode_octahedral( n );
}

vec3 GbufDecodeOctahedral( vec2 e )
{
	return decode_octahedral( e );
}

#endif
