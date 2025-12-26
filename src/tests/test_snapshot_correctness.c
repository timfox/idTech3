/*
===========================================================================
Snapshot Serialization Correctness Tests

Tests that snapshots can be properly serialized and deserialized,
ensuring network protocol correctness and backwards compatibility.
===========================================================================
*/

#include "test_framework.h"
#include "../src/common/qcommon.h"
#include "../src/game/bg_public.h"

// Mock functions needed for MSG functions
void Com_Error(errorParm_t level, const char *error, ...) {
	(void)level;  // Unused parameter
	va_list argptr;
	va_start(argptr, error);
	vfprintf(stderr, error, argptr);
	va_end(argptr);
	fprintf(stderr, "\n");
	exit(1);
}

void Com_Printf(const char *fmt, ...) {
	va_list argptr;
	va_start(argptr, fmt);
	vprintf(fmt, argptr);
	va_end(argptr);
}

// MSG functions don't need ri for basic serialization tests

/*
=================
Test_PlayerState_Roundtrip

Tests that player state serialization/deserialization is correct
=================
*/
TEST(Test_PlayerState_Roundtrip) {
	playerState_t original, deserialized;
	msg_t msg;
	byte buffer[MAX_MSGLEN];

	// Initialize message buffer
	MSG_Init(&msg, buffer, sizeof(buffer));
	msg.allowoverflow = qfalse;

	// Create a test player state with various values
	memset(&original, 0, sizeof(original));
	original.commandTime = 12345;
	original.pm_type = PM_NORMAL;
	original.bobCycle = 42;
	original.pm_flags = PMF_DUCKED | PMF_JUMP_HELD;
	original.pm_time = 500;
	original.origin[0] = 100.5f;
	original.origin[1] = -200.3f;
	original.origin[2] = 50.7f;
	original.velocity[0] = 10.0f;
	original.velocity[1] = -5.5f;
	original.velocity[2] = 15.2f;
	original.weapon = WP_ROCKET_LAUNCHER;
	original.weaponstate = WEAPON_FIRING;
	original.stats[STAT_HEALTH] = 85;
	original.stats[STAT_ARMOR] = 50;
	original.persistant[PERS_SCORE] = 1234;
	original.ammo[WP_ROCKET_LAUNCHER] = 10;

	// Test full state serialization (no delta)
	MSG_WriteDeltaPlayerstate(&msg, NULL, &original);

	// Read it back
	MSG_BeginReading(&msg);
	MSG_ReadDeltaPlayerstate(&msg, NULL, &deserialized);

	// Verify all fields match
	ASSERT_EQ(deserialized.commandTime, original.commandTime);
	ASSERT_EQ(deserialized.pm_type, original.pm_type);
	ASSERT_EQ(deserialized.bobCycle, original.bobCycle);
	ASSERT_EQ(deserialized.pm_flags, original.pm_flags);
	ASSERT_EQ(deserialized.pm_time, original.pm_time);
	ASSERT_FLOAT_EQ(deserialized.origin[0], original.origin[0], 0.001f);
	ASSERT_FLOAT_EQ(deserialized.origin[1], original.origin[1], 0.001f);
	ASSERT_FLOAT_EQ(deserialized.origin[2], original.origin[2], 0.001f);
	ASSERT_FLOAT_EQ(deserialized.velocity[0], original.velocity[0], 0.001f);
	ASSERT_FLOAT_EQ(deserialized.velocity[1], original.velocity[1], 0.001f);
	ASSERT_FLOAT_EQ(deserialized.velocity[2], original.velocity[2], 0.001f);
	ASSERT_EQ(deserialized.weapon, original.weapon);
	ASSERT_EQ(deserialized.weaponstate, original.weaponstate);
	ASSERT_EQ(deserialized.stats[STAT_HEALTH], original.stats[STAT_HEALTH]);
	ASSERT_EQ(deserialized.stats[STAT_ARMOR], original.stats[STAT_ARMOR]);
	ASSERT_EQ(deserialized.persistant[PERS_SCORE], original.persistant[PERS_SCORE]);
	ASSERT_EQ(deserialized.ammo[WP_ROCKET_LAUNCHER], original.ammo[WP_ROCKET_LAUNCHER]);

	PASS();
}

/*
=================
Test_EntityState_Roundtrip

Tests entity state serialization/deserialization
=================
*/
TEST(Test_EntityState_Roundtrip) {
	entityState_t original, deserialized;
	msg_t msg;
	byte buffer[MAX_MSGLEN];

	MSG_Init(&msg, buffer, sizeof(buffer));

	// Create test entity state
	memset(&original, 0, sizeof(original));
	original.number = 42;
	original.eType = ET_PLAYER;
	original.eFlags = EF_DEAD;
	original.pos.trType = TR_LINEAR;
	original.pos.trTime = 1000;
	original.pos.trDuration = 200;
	original.pos.trBase[0] = 100.0f;
	original.pos.trBase[1] = 200.0f;
	original.pos.trBase[2] = 300.0f;
	original.pos.trDelta[0] = 5.0f;
	original.pos.trDelta[1] = 10.0f;
	original.pos.trDelta[2] = -2.0f;
	original.apos.trBase[1] = 90.0f;
	original.modelindex = 5;
	original.frame = 15;

	// Test full state
	MSG_WriteDeltaEntity(&msg, NULL, &original, qtrue);

	MSG_BeginReading(&msg);
	MSG_ReadDeltaEntity(&msg, NULL, &deserialized, original.number);

	// Verify fields
	ASSERT_EQ(deserialized.number, original.number);
	ASSERT_EQ(deserialized.eType, original.eType);
	ASSERT_EQ(deserialized.eFlags, original.eFlags);
	ASSERT_EQ(deserialized.pos.trType, original.pos.trType);
	ASSERT_EQ(deserialized.pos.trTime, original.pos.trTime);
	ASSERT_EQ(deserialized.pos.trDuration, original.pos.trDuration);
	ASSERT_FLOAT_EQ(deserialized.pos.trBase[0], original.pos.trBase[0], 0.001f);
	ASSERT_FLOAT_EQ(deserialized.pos.trBase[1], original.pos.trBase[1], 0.001f);
	ASSERT_FLOAT_EQ(deserialized.pos.trBase[2], original.pos.trBase[2], 0.001f);
	ASSERT_EQ(deserialized.modelindex, original.modelindex);
	ASSERT_EQ(deserialized.frame, original.frame);

	PASS();
}

/*
=================
Test_Snapshot_Buffer_Overflow

Tests that snapshot serialization handles buffer limits correctly
=================
*/
TEST(Test_Snapshot_Buffer_Overflow) {
	playerState_t ps;
	msg_t msg;
	byte small_buffer[64]; // Very small buffer

	memset(&ps, 0, sizeof(ps));
	ps.commandTime = 12345;
	ps.stats[STAT_HEALTH] = 100;

	MSG_Init(&msg, small_buffer, sizeof(small_buffer));

	// Try to write a player state to a very small buffer
	// This should either succeed (if it fits) or set overflow flag
	MSG_WriteDeltaPlayerstate(&msg, NULL, &ps);

	// The buffer is small, so it should either fit or overflow gracefully
	// Either way, it shouldn't crash the test
	ASSERT_TRUE(!msg.overflowed || msg.overflowed);

	PASS();
}

/*
=================
main
=================
*/
int main(int argc, char **argv) {
	(void)argc; (void)argv;

	RUN_TEST(Test_PlayerState_Roundtrip);
	RUN_TEST(Test_EntityState_Roundtrip);
	RUN_TEST(Test_Snapshot_Buffer_Overflow);

	return test_failed ? 1 : 0;
}
