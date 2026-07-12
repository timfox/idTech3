/*
===========================================================================
Copyright (C) 2026 Gopex LLC. All rights reserved.

This file is original work by Gopex LLC and is not derived from
existing id Tech 3 / ioquake3 code.
The engine framework is based on id Tech 3 (GPLv2).

Dynamic window title system.
Optionally updates the OS window title bar during gameplay to
show score, player status, map name, events, FPS, or custom text.
===========================================================================
*/

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "q_shared.h"

#define WINTITLE_MAX_SECTIONS  8
#define WINTITLE_MAX_LEN       256

typedef enum {
	WINTITLE_SECTION_GAME_NAME,
	WINTITLE_SECTION_MAP_NAME,
	WINTITLE_SECTION_SCORE,
	WINTITLE_SECTION_PLAYER_NAME,
	WINTITLE_SECTION_EVENT,
	WINTITLE_SECTION_FPS,
	WINTITLE_SECTION_CUSTOM,
	WINTITLE_SECTION_COUNT
} winTitleSection_t;

void WinTitle_Init(void);
void WinTitle_Update(float frametime);

void WinTitle_SetSection(winTitleSection_t section, const char *text);
void WinTitle_ClearSection(winTitleSection_t section);
void WinTitle_SetFormat(const char *format);
void WinTitle_SetEvent(const char *event, float duration);
void WinTitle_SetScore(int score1, int score2);
void WinTitle_SetMapName(const char *mapName);
void WinTitle_SetPlayerName(const char *name);
void WinTitle_SetCustom(const char *text);
void WinTitle_ForceUpdate(void);

#ifdef __cplusplus
}
#endif
