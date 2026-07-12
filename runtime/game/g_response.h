/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Response rules engine for context-aware NPC dialogue.
Evaluates game state criteria to select appropriate voice lines
(combat callouts, idle chatter, warnings, reactions).
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"

#define RESPONSE_MAX_RULES       256
#define RESPONSE_MAX_CRITERIA     8
#define RESPONSE_MAX_RESPONSES    8
#define RESPONSE_MAX_CONCEPTS     64

typedef enum {
	RCRIT_HEALTH_BELOW,
	RCRIT_HEALTH_ABOVE,
	RCRIT_AMMO_BELOW,
	RCRIT_IN_COMBAT,
	RCRIT_ISOLATED,
	RCRIT_NEAR_ENTITY,
	RCRIT_IN_ZONE,
	RCRIT_DIRECTOR_PHASE,
	RCRIT_INTENSITY_ABOVE,
	RCRIT_INTENSITY_BELOW,
	RCRIT_RANDOM_CHANCE,
	RCRIT_COOLDOWN_ELAPSED,
	RCRIT_CONCEPT_MATCH,
	RCRIT_PLAYER_COUNT_ABOVE
} responseCriteriaType_t;

typedef struct responseCriteria_s {
	responseCriteriaType_t type;
	float                  value;
	char                   stringValue[64];
} responseCriteria_t;

typedef struct responseAction_s {
	char    soundFile[MAX_QPATH];
	char    animation[64];
	float   delay;
	float   weight;
} responseAction_t;

typedef struct responseRule_s {
	char                name[64];
	char                concept[64];
	responseCriteria_t  criteria[RESPONSE_MAX_CRITERIA];
	int                 numCriteria;
	responseAction_t    responses[RESPONSE_MAX_RESPONSES];
	int                 numResponses;
	float               cooldown;
	float               lastTriggered;
	qboolean            active;
} responseRule_t;

typedef struct responseContext_s {
	int     clientNum;
	float   health;
	float   ammo;
	qboolean inCombat;
	qboolean isolated;
	float   intensity;
	int     directorPhase;
	char    currentZone[64];
	int     playerCount;
	float   currentTime;
} responseContext_t;

void Response_Init(void);
void Response_Shutdown(void);

int  Response_AddRule(const char *name, const char *concept);
void Response_AddCriteria(int ruleId, responseCriteriaType_t type, float value, const char *strValue);
void Response_AddResponse(int ruleId, const char *soundFile, const char *animation, float delay, float weight);
void Response_SetCooldown(int ruleId, float cooldown);

const responseAction_t *Response_Evaluate(const char *concept, const responseContext_t *ctx);
void Response_TriggerConcept(const char *concept, const responseContext_t *ctx);

int  Response_LoadRulesFile(const char *filename);
int  Response_GetRuleCount(void);

#ifdef __cplusplus
}
#endif
