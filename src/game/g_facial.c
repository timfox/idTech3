/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Facial animation implementation.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "g_facial.h"
#include <math.h>

typedef struct faceInstance_s {
	qboolean        active;
	int             entityNum;
	float           flexValues[FLEX_COUNT];
	float           targetFlex[FLEX_COUNT];
	float           flexSpeed[FLEX_COUNT];

	expressionId_t  currentExpr;
	float           exprWeight;
	float           exprBlendSpeed;
	float           exprTarget;

	facePhonemeKey_t lipSyncKeys[FACE_MAX_PHONEME_KEYS];
	int             numLipSyncKeys;
	float           lipSyncTime;
	qboolean        lipSyncActive;

	float           blinkTimer;
	float           blinkInterval;
	float           blinkPhase;
	qboolean        isBlinking;
} faceInstance_t;

static faceInstance_t faces[FACE_MAX_INSTANCES];
static int faceCount = 0;
static phonemeDef_t phonemeDefs[PHON_COUNT];
static expressionDef_t exprDefs[EXPR_COUNT];

#define VALID_FACE(h) ((h) >= 0 && (h) < faceCount && faces[(h)].active)

static void Face_InitPhonemes(void) {
	int i;
	Com_Memset(phonemeDefs, 0, sizeof(phonemeDefs));

	for (i = 0; i < PHON_COUNT; i++) phonemeDefs[i].id = (phonemeId_t)i;

	phonemeDefs[PHON_AA].flexWeights[FLEX_JAW_OPEN] = 0.7f;
	phonemeDefs[PHON_AA].flexWeights[FLEX_LIP_LOWER_DROP] = 0.5f;

	phonemeDefs[PHON_AH].flexWeights[FLEX_JAW_OPEN] = 0.5f;
	phonemeDefs[PHON_AH].flexWeights[FLEX_LIP_LOWER_DROP] = 0.3f;

	phonemeDefs[PHON_EH].flexWeights[FLEX_JAW_OPEN] = 0.3f;
	phonemeDefs[PHON_EH].flexWeights[FLEX_LIP_STRETCH] = 0.4f;

	phonemeDefs[PHON_EY].flexWeights[FLEX_JAW_OPEN] = 0.2f;
	phonemeDefs[PHON_EY].flexWeights[FLEX_LIP_CORNER_PULL_L] = 0.5f;
	phonemeDefs[PHON_EY].flexWeights[FLEX_LIP_CORNER_PULL_R] = 0.5f;

	phonemeDefs[PHON_IH].flexWeights[FLEX_JAW_OPEN] = 0.15f;
	phonemeDefs[PHON_IH].flexWeights[FLEX_LIP_STRETCH] = 0.5f;

	phonemeDefs[PHON_IY].flexWeights[FLEX_LIP_STRETCH] = 0.7f;
	phonemeDefs[PHON_IY].flexWeights[FLEX_LIP_CORNER_PULL_L] = 0.4f;
	phonemeDefs[PHON_IY].flexWeights[FLEX_LIP_CORNER_PULL_R] = 0.4f;

	phonemeDefs[PHON_OH].flexWeights[FLEX_JAW_OPEN] = 0.5f;
	phonemeDefs[PHON_OH].flexWeights[FLEX_LIP_PUCKER] = 0.6f;

	phonemeDefs[PHON_OW].flexWeights[FLEX_JAW_OPEN] = 0.4f;
	phonemeDefs[PHON_OW].flexWeights[FLEX_LIP_PUCKER] = 0.8f;

	phonemeDefs[PHON_UH].flexWeights[FLEX_JAW_OPEN] = 0.25f;
	phonemeDefs[PHON_UH].flexWeights[FLEX_LIP_PUCKER] = 0.4f;

	phonemeDefs[PHON_UW].flexWeights[FLEX_LIP_PUCKER] = 0.9f;

	phonemeDefs[PHON_B_M_P].flexWeights[FLEX_JAW_CLENCH] = 0.3f;
	phonemeDefs[PHON_B_M_P].flexWeights[FLEX_LIP_UPPER_RAISE] = -0.2f;

	phonemeDefs[PHON_F_V].flexWeights[FLEX_JAW_OPEN] = 0.1f;
	phonemeDefs[PHON_F_V].flexWeights[FLEX_LIP_LOWER_DROP] = 0.3f;

	phonemeDefs[PHON_D_N_T].flexWeights[FLEX_JAW_OPEN] = 0.15f;
	phonemeDefs[PHON_D_N_T].flexWeights[FLEX_TONGUE_UP] = 0.7f;

	phonemeDefs[PHON_L].flexWeights[FLEX_JAW_OPEN] = 0.2f;
	phonemeDefs[PHON_L].flexWeights[FLEX_TONGUE_OUT] = 0.3f;
	phonemeDefs[PHON_L].flexWeights[FLEX_TONGUE_UP] = 0.5f;

	phonemeDefs[PHON_TH].flexWeights[FLEX_JAW_OPEN] = 0.1f;
	phonemeDefs[PHON_TH].flexWeights[FLEX_TONGUE_OUT] = 0.6f;

	phonemeDefs[PHON_W].flexWeights[FLEX_LIP_PUCKER] = 0.8f;

	phonemeDefs[PHON_R].flexWeights[FLEX_JAW_OPEN] = 0.15f;
	phonemeDefs[PHON_R].flexWeights[FLEX_LIP_PUCKER] = 0.3f;
}

static void Face_InitExpressions(void) {
	Com_Memset(exprDefs, 0, sizeof(exprDefs));

	exprDefs[EXPR_HAPPY].id = EXPR_HAPPY;
	Q_strncpyz(exprDefs[EXPR_HAPPY].name, "happy", sizeof(exprDefs[EXPR_HAPPY].name));
	exprDefs[EXPR_HAPPY].flexWeights[FLEX_LIP_CORNER_PULL_L] = 0.7f;
	exprDefs[EXPR_HAPPY].flexWeights[FLEX_LIP_CORNER_PULL_R] = 0.7f;
	exprDefs[EXPR_HAPPY].flexWeights[FLEX_CHEEK_RAISE_L] = 0.4f;
	exprDefs[EXPR_HAPPY].flexWeights[FLEX_CHEEK_RAISE_R] = 0.4f;
	exprDefs[EXPR_HAPPY].flexWeights[FLEX_EYE_SQUINT_L] = 0.2f;
	exprDefs[EXPR_HAPPY].flexWeights[FLEX_EYE_SQUINT_R] = 0.2f;

	exprDefs[EXPR_SAD].id = EXPR_SAD;
	Q_strncpyz(exprDefs[EXPR_SAD].name, "sad", sizeof(exprDefs[EXPR_SAD].name));
	exprDefs[EXPR_SAD].flexWeights[FLEX_LIP_CORNER_DEPRESS_L] = 0.6f;
	exprDefs[EXPR_SAD].flexWeights[FLEX_LIP_CORNER_DEPRESS_R] = 0.6f;
	exprDefs[EXPR_SAD].flexWeights[FLEX_BROW_RAISE_INNER_L] = 0.5f;
	exprDefs[EXPR_SAD].flexWeights[FLEX_BROW_RAISE_INNER_R] = 0.5f;

	exprDefs[EXPR_ANGRY].id = EXPR_ANGRY;
	Q_strncpyz(exprDefs[EXPR_ANGRY].name, "angry", sizeof(exprDefs[EXPR_ANGRY].name));
	exprDefs[EXPR_ANGRY].flexWeights[FLEX_BROW_LOWER_L] = 0.8f;
	exprDefs[EXPR_ANGRY].flexWeights[FLEX_BROW_LOWER_R] = 0.8f;
	exprDefs[EXPR_ANGRY].flexWeights[FLEX_NOSE_WRINKLE] = 0.4f;
	exprDefs[EXPR_ANGRY].flexWeights[FLEX_JAW_CLENCH] = 0.5f;
	exprDefs[EXPR_ANGRY].flexWeights[FLEX_LIP_CORNER_DEPRESS_L] = 0.3f;
	exprDefs[EXPR_ANGRY].flexWeights[FLEX_LIP_CORNER_DEPRESS_R] = 0.3f;

	exprDefs[EXPR_AFRAID].id = EXPR_AFRAID;
	Q_strncpyz(exprDefs[EXPR_AFRAID].name, "afraid", sizeof(exprDefs[EXPR_AFRAID].name));
	exprDefs[EXPR_AFRAID].flexWeights[FLEX_EYE_WIDE_L] = 0.7f;
	exprDefs[EXPR_AFRAID].flexWeights[FLEX_EYE_WIDE_R] = 0.7f;
	exprDefs[EXPR_AFRAID].flexWeights[FLEX_BROW_RAISE_INNER_L] = 0.8f;
	exprDefs[EXPR_AFRAID].flexWeights[FLEX_BROW_RAISE_INNER_R] = 0.8f;
	exprDefs[EXPR_AFRAID].flexWeights[FLEX_JAW_OPEN] = 0.3f;

	exprDefs[EXPR_SURPRISED].id = EXPR_SURPRISED;
	Q_strncpyz(exprDefs[EXPR_SURPRISED].name, "surprised", sizeof(exprDefs[EXPR_SURPRISED].name));
	exprDefs[EXPR_SURPRISED].flexWeights[FLEX_EYE_WIDE_L] = 0.8f;
	exprDefs[EXPR_SURPRISED].flexWeights[FLEX_EYE_WIDE_R] = 0.8f;
	exprDefs[EXPR_SURPRISED].flexWeights[FLEX_BROW_RAISE_OUTER_L] = 0.9f;
	exprDefs[EXPR_SURPRISED].flexWeights[FLEX_BROW_RAISE_OUTER_R] = 0.9f;
	exprDefs[EXPR_SURPRISED].flexWeights[FLEX_JAW_OPEN] = 0.6f;

	exprDefs[EXPR_PAIN].id = EXPR_PAIN;
	Q_strncpyz(exprDefs[EXPR_PAIN].name, "pain", sizeof(exprDefs[EXPR_PAIN].name));
	exprDefs[EXPR_PAIN].flexWeights[FLEX_BROW_LOWER_L] = 0.7f;
	exprDefs[EXPR_PAIN].flexWeights[FLEX_BROW_LOWER_R] = 0.7f;
	exprDefs[EXPR_PAIN].flexWeights[FLEX_EYE_SQUINT_L] = 0.8f;
	exprDefs[EXPR_PAIN].flexWeights[FLEX_EYE_SQUINT_R] = 0.8f;
	exprDefs[EXPR_PAIN].flexWeights[FLEX_NOSE_WRINKLE] = 0.6f;
	exprDefs[EXPR_PAIN].flexWeights[FLEX_LIP_CORNER_DEPRESS_L] = 0.5f;
	exprDefs[EXPR_PAIN].flexWeights[FLEX_LIP_CORNER_DEPRESS_R] = 0.5f;
	exprDefs[EXPR_PAIN].flexWeights[FLEX_JAW_OPEN] = 0.4f;
}

void Face_Init(void) {
	Com_Memset(faces, 0, sizeof(faces));
	faceCount = 0;
	Face_InitPhonemes();
	Face_InitExpressions();
	Com_Printf("Facial animation initialized (%d flex controllers, %d phonemes, %d expressions)\n",
		FLEX_COUNT, PHON_COUNT, EXPR_COUNT);
}

void Face_Shutdown(void) { faceCount = 0; }

faceHandle_t Face_Create(int entityNum) {
	int i, idx;
	if (faceCount >= FACE_MAX_INSTANCES) return -1;
	idx = faceCount++;
	Com_Memset(&faces[idx], 0, sizeof(faceInstance_t));
	faces[idx].active = qtrue;
	faces[idx].entityNum = entityNum;
	faces[idx].blinkInterval = 4.0f + ((float)(rand() & 0x7FFF) / 0x7FFF) * 3.0f;
	for (i = 0; i < FLEX_COUNT; i++) faces[idx].flexSpeed[i] = 8.0f;
	return idx;
}

void Face_Destroy(faceHandle_t h) {
	if (VALID_FACE(h)) faces[h].active = qfalse;
}

void Face_SetFlex(faceHandle_t h, flexControllerId_t f, float v) {
	if (VALID_FACE(h) && f < FLEX_COUNT) faces[h].targetFlex[f] = v < 0 ? 0 : (v > 1 ? 1 : v);
}

float Face_GetFlex(faceHandle_t h, flexControllerId_t f) {
	return (VALID_FACE(h) && f < FLEX_COUNT) ? faces[h].flexValues[f] : 0;
}

void Face_SetExpression(faceHandle_t h, expressionId_t expr, float weight, float blendTime) {
	if (!VALID_FACE(h) || expr >= EXPR_COUNT) return;
	faces[h].currentExpr = expr;
	faces[h].exprTarget = weight < 0 ? 0 : (weight > 1 ? 1 : weight);
	faces[h].exprBlendSpeed = blendTime > 0 ? 1.0f / blendTime : 10.0f;
}

void Face_ClearExpression(faceHandle_t h, float blendTime) {
	if (!VALID_FACE(h)) return;
	faces[h].exprTarget = 0;
	faces[h].exprBlendSpeed = blendTime > 0 ? 1.0f / blendTime : 10.0f;
}

void Face_StartLipSync(faceHandle_t h, const facePhonemeKey_t *keys, int numKeys) {
	if (!VALID_FACE(h) || !keys) return;
	int count = numKeys > FACE_MAX_PHONEME_KEYS ? FACE_MAX_PHONEME_KEYS : numKeys;
	Com_Memcpy(faces[h].lipSyncKeys, keys, count * sizeof(facePhonemeKey_t));
	faces[h].numLipSyncKeys = count;
	faces[h].lipSyncTime = 0;
	faces[h].lipSyncActive = qtrue;
}

void Face_StopLipSync(faceHandle_t h) {
	if (VALID_FACE(h)) faces[h].lipSyncActive = qfalse;
}

void Face_SetPhoneme(faceHandle_t h, phonemeId_t phoneme, float weight) {
	int f;
	if (!VALID_FACE(h) || phoneme >= PHON_COUNT) return;
	for (f = 0; f < FLEX_COUNT; f++) {
		faces[h].targetFlex[f] += phonemeDefs[phoneme].flexWeights[f] * weight;
		if (faces[h].targetFlex[f] > 1) faces[h].targetFlex[f] = 1;
		if (faces[h].targetFlex[f] < 0) faces[h].targetFlex[f] = 0;
	}
}

void Face_StartBlink(faceHandle_t h) {
	if (VALID_FACE(h)) faces[h].isBlinking = qtrue;
}

void Face_SetBlinkRate(faceHandle_t h, float bpm) {
	if (VALID_FACE(h) && bpm > 0) faces[h].blinkInterval = 60.0f / bpm;
}

void Face_Update(float dt) {
	int i, f;
	for (i = 0; i < faceCount; i++) {
		faceInstance_t *face = &faces[i];
		if (!face->active) continue;

		if (face->exprWeight != face->exprTarget) {
			float diff = face->exprTarget - face->exprWeight;
			float step = face->exprBlendSpeed * dt;
			if (fabsf(diff) < step) face->exprWeight = face->exprTarget;
			else face->exprWeight += (diff > 0 ? step : -step);
		}

		if (face->currentExpr < EXPR_COUNT && face->exprWeight > 0) {
			for (f = 0; f < FLEX_COUNT; f++) {
				float exprVal = exprDefs[face->currentExpr].flexWeights[f] * face->exprWeight;
				if (exprVal > face->targetFlex[f]) face->targetFlex[f] = exprVal;
			}
		}

		if (face->lipSyncActive && face->numLipSyncKeys > 0) {
			face->lipSyncTime += dt;
			int ki;
			phonemeId_t currentPhoneme = PHON_SILENCE;
			float currentIntensity = 0;

			for (ki = 0; ki < face->numLipSyncKeys; ki++) {
				if (face->lipSyncKeys[ki].time <= face->lipSyncTime) {
					currentPhoneme = face->lipSyncKeys[ki].phoneme;
					currentIntensity = face->lipSyncKeys[ki].intensity;
				} else break;
			}

			if (ki >= face->numLipSyncKeys) face->lipSyncActive = qfalse;

			for (f = 0; f < FLEX_COUNT; f++) {
				float phonVal = phonemeDefs[currentPhoneme].flexWeights[f] * currentIntensity;
				if (fabsf(phonVal) > fabsf(face->targetFlex[f])) face->targetFlex[f] = phonVal;
			}
		}

		face->blinkTimer += dt;
		if (face->blinkTimer >= face->blinkInterval) {
			face->blinkTimer = 0;
			face->isBlinking = qtrue;
		}
		if (face->isBlinking) {
			face->blinkPhase += dt * 8.0f;
			float blinkVal = sinf(face->blinkPhase * 3.14159f);
			if (blinkVal < 0) blinkVal = 0;
			face->targetFlex[FLEX_EYE_BLINK_L] = blinkVal;
			face->targetFlex[FLEX_EYE_BLINK_R] = blinkVal;
			if (face->blinkPhase >= 1.0f) {
				face->isBlinking = qfalse;
				face->blinkPhase = 0;
				face->targetFlex[FLEX_EYE_BLINK_L] = 0;
				face->targetFlex[FLEX_EYE_BLINK_R] = 0;
			}
		}

		for (f = 0; f < FLEX_COUNT; f++) {
			float diff = face->targetFlex[f] - face->flexValues[f];
			float step = face->flexSpeed[f] * dt;
			if (fabsf(diff) < step) face->flexValues[f] = face->targetFlex[f];
			else face->flexValues[f] += (diff > 0 ? step : -step);
		}

		Com_Memset(face->targetFlex, 0, sizeof(face->targetFlex));
	}
}

void Face_GetFlexWeights(faceHandle_t h, float *weights, int maxWeights) {
	int count, i;
	if (!VALID_FACE(h) || !weights) return;
	count = maxWeights < FLEX_COUNT ? maxWeights : FLEX_COUNT;
	for (i = 0; i < count; i++) weights[i] = faces[h].flexValues[i];
}
