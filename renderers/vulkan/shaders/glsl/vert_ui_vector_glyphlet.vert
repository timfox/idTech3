#version 450
/*
 * Loop & Blinn glyphlet vertex shader (mode 2 fallback path).
 * Transforms em-space glyphlet vertices to clip space.
 */
layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_canon;
layout(location = 2) in float in_triType;

layout(location = 0) out vec4 frag_color;
layout(location = 1) flat out float frag_triType;
layout(location = 2) out vec2 frag_canon;
layout(location = 13) out vec4 var_CurrentClip;
layout(location = 14) out vec4 var_PrevClip;

layout(push_constant) uniform Transform {
	mat4 mvp;
	mat4 prevMvp;
	vec4 color;
	vec4 glyphOriginScale; /* xy = screen origin, z = scale, w unused */
} pc;

void main() {
	vec2 screen = pc.glyphOriginScale.xy + vec2( in_pos.x * pc.glyphOriginScale.z, -in_pos.y * pc.glyphOriginScale.z );
	vec4 clip = pc.mvp * vec4( screen, 0.0, 1.0 );
	vec4 prevClip = pc.prevMvp * vec4( screen, 0.0, 1.0 );

	gl_Position = clip;
	frag_color = pc.color;
	frag_triType = in_triType;
	frag_canon = in_canon;
	var_CurrentClip = clip;
	var_PrevClip = prevClip;
}
