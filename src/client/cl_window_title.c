/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Dynamic window title implementation.
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "cl_window_title.h"

extern void Sys_UpdateWindowTitle(const char *title);

static cvar_t *cl_dynamicTitle;
static cvar_t *cl_titleFormat;
static cvar_t *cl_titleUpdateRate;

static char sections[WINTITLE_SECTION_COUNT][128];
static char titleFormat[WINTITLE_MAX_LEN];
static char lastTitle[WINTITLE_MAX_LEN];
static char eventText[128];
static float eventTimer;
static float updateTimer;
static float updateInterval;
static int fps;
static int frameCount;
static float fpsTimer;

void WinTitle_Init(void) {
	int i;

	cl_dynamicTitle = Cvar_Get("cl_dynamicTitle", "1", CVAR_ARCHIVE);
	Cvar_SetDescription(cl_dynamicTitle, "Enable dynamic window title updates during gameplay.\n"
		" 0 - Static title\n"
		" 1 - Update with game info\n");

	cl_titleFormat = Cvar_Get("cl_titleFormat", "{game} - {map} | {score} {event}", CVAR_ARCHIVE);
	Cvar_SetDescription(cl_titleFormat, "Window title format string. Tokens:\n"
		" {game} - Game name\n"
		" {map} - Current map\n"
		" {score} - Score display\n"
		" {player} - Player name\n"
		" {event} - Timed event text\n"
		" {fps} - Frames per second\n"
		" {custom} - Custom text\n");

	cl_titleUpdateRate = Cvar_Get("cl_titleUpdateRate", "0.5", CVAR_ARCHIVE);
	Cvar_SetDescription(cl_titleUpdateRate, "How often to update the window title (seconds).");

	Q_strncpyz(titleFormat, cl_titleFormat->string, sizeof(titleFormat));

	for (i = 0; i < WINTITLE_SECTION_COUNT; i++) {
		sections[i][0] = '\0';
	}

	/* Use cl_title (from gameinfo.txt when present), else default */
	if ( cl_title[0] != '\0' ) {
		Q_strncpyz( sections[WINTITLE_SECTION_GAME_NAME], cl_title, sizeof( sections[0] ) );
	} else {
		Q_strncpyz( sections[WINTITLE_SECTION_GAME_NAME], CLIENT_WINDOW_TITLE, sizeof( sections[0] ) );
	}

	lastTitle[0] = '\0';
	eventText[0] = '\0';
	eventTimer = 0;
	updateTimer = 0;
	updateInterval = 0.5f;
	fps = 0;
	frameCount = 0;
	fpsTimer = 0;
}

static void WinTitle_BuildTitle(char *out, int outSize) {
	const char *fmt;
	char *dst;
	int remaining;

	if (cl_titleFormat && cl_titleFormat->string[0]) {
		fmt = cl_titleFormat->string;
	} else {
		fmt = titleFormat;
	}

	dst = out;
	remaining = outSize - 1;

	while (*fmt && remaining > 0) {
		if (*fmt == '{') {
			const char *token = fmt + 1;
			const char *end = strchr(token, '}');
			if (end) {
				int tokenLen = (int)(end - token);
				const char *replacement = NULL;

				if (tokenLen == 4 && !Q_stricmpn(token, "game", 4)) {
					replacement = sections[WINTITLE_SECTION_GAME_NAME];
				} else if (tokenLen == 3 && !Q_stricmpn(token, "map", 3)) {
					replacement = sections[WINTITLE_SECTION_MAP_NAME];
				} else if (tokenLen == 5 && !Q_stricmpn(token, "score", 5)) {
					replacement = sections[WINTITLE_SECTION_SCORE];
				} else if (tokenLen == 6 && !Q_stricmpn(token, "player", 6)) {
					replacement = sections[WINTITLE_SECTION_PLAYER_NAME];
				} else if (tokenLen == 5 && !Q_stricmpn(token, "event", 5)) {
					replacement = (eventTimer > 0) ? eventText : "";
				} else if (tokenLen == 3 && !Q_stricmpn(token, "fps", 3)) {
					replacement = sections[WINTITLE_SECTION_FPS];
				} else if (tokenLen == 6 && !Q_stricmpn(token, "custom", 6)) {
					replacement = sections[WINTITLE_SECTION_CUSTOM];
				}

				if (replacement && replacement[0]) {
					int repLen = (int)strlen(replacement);
					if (repLen > remaining) repLen = remaining;
					Com_Memcpy(dst, replacement, repLen);
					dst += repLen;
					remaining -= repLen;
				}

				fmt = end + 1;
				continue;
			}
		}

		*dst++ = *fmt++;
		remaining--;
	}

	*dst = '\0';

	dst = out + strlen(out) - 1;
	while (dst > out && (*dst == ' ' || *dst == '|' || *dst == '-')) {
		*dst-- = '\0';
	}
}

void WinTitle_Update(float frametime) {
	char newTitle[WINTITLE_MAX_LEN];

	if (!cl_dynamicTitle || !cl_dynamicTitle->integer) return;

	frameCount++;
	fpsTimer += frametime;
	if (fpsTimer >= 1.0f) {
		fps = frameCount;
		frameCount = 0;
		fpsTimer -= 1.0f;
		Com_sprintf(sections[WINTITLE_SECTION_FPS], sizeof(sections[0]), "%d FPS", fps);
	}

	if (eventTimer > 0) {
		eventTimer -= frametime;
		if (eventTimer <= 0) {
			eventText[0] = '\0';
			eventTimer = 0;
		}
	}

	updateInterval = cl_titleUpdateRate ? cl_titleUpdateRate->value : 0.5f;
	if (updateInterval < 0.1f) updateInterval = 0.1f;

	updateTimer += frametime;
	if (updateTimer < updateInterval) return;
	updateTimer = 0;

	WinTitle_BuildTitle(newTitle, sizeof(newTitle));

	if (Q_stricmp(newTitle, lastTitle)) {
		Q_strncpyz(lastTitle, newTitle, sizeof(lastTitle));
		Sys_UpdateWindowTitle(newTitle);
	}
}

void WinTitle_SetSection(winTitleSection_t section, const char *text) {
	if (section < 0 || section >= WINTITLE_SECTION_COUNT) return;
	if (text) Q_strncpyz(sections[section], text, sizeof(sections[0]));
	else sections[section][0] = '\0';
}

void WinTitle_ClearSection(winTitleSection_t section) {
	if (section >= 0 && section < WINTITLE_SECTION_COUNT)
		sections[section][0] = '\0';
}

void WinTitle_SetFormat(const char *format) {
	if (format) Q_strncpyz(titleFormat, format, sizeof(titleFormat));
}

void WinTitle_SetEvent(const char *event, float duration) {
	if (event) Q_strncpyz(eventText, event, sizeof(eventText));
	else eventText[0] = '\0';
	eventTimer = duration > 0 ? duration : 3.0f;
}

void WinTitle_SetScore(int score1, int score2) {
	Com_sprintf(sections[WINTITLE_SECTION_SCORE], sizeof(sections[0]), "%d - %d", score1, score2);
}

void WinTitle_SetMapName(const char *mapName) {
	WinTitle_SetSection(WINTITLE_SECTION_MAP_NAME, mapName);
}

void WinTitle_SetPlayerName(const char *name) {
	WinTitle_SetSection(WINTITLE_SECTION_PLAYER_NAME, name);
}

void WinTitle_SetCustom(const char *text) {
	WinTitle_SetSection(WINTITLE_SECTION_CUSTOM, text);
}

void WinTitle_ForceUpdate(void) {
	updateTimer = updateInterval + 1.0f;
}
