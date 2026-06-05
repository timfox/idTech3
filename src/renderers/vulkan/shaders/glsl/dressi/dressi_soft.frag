#version 450
/*
 * HardSoftRas fragment: signed pixel-to-edge distance, Shift() depth, depth peel, G-buffer outputs.
 */
layout( location = 0 ) flat in int vTriId;

layout( push_constant ) uniform DressiSoftFragPush {
	vec4 extent;   /* xy=resolution */
	vec4 params;   /* x=prev peel depth (sample), y=peel index, z=sigma, w=unused */
} pc;

layout( set = 0, binding = 0 ) uniform sampler2D prevPeelDepthTex;

layout( std430, set = 0, binding = 1 ) readonly buffer TriClipBuf {
	vec4 triClip[];
};

layout( location = 0 ) out vec4 outGBuffer; /* r=signed dist, gba=shaded rgb */

float signedDistPointSegment( vec2 p, vec2 a, vec2 b )
{
	vec2 ab = b - a;
	vec2 ap = p - a;
	float t = clamp( dot( ap, ab ) / max( dot( ab, ab ), 1e-8 ), 0.0, 1.0 );
	vec2 c = a + ab * t;
	return length( p - c );
}

float signedDistTri( vec2 p, vec2 v0, vec2 v1, vec2 v2 )
{
	float d0 = signedDistPointSegment( p, v0, v1 );
	float d1 = signedDistPointSegment( p, v1, v2 );
	float d2 = signedDistPointSegment( p, v2, v0 );
	float d = min( d0, min( d1, d2 ) );
	/* Inside test via cross products */
	vec2 e0 = v1 - v0;
	vec2 e1 = v2 - v1;
	vec2 e2 = v0 - v2;
	float c0 = e0.x * ( p.y - v0.y ) - e0.y * ( p.x - v0.x );
	float c1 = e1.x * ( p.y - v1.y ) - e1.y * ( p.x - v1.x );
	float c2 = e2.x * ( p.y - v2.y ) - e2.y * ( p.x - v2.x );
	bool inside = ( c0 >= 0.0 && c1 >= 0.0 && c2 >= 0.0 ) || ( c0 <= 0.0 && c1 <= 0.0 && c2 <= 0.0 );
	return inside ? d : -d;
}

void main()
{
	int base = vTriId * 3;
	vec4 c0 = triClip[base + 0];
	vec4 c1 = triClip[base + 1];
	vec4 c2 = triClip[base + 2];
	vec2 s0 = c0.xy / max( abs( c0.w ), 1e-6 );
	vec2 s1 = c1.xy / max( abs( c1.w ), 1e-6 );
	vec2 s2 = c2.xy / max( abs( c2.w ), 1e-6 );
	vec2 pixNdc = gl_FragCoord.xy / pc.extent.xy * 2.0 - 1.0;
	pixNdc.y = -pixNdc.y;

	float dist = signedDistTri( pixNdc, s0, s1, s2 );

	float hardDepth = gl_FragCoord.z;
	float softDepth = dist * 0.5 + 0.5;
	float depth = ( dist >= 0.0 ) ? ( hardDepth * 0.5 ) : softDepth;

	if ( pc.params.y > 0.5 ) {
		vec2 uv = gl_FragCoord.xy / pc.extent.xy;
		float prev = texture( prevPeelDepthTex, uv ).r;
		if ( depth <= prev + 1e-6 ) {
			discard;
		}
	}

	gl_FragDepth = depth;

	vec3 shade = vec3( 0.85, 0.82, 0.78 );
	outGBuffer = vec4( dist, shade );
}
