#version 450
/*
 * GPU vector font — Lengyel JCGT 2017 / Slug winding coverage.
 * Dual curve lists: X-sorted for horizontal rays, Y-sorted for vertical.
 * reserved[0]=curveStartX, reserved[1]=curveCount, reserved[2]=curveTexWidth,
 * reserved[3]=coverageMode (0 center, 1 dual-axis, 2 adaptive boundary SS).
 *
 * Atlas-free: curve control points + acceleration live in GPU textures/buffers;
 * no raster glyph/MSDF atlas is used.
 */
layout(location = 0) centroid in vec4 frag_color0;
layout(location = 1) centroid in vec2 frag_tex_coord0;
layout(location = 13) in vec4 var_CurrentClip;
layout(location = 14) in vec4 var_PrevClip;

layout(set = 1, binding = 0) uniform sampler2D curveTexture;

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_motion;

layout(push_constant) uniform Transform {
	mat4 mvp;
	mat4 prevMvp;
	float curveStart;
	float curveCount;
	float curveTexWidth;
	float coverageMode;
	float _pad2[7];
} pc;

uint calcRootCode( float y1, float y2, float y3 ) {
	uint i1 = floatBitsToUint( y1 ) >> 31u;
	uint i2 = floatBitsToUint( y2 ) >> 30u;
	uint i3 = floatBitsToUint( y3 ) >> 29u;
	uint shift = ( i2 & 2u ) | ( i1 & ~2u );
	shift = ( i3 & 4u ) | ( shift & ~4u );
	return ( ( 0x2E74u >> shift ) & 0x0101u );
}

vec2 solveHorizPoly( vec4 p12, vec2 p3 ) {
	vec2 a = p12.xy - p12.zw * 2.0 + p3;
	vec2 b = p12.xy - p12.zw;
	float ra = 1.0 / a.y;
	float rb = 0.5 / b.y;
	float d = sqrt( max( b.y * b.y - a.y * p12.y, 0.0 ) );
	float t1 = ( b.y - d ) * ra;
	float t2 = ( b.y + d ) * ra;
	if ( abs( a.y ) < 1.0 / 65536.0 ) {
		t1 = t2 = p12.y * rb;
	}
	return vec2(
		( a.x * t1 - b.x * 2.0 ) * t1 + p12.x,
		( a.x * t2 - b.x * 2.0 ) * t2 + p12.x );
}

vec2 solveVertPoly( vec4 p12, vec2 p3 ) {
	vec2 a = p12.xy - p12.zw * 2.0 + p3;
	vec2 b = p12.xy - p12.zw;
	float ra = 1.0 / a.x;
	float rb = 0.5 / b.x;
	float d = sqrt( max( b.x * b.x - a.x * p12.x, 0.0 ) );
	float t1 = ( b.x - d ) * ra;
	float t2 = ( b.x + d ) * ra;
	if ( abs( a.x ) < 1.0 / 65536.0 ) {
		t1 = t2 = p12.x * rb;
	}
	return vec2(
		( a.y * t1 - b.y * 2.0 ) * t1 + p12.y,
		( a.y * t2 - b.y * 2.0 ) * t2 + p12.y );
}

vec4 fetchCurveTexel( int index ) {
	int w = max( int( pc.curveTexWidth ), 1 );
	ivec2 loc = ivec2( index % w, index / w );
	return texelFetch( curveTexture, loc, 0 );
}

/* Dual-axis analytical coverage (Lengyel). startX / startY select sorted lists. */
float renderGlyphCoverageAt( vec2 renderCoord, int startX, int startY, int count ) {
	vec2 emsPerPixel = fwidth( renderCoord );
	vec2 pixelsPerEm = 1.0 / max( emsPerPixel, vec2( 1.0 / 65536.0 ) );
	float xcov = 0.0;
	float xwgt = 0.0;
	float ycov = 0.0;
	float ywgt = 0.0;
	int i;

	for ( i = 0; i < count; i++ ) {
		int idx = startX + i * 2;
		vec4 p12 = fetchCurveTexel( idx ) - vec4( renderCoord, renderCoord );
		vec2 p3 = fetchCurveTexel( idx + 1 ).xy - renderCoord;

		if ( max( max( p12.x, p12.z ), p3.x ) * pixelsPerEm.x < -0.5 ) {
			break;
		}

		uint code = calcRootCode( p12.y, p12.w, p3.y );
		if ( code != 0u ) {
			vec2 r = solveHorizPoly( p12, p3 ) * pixelsPerEm.x;
			if ( ( code & 1u ) != 0u ) {
				xcov += clamp( r.x + 0.5, 0.0, 1.0 );
				xwgt = max( xwgt, clamp( 1.0 - abs( r.x ) * 2.0, 0.0, 1.0 ) );
			}
			if ( code > 1u ) {
				xcov -= clamp( r.y + 0.5, 0.0, 1.0 );
				xwgt = max( xwgt, clamp( 1.0 - abs( r.y ) * 2.0, 0.0, 1.0 ) );
			}
		}
	}

	for ( i = 0; i < count; i++ ) {
		int idx = startY + i * 2;
		vec4 p12 = fetchCurveTexel( idx ) - vec4( renderCoord, renderCoord );
		vec2 p3 = fetchCurveTexel( idx + 1 ).xy - renderCoord;

		if ( max( max( p12.y, p12.w ), p3.y ) * pixelsPerEm.y < -0.5 ) {
			break;
		}

		uint code = calcRootCode( p12.x, p12.z, p3.x );
		if ( code != 0u ) {
			vec2 r = solveVertPoly( p12, p3 ) * pixelsPerEm.y;
			if ( ( code & 1u ) != 0u ) {
				ycov -= clamp( r.x + 0.5, 0.0, 1.0 );
				ywgt = max( ywgt, clamp( 1.0 - abs( r.x ) * 2.0, 0.0, 1.0 ) );
			}
			if ( code > 1u ) {
				ycov += clamp( r.y + 0.5, 0.0, 1.0 );
				ywgt = max( ywgt, clamp( 1.0 - abs( r.y ) * 2.0, 0.0, 1.0 ) );
			}
		}
	}

	float coverage = max(
		abs( xcov * xwgt + ycov * ywgt ) / max( xwgt + ywgt, 1.0 / 65536.0 ),
		min( abs( xcov ), abs( ycov ) ) );
	return clamp( coverage, 0.0, 1.0 );
}

float renderGlyphCoverage( vec2 renderCoord ) {
	int startX = int( pc.curveStart );
	int count = int( pc.curveCount );
	int startY = startX + count * 2; /* Y-sorted duplicate list */
	int mode = int( pc.coverageMode + 0.5 );

	if ( mode <= 0 ) {
		/* Diagnostic: single dual-axis sample at pixel center. */
		return renderGlyphCoverageAt( renderCoord, startX, startY, count );
	}

	float center = renderGlyphCoverageAt( renderCoord, startX, startY, count );
	if ( mode == 1 ) {
		return center;
	}

	/* Mode 2+: adaptive boundary supersampling in stable pixel-space offsets. */
	vec2 emsPerPixel = fwidth( renderCoord );
	bool nearBoundary = center > 0.02 && center < 0.98;
	if ( !nearBoundary && mode < 3 ) {
		return center;
	}

	/* Rotated stratified 4-tap (HIGH) or 8-tap (ULTRA when mode>=3). */
	vec2 offs4[4] = vec2[](
		vec2( -0.375, -0.125 ),
		vec2(  0.125, -0.375 ),
		vec2( -0.125,  0.375 ),
		vec2(  0.375,  0.125 ) );
	float accum = center;
	float wsum = 1.0;
	int n = ( mode >= 3 ) ? 8 : 4;
	for ( int s = 0; s < n; s++ ) {
		vec2 o = ( s < 4 ) ? offs4[s] : offs4[s - 4] * vec2( -1.0, 1.0 ) + vec2( 0.0625, -0.0625 );
		vec2 samplePos = renderCoord + o * emsPerPixel;
		accum += renderGlyphCoverageAt( samplePos, startX, startY, count );
		wsum += 1.0;
	}
	return clamp( accum / wsum, 0.0, 1.0 );
}

void main() {
	out_motion = vec2( 0.0 );
	if ( abs( var_CurrentClip.w ) > 1e-6 && abs( var_PrevClip.w ) > 1e-6 ) {
		vec2 currUV = var_CurrentClip.xy / var_CurrentClip.w * 0.5 + 0.5;
		vec2 prevUV = var_PrevClip.xy / var_PrevClip.w * 0.5 + 0.5;
		out_motion = currUV - prevUV;
	}

	float cov = renderGlyphCoverage( frag_tex_coord0 );
	/* Premultiplied linear-light coverage (blend: ONE, ONE_MINUS_SRC_ALPHA). */
	vec3 rgb = frag_color0.rgb * ( frag_color0.a * cov );
	out_color = vec4( rgb, frag_color0.a * cov );
}
