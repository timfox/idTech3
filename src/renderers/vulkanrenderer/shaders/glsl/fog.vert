#version 450

// 128 bytes
layout(push_constant) uniform Transform {
	mat4 mvp;
	mat4 prevMvp;
};

layout(set = 0, binding = 0) uniform UBO {
	// light/env parameters:
	vec4 eyePos;				// vertex
	vec4 lightPos;				// vertex: light origin
	vec4 lightColor;			// fragment: rgb + 1/(r*r)
	vec4 lightVector;			// fragment: linear dynamic light
//#ifdef USE_FOG
	// fog parameters:
	vec4 fogDistanceVector;		// vertex
	vec4 fogDepthVector;		// vertex
	vec4 fogEyeT;				// vertex
	vec4 fogColor;				// fragment
//#endif
};

layout(location = 0) in vec3 in_position;
//layout(location = 1) in vec4 in_color;
//layout(location = 2) in vec2 in_tex_coord0;
//layout(location = 3) in vec2 in_tex_coord1;
//layout(location = 4) in vec2 in_tex_coord2;

//layout(location = 0) out vec4 frag_color;
//layout(location = 1) out vec2 frag_tex_coord0;
//layout(location = 2) out vec2 frag_tex_coord1;
//layout(location = 3) out vec2 frag_tex_coord1;
layout(location = 4) out vec2 fog_tex_coord;
layout(location = 13) out vec4 var_CurrentClip;
layout(location = 14) out vec4 var_PrevClip;

out gl_PerVertex {
	vec4 gl_Position;
};

void main() {
	vec4 currClip = mvp * vec4(in_position, 1.0);
	vec4 prevClip = prevMvp * vec4(in_position, 1.0);
	gl_Position = currClip;
	var_CurrentClip = currClip;
	var_PrevClip = prevClip;

	//frag_color = in_color;
	//frag_tex_coord0 = in_tex_coord0;

	// fog calculations...

	float s = dot(in_position, fogDistanceVector.xyz) + fogDistanceVector.w;
	float t = dot(in_position, fogDepthVector.xyz) + fogDepthVector.w;

	if ( fogEyeT.y == 1.0 ) {
		if ( t < 0.0 ) {
			t = 1.0 / 32.0;
		} else {
			t = 31.0 / 32.0;
		}
	} else {
		if ( t < 1.0 ) {
			t = 1.0 / 32.0;
		} else {
			t = 1.0 / 32.0 + (30.0 / 32.0 * t) / ( t - fogEyeT.x );
		}
	}

	fog_tex_coord = vec2(s, t);
}
