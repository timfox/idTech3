#version 450

layout(set = 0, binding = 0) uniform sampler2D texture0;
layout(set = 1, binding = 0) uniform sampler2D texture1;
layout(set = 2, binding = 0) uniform sampler2D texture2;
layout(set = 3, binding = 0) uniform sampler2D texture3;

layout(location = 0) in vec2 tex_coord;

layout(location = 0) out vec4 out_color;

//layout(constant_id = 0) const float gamma = 1.0;
//layout(constant_id = 1) const float obScale = 2.0;
//layout(constant_id = 2) const float greyscale = 0.0;
//layout(constant_id = 3) const float threshold = 0.6;
layout(constant_id = 4) const float factor = 0.5;
layout(constant_id = 27) const float scatter = 0.72;
layout(constant_id = 28) const int preserve_energy = 1;

//const vec3 sRGB = { 0.2126, 0.7152, 0.0722 };

void main()
{
	float s = clamp( scatter, 0.05, 1.0 );
	float w0 = 1.0;
	float w1 = s;
	float w2 = s * s;
	float w3 = s * s * s;
	float norm = 1.0;
	vec3 base;

	if ( preserve_energy != 0 ) {
		norm = 1.0 / max( w0 + w1 + w2 + w3, 1e-4 );
	}

	base = textureLod( texture0, tex_coord, 0.0 ).rgb * w0 +
		textureLod( texture1, tex_coord, 0.0 ).rgb * w1 +
		textureLod( texture2, tex_coord, 0.0 ).rgb * w2 +
		textureLod( texture3, tex_coord, 0.0 ).rgb * w3;

	if ( dot(base,base) == 0.0 )
	{
		discard;
	}

	out_color = vec4( base * factor * norm, 0.0 );
}
