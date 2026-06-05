#version 450
/*
 * HardSoftRas vertex: enlarge clip-space position away from triangle centroid (Dressi / Takimoto et al. 2022).
 */
layout( location = 0 ) in vec3 aPos;
layout( location = 1 ) in vec3 aCentroid;

layout( push_constant ) uniform DressiSoftVertPush {
	mat4 mvp;
	vec4 params; /* x=blur radius r in NDC units */
} pc;

layout( location = 0 ) flat out int vTriId;

void main()
{
	vTriId = gl_VertexIndex / 3;

	vec4 hard = pc.mvp * vec4( aPos, 1.0 );
	vec4 cent = pc.mvp * vec4( aCentroid, 1.0 );
	vec2 ndc = hard.xy / max( abs( hard.w ), 1e-6 );
	vec2 cndc = cent.xy / max( abs( cent.w ), 1e-6 );
	vec2 dir = ndc - cndc;
	float len = length( dir );
	if ( len > 1e-6 && pc.params.x > 0.0 ) {
		ndc += ( dir / len ) * pc.params.x;
	}
	hard.xy = ndc * hard.w;
	gl_Position = hard;
}
