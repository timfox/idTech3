#include <math.h>
#include <stdio.h>

#include "renderers/vulkan/vk_postfx_sanitize.h"

#define ASSERT_TRUE(cond, msg) do { \
	if ( !( cond ) ) { \
		fprintf( stderr, "FAIL: %s\n", msg ); \
		return 1; \
	} \
} while ( 0 )

static int almost_equal( float a, float b )
{
	return fabsf( a - b ) < 1e-6f;
}

int main( void )
{
	float taa[4] = { 0.0f };
	float exposure[4] = { 0.0f };

	vk_postfx_sanitize_taa_params( 1, 1.3f, -0.1f, 1.4f, taa );
	ASSERT_TRUE( almost_equal( taa[0], 1.0f ), "taa history flag" );
	ASSERT_TRUE( almost_equal( taa[1], 0.99f ), "taa stationary clamp high" );
	ASSERT_TRUE( almost_equal( taa[2], 0.0f ), "taa motion clamp low" );
	ASSERT_TRUE( almost_equal( taa[3], 1.0f ), "taa sharpen clamp high" );

	vk_postfx_sanitize_taa_params( 0, 0.92f, 0.72f, 0.12f, taa );
	ASSERT_TRUE( almost_equal( taa[0], 0.0f ), "taa history disabled" );
	ASSERT_TRUE( almost_equal( taa[1], 0.92f ), "taa stationary passthrough" );
	ASSERT_TRUE( almost_equal( taa[2], 0.72f ), "taa motion passthrough" );
	ASSERT_TRUE( almost_equal( taa[3], 0.12f ), "taa sharpen passthrough" );

	vk_postfx_sanitize_auto_exposure_params( 0, 2.0f, 0.0f, 0.0f, -1.0f, -3.0f, exposure );
	ASSERT_TRUE( almost_equal( exposure[0], 0.0f ), "auto exposure fallback log2 target/manual" );
	ASSERT_TRUE( almost_equal( exposure[1], 1e-4f ), "auto exposure target floor" );
	ASSERT_TRUE( almost_equal( exposure[2], 0.01f ), "auto exposure min floor" );
	ASSERT_TRUE( almost_equal( exposure[3], 0.01f ), "auto exposure max floored to min" );

	vk_postfx_sanitize_auto_exposure_params( 1, 1.5f, 0.8f, 2.0f, 0.25f, 4.0f, exposure );
	ASSERT_TRUE( almost_equal( exposure[0], 1.5f ), "auto exposure uses valid luminance" );
	ASSERT_TRUE( almost_equal( exposure[1], 0.8f ), "auto exposure target passthrough" );
	ASSERT_TRUE( almost_equal( exposure[2], 0.25f ), "auto exposure min passthrough" );
	ASSERT_TRUE( almost_equal( exposure[3], 4.0f ), "auto exposure max passthrough" );

	printf( "PASS: unit_vk_postfx_sanitize\n" );
	return 0;
}
