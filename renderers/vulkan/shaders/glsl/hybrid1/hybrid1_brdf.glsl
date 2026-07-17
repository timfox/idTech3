/* GGX / VNDF helpers for Hybrid1 specular. */

float hybrid1_dGgx( float NdotH, float alpha )
{
	float a2 = max( alpha * alpha, 1e-6 );
	float d = max( ( NdotH * a2 - NdotH ) * NdotH + 1.0, 1e-6 );
	return a2 / ( 3.14159265 * d * d );
}

vec3 hybrid1_ggxVndf( vec3 V, vec3 N, float roughness, vec2 u )
{
	float a = max( roughness * roughness, 0.001 );
	float nLen = length( N );
	float vLen = length( V );
	if ( nLen < 1e-6 || vLen < 1e-6 ) {
		return vec3( 0.0, 0.0, 1.0 );
	}
	N /= nLen;
	V /= vLen;
	/* Heitz VNDF assumes V in the upper hemisphere about N. */
	if ( dot( V, N ) <= 1e-4 ) {
		return N;
	}
	vec3 up = abs( N.z ) < 0.999 ? vec3( 0.0, 0.0, 1.0 ) : vec3( 1.0, 0.0, 0.0 );
	vec3 T = cross( up, N );
	float tLen = length( T );
	if ( tLen < 1e-6 ) {
		return N;
	}
	T /= tLen;
	vec3 B = cross( N, T );
	vec3 Ve = vec3( dot( V, T ), dot( V, B ), max( dot( V, N ), 1e-4 ) );
	vec3 Vh = vec3( a * Ve.x, a * Ve.y, Ve.z );
	float vhLen = length( Vh );
	if ( vhLen < 1e-6 ) {
		return N;
	}
	Vh /= vhLen;
	float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
	vec3 T1 = lensq > 1e-8 ? vec3( -Vh.y, Vh.x, 0.0 ) * inversesqrt( lensq ) : vec3( 1.0, 0.0, 0.0 );
	vec3 T2 = cross( Vh, T1 );
	float r = sqrt( clamp( u.x, 0.0, 1.0 ) );
	float phi = 6.2831853 * fract( u.y );
	float t1 = r * cos( phi );
	float t2 = r * sin( phi );
	float s = 0.5 * ( 1.0 + Vh.z );
	t2 = ( 1.0 - s ) * sqrt( max( 1.0 - t1 * t1, 0.0 ) ) + s * t2;
	float nhZ = sqrt( max( 1.0 - t1 * t1 - t2 * t2, 0.0 ) );
	vec3 Nh = t1 * T1 + t2 * T2 + nhZ * Vh;
	vec3 Ne = vec3( a * Nh.x, a * Nh.y, max( Nh.z, 0.0 ) );
	float neLen = length( Ne );
	if ( neLen < 1e-6 ) {
		return N;
	}
	Ne /= neLen;
	vec3 H = T * Ne.x + B * Ne.y + N * Ne.z;
	float hLen = length( H );
	if ( hLen < 1e-6 || !all( equal( H, H ) ) ) {
		return N;
	}
	return H / hLen;
}

vec3 hybrid1_fresnelSchlick( float cosTheta, vec3 F0 )
{
	return F0 + ( 1.0 - F0 ) * pow( clamp( 1.0 - cosTheta, 0.0, 1.0 ), 5.0 );
}
