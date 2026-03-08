#version 450

layout(set = 0, binding = 0) uniform sampler2D texture0;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

void main()
{
	vec4 overlay = textureLod( texture0, frag_tex_coord, 0.0 );
	out_color = clamp( overlay, 0.0, 1.0 );
}
