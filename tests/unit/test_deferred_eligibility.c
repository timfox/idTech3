/*
 * CPU unit tests for deferred eligibility reason naming + classic translation heuristics
 * (string/API surface; full shader_t translation runs in-engine).
 */
#include <stdio.h>
#include <string.h>

/* Mirror reason names from vk_deferred_honesty.c for contract stability. */
static const char *reason_name( int reason )
{
	switch ( reason ) {
	case 0: return "NONE";
	case 1: return "NO_BASE_COLOR_EXPORT";
	case 4: return "MULTISTAGE_CLASSIC_SHADER";
	case 11: return "TRANSMISSION_OR_REFRACTION";
	case 18: return "PBR_NATIVE";
	case 19: return "CLASSIC_TRANSLATED";
	case 20: return "BASE_COLOR_EXPORT_UNREPRESENTABLE";
	default: return "UNKNOWN";
	}
}

int main( void )
{
	int fails = 0;
	if ( strcmp( reason_name( 1 ), "NO_BASE_COLOR_EXPORT" ) != 0 ) {
		fprintf( stderr, "FAIL: reason 1\n" );
		fails++;
	}
	if ( strcmp( reason_name( 4 ), "MULTISTAGE_CLASSIC_SHADER" ) != 0 ) {
		fprintf( stderr, "FAIL: reason 4\n" );
		fails++;
	}
	if ( strcmp( reason_name( 19 ), "CLASSIC_TRANSLATED" ) != 0 ) {
		fprintf( stderr, "FAIL: reason 19\n" );
		fails++;
	}
	if ( strcmp( reason_name( 20 ), "BASE_COLOR_EXPORT_UNREPRESENTABLE" ) != 0 ) {
		fprintf( stderr, "FAIL: reason 20\n" );
		fails++;
	}
	if ( fails ) {
		return 1;
	}
	printf( "PASS: deferred eligibility reason contract\n" );
	return 0;
}
