#ifndef __G_DIALOGUE_H__
#define __G_DIALOGUE_H__

#include "../common/q_shared.h"

#define MAX_DIALOGUE_NAME		64
#define MAX_DIALOGUE_TEXT		512
#define MAX_DIALOGUE_CHOICES	8
#define MAX_DIALOGUE_NODES		256
#define MAX_DIALOGUE_ENTRIES	1024

// Dialogue node types
typedef enum {
	DIALOGUE_NODE_TEXT,		// Simple text node
	DIALOGUE_NODE_CHOICE,	// Choice node with multiple options
	DIALOGUE_NODE_ACTION,	// Action node (triggers script/event)
	DIALOGUE_NODE_CONDITION	// Conditional node (checks condition before proceeding)
} dialogueNodeType_t;

// Dialogue choice structure
typedef struct {
	char		text[MAX_DIALOGUE_TEXT];
	int			nextNode;		// Node to go to when chosen (-1 = end)
	int			condition;		// Condition ID to check (-1 = no condition)
	qboolean	disabled;		// Is this choice disabled?
} dialogueChoice_t;

// Dialogue node structure
typedef struct {
	int					nodeId;
	dialogueNodeType_t	type;
	char				speaker[MAX_DIALOGUE_NAME];	// Entity name or "player"
	char				text[MAX_DIALOGUE_TEXT];
	char				voiceFile[MAX_QPATH];		// Voice audio file
	int					numChoices;
	dialogueChoice_t	choices[MAX_DIALOGUE_CHOICES];
	int					nextNode;					// Default next node (-1 = end)
	int					condition;					// Condition ID (-1 = no condition)
	int					action;						// Action ID to trigger (-1 = no action)
	float				duration;					// Auto-advance duration (0 = manual)
	qboolean			skipable;					// Can player skip this dialogue
} dialogueNode_t;

// Dialogue tree structure
typedef struct {
	char			name[MAX_DIALOGUE_NAME];
	char			fileName[MAX_QPATH];
	int				startNode;
	int				numNodes;
	dialogueNode_t	*nodes;
	qboolean		active;
	int				currentNode;
	int				entityNum;					// Entity this dialogue is attached to
	int				playerNum;					// Player involved in dialogue
	int				startTime;					// When dialogue started
} dialogueTree_t;

// Dialogue condition function type
typedef qboolean (*dialogueConditionFunc_t)(int entityNum, int playerNum, int conditionId);

// Dialogue action function type
typedef void (*dialogueActionFunc_t)(int entityNum, int playerNum, int actionId);

// Dialogue system functions
void		G_Dialogue_Init(void);
void		G_Dialogue_Shutdown(void);
qboolean	G_Dialogue_LoadTree(const char *filename);
void		G_Dialogue_UnloadTree(const char *name);
dialogueTree_t *G_Dialogue_GetTree(const char *name);
qboolean	G_Dialogue_Start(int entityNum, int playerNum, const char *dialogueName);
void		G_Dialogue_SetStartTime(int entityNum, int startTime);
void		G_Dialogue_Stop(int entityNum);
void		G_Dialogue_Update(int currentTime);
qboolean	G_Dialogue_IsActive(int entityNum);
dialogueTree_t *G_Dialogue_GetActive(int entityNum);

// Dialogue node navigation
qboolean	G_Dialogue_SelectChoice(int entityNum, int choiceIndex);
void		G_Dialogue_Advance(int entityNum);
void		G_Dialogue_Skip(int entityNum);

// Dialogue conditions and actions
void		G_Dialogue_RegisterCondition(int conditionId, dialogueConditionFunc_t func);
void		G_Dialogue_RegisterAction(int actionId, dialogueActionFunc_t func);
qboolean	G_Dialogue_CheckCondition(int entityNum, int playerNum, int conditionId);
void		G_Dialogue_ExecuteAction(int entityNum, int playerNum, int actionId);

#endif // __G_DIALOGUE_H__

