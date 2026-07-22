#version 450

layout(set = 0, binding = 0) uniform sampler2D sceneBaseTex;
layout(set = 0, binding = 1) uniform sampler2D litTex;

layout(push_constant) uniform CompositePC {
	uint additive;
	uint hybridCompare;
} pc;

layout(location = 0) in vec2 frag_tex_coord;
layout(location = 0) out vec4 out_color;

void main() {
	vec3 dynamic = texture( litTex, frag_tex_coord ).rgb;
	vec3 base = texture( sceneBaseTex, frag_tex_coord ).rgb;
	vec3 deferred = ( pc.additive != 0u ) ? ( base + dynamic ) : dynamic;

	/*
	 * hybridCompare:
	 * 1 = split L deferred / R Forward+ (discard right → keep scene Forward+)
	 * 2 = abs RGB diff
	 * 3 = relative luma
	 * 4–5 = tinted energy proxies (full-frame deferred composite with heat)
	 * 6–8 = mismatch markers (magenta / cyan heat on deferred)
	 */
	if ( pc.hybridCompare == 1u ) {
		if ( frag_tex_coord.x >= 0.5 ) {
			discard;
		}
		out_color = vec4( deferred, 1.0 );
		return;
	}
	if ( pc.hybridCompare >= 2u ) {
		vec3 fpApprox = base; /* scene already has Forward+ opaque shade when handoff off on right */
		vec3 diff = abs( deferred - fpApprox );
		float lumaD = dot( deferred, vec3( 0.2126, 0.7152, 0.0722 ) );
		float lumaF = dot( fpApprox, vec3( 0.2126, 0.7152, 0.0722 ) );
		float rel = abs( lumaD - lumaF ) / max( max( lumaD, lumaF ), 1e-3 );
		if ( pc.hybridCompare == 2u ) {
			out_color = vec4( diff * 4.0, 1.0 );
			return;
		}
		if ( pc.hybridCompare == 3u ) {
			out_color = vec4( vec3( rel * 4.0 ), 1.0 );
			return;
		}
		if ( pc.hybridCompare == 4u ) {
			out_color = vec4( mix( deferred, vec3( rel, 0.2, 0.05 ), clamp( rel * 2.0, 0.0, 1.0 ) ), 1.0 );
			return;
		}
		if ( pc.hybridCompare == 5u ) {
			out_color = vec4( mix( deferred, vec3( 0.1, 0.3, rel ), clamp( rel * 2.0, 0.0, 1.0 ) ), 1.0 );
			return;
		}
		if ( pc.hybridCompare == 6u ) {
			/* Cluster index mismatch proxy: large relative error → magenta. */
			float m = step( 0.08, rel );
			out_color = vec4( mix( deferred, vec3( 1.0, 0.0, 1.0 ), m ), 1.0 );
			return;
		}
		if ( pc.hybridCompare == 7u ) {
			float m = step( 0.05, length( diff ) );
			out_color = vec4( mix( deferred, vec3( 0.0, 1.0, 1.0 ), m ), 1.0 );
			return;
		}
		if ( pc.hybridCompare == 8u ) {
			float m = step( 0.1, abs( lumaD - lumaF ) );
			out_color = vec4( mix( deferred, vec3( 1.0, 0.6, 0.0 ), m ), 1.0 );
			return;
		}
	}

	if ( pc.additive != 0u ) {
		out_color = vec4( deferred, 1.0 );
		return;
	}
	out_color = vec4( dynamic, 1.0 );
}
