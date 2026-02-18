#version 450

layout(set = 0, binding = 0) uniform sampler2D sceneTexture;
layout(set = 0, binding = 1) uniform sampler2D bloomTexture0;
layout(set = 0, binding = 2) uniform sampler2D bloomTexture1;
layout(set = 0, binding = 3) uniform sampler2D bloomTexture2;
layout(set = 0, binding = 4) uniform sampler2D bloomTexture3;

layout(location = 0) in vec2 frag_tex_coord;

layout(location = 0) out vec4 out_color;

layout(constant_id = 0) const float bloomIntensity = 0.1;
layout(constant_id = 1) const float exposure = 1.0;
layout(constant_id = 2) const float bloomClamp = 8.0;
layout(constant_id = 3) const int tonemap_mode = 1;
layout(constant_id = 4) const int postprocess_enabled = 1;

layout(push_constant) uniform PaniniPC {
	float aspect;
	float paniniD;
	float paniniS;
	float padding;
} paniniPC;

vec3 Tonemap_ACES( vec3 x ) {
	const float a = 2.51;
	const float b = 0.03;
	const float c = 2.43;
	const float d = 0.59;
	const float e = 0.14;
	return clamp( (x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0 );
}

vec3 Tonemap_Reinhard( vec3 x ) {
	return x / ( x + vec3( 1.0 ) );
}

vec2 panini_project( vec2 uv, float aspect, float d, float s ) {
	float safeAspect = max( aspect, 0.0001 );

	vec2 p = uv * 2.0 - 1.0;
	p.x *= safeAspect;

	float x2 = p.x * p.x;
	float y2 = p.y * p.y;

	float invLen = inversesqrt( 1.0 + x2 + y2 );
	float c = ( d + 1.0 ) / ( d + invLen );
	vec2 pp = p * c;

	pp.y = mix( pp.y, pp.y * ( 1.0 + s * ( abs( pp.x ) / safeAspect ) ), s );

	pp.x /= safeAspect;

	return pp * 0.5 + 0.5;
}

vec3 gatherBloom( vec2 uv ) {
	vec3 bloom = texture( bloomTexture0, uv ).rgb;
	bloom += texture( bloomTexture1, uv ).rgb;
	bloom += texture( bloomTexture2, uv ).rgb;
	bloom += texture( bloomTexture3, uv ).rgb;
	return bloom;
}

void main() {
	vec2 uv = frag_tex_coord;

	if ( paniniPC.paniniD > 0.0001 ) {
		uv = panini_project( uv, paniniPC.aspect, paniniPC.paniniD, paniniPC.paniniS );
	}

	uv = clamp( uv, 0.0, 1.0 );

	vec3 scene = texture( sceneTexture, uv ).rgb;
	vec3 bloom = gatherBloom( uv );
	vec3 color = scene;

	if ( postprocess_enabled != 0 ) {
		bloom = min( bloom, vec3( bloomClamp ) );
		color = scene + bloom * bloomIntensity;
		color *= exposure;
	}

	if ( tonemap_mode == 2 ) {
		color = Tonemap_ACES( color );
	} else {
		color = Tonemap_Reinhard( color );
	}

	color = clamp( color, vec3( 0.0 ), vec3( 1.0 ) );
	out_color = vec4( color, 1.0 );
}
