/*
 * Variable-Rate Compute Shading (VRCS) shared helpers.
 * Rates are per 2x2 pixel block in the shading-rate image (SRI).
 */

#ifndef VRCS_GLSL
#define VRCS_GLSL

const uint VRCS_RATE_1X1 = 0u;
const uint VRCS_RATE_2X1 = 1u; /* horizontal half: left primary, right copy */
const uint VRCS_RATE_1X2 = 2u; /* vertical half: top primary, bottom copy */
const uint VRCS_RATE_2X2 = 3u; /* top-left primary; H/V/diag copies */

const uint VRCS_COPY_H = 1u;
const uint VRCS_COPY_V = 2u;
const uint VRCS_COPY_D = 4u;

const uint VRCS_TILE = 16u;
const uint VRCS_WAVE = 32u;
const uint VRCS_MAX_PACK = 224u; /* 256 - 32: need to retire >=1 wave */

const uint VRCS_FLAG_FORCE_1X1 = 1u;
const uint VRCS_FLAG_ALL_2X2 = 2u;
const uint VRCS_FLAG_SKY = 4u;

uint vrcsPackPayload( uint localX, uint localY, uint copyBits ) {
	return ( localX & 0xFu ) | ( ( localY & 0xFu ) << 4u ) | ( ( copyBits & 0x7u ) << 8u );
}

uvec2 vrcsUnpackLocalXY( uint payload ) {
	return uvec2( payload & 0xFu, ( payload >> 4u ) & 0xFu );
}

uint vrcsUnpackCopyBits( uint payload ) {
	return ( payload >> 8u ) & 0x7u;
}

/* Within a 2x2: which of the four pixels is primary for this rate? */
bool vrcsIsPrimaryInQuad( uint rate, uint qx, uint qy ) {
	if ( rate == VRCS_RATE_1X1 ) {
		return true;
	}
	if ( rate == VRCS_RATE_2X1 ) {
		return qx == 0u;
	}
	if ( rate == VRCS_RATE_1X2 ) {
		return qy == 0u;
	}
	/* 2x2 */
	return qx == 0u && qy == 0u;
}

uint vrcsCopyBitsForPrimary( uint rate, uint qx, uint qy ) {
	if ( rate == VRCS_RATE_1X1 ) {
		return 0u;
	}
	if ( rate == VRCS_RATE_2X1 ) {
		return ( qx == 0u ) ? VRCS_COPY_H : 0u;
	}
	if ( rate == VRCS_RATE_1X2 ) {
		return ( qy == 0u ) ? VRCS_COPY_V : 0u;
	}
	if ( qx == 0u && qy == 0u ) {
		return VRCS_COPY_H | VRCS_COPY_V | VRCS_COPY_D;
	}
	return 0u;
}

float vrcsLuma( vec3 c ) {
	return dot( c, vec3( 0.2126, 0.7152, 0.0722 ) );
}

#endif
