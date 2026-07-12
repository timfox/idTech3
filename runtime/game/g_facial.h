/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Facial animation system with FACS Action Units, flex controllers,
and phoneme lip sync. Drives morph targets / blend shapes on character
face meshes for speech, emotion, and reactive expressions.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"

#define FACE_MAX_FLEX_CONTROLLERS 48
#define FACE_MAX_PHONEMES         16
#define FACE_MAX_EXPRESSIONS      32
#define FACE_MAX_INSTANCES        64

/*
 * Flex controllers — engine-native blend channels (often 1:1 with morph names).
 */
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

/*
 * FACS Action Units (Ekman & Friesen). Intensities are 0..1.
 * Bilateral AUs drive left/right flexes; use Face_SetAUSide for asymmetry.
 * See docs/FACS.md.
 */
typedef enum {
	FACS_AU1 = 0,	/* Inner Brow Raiser */
	FACS_AU2,		/* Outer Brow Raiser */
	FACS_AU4,		/* Brow Lowerer */
	FACS_AU5,		/* Upper Lid Raiser */
	FACS_AU6,		/* Cheek Raiser */
	FACS_AU7,		/* Lid Tightener */
	FACS_AU9,		/* Nose Wrinkler */
	FACS_AU10,		/* Upper Lip Raiser */
	FACS_AU12,		/* Lip Corner Puller (smile) */
	FACS_AU14,		/* Dimpler */
	FACS_AU15,		/* Lip Corner Depressor */
	FACS_AU16,		/* Lower Lip Depressor */
	FACS_AU17,		/* Chin Raiser */
	FACS_AU18,		/* Lip Pucker */
	FACS_AU20,		/* Lip Stretcher */
	FACS_AU22,		/* Lip Funneler */
	FACS_AU23,		/* Lip Tightener */
	FACS_AU24,		/* Lip Pressor */
	FACS_AU25,		/* Lips Part */
	FACS_AU26,		/* Jaw Drop */
	FACS_AU27,		/* Mouth Stretch */
	FACS_AU43,		/* Eyes Closed */
	FACS_AU_COUNT
} facsActionUnit_t;

typedef enum {
	FACS_SIDE_BOTH = 0,
	FACS_SIDE_LEFT,
	FACS_SIDE_RIGHT
} facsSide_t;

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
faceHandle_t Face_FindByEntityNum(int entityNum);

void Face_SetFlex(faceHandle_t handle, flexControllerId_t flex, float value);
float Face_GetFlex(faceHandle_t handle, flexControllerId_t flex);
void Face_SetExpression(faceHandle_t handle, expressionId_t expr, float weight, float blendTime);
void Face_ClearExpression(faceHandle_t handle, float blendTime);

/* FACS Action Units */
void Face_SetAU(faceHandle_t handle, facsActionUnit_t au, float intensity);
void Face_SetAUSide(faceHandle_t handle, facsActionUnit_t au, facsSide_t side, float intensity);
float Face_GetAU(faceHandle_t handle, facsActionUnit_t au);
float Face_GetAUSide(faceHandle_t handle, facsActionUnit_t au, facsSide_t side);
void Face_ClearAUs(faceHandle_t handle);
const char *Face_AUName(facsActionUnit_t au);
facsActionUnit_t Face_AUFromName(const char *name);
const char *Face_FlexMorphName(flexControllerId_t flex);

void Face_StartLipSync(faceHandle_t handle, const facePhonemeKey_t *keys, int numKeys);
void Face_StopLipSync(faceHandle_t handle);
void Face_SetPhoneme(faceHandle_t handle, phonemeId_t phoneme, float weight);

void Face_StartBlink(faceHandle_t handle);
void Face_SetBlinkRate(faceHandle_t handle, float blinksPerMinute);

void Face_Update(float dt);
void Face_Init(void);
void Face_Shutdown(void);

void Face_GetFlexWeights(faceHandle_t handle, float *weights, int maxWeights);

/*
 * Apply current flex weights as morph targets for a refEntity near entityNum.
 * morphApply(ent, morphName, weight) — typically re.SetEntityMorphWeight.
 */
typedef void (*faceMorphApplyFn_t)(void *ent, const char *name, float weight);
void Face_ApplyMorphsToEntity(int entityNum, void *refEntity, faceMorphApplyFn_t apply);

#ifdef __cplusplus
}
#endif
