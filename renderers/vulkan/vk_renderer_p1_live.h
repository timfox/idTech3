#pragma once

/*
 * Renderer IQ Phase 1.6 — live GPU certification state machine.
 * Do not evaluate before fence; do not advance on frame-count alone.
 * See docs/RENDERER_IQ_LIVE_CERTIFICATION.md.
 */


#include "../common/tr_types.h"
#include "vk_renderer_p1_cert.h"

typedef enum rendererP1LiveState_e {
	P1_LIVE_IDLE = 0,
	P1_LIVE_PREFLIGHT,
	P1_LIVE_WAIT_FOR_WORLD,
	P1_LIVE_WAIT_FOR_RESOURCES,
	P1_LIVE_ARM_STAGE,
	P1_LIVE_ARM_CASE,
	P1_LIVE_WARMUP,
	P1_LIVE_RENDER,
	P1_LIVE_WAIT_FOR_CAPTURE_POINT,
	P1_LIVE_REQUEST_READBACK,
	P1_LIVE_WAIT_FOR_READBACK,
	P1_LIVE_VALIDATE_FRAME_IDENTITY,
	P1_LIVE_EVALUATE,
	P1_LIVE_RECORD_EVIDENCE,
	P1_LIVE_ADVANCE_CASE,
	P1_LIVE_ADVANCE_STAGE,
	P1_LIVE_LIFECYCLE_TRANSITION,
	P1_LIVE_WAIT_FOR_RECREATE,
	P1_LIVE_WAIT_FOR_STABLE_FRAME,
	P1_LIVE_COMPLETE,
	P1_LIVE_FAILED,
	P1_LIVE_ABORTED
} rendererP1LiveState_t;

typedef enum {
	IQ_FIXTURE_OK = 0,
	IQ_FIXTURE_NOT_ARMED,
	IQ_FIXTURE_NOT_SUBMITTED,
	IQ_FIXTURE_NOT_VISIBLE,
	IQ_FIXTURE_CLIPPED,
	IQ_FIXTURE_BEHIND_DEPTH,
	IQ_FIXTURE_WRONG_RENDER_PATH,
	IQ_FIXTURE_REGION_EMPTY,
	IQ_FIXTURE_ID_MISMATCH,
	IQ_FIXTURE_TARGET_UNCHANGED
} iqFixtureFail_t;

typedef enum {
	IQ_READBACK_OK = 0,
	IQ_READBACK_FRAME_MISMATCH,
	IQ_READBACK_GENERATION_MISMATCH,
	IQ_READBACK_CASE_MISMATCH,
	IQ_READBACK_PROFILE_MISMATCH,
	IQ_READBACK_STALE,
	IQ_READBACK_EMPTY
} iqReadbackFail_t;

typedef struct rendererP1LiveTransition_s {
	rendererP1LiveState_t from;
	rendererP1LiveState_t to;
	p1CertStage_t stage;
	uint32_t caseId;
	uint32_t subcaseId;
	uint64_t frameEntered;
	uint32_t framesElapsed;
	uint32_t expectedCapturePoint;
	uint32_t expectedGeneration;
	uint32_t observedGeneration;
	uint64_t readbackTicket;
	uint32_t timeoutFrames;
	char failureReason[192];
} rendererP1LiveTransition_t;

typedef struct rendererP1LiveStamp_s {
	uint64_t fixtureFrame;
	uint64_t snapshotFrame;
	uint64_t readbackFrame;
	uint32_t resourceGeneration;
	uint32_t expectedGeneration;
	uint32_t caseId;
	uint32_t subcaseId;
	uint32_t profileHash;
	uint32_t thresholdHash;
} rendererP1LiveStamp_t;

void vk_renderer_p1_live_register( void );
void vk_renderer_p1_live_begin_frame( void );
void vk_renderer_p1_live_on_bloom_extract( void );
void vk_renderer_p1_live_finalize_frame( int cmdIndex );

rendererP1LiveState_t vk_renderer_p1_live_state( void );
const char *vk_renderer_p1_live_state_name( rendererP1LiveState_t s );
const rendererP1LiveTransition_t *vk_renderer_p1_live_last_transition( void );
const rendererP1LiveStamp_t *vk_renderer_p1_live_stamp( void );

qboolean vk_renderer_p1_live_running( void );
qboolean vk_renderer_p1_preflight( char *errBuf, int errBufSize, char *actionBuf, int actionBufSize );

/* Orchestration entry points (also console: renderer_p1_certify / iq_certify_*). */
void vk_renderer_p1_live_start( const char *group ); /* core|temporal|edges|lighting|full */
void vk_renderer_p1_live_abort( const char *reason );
void vk_renderer_p1_live_retry( void );
void vk_renderer_p1_live_retry_stage( p1CertStage_t stage );
void vk_renderer_p1_live_resume( void );
void vk_renderer_p1_live_from( p1CertStage_t stage );

