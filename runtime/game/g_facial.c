/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Facial animation implementation with FACS Action Unit layer.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "g_facial.h"
#include <math.h>
#include <string.h>

typedef struct faceInstance_s {
	qboolean        active;
	int             entityNum;
	float           flexValues[FLEX_COUNT];
	float           targetFlex[FLEX_COUNT];
	float           flexSpeed[FLEX_COUNT];

	float           auL[FACS_AU_COUNT];
	float           auR[FACS_AU_COUNT];

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
static cvar_t *com_faceFacs;

#define VALID_FACE(h) ((h) >= 0 && (h) < faceCount && faces[(h)].active)

typedef struct facsAuInfo_s {
	facsActionUnit_t id;
	int              number;		/* FACS AU number (1, 2, 4, …) */
	const char      *name;			/* "AU12" */
	const char      *description;
	flexControllerId_t flexL;		/* left or primary */
	flexControllerId_t flexR;		/* right; FLEX_COUNT = unilateral / use flexL only */
	float            scaleL;
	float            scaleR;
} facsAuInfo_t;

static const facsAuInfo_t s_facsTable[FACS_AU_COUNT] = {
	{ FACS_AU1,  1,  "AU1",  "Inner Brow Raiser",     FLEX_BROW_RAISE_INNER_L, FLEX_BROW_RAISE_INNER_R, 1.0f, 1.0f },
	{ FACS_AU2,  2,  "AU2",  "Outer Brow Raiser",     FLEX_BROW_RAISE_OUTER_L, FLEX_BROW_RAISE_OUTER_R, 1.0f, 1.0f },
	{ FACS_AU4,  4,  "AU4",  "Brow Lowerer",          FLEX_BROW_LOWER_L,       FLEX_BROW_LOWER_R,       1.0f, 1.0f },
	{ FACS_AU5,  5,  "AU5",  "Upper Lid Raiser",      FLEX_EYE_WIDE_L,         FLEX_EYE_WIDE_R,         1.0f, 1.0f },
	{ FACS_AU6,  6,  "AU6",  "Cheek Raiser",          FLEX_CHEEK_RAISE_L,      FLEX_CHEEK_RAISE_R,      1.0f, 1.0f },
	{ FACS_AU7,  7,  "AU7",  "Lid Tightener",         FLEX_EYE_SQUINT_L,       FLEX_EYE_SQUINT_R,       1.0f, 1.0f },
	{ FACS_AU9,  9,  "AU9",  "Nose Wrinkler",         FLEX_NOSE_WRINKLE,       FLEX_COUNT,              1.0f, 0.0f },
	{ FACS_AU10, 10, "AU10", "Upper Lip Raiser",      FLEX_LIP_UPPER_RAISE,    FLEX_COUNT,              1.0f, 0.0f },
	{ FACS_AU12, 12, "AU12", "Lip Corner Puller",     FLEX_LIP_CORNER_PULL_L,  FLEX_LIP_CORNER_PULL_R,  1.0f, 1.0f },
	{ FACS_AU14, 14, "AU14", "Dimpler",               FLEX_DIMPLE_L,           FLEX_DIMPLE_R,           1.0f, 1.0f },
	{ FACS_AU15, 15, "AU15", "Lip Corner Depressor",  FLEX_LIP_CORNER_DEPRESS_L, FLEX_LIP_CORNER_DEPRESS_R, 1.0f, 1.0f },
	{ FACS_AU16, 16, "AU16", "Lower Lip Depressor",   FLEX_LIP_LOWER_DROP,     FLEX_COUNT,              1.0f, 0.0f },
	{ FACS_AU17, 17, "AU17", "Chin Raiser",           FLEX_CHIN_RAISE,         FLEX_COUNT,              1.0f, 0.0f },
	{ FACS_AU18, 18, "AU18", "Lip Pucker",            FLEX_LIP_PUCKER,         FLEX_COUNT,              1.0f, 0.0f },
	{ FACS_AU20, 20, "AU20", "Lip Stretcher",         FLEX_LIP_STRETCH,        FLEX_COUNT,              1.0f, 0.0f },
	{ FACS_AU22, 22, "AU22", "Lip Funneler",          FLEX_LIP_PUCKER,         FLEX_COUNT,              0.7f, 0.0f },
	{ FACS_AU23, 23, "AU23", "Lip Tightener",         FLEX_JAW_CLENCH,         FLEX_COUNT,              0.5f, 0.0f },
	{ FACS_AU24, 24, "AU24", "Lip Pressor",           FLEX_JAW_CLENCH,         FLEX_COUNT,              0.8f, 0.0f },
	{ FACS_AU25, 25, "AU25", "Lips Part",             FLEX_JAW_OPEN,           FLEX_COUNT,              0.25f, 0.0f },
	{ FACS_AU26, 26, "AU26", "Jaw Drop",              FLEX_JAW_OPEN,           FLEX_COUNT,              1.0f, 0.0f },
	{ FACS_AU27, 27, "AU27", "Mouth Stretch",         FLEX_JAW_OPEN,           FLEX_LIP_STRETCH,        0.9f, 0.6f },
	{ FACS_AU43, 43, "AU43", "Eyes Closed",           FLEX_EYE_BLINK_L,        FLEX_EYE_BLINK_R,        1.0f, 1.0f },
};

static const char *s_flexMorphNames[FLEX_COUNT] = {
	"jawOpen", "jawClench",
	"lipUpperRaise", "lipLowerDrop",
	"lipCornerPull_L", "lipCornerPull_R",
	"lipCornerDepress_L", "lipCornerDepress_R",
	"lipPucker", "lipStretch",
	"browRaiseInner_L", "browRaiseInner_R",
	"browRaiseOuter_L", "browRaiseOuter_R",
	"browLower_L", "browLower_R",
	"eyeBlink_L", "eyeBlink_R",
	"eyeSquint_L", "eyeSquint_R",
	"eyeWide_L", "eyeWide_R",
	"noseWrinkle", "noseFlare",
	"cheekPuff_L", "cheekPuff_R",
	"cheekRaise_L", "cheekRaise_R",
	"tongueOut", "tongueUp",
	"chinRaise",
	"dimple_L", "dimple_R"
};

static float Face_Clamp01(float v) {
	if (v < 0.0f) {
		return 0.0f;
	}
	if (v > 1.0f) {
		return 1.0f;
	}
	return v;
}

static void Face_MaxFlex(faceInstance_t *face, flexControllerId_t flex, float value) {
	if (flex >= FLEX_COUNT) {
		return;
	}
	value = Face_Clamp01(value);
	if (value > face->targetFlex[flex]) {
		face->targetFlex[flex] = value;
	}
}

static void Face_ApplyAUsToTargets(faceInstance_t *face) {
	int a;

	if (com_faceFacs && !com_faceFacs->integer) {
		return;
	}

	for (a = 0; a < FACS_AU_COUNT; a++) {
		const facsAuInfo_t *info = &s_facsTable[a];
		float l = face->auL[a];
		float r = face->auR[a];

		if (l <= 0.0f && r <= 0.0f) {
			continue;
		}

		Face_MaxFlex(face, info->flexL, l * info->scaleL);
		if (info->flexR < FLEX_COUNT) {
			Face_MaxFlex(face, info->flexR, r * info->scaleR);
		} else if (r > l) {
			/* Unilateral AU with only primary flex: take max side. */
			Face_MaxFlex(face, info->flexL, r * info->scaleL);
		}
	}
}

static void Face_InitPhonemes(void) {
	int i;
	Com_Memset(phonemeDefs, 0, sizeof(phonemeDefs));

	for (i = 0; i < PHON_COUNT; i++) {
		phonemeDefs[i].id = (phonemeId_t)i;
	}

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

	/* Expressions composed to match common FACS emotion recipes. */
	exprDefs[EXPR_HAPPY].id = EXPR_HAPPY;
	Q_strncpyz(exprDefs[EXPR_HAPPY].name, "happy", sizeof(exprDefs[EXPR_HAPPY].name));
	exprDefs[EXPR_HAPPY].flexWeights[FLEX_LIP_CORNER_PULL_L] = 0.7f;	/* AU12 */
	exprDefs[EXPR_HAPPY].flexWeights[FLEX_LIP_CORNER_PULL_R] = 0.7f;
	exprDefs[EXPR_HAPPY].flexWeights[FLEX_CHEEK_RAISE_L] = 0.4f;		/* AU6 */
	exprDefs[EXPR_HAPPY].flexWeights[FLEX_CHEEK_RAISE_R] = 0.4f;
	exprDefs[EXPR_HAPPY].flexWeights[FLEX_EYE_SQUINT_L] = 0.2f;		/* AU7 */
	exprDefs[EXPR_HAPPY].flexWeights[FLEX_EYE_SQUINT_R] = 0.2f;

	exprDefs[EXPR_SAD].id = EXPR_SAD;
	Q_strncpyz(exprDefs[EXPR_SAD].name, "sad", sizeof(exprDefs[EXPR_SAD].name));
	exprDefs[EXPR_SAD].flexWeights[FLEX_LIP_CORNER_DEPRESS_L] = 0.6f;	/* AU15 */
	exprDefs[EXPR_SAD].flexWeights[FLEX_LIP_CORNER_DEPRESS_R] = 0.6f;
	exprDefs[EXPR_SAD].flexWeights[FLEX_BROW_RAISE_INNER_L] = 0.5f;	/* AU1 */
	exprDefs[EXPR_SAD].flexWeights[FLEX_BROW_RAISE_INNER_R] = 0.5f;

	exprDefs[EXPR_ANGRY].id = EXPR_ANGRY;
	Q_strncpyz(exprDefs[EXPR_ANGRY].name, "angry", sizeof(exprDefs[EXPR_ANGRY].name));
	exprDefs[EXPR_ANGRY].flexWeights[FLEX_BROW_LOWER_L] = 0.8f;		/* AU4 */
	exprDefs[EXPR_ANGRY].flexWeights[FLEX_BROW_LOWER_R] = 0.8f;
	exprDefs[EXPR_ANGRY].flexWeights[FLEX_NOSE_WRINKLE] = 0.4f;		/* AU9 */
	exprDefs[EXPR_ANGRY].flexWeights[FLEX_JAW_CLENCH] = 0.5f;		/* AU23/24 */
	exprDefs[EXPR_ANGRY].flexWeights[FLEX_LIP_CORNER_DEPRESS_L] = 0.3f;
	exprDefs[EXPR_ANGRY].flexWeights[FLEX_LIP_CORNER_DEPRESS_R] = 0.3f;

	exprDefs[EXPR_AFRAID].id = EXPR_AFRAID;
	Q_strncpyz(exprDefs[EXPR_AFRAID].name, "afraid", sizeof(exprDefs[EXPR_AFRAID].name));
	exprDefs[EXPR_AFRAID].flexWeights[FLEX_EYE_WIDE_L] = 0.7f;		/* AU5 */
	exprDefs[EXPR_AFRAID].flexWeights[FLEX_EYE_WIDE_R] = 0.7f;
	exprDefs[EXPR_AFRAID].flexWeights[FLEX_BROW_RAISE_INNER_L] = 0.8f;	/* AU1 */
	exprDefs[EXPR_AFRAID].flexWeights[FLEX_BROW_RAISE_INNER_R] = 0.8f;
	exprDefs[EXPR_AFRAID].flexWeights[FLEX_JAW_OPEN] = 0.3f;			/* AU26 */

	exprDefs[EXPR_SURPRISED].id = EXPR_SURPRISED;
	Q_strncpyz(exprDefs[EXPR_SURPRISED].name, "surprised", sizeof(exprDefs[EXPR_SURPRISED].name));
	exprDefs[EXPR_SURPRISED].flexWeights[FLEX_EYE_WIDE_L] = 0.8f;
	exprDefs[EXPR_SURPRISED].flexWeights[FLEX_EYE_WIDE_R] = 0.8f;
	exprDefs[EXPR_SURPRISED].flexWeights[FLEX_BROW_RAISE_OUTER_L] = 0.9f;	/* AU2 */
	exprDefs[EXPR_SURPRISED].flexWeights[FLEX_BROW_RAISE_OUTER_R] = 0.9f;
	exprDefs[EXPR_SURPRISED].flexWeights[FLEX_JAW_OPEN] = 0.6f;

	exprDefs[EXPR_DISGUSTED].id = EXPR_DISGUSTED;
	Q_strncpyz(exprDefs[EXPR_DISGUSTED].name, "disgusted", sizeof(exprDefs[EXPR_DISGUSTED].name));
	exprDefs[EXPR_DISGUSTED].flexWeights[FLEX_NOSE_WRINKLE] = 0.8f;	/* AU9 */
	exprDefs[EXPR_DISGUSTED].flexWeights[FLEX_LIP_UPPER_RAISE] = 0.5f;	/* AU10 */
	exprDefs[EXPR_DISGUSTED].flexWeights[FLEX_BROW_LOWER_L] = 0.3f;
	exprDefs[EXPR_DISGUSTED].flexWeights[FLEX_BROW_LOWER_R] = 0.3f;

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

	exprDefs[EXPR_SHOUTING].id = EXPR_SHOUTING;
	Q_strncpyz(exprDefs[EXPR_SHOUTING].name, "shouting", sizeof(exprDefs[EXPR_SHOUTING].name));
	exprDefs[EXPR_SHOUTING].flexWeights[FLEX_JAW_OPEN] = 0.85f;		/* AU27 */
	exprDefs[EXPR_SHOUTING].flexWeights[FLEX_LIP_STRETCH] = 0.5f;
	exprDefs[EXPR_SHOUTING].flexWeights[FLEX_BROW_LOWER_L] = 0.4f;
	exprDefs[EXPR_SHOUTING].flexWeights[FLEX_BROW_LOWER_R] = 0.4f;
}

void Face_Init(void) {
	Com_Memset(faces, 0, sizeof(faces));
	faceCount = 0;
	Face_InitPhonemes();
	Face_InitExpressions();

	com_faceFacs = Cvar_Get("com_faceFacs", "1", CVAR_ARCHIVE);
	Cvar_SetDescription(com_faceFacs,
		"Enable FACS Action Unit layer for facial animation (AU1–AU43 → flex/morphs).");

	Com_Printf("Facial animation initialized (%d flex, %d FACS AUs, %d phonemes, %d expressions; com_faceFacs %d)\n",
		FLEX_COUNT, FACS_AU_COUNT, PHON_COUNT, EXPR_COUNT,
		com_faceFacs->integer);
}

void Face_Shutdown(void) { faceCount = 0; }

faceHandle_t Face_Create(int entityNum) {
	int i, idx;
	if (faceCount >= FACE_MAX_INSTANCES) {
		return -1;
	}
	idx = faceCount++;
	Com_Memset(&faces[idx], 0, sizeof(faceInstance_t));
	faces[idx].active = qtrue;
	faces[idx].entityNum = entityNum;
	faces[idx].blinkInterval = 4.0f + ((float)(rand() & 0x7FFF) / 0x7FFF) * 3.0f;
	for (i = 0; i < FLEX_COUNT; i++) {
		faces[idx].flexSpeed[i] = 8.0f;
	}
	return idx;
}

void Face_Destroy(faceHandle_t h) {
	if (VALID_FACE(h)) {
		faces[h].active = qfalse;
	}
}

faceHandle_t Face_FindByEntityNum(int entityNum) {
	int i;
	for (i = 0; i < faceCount; i++) {
		if (faces[i].active && faces[i].entityNum == entityNum) {
			return i;
		}
	}
	return -1;
}

void Face_SetFlex(faceHandle_t h, flexControllerId_t f, float v) {
	if (VALID_FACE(h) && f < FLEX_COUNT) {
		faces[h].targetFlex[f] = Face_Clamp01(v);
	}
}

float Face_GetFlex(faceHandle_t h, flexControllerId_t f) {
	return (VALID_FACE(h) && f < FLEX_COUNT) ? faces[h].flexValues[f] : 0;
}

void Face_SetExpression(faceHandle_t h, expressionId_t expr, float weight, float blendTime) {
	if (!VALID_FACE(h) || expr >= EXPR_COUNT) {
		return;
	}
	faces[h].currentExpr = expr;
	faces[h].exprTarget = Face_Clamp01(weight);
	faces[h].exprBlendSpeed = blendTime > 0 ? 1.0f / blendTime : 10.0f;
}

void Face_ClearExpression(faceHandle_t h, float blendTime) {
	if (!VALID_FACE(h)) {
		return;
	}
	faces[h].exprTarget = 0;
	faces[h].exprBlendSpeed = blendTime > 0 ? 1.0f / blendTime : 10.0f;
}

void Face_SetAU(faceHandle_t h, facsActionUnit_t au, float intensity) {
	Face_SetAUSide(h, au, FACS_SIDE_BOTH, intensity);
}

void Face_SetAUSide(faceHandle_t h, facsActionUnit_t au, facsSide_t side, float intensity) {
	if (!VALID_FACE(h) || au >= FACS_AU_COUNT) {
		return;
	}
	intensity = Face_Clamp01(intensity);
	if (side == FACS_SIDE_LEFT) {
		faces[h].auL[au] = intensity;
	} else if (side == FACS_SIDE_RIGHT) {
		faces[h].auR[au] = intensity;
	} else {
		faces[h].auL[au] = intensity;
		faces[h].auR[au] = intensity;
	}
}

float Face_GetAU(faceHandle_t h, facsActionUnit_t au) {
	float l, r;
	if (!VALID_FACE(h) || au >= FACS_AU_COUNT) {
		return 0.0f;
	}
	l = faces[h].auL[au];
	r = faces[h].auR[au];
	return (l > r) ? l : r;
}

float Face_GetAUSide(faceHandle_t h, facsActionUnit_t au, facsSide_t side) {
	if (!VALID_FACE(h) || au >= FACS_AU_COUNT) {
		return 0.0f;
	}
	if (side == FACS_SIDE_RIGHT) {
		return faces[h].auR[au];
	}
	if (side == FACS_SIDE_LEFT) {
		return faces[h].auL[au];
	}
	return Face_GetAU(h, au);
}

void Face_ClearAUs(faceHandle_t h) {
	if (!VALID_FACE(h)) {
		return;
	}
	Com_Memset(faces[h].auL, 0, sizeof(faces[h].auL));
	Com_Memset(faces[h].auR, 0, sizeof(faces[h].auR));
}

const char *Face_AUName(facsActionUnit_t au) {
	if (au >= FACS_AU_COUNT) {
		return "";
	}
	return s_facsTable[au].name;
}

facsActionUnit_t Face_AUFromName(const char *name) {
	int i;
	int num;

	if (!name || !name[0]) {
		return FACS_AU_COUNT;
	}

	for (i = 0; i < FACS_AU_COUNT; i++) {
		if (!Q_stricmp(name, s_facsTable[i].name)) {
			return (facsActionUnit_t)i;
		}
	}

	/* Accept "12", "au12", "AU_12" */
	if (!Q_stricmpn(name, "AU", 2)) {
		name += 2;
		if (*name == '_' || *name == '-') {
			name++;
		}
	}
	num = atoi(name);
	for (i = 0; i < FACS_AU_COUNT; i++) {
		if (s_facsTable[i].number == num) {
			return (facsActionUnit_t)i;
		}
	}
	return FACS_AU_COUNT;
}

const char *Face_FlexMorphName(flexControllerId_t flex) {
	if (flex >= FLEX_COUNT) {
		return "";
	}
	return s_flexMorphNames[flex];
}

void Face_StartLipSync(faceHandle_t h, const facePhonemeKey_t *keys, int numKeys) {
	int count;
	if (!VALID_FACE(h) || !keys) {
		return;
	}
	count = numKeys > FACE_MAX_PHONEME_KEYS ? FACE_MAX_PHONEME_KEYS : numKeys;
	Com_Memcpy(faces[h].lipSyncKeys, keys, count * sizeof(facePhonemeKey_t));
	faces[h].numLipSyncKeys = count;
	faces[h].lipSyncTime = 0;
	faces[h].lipSyncActive = qtrue;
}

void Face_StopLipSync(faceHandle_t h) {
	if (VALID_FACE(h)) {
		faces[h].lipSyncActive = qfalse;
	}
}

void Face_SetPhoneme(faceHandle_t h, phonemeId_t phoneme, float weight) {
	int f;
	if (!VALID_FACE(h) || phoneme >= PHON_COUNT) {
		return;
	}
	for (f = 0; f < FLEX_COUNT; f++) {
		faces[h].targetFlex[f] += phonemeDefs[phoneme].flexWeights[f] * weight;
		if (faces[h].targetFlex[f] > 1) {
			faces[h].targetFlex[f] = 1;
		}
		if (faces[h].targetFlex[f] < 0) {
			faces[h].targetFlex[f] = 0;
		}
	}
}

void Face_StartBlink(faceHandle_t h) {
	if (VALID_FACE(h)) {
		faces[h].isBlinking = qtrue;
	}
}

void Face_SetBlinkRate(faceHandle_t h, float bpm) {
	if (VALID_FACE(h) && bpm > 0) {
		faces[h].blinkInterval = 60.0f / bpm;
	}
}

void Face_Update(float dt) {
	int i, f;
	for (i = 0; i < faceCount; i++) {
		faceInstance_t *face = &faces[i];
		if (!face->active) {
			continue;
		}

		Face_ApplyAUsToTargets(face);

		if (face->exprWeight != face->exprTarget) {
			float diff = face->exprTarget - face->exprWeight;
			float step = face->exprBlendSpeed * dt;
			if (fabsf(diff) < step) {
				face->exprWeight = face->exprTarget;
			} else {
				face->exprWeight += (diff > 0 ? step : -step);
			}
		}

		if (face->currentExpr < EXPR_COUNT && face->exprWeight > 0) {
			for (f = 0; f < FLEX_COUNT; f++) {
				float exprVal = exprDefs[face->currentExpr].flexWeights[f] * face->exprWeight;
				if (exprVal > face->targetFlex[f]) {
					face->targetFlex[f] = exprVal;
				}
			}
		}

		if (face->lipSyncActive && face->numLipSyncKeys > 0) {
			int ki;
			phonemeId_t currentPhoneme = PHON_SILENCE;
			float currentIntensity = 0;

			face->lipSyncTime += dt;

			for (ki = 0; ki < face->numLipSyncKeys; ki++) {
				if (face->lipSyncKeys[ki].time <= face->lipSyncTime) {
					currentPhoneme = face->lipSyncKeys[ki].phoneme;
					currentIntensity = face->lipSyncKeys[ki].intensity;
				} else {
					break;
				}
			}

			if (ki >= face->numLipSyncKeys) {
				face->lipSyncActive = qfalse;
			}

			for (f = 0; f < FLEX_COUNT; f++) {
				float phonVal = phonemeDefs[currentPhoneme].flexWeights[f] * currentIntensity;
				if (fabsf(phonVal) > fabsf(face->targetFlex[f])) {
					face->targetFlex[f] = phonVal;
				}
			}
		}

		face->blinkTimer += dt;
		if (face->blinkTimer >= face->blinkInterval) {
			face->blinkTimer = 0;
			face->isBlinking = qtrue;
		}
		if (face->isBlinking) {
			float blinkVal;
			face->blinkPhase += dt * 8.0f;
			blinkVal = sinf(face->blinkPhase * 3.14159f);
			if (blinkVal < 0) {
				blinkVal = 0;
			}
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
			if (fabsf(diff) < step) {
				face->flexValues[f] = face->targetFlex[f];
			} else {
				face->flexValues[f] += (diff > 0 ? step : -step);
			}
		}

		Com_Memset(face->targetFlex, 0, sizeof(face->targetFlex));
	}
}

void Face_GetFlexWeights(faceHandle_t h, float *weights, int maxWeights) {
	int count, i;
	if (!VALID_FACE(h) || !weights) {
		return;
	}
	count = maxWeights < FLEX_COUNT ? maxWeights : FLEX_COUNT;
	for (i = 0; i < count; i++) {
		weights[i] = faces[h].flexValues[i];
	}
}

void Face_ApplyMorphsToEntity(int entityNum, void *refEntity, faceMorphApplyFn_t apply) {
	faceHandle_t h;
	int f, a;
	float weights[FLEX_COUNT];

	if (!refEntity || !apply || entityNum < 0) {
		return;
	}

	h = Face_FindByEntityNum(entityNum);
	if (!VALID_FACE(h)) {
		return;
	}

	Face_GetFlexWeights(h, weights, FLEX_COUNT);
	for (f = 0; f < FLEX_COUNT; f++) {
		if (weights[f] > 0.001f) {
			apply(refEntity, s_flexMorphNames[f], weights[f]);
		}
	}

	/* Also emit AU-named morphs (AU12, etc.) for assets authored to FACS names. */
	if (!com_faceFacs || com_faceFacs->integer) {
		for (a = 0; a < FACS_AU_COUNT; a++) {
			float intensity = Face_GetAU(h, (facsActionUnit_t)a);
			if (intensity > 0.001f) {
				apply(refEntity, s_facsTable[a].name, intensity);
			}
		}
	}
}
