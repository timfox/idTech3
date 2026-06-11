#pragma once

#include "../qcommon/q_shared.h"

#define VUDA_MAX_SLOTS          3
#define VUDA_SLOT_NEURAL        0
#define VUDA_SLOT_PHYSICS       1
#define VUDA_SLOT_INFERENCE     2

#define VUDA_JOB_HEARTBEAT      0
#define VUDA_JOB_NEURAL_STAGE   1
#define VUDA_JOB_PHYSICS_TICK   2
#define VUDA_JOB_INFERENCE      3

typedef struct vudaSlotExport_s {
	int         fd;
	uint64_t    size;
	uint32_t    memoryTypeIndex;
	qboolean    valid;
} vudaSlotExport_t;

typedef struct vudaSyncExport_s {
	int         fd;
	qboolean    valid;
} vudaSyncExport_t;

typedef struct vudaExportBundle_s {
	qboolean            interopReady;
	vudaSlotExport_t    slots[VUDA_MAX_SLOTS];
	vudaSyncExport_t    cudaWait;   /* CUDA waits until Vulkan signals */
	vudaSyncExport_t    cudaSignal; /* CUDA signals when compute done */
	uint64_t            renderTimeline;
	uint64_t            cudaTimeline;
} vudaExportBundle_t;
