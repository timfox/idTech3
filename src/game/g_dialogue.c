#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../qcommon/i18n.h"
#include "g_dialogue.h"

// Memory allocation - use Z_Malloc with TAG_GENERAL
// Time will be passed as parameter to update function
#define G_Alloc(size) Z_Malloc(size, TAG_GENERAL, qtrue)
#define G_Free(ptr) Z_Free(ptr)

#define MAX_DIALOGUE_TREES		64

static dialogueTree_t dialogueTrees[MAX_DIALOGUE_TREES];
static int numDialogueTrees = 0;

static dialogueConditionFunc_t conditionFuncs[256];
static dialogueActionFunc_t actionFuncs[256];

static cvar_t *g_dialogue_enabled;

/*
=================
G_Dialogue_FindTree
=================
Find a dialogue tree by name
=================
*/
static dialogueTree_t *G_Dialogue_FindTree(const char *name)
{
	int i;
	
	if (!name || !*name)
		return NULL;
	
	for (i = 0; i < numDialogueTrees; i++) {
		if (Q_stricmp(dialogueTrees[i].name, name) == 0) {
			return &dialogueTrees[i];
		}
	}
	
	return NULL;
}

/*
=================
G_Dialogue_FindFreeSlot
=================
Find a free dialogue tree slot
=================
*/
static int G_Dialogue_FindFreeSlot(void)
{
	int i;
	
	for (i = 0; i < MAX_DIALOGUE_TREES; i++) {
		if (!dialogueTrees[i].nodes || dialogueTrees[i].numNodes == 0) {
			return i;
		}
	}
	
	return -1;
}

/*
=================
G_Dialogue_ParseNode
=================
Parse a single dialogue node from file
=================
*/
static qboolean G_Dialogue_ParseNode(dialogueTree_t *tree, dialogueNode_t *node, const char *buffer, int *offset)
{
	char line[MAX_DIALOGUE_TEXT];
	char key[MAX_DIALOGUE_NAME];
	char value[MAX_DIALOGUE_TEXT];
	char *linePtr;
	int len;
	
	if (!tree || !node || !buffer || !offset)
		return qfalse;
	
	Com_Memset(node, 0, sizeof(dialogueNode_t));
	node->nextNode = -1;
	node->condition = -1;
	node->action = -1;
	node->duration = 0.0f;
	node->skipable = qtrue;
	
	// Parse node properties
	while (*offset < strlen(buffer)) {
		// Get next line
		linePtr = (char *)buffer + *offset;
		len = 0;
		while (linePtr[len] != '\n' && linePtr[len] != '\0' && len < sizeof(line) - 1) {
			line[len] = linePtr[len];
			len++;
		}
		line[len] = '\0';
		*offset += len + 1;
		
		// Skip empty lines and comments
		if (len == 0 || line[0] == '#' || line[0] == ';') {
			if (linePtr[len] == '\0')
				break;
			continue;
		}
		
		// Check for node end marker
		if (Q_stricmp(line, "endnode") == 0 || Q_stricmp(line, "}") == 0) {
			break;
		}
		
		// Parse key=value pairs
		char *equals = strchr(line, '=');
		if (!equals) {
			// Try to parse as node type
			if (Q_stricmp(line, "node") == 0 || Q_stricmp(line, "{") == 0) {
				continue; // Node start marker
			}
			continue;
		}
		
		*equals = '\0';
		Q_strncpyz(key, line, sizeof(key));
		Q_strncpyz(value, equals + 1, sizeof(value));
		
		// Trim whitespace
		Q_TrimWhitespace(key);
		Q_TrimWhitespace(value);
		
		// Parse properties
		if (Q_stricmp(key, "id") == 0) {
			node->nodeId = atoi(value);
		} else if (Q_stricmp(key, "type") == 0) {
			if (Q_stricmp(value, "text") == 0) {
				node->type = DIALOGUE_NODE_TEXT;
			} else if (Q_stricmp(value, "choice") == 0) {
				node->type = DIALOGUE_NODE_CHOICE;
			} else if (Q_stricmp(value, "action") == 0) {
				node->type = DIALOGUE_NODE_ACTION;
			} else if (Q_stricmp(value, "condition") == 0) {
				node->type = DIALOGUE_NODE_CONDITION;
			}
		} else if (Q_stricmp(key, "speaker") == 0) {
			Q_strncpyz(node->speaker, value, sizeof(node->speaker));
		} else if (Q_stricmp(key, "text") == 0) {
			Q_strncpyz(node->text, value, sizeof(node->text));
		} else if (Q_stricmp(key, "voice") == 0) {
			Q_strncpyz(node->voiceFile, value, sizeof(node->voiceFile));
		} else if (Q_stricmp(key, "next") == 0) {
			node->nextNode = atoi(value);
		} else if (Q_stricmp(key, "condition") == 0) {
			node->condition = atoi(value);
		} else if (Q_stricmp(key, "action") == 0) {
			node->action = atoi(value);
		} else if (Q_stricmp(key, "duration") == 0) {
			node->duration = Q_atof(value);
		} else if (Q_stricmp(key, "skipable") == 0) {
			node->skipable = (Q_stricmp(value, "true") == 0 || Q_stricmp(value, "1") == 0);
		} else if (Q_stricmp(key, "choice") == 0) {
			// Parse choice: "text=value,next=nodeId,condition=condId"
			if (node->numChoices < MAX_DIALOGUE_CHOICES) {
				dialogueChoice_t *choice = &node->choices[node->numChoices];
				char *comma;
				char choiceLine[MAX_DIALOGUE_TEXT];
				
				Q_strncpyz(choiceLine, value, sizeof(choiceLine));
				choice->nextNode = -1;
				choice->condition = -1;
				choice->disabled = qfalse;
				
				// Parse choice properties
				char *token = choiceLine;
				while (token && *token) {
					comma = strchr(token, ',');
					if (comma)
						*comma = '\0';
					
					char *eq = strchr(token, '=');
					if (eq) {
						*eq = '\0';
						char *ckey = token;
						char *cvalue = eq + 1;
						Q_TrimWhitespace(ckey);
						Q_TrimWhitespace(cvalue);
						
						if (Q_stricmp(ckey, "text") == 0) {
							Q_strncpyz(choice->text, cvalue, sizeof(choice->text));
						} else if (Q_stricmp(ckey, "next") == 0) {
							choice->nextNode = atoi(cvalue);
						} else if (Q_stricmp(ckey, "condition") == 0) {
							choice->condition = atoi(cvalue);
						} else if (Q_stricmp(ckey, "disabled") == 0) {
							choice->disabled = (Q_stricmp(cvalue, "true") == 0 || Q_stricmp(cvalue, "1") == 0);
						}
					}
					
					if (comma) {
						token = comma + 1;
					} else {
						break;
					}
				}
				
				node->numChoices++;
			}
		}
		
		if (linePtr[len] == '\0')
			break;
	}
	
	return qtrue;
}

/*
=================
G_Dialogue_LoadTree
=================
Load a dialogue tree from file
Format: INI-like with node blocks
=================
*/
qboolean G_Dialogue_LoadTree(const char *filename)
{
	fileHandle_t f;
	int len;
	char *buffer;
	char line[MAX_DIALOGUE_TEXT];
	char key[MAX_DIALOGUE_NAME];
	char value[MAX_DIALOGUE_TEXT];
	dialogueTree_t *tree;
	int slot;
	int offset = 0;
	int nodeIndex = 0;
	
	if (!filename || !*filename)
		return qfalse;
	
	if (!g_dialogue_enabled || !g_dialogue_enabled->integer)
		return qfalse;
	
	// Check if already loaded
	tree = G_Dialogue_FindTree(filename);
	if (tree) {
		return qtrue; // Already loaded
	}
	
	// Find free slot
	slot = G_Dialogue_FindFreeSlot();
	if (slot < 0) {
		Com_Printf("G_Dialogue_LoadTree: Maximum dialogue trees reached\n");
		return qfalse;
	}
	
	tree = &dialogueTrees[slot];
	Com_Memset(tree, 0, sizeof(dialogueTree_t));
	
	// Load file
	len = FS_FOpenFileRead(filename, &f, qfalse);
	if (len <= 0 || !f) {
		Com_Printf("G_Dialogue_LoadTree: Could not load %s\n", filename);
		return qfalse;
	}
	
	buffer = (char *)Z_Malloc(len + 1);
	if (!buffer) {
		FS_FCloseFile(f);
		return qfalse;
	}
	
	FS_Read(buffer, len, f);
	buffer[len] = '\0';
	FS_FCloseFile(f);
	
	// Parse dialogue tree header
	Q_strncpyz(tree->fileName, filename, sizeof(tree->fileName));
	
	// Extract name from filename
	const char *nameStart = strrchr(filename, '/');
	if (nameStart) {
		nameStart++;
	} else {
		nameStart = filename;
	}
	const char *nameEnd = strrchr(nameStart, '.');
	if (nameEnd) {
		Q_strncpyz(tree->name, nameStart, nameEnd - nameStart + 1);
		tree->name[nameEnd - nameStart] = '\0';
	} else {
		Q_strncpyz(tree->name, nameStart, sizeof(tree->name));
	}
	
	// Count nodes first
	int nodeCount = 0;
	int tempOffset = 0;
	while (tempOffset < len) {
		if (strstr(buffer + tempOffset, "node") || strstr(buffer + tempOffset, "{")) {
			nodeCount++;
		}
		tempOffset++;
	}
	
	if (nodeCount == 0) {
		// Try alternative format: count lines starting with numbers (node IDs)
		nodeCount = 1; // At least one node
	}
	
	// Allocate nodes
	tree->numNodes = nodeCount;
	tree->nodes = (dialogueNode_t *)G_Alloc(sizeof(dialogueNode_t) * nodeCount);
	Com_Memset(tree->nodes, 0, sizeof(dialogueNode_t) * nodeCount);
	
	// Parse nodes
	offset = 0;
	nodeIndex = 0;
	while (offset < len && nodeIndex < nodeCount) {
		// Look for node start
		char *nodeStart = strstr(buffer + offset, "node");
		if (!nodeStart) {
			nodeStart = strstr(buffer + offset, "{");
		}
		
		if (!nodeStart) {
			break;
		}
		
		offset = nodeStart - buffer;
		if (nodeStart[0] == '{' || (nodeStart[0] == 'n' && nodeStart[4] == '\n')) {
			offset += (nodeStart[0] == '{') ? 1 : 5;
		} else {
			offset += 4;
		}
		
		// Skip whitespace
		while (offset < len && (buffer[offset] == ' ' || buffer[offset] == '\t')) {
			offset++;
		}
		
		// Parse node
		if (G_Dialogue_ParseNode(tree, &tree->nodes[nodeIndex], buffer, &offset)) {
			nodeIndex++;
		} else {
			break;
		}
	}
	
	tree->numNodes = nodeIndex;
	tree->startNode = (nodeIndex > 0) ? tree->nodes[0].nodeId : -1;
	
	Z_Free(buffer);
	
	if (tree->numNodes == 0) {
		Com_Printf("G_Dialogue_LoadTree: No nodes found in %s\n", filename);
		if (tree->nodes) {
			G_Free(tree->nodes);
			tree->nodes = NULL;
		}
		return qfalse;
	}
	
	numDialogueTrees++;
	Com_Printf("G_Dialogue_LoadTree: Loaded %s with %d nodes\n", tree->name, tree->numNodes);
	return qtrue;
}

/*
=================
G_Dialogue_UnloadTree
=================
Unload a dialogue tree
=================
*/
void G_Dialogue_UnloadTree(const char *name)
{
	dialogueTree_t *tree;
	
	tree = G_Dialogue_FindTree(name);
	if (!tree)
		return;
	
	if (tree->nodes) {
		G_Free(tree->nodes);
		tree->nodes = NULL;
	}
	
	Com_Memset(tree, 0, sizeof(dialogueTree_t));
	numDialogueTrees--;
}

/*
=================
G_Dialogue_GetTree
=================
Get a dialogue tree by name
=================
*/
dialogueTree_t *G_Dialogue_GetTree(const char *name)
{
	return G_Dialogue_FindTree(name);
}

/*
=================
G_Dialogue_FindNode
=================
Find a node by ID in a tree
=================
*/
static dialogueNode_t *G_Dialogue_FindNode(dialogueTree_t *tree, int nodeId)
{
	int i;
	
	if (!tree || !tree->nodes)
		return NULL;
	
	for (i = 0; i < tree->numNodes; i++) {
		if (tree->nodes[i].nodeId == nodeId) {
			return &tree->nodes[i];
		}
	}
	
	return NULL;
}

/*
=================
G_Dialogue_Start
=================
Start a dialogue tree
=================
*/
qboolean G_Dialogue_Start(int entityNum, int playerNum, const char *dialogueName)
{
	dialogueTree_t *tree;
	dialogueTree_t *activeTree;
	
	if (!g_dialogue_enabled || !g_dialogue_enabled->integer)
		return qfalse;
	
	// Check if entity already has active dialogue
	activeTree = G_Dialogue_GetActive(entityNum);
	if (activeTree) {
		Com_DPrintf("G_Dialogue_Start: Entity %d already has active dialogue\n", entityNum);
		return qfalse;
	}
	
	// Find dialogue tree
	tree = G_Dialogue_FindTree(dialogueName);
	if (!tree) {
		// Try to load it
		char filename[MAX_QPATH];
		Com_sprintf(filename, sizeof(filename), "dialogues/%s.dlg", dialogueName);
		if (!G_Dialogue_LoadTree(filename)) {
			Com_Printf("G_Dialogue_Start: Dialogue tree '%s' not found\n", dialogueName);
			return qfalse;
		}
		tree = G_Dialogue_FindTree(dialogueName);
		if (!tree)
			return qfalse;
	}
	
	// Start dialogue
	tree->active = qtrue;
	tree->currentNode = tree->startNode;
	tree->entityNum = entityNum;
	tree->playerNum = playerNum;
	tree->startTime = 0; // Will be set by caller with current time via G_Dialogue_SetStartTime
	
	// Execute start action if present
	dialogueNode_t *startNode = G_Dialogue_FindNode(tree, tree->startNode);
	if (startNode && startNode->action >= 0) {
		G_Dialogue_ExecuteAction(entityNum, playerNum, startNode->action);
	}
	
	return qtrue;
}

/*
=================
G_Dialogue_SetStartTime
=================
Set the start time for a dialogue (call after G_Dialogue_Start)
=================
*/
void G_Dialogue_SetStartTime(int entityNum, int startTime)
{
	dialogueTree_t *tree;
	
	tree = G_Dialogue_GetActive(entityNum);
	if (tree) {
		tree->startTime = startTime;
	}
}

/*
=================
G_Dialogue_Stop
=================
Stop active dialogue for an entity
=================
*/
void G_Dialogue_Stop(int entityNum)
{
	int i;
	
	for (i = 0; i < numDialogueTrees; i++) {
		if (dialogueTrees[i].active && dialogueTrees[i].entityNum == entityNum) {
			dialogueTrees[i].active = qfalse;
			dialogueTrees[i].currentNode = -1;
			return;
		}
	}
}

/*
=================
G_Dialogue_GetActive
=================
Get active dialogue tree for an entity
=================
*/
dialogueTree_t *G_Dialogue_GetActive(int entityNum)
{
	int i;
	
	for (i = 0; i < numDialogueTrees; i++) {
		if (dialogueTrees[i].active && dialogueTrees[i].entityNum == entityNum) {
			return &dialogueTrees[i];
		}
	}
	
	return NULL;
}

/*
=================
G_Dialogue_IsActive
=================
Check if entity has active dialogue
=================
*/
qboolean G_Dialogue_IsActive(int entityNum)
{
	return G_Dialogue_GetActive(entityNum) != NULL;
}

/*
=================
G_Dialogue_SelectChoice
=================
Select a choice in the current dialogue node
=================
*/
qboolean G_Dialogue_SelectChoice(int entityNum, int choiceIndex)
{
	dialogueTree_t *tree;
	dialogueNode_t *node;
	dialogueChoice_t *choice;
	
	tree = G_Dialogue_GetActive(entityNum);
	if (!tree)
		return qfalse;
	
	node = G_Dialogue_FindNode(tree, tree->currentNode);
	if (!node || node->type != DIALOGUE_NODE_CHOICE)
		return qfalse;
	
	if (choiceIndex < 0 || choiceIndex >= node->numChoices)
		return qfalse;
	
	choice = &node->choices[choiceIndex];
	if (choice->disabled)
		return qfalse;
	
	// Check condition
	if (choice->condition >= 0) {
		if (!G_Dialogue_CheckCondition(entityNum, tree->playerNum, choice->condition)) {
			return qfalse;
		}
	}
	
	// Advance to next node
	if (choice->nextNode >= 0) {
		tree->currentNode = choice->nextNode;
		node = G_Dialogue_FindNode(tree, tree->currentNode);
		if (node && node->action >= 0) {
			G_Dialogue_ExecuteAction(entityNum, tree->playerNum, node->action);
		}
	} else {
		// End dialogue
		G_Dialogue_Stop(entityNum);
	}
	
	return qtrue;
}

/*
=================
G_Dialogue_Advance
=================
Advance to next node in dialogue
=================
*/
void G_Dialogue_Advance(int entityNum)
{
	dialogueTree_t *tree;
	dialogueNode_t *node;
	
	tree = G_Dialogue_GetActive(entityNum);
	if (!tree)
		return;
	
	node = G_Dialogue_FindNode(tree, tree->currentNode);
	if (!node)
		return;
	
	// Check condition if present
	if (node->condition >= 0) {
		if (!G_Dialogue_CheckCondition(entityNum, tree->playerNum, node->condition)) {
			// Condition failed, end dialogue
			G_Dialogue_Stop(entityNum);
			return;
		}
	}
	
	// Execute action if present
	if (node->action >= 0) {
		G_Dialogue_ExecuteAction(entityNum, tree->playerNum, node->action);
	}
	
	// Advance to next node
	if (node->nextNode >= 0) {
		tree->currentNode = node->nextNode;
		node = G_Dialogue_FindNode(tree, tree->currentNode);
		if (node && node->action >= 0) {
			G_Dialogue_ExecuteAction(entityNum, tree->playerNum, node->action);
		}
	} else {
		// End dialogue
		G_Dialogue_Stop(entityNum);
	}
}

/*
=================
G_Dialogue_Skip
=================
Skip current dialogue node
=================
*/
void G_Dialogue_Skip(int entityNum)
{
	dialogueTree_t *tree;
	dialogueNode_t *node;
	
	tree = G_Dialogue_GetActive(entityNum);
	if (!tree)
		return;
	
	node = G_Dialogue_FindNode(tree, tree->currentNode);
	if (!node)
		return;
	
	if (!node->skipable)
		return;
	
	G_Dialogue_Advance(entityNum);
}

/*
=================
G_Dialogue_Update
=================
Update all active dialogues
=================
*/
void G_Dialogue_Update(int currentTime)
{
	int i;
	dialogueTree_t *tree;
	dialogueNode_t *node;
	
	if (!g_dialogue_enabled || !g_dialogue_enabled->integer)
		return;
	
	for (i = 0; i < numDialogueTrees; i++) {
		tree = &dialogueTrees[i];
		if (!tree->active)
			continue;
		
		node = G_Dialogue_FindNode(tree, tree->currentNode);
		if (!node) {
			G_Dialogue_Stop(tree->entityNum);
			continue;
		}
		
		// Auto-advance if duration is set
		if (node->duration > 0.0f && tree->startTime > 0) {
			float elapsed = (currentTime - tree->startTime) / 1000.0f;
			if (elapsed >= node->duration) {
				G_Dialogue_Advance(tree->entityNum);
			}
		}
	}
}

/*
=================
G_Dialogue_RegisterCondition
=================
Register a condition function
=================
*/
void G_Dialogue_RegisterCondition(int conditionId, dialogueConditionFunc_t func)
{
	if (conditionId >= 0 && conditionId < 256) {
		conditionFuncs[conditionId] = func;
	}
}

/*
=================
G_Dialogue_RegisterAction
=================
Register an action function
=================
*/
void G_Dialogue_RegisterAction(int actionId, dialogueActionFunc_t func)
{
	if (actionId >= 0 && actionId < 256) {
		actionFuncs[actionId] = func;
	}
}

/*
=================
G_Dialogue_CheckCondition
=================
Check a dialogue condition
=================
*/
qboolean G_Dialogue_CheckCondition(int entityNum, int playerNum, int conditionId)
{
	if (conditionId < 0 || conditionId >= 256)
		return qtrue; // No condition = always true
	
	if (conditionFuncs[conditionId]) {
		return conditionFuncs[conditionId](entityNum, playerNum, conditionId);
	}
	
	// Default: condition not registered = false
	return qfalse;
}

/*
=================
G_Dialogue_ExecuteAction
=================
Execute a dialogue action
=================
*/
void G_Dialogue_ExecuteAction(int entityNum, int playerNum, int actionId)
{
	if (actionId < 0 || actionId >= 256)
		return;
	
	if (actionFuncs[actionId]) {
		actionFuncs[actionId](entityNum, playerNum, actionId);
	}
}

/*
=================
G_Dialogue_Init
=================
Initialize dialogue system
=================
*/
void G_Dialogue_Init(void)
{
	int i;
	
	g_dialogue_enabled = Cvar_Get("g_dialogue_enabled", "1", CVAR_ARCHIVE);
	Cvar_SetDescription(g_dialogue_enabled, "Enable dialogue system");
	
	// Initialize dialogue trees
	for (i = 0; i < MAX_DIALOGUE_TREES; i++) {
		Com_Memset(&dialogueTrees[i], 0, sizeof(dialogueTree_t));
	}
	
	numDialogueTrees = 0;
	
	// Clear condition and action functions
	Com_Memset(conditionFuncs, 0, sizeof(conditionFuncs));
	Com_Memset(actionFuncs, 0, sizeof(actionFuncs));
	
	Com_Printf("Dialogue system initialized\n");
}

/*
=================
G_Dialogue_Shutdown
=================
Shutdown dialogue system
=================
*/
void G_Dialogue_Shutdown(void)
{
	int i;
	
	// Stop all active dialogues
	for (i = 0; i < numDialogueTrees; i++) {
		if (dialogueTrees[i].active) {
			G_Dialogue_Stop(dialogueTrees[i].entityNum);
		}
		if (dialogueTrees[i].nodes) {
			G_Free(dialogueTrees[i].nodes);
			dialogueTrees[i].nodes = NULL;
		}
	}
	
	numDialogueTrees = 0;
}

