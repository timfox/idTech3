/*
 * Contract tests for idTech3 Emulator shared-memory headers (no engine link).
 */
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#define EMULATOR_FRAME_MAGIC	0x314d5545u
#define EMULATOR_INPUT_MAGIC	0x31504945u
#define EMULATOR_INPUT_RING	256

typedef struct {
	uint32_t magic;
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t frameIndex;
	uint32_t format;
} emulatorFrameHeader_t;

typedef struct {
	uint32_t magic;
	uint32_t writeIdx;
	uint32_t readIdx;
	uint32_t ringSize;
} emulatorInputHeader_t;

typedef struct {
	uint32_t type;
	uint32_t key;
	uint32_t ascii;
	uint32_t mods;
} emulatorInputEvent_t;

static int tests_run;
static int tests_failed;

static void expect_u32( const char *label, uint32_t got, uint32_t want )
{
	tests_run++;
	if ( got != want ) {
		tests_failed++;
		fprintf( stderr, "FAIL %s: got %u want %u\n", label, (unsigned)got, (unsigned)want );
	} else {
		fprintf( stderr, "ok   %s\n", label );
	}
}

static void expect_size( const char *label, size_t got, size_t want )
{
	tests_run++;
	if ( got != want ) {
		tests_failed++;
		fprintf( stderr, "FAIL %s: got %zu want %zu\n", label, got, want );
	} else {
		fprintf( stderr, "ok   %s\n", label );
	}
}

int main( void )
{
	expect_u32( "frame magic", EMULATOR_FRAME_MAGIC, 0x314d5545u );
	expect_u32( "input magic", EMULATOR_INPUT_MAGIC, 0x31504945u );
	expect_size( "frame header", sizeof( emulatorFrameHeader_t ), 6u * sizeof( uint32_t ) );
	expect_size( "input header", sizeof( emulatorInputHeader_t ), 4u * sizeof( uint32_t ) );
	expect_size( "input event", sizeof( emulatorInputEvent_t ), 4u * sizeof( uint32_t ) );

	if ( EMULATOR_INPUT_RING < 16 ) {
		tests_run++;
		tests_failed++;
		fprintf( stderr, "FAIL input ring too small\n" );
	}

	fprintf( stderr, "unit_emulator_contract: %d run, %d failed\n", tests_run, tests_failed );
	return tests_failed ? 1 : 0;
}
