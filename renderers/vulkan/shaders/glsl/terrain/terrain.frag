#version 450
/*
 * Terrain fragment shader - diffuse + optional splat control blend (up to 4 layers).
 */
layout(location = 0) in vec2 inTexCoord;
layout(location = 1) in vec4 inWeights; /* vertex RGBA = splat weights when materialBlend splat */

layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform sampler2D heightmap;
layout(binding = 1) uniform sampler2D diffuseMap;
layout(binding = 2) uniform sampler2D splatMap;
layout(binding = 3) uniform sampler2D layer1Map;
layout(binding = 4) uniform sampler2D layer2Map;
layout(binding = 5) uniform sampler2D layer3Map;

layout(constant_id = 0) const int use_splat = 0;

void main() {
	if ( use_splat != 0 ) {
		vec4 w = max( inWeights, vec4( 0.0 ) );
		/* Prefer splat texture when bound; else vertex weights. */
		vec4 sw = texture( splatMap, inTexCoord );
		if ( dot( sw, sw ) > 0.001 ) {
			w = max( sw, vec4( 0.0 ) );
		}
		float sum = max( w.r + w.g + w.b + w.a, 1e-4 );
		w /= sum;
		vec3 c0 = texture( diffuseMap, inTexCoord ).rgb;
		vec3 c1 = texture( layer1Map, inTexCoord ).rgb;
		vec3 c2 = texture( layer2Map, inTexCoord ).rgb;
		vec3 c3 = texture( layer3Map, inTexCoord ).rgb;
		outColor = vec4( c0 * w.r + c1 * w.g + c2 * w.b + c3 * w.a, 1.0 );
	} else {
		vec4 diffuse = texture( diffuseMap, inTexCoord );
		outColor = vec4( diffuse.rgb, 1.0 );
	}
}
