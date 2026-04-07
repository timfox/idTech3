/*
===========================================================================
Engine-level telemetry, replay timing, save slots, quests, and dialogue
lines for Lua/script integration (client-side game systems).
===========================================================================
*/

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "g_engine_systems.h"
#include <string.h>

#define TELEMETRY_MAX 64
#define SAVE_SLOTS 8
#define QUEST_MAX 64
#define DIALOGUE_MAX 32

typedef struct {
	char name[64];
	double value;
} telemEntry_t;

static telemEntry_t s_telem[TELEMETRY_MAX];
static int s_telemCount;
static cvar_t *cv_engine_telemetry;

static int s_replayFrame;
static int s_replayBaseTime;
static qboolean s_replayActive;
static cvar_t *cv_engine_replay;

typedef struct {
	char label[128];
	qboolean used;
} saveSlot_t;

static saveSlot_t s_saves[SAVE_SLOTS];
static int s_lastSaveSlot;

typedef struct {
	char id[64];
	char title[128];
	char stage[128];
	qboolean used;
} questEntry_t;

static questEntry_t s_quests[QUEST_MAX];
static int s_questCount;

typedef struct {
	char speaker[64];
	char text[512];
	qboolean used;
} dialogueLine_t;

static dialogueLine_t s_dialogue[DIALOGUE_MAX];
static int s_dialogueCount;

/* ---- Telemetry ---- */

void EngineTelemetry_Init( void ) {
	Com_Memset( s_telem, 0, sizeof( s_telem ) );
	s_telemCount = 0;
	cv_engine_telemetry = Cvar_Get( "engine_telemetry", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cv_engine_telemetry, "Enable engine telemetry counters (Engine.Telemetry.* Lua API)." );
	Com_Printf( "EngineTelemetry: initialized (cvar engine_telemetry=%d)\n", cv_engine_telemetry ? cv_engine_telemetry->integer : 1 );
}

void EngineTelemetry_Shutdown( void ) {
	s_telemCount = 0;
}

void EngineTelemetry_Record( const char *name, double value ) {
	int i;
	if ( !name || !name[0] || !cv_engine_telemetry || !cv_engine_telemetry->integer ) {
		return;
	}
	for ( i = 0; i < s_telemCount; i++ ) {
		if ( !Q_stricmp( s_telem[i].name, name ) ) {
			s_telem[i].value = value;
			return;
		}
	}
	if ( s_telemCount >= TELEMETRY_MAX ) {
		return;
	}
	Q_strncpyz( s_telem[s_telemCount].name, name, sizeof( s_telem[0].name ) );
	s_telem[s_telemCount].value = value;
	s_telemCount++;
}

double EngineTelemetry_Get( const char *name ) {
	int i;
	if ( !name ) {
		return 0.0;
	}
	for ( i = 0; i < s_telemCount; i++ ) {
		if ( !Q_stricmp( s_telem[i].name, name ) ) {
			return s_telem[i].value;
		}
	}
	return 0.0;
}

void EngineTelemetry_Clear( void ) {
	s_telemCount = 0;
}

/* ---- Replay / deterministic frame index ---- */

void EngineReplay_Init( void ) {
	s_replayFrame = 0;
	s_replayBaseTime = 0;
	s_replayActive = qfalse;
	cv_engine_replay = Cvar_Get( "engine_replay", "1", CVAR_ARCHIVE_ND );
	Cvar_SetDescription( cv_engine_replay, "Drive Engine.Replay frame index from client snapshots (serverTime)." );
	Com_Printf( "EngineReplay: frame counter ready (cvar engine_replay=%d)\n",
		cv_engine_replay ? cv_engine_replay->integer : 1 );
}

void EngineReplay_Shutdown( void ) {
	s_replayActive = qfalse;
	s_replayFrame = 0;
}

void EngineReplay_BeginFrame( int serverTime ) {
	if ( !cv_engine_replay || !cv_engine_replay->integer ) {
		return;
	}
	if ( !s_replayActive ) {
		s_replayBaseTime = serverTime;
		s_replayFrame = 0;
		s_replayActive = qtrue;
	}
	s_replayFrame++;
	(void)serverTime;
}

void EngineReplay_EndFrame( void ) {
	if ( !cv_engine_replay || !cv_engine_replay->integer ) {
		return;
	}
	/* Hook for future record buffer */
}

int EngineReplay_GetFrameIndex( void ) {
	return s_replayFrame;
}

int EngineReplay_GetBaseTime( void ) {
	return s_replayBaseTime;
}

/* ---- Save slots (client-local metadata in memory + optional file) ---- */

void EngineSave_Init( void ) {
	Com_Memset( s_saves, 0, sizeof( s_saves ) );
	s_lastSaveSlot = -1;
	Com_Printf( "EngineSave: %d logical slots (Engine.Save.*)\n", SAVE_SLOTS );
}

void EngineSave_Shutdown( void ) {
	Com_Memset( s_saves, 0, sizeof( s_saves ) );
	s_lastSaveSlot = -1;
}

qboolean EngineSave_WriteSlot( int slot, const char *label ) {
	char path[MAX_OSPATH];
	fileHandle_t f;
	int len;

	if ( slot < 0 || slot >= SAVE_SLOTS || !label ) {
		return qfalse;
	}
	Q_strncpyz( s_saves[slot].label, label, sizeof( s_saves[0].label ) );
	s_saves[slot].used = qtrue;
	s_lastSaveSlot = slot;

	Com_sprintf( path, sizeof( path ), "save/engine_slot_%d.txt", slot );
	len = (int)strlen( label );
	f = FS_FOpenFileWrite( path );
	if ( f ) {
		FS_Write( label, len, f );
		FS_FCloseFile( f );
	}
	return qtrue;
}

qboolean EngineSave_ReadSlot( int slot, char *labelOut, int labelLen ) {
	char path[MAX_OSPATH];
	fileHandle_t f;
	int len;

	if ( slot < 0 || slot >= SAVE_SLOTS || !labelOut || labelLen < 1 ) {
		return qfalse;
	}
	labelOut[0] = '\0';
	Com_sprintf( path, sizeof( path ), "save/engine_slot_%d.txt", slot );
	len = FS_FOpenFileRead( path, &f, qfalse );
	if ( len > 0 && f ) {
		if ( len >= labelLen ) {
			len = labelLen - 1;
		}
		FS_Read( labelOut, len, f );
		labelOut[len] = '\0';
		FS_FCloseFile( f );
		return qtrue;
	}
	if ( s_saves[slot].used ) {
		Q_strncpyz( labelOut, s_saves[slot].label, labelLen );
		return qtrue;
	}
	return qfalse;
}

int EngineSave_LastSlot( void ) {
	return s_lastSaveSlot;
}

/* ---- Quests ---- */

void EngineQuest_Init( void ) {
	s_questCount = 0;
	Com_Memset( s_quests, 0, sizeof( s_quests ) );
	Com_Printf( "EngineQuest: module ready (Engine.Quest.*)\n" );
}

void EngineQuest_Shutdown( void ) {
	s_questCount = 0;
}

int EngineQuest_Add( const char *id, const char *title, const char *stage ) {
	int i;
	if ( !id || !id[0] ) {
		return -1;
	}
	for ( i = 0; i < s_questCount; i++ ) {
		if ( s_quests[i].used && !Q_stricmp( s_quests[i].id, id ) ) {
			if ( title ) {
				Q_strncpyz( s_quests[i].title, title, sizeof( s_quests[0].title ) );
			}
			if ( stage ) {
				Q_strncpyz( s_quests[i].stage, stage, sizeof( s_quests[0].stage ) );
			}
			return i;
		}
	}
	if ( s_questCount >= QUEST_MAX ) {
		return -1;
	}
	i = s_questCount++;
	s_quests[i].used = qtrue;
	Q_strncpyz( s_quests[i].id, id, sizeof( s_quests[0].id ) );
	Q_strncpyz( s_quests[i].title, title ? title : "", sizeof( s_quests[0].title ) );
	Q_strncpyz( s_quests[i].stage, stage ? stage : "active", sizeof( s_quests[0].stage ) );
	return i;
}

qboolean EngineQuest_SetStage( const char *id, const char *stage ) {
	int i;
	if ( !id || !stage ) {
		return qfalse;
	}
	for ( i = 0; i < s_questCount; i++ ) {
		if ( s_quests[i].used && !Q_stricmp( s_quests[i].id, id ) ) {
			Q_strncpyz( s_quests[i].stage, stage, sizeof( s_quests[0].stage ) );
			return qtrue;
		}
	}
	return qfalse;
}

const char *EngineQuest_GetStage( const char *id ) {
	int i;
	if ( !id ) {
		return "";
	}
	for ( i = 0; i < s_questCount; i++ ) {
		if ( s_quests[i].used && !Q_stricmp( s_quests[i].id, id ) ) {
			return s_quests[i].stage;
		}
	}
	return "";
}

int EngineQuest_Count( void ) {
	return s_questCount;
}

/* ---- Dialogue ---- */

void EngineDialogue_Init( void ) {
	s_dialogueCount = 0;
	Com_Memset( s_dialogue, 0, sizeof( s_dialogue ) );
	Com_Printf( "EngineDialogue: buffer ready (Engine.Dialogue.*)\n" );
}

void EngineDialogue_Shutdown( void ) {
	s_dialogueCount = 0;
}

int EngineDialogue_Start( const char *speaker, const char *text ) {
	int i;
	if ( !text ) {
		return -1;
	}
	if ( s_dialogueCount >= DIALOGUE_MAX ) {
		memmove( s_dialogue, s_dialogue + 1, sizeof( s_dialogue[0] ) * (DIALOGUE_MAX - 1) );
		s_dialogueCount = DIALOGUE_MAX - 1;
	}
	i = s_dialogueCount++;
	s_dialogue[i].used = qtrue;
	Q_strncpyz( s_dialogue[i].speaker, speaker ? speaker : "", sizeof( s_dialogue[0].speaker ) );
	Q_strncpyz( s_dialogue[i].text, text, sizeof( s_dialogue[0].text ) );
	return i;
}

void EngineDialogue_Clear( void ) {
	s_dialogueCount = 0;
	Com_Memset( s_dialogue, 0, sizeof( s_dialogue ) );
}

int EngineDialogue_ActiveCount( void ) {
	return s_dialogueCount;
}
