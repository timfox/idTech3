/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Response rules implementation.
Criteria-based rule evaluation with weighted random response selection.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "g_response.h"

static responseRule_t rules[RESPONSE_MAX_RULES];
static int numRules = 0;

void Response_Init(void) {
	Com_Memset(rules, 0, sizeof(rules));
	numRules = 0;
	Com_Printf("Response rules engine initialized\n");
}

void Response_Shutdown(void) {
	numRules = 0;
}

int Response_AddRule(const char *name, const char *concept) {
	if (numRules >= RESPONSE_MAX_RULES) return -1;
	int idx = numRules++;
	Q_strncpyz(rules[idx].name, name, sizeof(rules[idx].name));
	Q_strncpyz(rules[idx].concept, concept, sizeof(rules[idx].concept));
	rules[idx].active = qtrue;
	rules[idx].cooldown = 5.0f;
	rules[idx].lastTriggered = -999.0f;
	return idx;
}

void Response_AddCriteria(int id, responseCriteriaType_t type, float value, const char *strValue) {
	if (id < 0 || id >= numRules) return;
	responseRule_t *r = &rules[id];
	if (r->numCriteria >= RESPONSE_MAX_CRITERIA) return;
	int ci = r->numCriteria++;
	r->criteria[ci].type = type;
	r->criteria[ci].value = value;
	if (strValue) Q_strncpyz(r->criteria[ci].stringValue, strValue, sizeof(r->criteria[ci].stringValue));
}

void Response_AddResponse(int id, const char *soundFile, const char *animation, float delay, float weight) {
	if (id < 0 || id >= numRules) return;
	responseRule_t *r = &rules[id];
	if (r->numResponses >= RESPONSE_MAX_RESPONSES) return;
	int ri = r->numResponses++;
	Q_strncpyz(r->responses[ri].soundFile, soundFile, sizeof(r->responses[ri].soundFile));
	if (animation) Q_strncpyz(r->responses[ri].animation, animation, sizeof(r->responses[ri].animation));
	r->responses[ri].delay = delay;
	r->responses[ri].weight = weight > 0 ? weight : 1.0f;
}

void Response_SetCooldown(int id, float cooldown) {
	if (id >= 0 && id < numRules) rules[id].cooldown = cooldown;
}

static qboolean Response_CheckCriteria(const responseCriteria_t *c, const responseContext_t *ctx) {
	switch (c->type) {
		case RCRIT_HEALTH_BELOW:    return ctx->health < c->value;
		case RCRIT_HEALTH_ABOVE:    return ctx->health > c->value;
		case RCRIT_AMMO_BELOW:      return ctx->ammo < c->value;
		case RCRIT_IN_COMBAT:       return ctx->inCombat;
		case RCRIT_ISOLATED:        return ctx->isolated;
		case RCRIT_INTENSITY_ABOVE: return ctx->intensity > c->value;
		case RCRIT_INTENSITY_BELOW: return ctx->intensity < c->value;
		case RCRIT_DIRECTOR_PHASE:  return ctx->directorPhase == (int)c->value;
		case RCRIT_IN_ZONE:         return !Q_stricmp(ctx->currentZone, c->stringValue);
		case RCRIT_RANDOM_CHANCE:   return ((float)(rand() & 0x7FFF) / 0x7FFF) < c->value;
		case RCRIT_COOLDOWN_ELAPSED: return qtrue;
		case RCRIT_PLAYER_COUNT_ABOVE: return ctx->playerCount > (int)c->value;
		case RCRIT_CONCEPT_MATCH:   return qtrue;
		default:                    return qfalse;
	}
}

const responseAction_t *Response_Evaluate(const char *concept, const responseContext_t *ctx) {
	int i, ci;
	float totalWeight, roll, accum;

	for (i = 0; i < numRules; i++) {
		responseRule_t *r = &rules[i];
		if (!r->active || r->numResponses == 0) continue;
		if (Q_stricmp(r->concept, concept)) continue;
		if (ctx->currentTime - r->lastTriggered < r->cooldown) continue;

		qboolean allMatch = qtrue;
		for (ci = 0; ci < r->numCriteria; ci++) {
			if (!Response_CheckCriteria(&r->criteria[ci], ctx)) {
				allMatch = qfalse;
				break;
			}
		}
		if (!allMatch) continue;

		totalWeight = 0;
		int ri2;
		for (ri2 = 0; ri2 < r->numResponses; ri2++)
			totalWeight += r->responses[ri2].weight;

		roll = ((float)(rand() & 0x7FFF) / 0x7FFF) * totalWeight;
		accum = 0;
		for (ri2 = 0; ri2 < r->numResponses; ri2++) {
			accum += r->responses[ri2].weight;
			if (roll <= accum) {
				r->lastTriggered = ctx->currentTime;
				return &r->responses[ri2];
			}
		}
	}
	return NULL;
}

extern sfxHandle_t S_RegisterSound(const char *name, qboolean compressed);
extern void S_StartLocalSound(sfxHandle_t sfx, int channelNum);

void Response_TriggerConcept(const char *concept, const responseContext_t *ctx) {
	const responseAction_t *action = Response_Evaluate(concept, ctx);
	if (action) {
		Com_DPrintf("Response: [%s] -> %s\n", concept, action->soundFile);
		if (action->soundFile[0]) {
			sfxHandle_t sfx = S_RegisterSound(action->soundFile, qfalse);
			if (sfx) S_StartLocalSound(sfx, 0);
		}
	}
}

int Response_LoadRulesFile(const char *filename) {
	(void)filename;
	return 0;
}

int Response_GetRuleCount(void) { return numRules; }
