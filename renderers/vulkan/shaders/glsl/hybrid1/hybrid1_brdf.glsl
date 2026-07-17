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
	vec3 up = abs( N.z ) < 0.999 ? vec3( 0.0, 0.0, 1.0 ) : vec3( 1.0, 0.0, 0.0 );
	vec3 T = normalize( cross( up, N ) );
	vec3 B = cross( N, T );
	vec3 Ve = vec3( dot( V, T ), dot( V, B ), dot( V, N ) );
	vec3 Vh = normalize( vec3( a * Ve.x, a * Ve.y, Ve.z ) );
	float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
	vec3 T1 = lensq > 0.0 ? vec3( -Vh.y, Vh.x, 0.0 ) / sqrt( lensq ) : vec3( 1.0, 0.0, 0.0 );
	vec3 T2 = cross( Vh, T1 );
	float r = sqrt( u.x );
	float phi = 6.2831853 * u.y;
	float t1 = r * cos( phi );
	float t2 = r * sin( phi );
	float s = 0.5 * ( 1.0 + Vh.z );
	t2 = ( 1.0 - s ) * sqrt( max( 1.0 - t1 * t1, 0.0 ) ) + s * t2;
	vec3 Nh = t1 * T1 + t2 * T2 + sqrt( max( 1.0 - t1 * t1 - t2 * t2, 0.0 ) ) * Vh;
	vec3 Ne = normalize( vec3( a * Nh.x, a * Nh.y, max( Nh.z, 0.0 ) ) );
	return normalize( T * Ne.x + B * Ne.y + N * Ne.z );
}

vec3 hybrid1_fresnelSchlick( float cosTheta, vec3 F0 )
{
	return F0 + ( 1.0 - F0 ) * pow( clamp( 1.0 - cosTheta, 0.0, 1.0 ), 5.0 );
}
