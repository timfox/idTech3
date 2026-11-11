#pragma once

/*
 * Phase 1.6 — failure bundles under render_cert/failures/p1_<stage>_<case>_<ts>/
 */


#include "../common/tr_types.h"
#include "vk_renderer_p1_cert.h"

typedef enum {
	P1_FAIL_CLASS_RENDERER_BUG = 0,
	P1_FAIL_CLASS_FIXTURE_BUG,
	P1_FAIL_CLASS_READBACK_BUG,
	P1_FAIL_CLASS_METRIC_BUG,
	P1_FAIL_CLASS_THRESHOLD_BUG,
	P1_FAIL_CLASS_RESOURCE_LIFETIME_BUG,
	P1_FAIL_CLASS_SYNC_BUG,
	P1_FAIL_CLASS_PREFLIGHT
} p1FailClass_t;

typedef struct p1FailureBundle_s {
	char path[MAX_OSPATH];
	p1CertStage_t stage;
	uint32_t caseId;
	p1FailClass_t klass;
	char reason[256];
	uint64_t timestamp;
	qboolean valid;
} p1FailureBundle_t;

void vk_renderer_p1_failure_register( void );
const p1FailureBundle_t *vk_renderer_p1_last_failure( void );

/* Write metadata + console snippet; returns bundle directory path. */
qboolean vk_renderer_p1_failure_capture( p1CertStage_t stage, uint32_t caseId,
	p1FailClass_t klass, const char *reason, const char *extraMeta );

