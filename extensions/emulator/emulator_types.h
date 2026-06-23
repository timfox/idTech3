/*
 * idTech3 in-engine OS sandbox — shared types (QEMU guest display bridge).
 */
#pragma once

#include "q_shared.h"

#define EMULATOR_FRAME_MAGIC	0x314d5545u /* 'EUM1' */
#define EMULATOR_DEFAULT_WIDTH	640
#define EMULATOR_DEFAULT_HEIGHT	480
#define EMULATOR_MAX_WIDTH	1920
#define EMULATOR_MAX_HEIGHT	1080
#define EMULATOR_SHM_NAME		"/idtech3_emulator_frame"
#define EMULATOR_INPUT_SHM_NAME	"/idtech3_emulator_input"
#define EMULATOR_INPUT_MAGIC	0x31504945u /* 'EIP1' */
#define EMULATOR_INPUT_RING	256

typedef enum {
	EMULATOR_INPUT_KEY_DOWN = 0,
	EMULATOR_INPUT_KEY_UP,
	EMULATOR_INPUT_CHAR
} emulatorInputType_t;

typedef enum {
	EMULATOR_STATE_IDLE = 0,
	EMULATOR_STATE_STARTING,
	EMULATOR_STATE_RUNNING,
	EMULATOR_STATE_STOPPING,
	EMULATOR_STATE_ERROR
} emulatorState_t;

typedef struct {
	uint32_t magic;
	uint32_t width;
	uint32_t height;
	uint32_t stride;
	uint32_t frameIndex;
	uint32_t format; /* 0 = RGBA8888 */
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

typedef struct {
	emulatorState_t state;
	int pid;
	int width;
	int height;
	uint32_t frameIndex;
	qboolean guestRunning;
	qboolean shmAttached;
} emulatorStatus_t;
