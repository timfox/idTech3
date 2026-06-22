/* Shared helpers — Granja/Pereira hybrid SVGF denoising (July 2021). */

const vec3 HYBRID1_LUMA = vec3( 0.2126, 0.7152, 0.0722 );

float hybrid1_luma( vec3 c )
{
	return dot( HYBRID1_LUMA, c );
}

vec3 hybrid1_decodeNormal( sampler2D normalTex, vec2 uv, uint hasGBuffer )
{
	if ( hasGBuffer != 0u ) {
		vec3 n = texture( normalTex, uv ).xyz * 2.0 - 1.0;
		float len = length( n );
		if ( len > 0.01 ) {
			return n / len;
		}
	}
	return vec3( 0.0, 0.0, 1.0 );
}

float hybrid1_decodeRoughness( sampler2D materialTex, vec2 uv, uint hasGBuffer )
{
	if ( hasGBuffer != 0u ) {
		return clamp( texture( materialTex, uv ).g, 0.02, 1.0 );
	}
	return 0.5;
}

vec3 hybrid1_worldPosFromDepth( mat4 invViewProj, vec2 uv, float depth )
{
	vec4 clip = vec4( uv * 2.0 - 1.0, depth, 1.0 );
	vec4 worldPos = invViewProj * clip;
	worldPos /= max( abs( worldPos.w ), 1e-6 );
	return worldPos.xyz;
}

vec2 hybrid1_reprojectUV( mat4 invViewProj, mat4 prevViewProj, vec2 uv, float depth )
{
	vec3 worldPos = hybrid1_worldPosFromDepth( invViewProj, uv, depth );
	vec4 prevClip = prevViewProj * vec4( worldPos, 1.0 );
	return prevClip.xy / max( prevClip.w, 1e-6 ) * 0.5 + 0.5;
}

vec2 hybrid1_historyUV( mat4 invViewProj, mat4 prevViewProj, vec2 uv, float depth,
	sampler2D motionTex, uint useMotion )
{
	if ( useMotion != 0u ) {
		vec2 motion = texture( motionTex, uv ).rg;
		return uv - motion;
	}
	return hybrid1_reprojectUV( invViewProj, prevViewProj, uv, depth );
}

void hybrid1_varianceColorClamp3( sampler2D srcTex, vec2 uv, vec2 texel, float gamma,
	out vec3 mn, out vec3 mx, out vec3 mean, out vec3 variance )
{
	vec3 m1 = vec3( 0.0 );
	vec3 m2 = vec3( 0.0 );
	mn = vec3( 1e30 );
	mx = vec3( -1e30 );
	int y;
	int x;
	for ( y = -1; y <= 1; y++ ) {
		for ( x = -1; x <= 1; x++ ) {
			vec3 s = texture( srcTex, uv + vec2( float( x ), float( y ) ) * texel ).rgb;
			mn = min( mn, s );
			mx = max( mx, s );
			m1 += s;
			m2 += s * s;
		}
	}
	mean = m1 * ( 1.0 / 9.0 );
	variance = max( m2 * ( 1.0 / 9.0 ) - mean * mean, vec3( 0.0 ) );
	vec3 sigma = sqrt( variance ) * gamma;
	mn = mean - sigma;
	mx = mean + sigma;
}

float hybrid1_varianceColorClamp1( sampler2D srcTex, vec2 uv, vec2 texel, float gamma,
	out float mn, out float mx, out float mean, out float variance )
{
	float m1 = 0.0;
	float m2 = 0.0;
	mn = 1e30;
	mx = -1e30;
	int y;
	int x;
	for ( y = -1; y <= 1; y++ ) {
		for ( x = -1; x <= 1; x++ ) {
			float s = texture( srcTex, uv + vec2( float( x ), float( y ) ) * texel ).r;
			mn = min( mn, s );
			mx = max( mx, s );
			m1 += s;
			m2 += s * s;
		}
	}
	mean = m1 * ( 1.0 / 9.0 );
	variance = max( m2 * ( 1.0 / 9.0 ) - mean * mean, 0.0 );
	float sigma = sqrt( variance ) * gamma;
	mn = mean - sigma;
	mx = mean + sigma;
	return mean;
}

vec3 hybrid1_clampVec3( vec3 v, vec3 mn, vec3 mx )
{
	return clamp( v, mn, mx );
}

float hybrid1_clampFloat( float v, float mn, float mx )
{
	return clamp( v, mn, mx );
}

float hybrid1_atrousWeight( float centerL, float sampleL, float centerVar, float phiColor )
{
	float diff = abs( centerL - sampleL );
	float v = max( centerVar, 1e-4 );
	return exp( -diff / max( phiColor * sqrt( v ), 1e-4 ) );
}
