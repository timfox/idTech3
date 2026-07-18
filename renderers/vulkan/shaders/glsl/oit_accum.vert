#version 450
/* OIT accumulation VS. Third mat4 is object→world when Forward+-lit OIT is on. */
layout(push_constant) uniform Transform {
	mat4 mvp;
	mat4 prevMvp;
	mat4 model;
};

layout(location = 0) in vec4 in_position;
layout(location = 1) in vec4 in_color0;
layout(location = 2) in vec2 in_tex_coord0;

layout(location = 0) out vec2 frag_tex_coord0;
layout(location = 1) out vec4 frag_color0;
layout(location = 2) out vec3 frag_world_pos;

void main() {
	gl_Position = mvp * in_position;
	frag_tex_coord0 = in_tex_coord0;
	frag_color0 = in_color0;
	frag_world_pos = (model * in_position).xyz;
}
