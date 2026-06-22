/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Facial animation system with flex controllers and phoneme lip sync.
Drives morph targets / blend shapes on character face meshes for
speech, emotion, and reactive expressions.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "../qcommon/q_shared.h"

#define FACE_MAX_FLEX_CONTROLLERS 48
#define FACE_MAX_PHONEMES         16
#define FACE_MAX_EXPRESSIONS      32
#define FACE_MAX_INSTANCES        64

typedef enum {
	FLEX_JAW_OPEN,
	FLEX_JAW_CLENCH,
	FLEX_LIP_UPPER_RAISE,
	FLEX_LIP_LOWER_DROP,
	FLEX_LIP_CORNER_PULL_L,
	FLEX_LIP_CORNER_PULL_R,
	FLEX_LIP_CORNER_DEPRESS_L,
	FLEX_LIP_CORNER_DEPRESS_R,
	FLEX_LIP_PUCKER,
	FLEX_LIP_STRETCH,
	FLEX_BROW_RAISE_INNER_L,
	FLEX_BROW_RAISE_INNER_R,
	FLEX_BROW_RAISE_OUTER_L,
	FLEX_BROW_RAISE_OUTER_R,
	FLEX_BROW_LOWER_L,
	FLEX_BROW_LOWER_R,
	FLEX_EYE_BLINK_L,
	FLEX_EYE_BLINK_R,
	FLEX_EYE_SQUINT_L,
	FLEX_EYE_SQUINT_R,
	FLEX_EYE_WIDE_L,
	FLEX_EYE_WIDE_R,
	FLEX_NOSE_WRINKLE,
	FLEX_NOSE_FLARE,
	FLEX_CHEEK_PUFF_L,
	FLEX_CHEEK_PUFF_R,
	FLEX_CHEEK_RAISE_L,
	FLEX_CHEEK_RAISE_R,
	FLEX_TONGUE_OUT,
	FLEX_TONGUE_UP,
	FLEX_CHIN_RAISE,
	FLEX_DIMPLE_L,
	FLEX_DIMPLE_R,
	FLEX_COUNT
} flexControllerId_t;

typedef enum {
	PHON_SILENCE,
	PHON_AA,
	PHON_AH,
	PHON_AO,
	PHON_AW,
	PHON_EH,
	PHON_ER,
	PHON_EY,
	PHON_IH,
	PHON_IY,
	PHON_OH,
	PHON_OW,
	PHON_UH,
	PHON_UW,
	PHON_B_M_P,
	PHON_CH_J_SH,
	PHON_D_N_T,
	PHON_F_V,
	PHON_G_K_NG,
	PHON_L,
	PHON_R,
	PHON_S_Z,
	PHON_TH,
	PHON_W,
	PHON_Y,
	PHON_COUNT
} phonemeId_t;

typedef enum {
	EXPR_NEUTRAL,
	EXPR_HAPPY,
	EXPR_SAD,
	EXPR_ANGRY,
	EXPR_AFRAID,
	EXPR_SURPRISED,
	EXPR_DISGUSTED,
	EXPR_PAIN,
	EXPR_DEAD,
	EXPR_SHOUTING,
	EXPR_CONCENTRATING,
	EXPR_COUNT
} expressionId_t;

typedef struct phonemeDef_s {
	phonemeId_t id;
	float       flexWeights[FLEX_COUNT];
} phonemeDef_t;

typedef struct expressionDef_s {
	expressionId_t id;
	char           name[32];
	float          flexWeights[FLEX_COUNT];
} expressionDef_t;

typedef struct facePhonemeKey_s {
	float       time;
	phonemeId_t phoneme;
	float       intensity;
} facePhonemeKey_t;

#define FACE_MAX_PHONEME_KEYS 256

typedef int faceHandle_t;

faceHandle_t Face_Create(int entityNum);
void         Face_Destroy(faceHandle_t handle);

void Face_SetFlex(faceHandle_t handle, flexControllerId_t flex, float value);
float Face_GetFlex(faceHandle_t handle, flexControllerId_t flex);
void Face_SetExpression(faceHandle_t handle, expressionId_t expr, float weight, float blendTime);
void Face_ClearExpression(faceHandle_t handle, float blendTime);

void Face_StartLipSync(faceHandle_t handle, const facePhonemeKey_t *keys, int numKeys);
void Face_StopLipSync(faceHandle_t handle);
void Face_SetPhoneme(faceHandle_t handle, phonemeId_t phoneme, float weight);

void Face_StartBlink(faceHandle_t handle);
void Face_SetBlinkRate(faceHandle_t handle, float blinksPerMinute);

void Face_Update(float dt);
void Face_Init(void);
void Face_Shutdown(void);

void Face_GetFlexWeights(faceHandle_t handle, float *weights, int maxWeights);

#ifdef __cplusplus
}
#endif
