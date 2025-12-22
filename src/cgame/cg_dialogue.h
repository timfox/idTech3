#ifndef __CG_DIALOGUE_H__
#define __CG_DIALOGUE_H__

#include "../common/q_shared.h"

#define MAX_DIALOGUE_DISPLAY		8
#define DIALOGUE_TEXT_WIDTH		600
#define DIALOGUE_CHOICE_SPACING	24
#define DIALOGUE_FADE_TIME		300

// Dialogue display structure (client-side representation)
typedef struct {
	int			entityNum;
	int			playerNum;
	char		speaker[MAX_DIALOGUE_NAME];
	char		text[MAX_DIALOGUE_TEXT];
	int			numChoices;
	char		choices[8][MAX_DIALOGUE_TEXT];
	int			startTime;
	int			duration;
	qboolean	visible;
	qboolean	skipable;
	float		alpha;
} dialogueDisplay_t;

// Client-side dialogue functions
void	CG_Dialogue_Init(void);
void	CG_Dialogue_Shutdown(void);
void	CG_Dialogue_Start(int entityNum, int playerNum, const char *speaker, const char *text, int numChoices, const char **choices, int duration, qboolean skipable);
void	CG_Dialogue_Stop(int entityNum);
void	CG_Dialogue_Update(int currentTime);
void	CG_Dialogue_Draw(void);
qboolean CG_Dialogue_IsActive(int entityNum);
void	CG_Dialogue_SelectChoice(int entityNum, int choiceIndex);
void	CG_Dialogue_Advance(int entityNum);
void	CG_Dialogue_Skip(int entityNum);

#endif // __CG_DIALOGUE_H__

