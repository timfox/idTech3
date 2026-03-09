#version 450

// 128 bytes
layout(push_constant) uniform Transform {
	mat4 mvp;
	mat4 prevMvp;
};

layout(location = 0) in vec3 in_position;
//layout(location = 1) in vec4 in_color;
//layout(location = 2) in vec2 in_tex_coord0;
//layout(location = 3) in vec2 in_tex_coord1;

//layout(location = 0) out vec4 frag_color;
//layout(location = 1) out vec2 frag_tex_coord0;
//layout(location = 2) out vec2 frag_tex_coord1;
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
}
