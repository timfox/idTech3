#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/i18n.h"
#include "cg_local.h"
#include "cg_dialogue.h"

static dialogueDisplay_t dialogueDisplays[MAX_DIALOGUE_DISPLAY];
static int numActiveDialogues = 0;

/*
=================
CG_Dialogue_FindDisplay
=================
Find dialogue display by entity number
=================
*/
static dialogueDisplay_t *CG_Dialogue_FindDisplay(int entityNum)
{
	int i;
	
	for (i = 0; i < MAX_DIALOGUE_DISPLAY; i++) {
		if (dialogueDisplays[i].visible && dialogueDisplays[i].entityNum == entityNum) {
			return &dialogueDisplays[i];
		}
	}
	
	return NULL;
}

/*
=================
CG_Dialogue_FindFreeSlot
=================
Find a free dialogue display slot
=================
*/
static int CG_Dialogue_FindFreeSlot(void)
{
	int i;
	
	for (i = 0; i < MAX_DIALOGUE_DISPLAY; i++) {
		if (!dialogueDisplays[i].visible) {
			return i;
		}
	}
	
	return -1;
}

/*
=================
CG_Dialogue_Init
=================
Initialize client-side dialogue system
=================
*/
void CG_Dialogue_Init(void)
{
	int i;
	
	cg_dialogue_enabled = Cvar_Get("cg_dialogue_enabled", "1", CVAR_ARCHIVE);
	Cvar_SetDescription(cg_dialogue_enabled, "Enable dialogue system display");
	
	cg_dialogue_font = Cvar_Get("cg_dialogue_font", "0", CVAR_ARCHIVE);
	Cvar_SetDescription(cg_dialogue_font, "Font number for dialogue text");
	
	cg_dialogue_scale = Cvar_Get("cg_dialogue_scale", "1.0", CVAR_ARCHIVE);
	Cvar_SetDescription(cg_dialogue_scale, "Scale factor for dialogue text");
	
	cg_dialogue_x = Cvar_Get("cg_dialogue_x", "320", CVAR_ARCHIVE);
	Cvar_SetDescription(cg_dialogue_x, "X position for dialogue box");
	
	cg_dialogue_y = Cvar_Get("cg_dialogue_y", "400", CVAR_ARCHIVE);
	Cvar_SetDescription(cg_dialogue_y, "Y position for dialogue box");
	
	// Initialize dialogue displays
	for (i = 0; i < MAX_DIALOGUE_DISPLAY; i++) {
		Com_Memset(&dialogueDisplays[i], 0, sizeof(dialogueDisplay_t));
		dialogueDisplays[i].entityNum = -1;
		dialogueDisplays[i].playerNum = -1;
	}
	
	numActiveDialogues = 0;
}

/*
=================
CG_Dialogue_Shutdown
=================
Shutdown client-side dialogue system
=================
*/
void CG_Dialogue_Shutdown(void)
{
	int i;
	
	for (i = 0; i < MAX_DIALOGUE_DISPLAY; i++) {
		if (dialogueDisplays[i].visible) {
			CG_Dialogue_Stop(dialogueDisplays[i].entityNum);
		}
	}
	
	numActiveDialogues = 0;
}

/*
=================
CG_Dialogue_Start
=================
Start displaying a dialogue
=================
*/
void CG_Dialogue_Start(int entityNum, int playerNum, const char *speaker, const char *text, int numChoices, const char **choices, int duration, qboolean skipable)
{
	int slot;
	dialogueDisplay_t *display;
	int i;
	
	if (!cg_dialogue_enabled || !cg_dialogue_enabled->integer)
		return;
	
	// Check if already displaying for this entity
	display = CG_Dialogue_FindDisplay(entityNum);
	if (display) {
		CG_Dialogue_Stop(entityNum);
	}
	
	// Find free slot
	slot = CG_Dialogue_FindFreeSlot();
	if (slot < 0) {
		Com_Printf("CG_Dialogue_Start: Maximum dialogue displays reached\n");
		return;
	}
	
	display = &dialogueDisplays[slot];
	Com_Memset(display, 0, sizeof(dialogueDisplay_t));
	
	display->entityNum = entityNum;
	display->playerNum = playerNum;
	Q_strncpyz(display->speaker, speaker, sizeof(display->speaker));
	
	// Translate text using i18n
	if (text && *text) {
		const char *translated = CL_Localize(text);
		Q_strncpyz(display->text, translated, sizeof(display->text));
	} else {
		display->text[0] = '\0';
	}
	
	display->numChoices = (numChoices > 8) ? 8 : numChoices;
	for (i = 0; i < display->numChoices; i++) {
		if (choices && choices[i]) {
			const char *translated = CL_Localize(choices[i]);
			Q_strncpyz(display->choices[i], translated, sizeof(display->choices[i]));
		} else {
			display->choices[i][0] = '\0';
		}
	}
	
	display->startTime = cg.time;
	display->duration = duration;
	display->skipable = skipable;
	display->visible = qtrue;
	display->alpha = 1.0f;
	
	numActiveDialogues++;
}

/*
=================
CG_Dialogue_Stop
=================
Stop displaying a dialogue
=================
*/
void CG_Dialogue_Stop(int entityNum)
{
	dialogueDisplay_t *display;
	
	display = CG_Dialogue_FindDisplay(entityNum);
	if (!display)
		return;
	
	display->visible = qfalse;
	display->entityNum = -1;
	display->playerNum = -1;
	numActiveDialogues--;
}

/*
=================
CG_Dialogue_IsActive
=================
Check if dialogue is active for entity
=================
*/
qboolean CG_Dialogue_IsActive(int entityNum)
{
	return CG_Dialogue_FindDisplay(entityNum) != NULL;
}

/*
=================
CG_Dialogue_SelectChoice
=================
Select a choice in the dialogue
=================
*/
void CG_Dialogue_SelectChoice(int entityNum, int choiceIndex)
{
	dialogueDisplay_t *display;
	
	display = CG_Dialogue_FindDisplay(entityNum);
	if (!display)
		return;
	
	if (choiceIndex < 0 || choiceIndex >= display->numChoices)
		return;
	
	// Send choice selection to server
	char cmd[64];
	Com_sprintf(cmd, sizeof(cmd), "dialogue_choice %d %d", entityNum, choiceIndex);
	trap_SendClientCommand(cmd);
}

/*
=================
CG_Dialogue_Advance
=================
Advance to next dialogue node
=================
*/
void CG_Dialogue_Advance(int entityNum)
{
	char cmd[64];
	Com_sprintf(cmd, sizeof(cmd), "dialogue_advance %d", entityNum);
	trap_SendClientCommand(cmd);
}

/*
=================
CG_Dialogue_Skip
=================
Skip current dialogue node
=================
*/
void CG_Dialogue_Skip(int entityNum)
{
	dialogueDisplay_t *display;
	
	display = CG_Dialogue_FindDisplay(entityNum);
	if (!display)
		return;
	
	if (!display->skipable)
		return;
	
	char cmd[64];
	Com_sprintf(cmd, sizeof(cmd), "dialogue_skip %d", entityNum);
	trap_SendClientCommand(cmd);
}

/*
=================
CG_Dialogue_Update
=================
Update dialogue displays (fade, auto-advance, etc.)
=================
*/
void CG_Dialogue_Update(int currentTime)
{
	int i;
	dialogueDisplay_t *display;
	float elapsed;
	
	if (!cg_dialogue_enabled || !cg_dialogue_enabled->integer)
		return;
	
	for (i = 0; i < MAX_DIALOGUE_DISPLAY; i++) {
		display = &dialogueDisplays[i];
		if (!display->visible)
			continue;
		
		elapsed = (currentTime - display->startTime) / 1000.0f;
		
		// Auto-advance if duration is set
		if (display->duration > 0 && elapsed >= display->duration) {
			CG_Dialogue_Advance(display->entityNum);
			continue;
		}
		
		// Fade in
		if (elapsed < DIALOGUE_FADE_TIME / 1000.0f) {
			display->alpha = elapsed / (DIALOGUE_FADE_TIME / 1000.0f);
			if (display->alpha > 1.0f)
				display->alpha = 1.0f;
		} else {
			display->alpha = 1.0f;
		}
	}
}

/*
=================
CG_Dialogue_DrawText
=================
Draw text with word wrapping
=================
*/
static void CG_Dialogue_DrawText(float x, float y, float scale, vec4_t color, const char *text, int maxWidth)
{
	char *textPtr;
	char line[256];
	int len;
	int charWidth;
	int lineWidth;
	int i;
	float drawY;
	
	if (!text || !*text)
		return;
	
	textPtr = (char *)text;
	drawY = y;
	lineWidth = 0;
	len = 0;
	
	while (*textPtr) {
		// Get character width (approximate)
		charWidth = 8; // Default character width
		
		// Check if adding this character would exceed max width
		if (lineWidth + charWidth > maxWidth && len > 0) {
			// Draw current line
			line[len] = '\0';
			CG_DrawStringExt(x, drawY, line, color, qfalse, qfalse, scale, scale, 0);
			drawY += 16 * scale;
			
			// Start new line
			len = 0;
			lineWidth = 0;
		}
		
		// Add character to line
		if (len < sizeof(line) - 1) {
			line[len++] = *textPtr;
			lineWidth += charWidth;
		}
		
		textPtr++;
	}
	
	// Draw remaining line
	if (len > 0) {
		line[len] = '\0';
		CG_DrawStringExt(x, drawY, line, color, qfalse, qfalse, scale, scale, 0);
	}
}

/*
=================
CG_Dialogue_Draw
=================
Draw all active dialogue displays
=================
*/
void CG_Dialogue_Draw(void)
{
	int i;
	dialogueDisplay_t *display;
	float x, y, scale;
	vec4_t bgColor;
	vec4_t textColor;
	vec4_t speakerColor;
	vec4_t choiceColor;
	int j;
	float choiceY;
	char choiceText[64];
	
	if (!cg_dialogue_enabled || !cg_dialogue_enabled->integer)
		return;
	
	if (numActiveDialogues == 0)
		return;
	
	scale = cg_dialogue_scale->value;
	x = cg_dialogue_x->value;
	y = cg_dialogue_y->value;
	
	// Background color (semi-transparent black)
	bgColor[0] = 0.0f;
	bgColor[1] = 0.0f;
	bgColor[2] = 0.0f;
	bgColor[3] = 0.7f;
	
	// Text colors
	textColor[0] = 1.0f;
	textColor[1] = 1.0f;
	textColor[2] = 1.0f;
	textColor[3] = 1.0f;
	
	speakerColor[0] = 1.0f;
	speakerColor[1] = 0.8f;
	speakerColor[2] = 0.2f;
	speakerColor[3] = 1.0f;
	
	choiceColor[0] = 0.8f;
	choiceColor[1] = 0.8f;
	choiceColor[2] = 1.0f;
	choiceColor[3] = 1.0f;
	
	for (i = 0; i < MAX_DIALOGUE_DISPLAY; i++) {
		display = &dialogueDisplays[i];
		if (!display->visible)
			continue;
		
		// Apply alpha
		bgColor[3] = 0.7f * display->alpha;
		textColor[3] = display->alpha;
		speakerColor[3] = display->alpha;
		choiceColor[3] = display->alpha;
		
		// Draw background box
		trap_R_SetColor(bgColor);
		CG_DrawPic(x - DIALOGUE_TEXT_WIDTH / 2, y - 20, DIALOGUE_TEXT_WIDTH, 200, cgs.media.whiteShader);
		
		// Draw speaker name
		if (display->speaker[0]) {
			char speakerLabel[128];
			Com_sprintf(speakerLabel, sizeof(speakerLabel), "%s:", display->speaker);
			CG_DrawStringExt(x - DIALOGUE_TEXT_WIDTH / 2 + 10, y, speakerLabel, speakerColor, qfalse, qfalse, scale, scale, 0);
		}
		
		// Draw dialogue text
		if (display->text[0]) {
			CG_Dialogue_DrawText(x - DIALOGUE_TEXT_WIDTH / 2 + 10, y + 30, scale, textColor, display->text, DIALOGUE_TEXT_WIDTH - 20);
		}
		
		// Draw choices
		choiceY = y + 100;
		for (j = 0; j < display->numChoices; j++) {
			if (display->choices[j][0]) {
				Com_sprintf(choiceText, sizeof(choiceText), "%d. %s", j + 1, display->choices[j]);
				CG_DrawStringExt(x - DIALOGUE_TEXT_WIDTH / 2 + 20, choiceY, choiceText, choiceColor, qfalse, qfalse, scale, scale, 0);
				choiceY += DIALOGUE_CHOICE_SPACING * scale;
			}
		}
		
		// Draw skip hint if skipable
		if (display->skipable) {
			vec4_t hintColor = {0.6f, 0.6f, 0.6f, display->alpha * 0.8f};
			CG_DrawStringExt(x + DIALOGUE_TEXT_WIDTH / 2 - 100, y + 180, "Press SPACE to skip", hintColor, qfalse, qfalse, scale * 0.8f, scale * 0.8f, 0);
		}
		
		// Reset color
		trap_R_SetColor(NULL);
	}
}

